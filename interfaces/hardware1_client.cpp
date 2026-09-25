#include "hardware1_client.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QVariantMap>

#include <utility>

namespace adrenalin::contracts::hardware1 {
namespace {

bool isCanonicalUuid(const QString &value)
{
    const QUuid uuid(value);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces).compare(value, Qt::CaseInsensitive) == 0;
}

bool isStableSubject(const QString &kind, const QString &id)
{
    static const QRegularExpression kindSyntax(QStringLiteral("^[A-Z][A-Z0-9_]{0,63}$"));
    if (!kindSyntax.match(kind).hasMatch() || id.isEmpty()) {
        return false;
    }
    if (kind == QLatin1String("PLATFORM") || kind == QLatin1String("NOTIFICATION")) {
        return id == QLatin1String("platform");
    }
    if (kind == QLatin1String("SERVICE")) {
        return id == QLatin1String("service.readiness");
    }
    return true;
}

bool isStableHardwareSubject(const QString &kind, const QString &id)
{
    return isValidSubjectKind(kind) && isStableSubject(kind, id);
}

bool isDeviceSnapshotValid(const QList<Device> &devices)
{
    QSet<QString> subjects;
    for (const Device &device : devices) {
        if (!isStableHardwareSubject(device.subjectKind, device.subjectId)
            || device.subjectKind == QLatin1String("PLATFORM")
            || device.identityEvidence.isEmpty()) {
            return false;
        }
        const QString key = device.subjectKind + QLatin1Char('\n') + device.subjectId;
        if (subjects.contains(key)) {
            return false;
        }
        subjects.insert(key);
    }
    return true;
}

} // namespace

Client::Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
               QObject *parent)
    : QObject(parent),
      hardwareProxy_(serviceName, objectPath, connection, this),
      serviceProxy_(serviceName, objectPath, connection, this),
      connection_(connection),
      ownerWatcher_(serviceName, connection, QDBusServiceWatcher::WatchForOwnerChange, this),
      serviceName_(std::move(serviceName)),
      objectPath_(std::move(objectPath))
{
    registerMetaTypes();
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &Client::onServiceOwnerChanged);
    connect(&serviceProxy_, &OrgAdrenalinlinuxSession1Service1Interface::EventPublished,
            this, &Client::onEventPublished);
    connect(&hardwareProxy_, &OrgAdrenalinlinuxSession1Hardware1Interface::InventoryChanged,
            this, &Client::onInventoryChanged);
    connect(&hardwareProxy_, &OrgAdrenalinlinuxSession1Hardware1Interface::CapabilityGraphChanged,
            this, &Client::onCapabilityGraphChanged);
    refresh();
}

bool Client::available() const { return available_; }
QString Client::status() const { return status_; }
QList<Device> Client::devices() const { return devices_; }
QVariantList Client::deviceData() const
{
    QVariantList result;
    result.reserve(devices_.size());
    for (const Device &device : devices_) {
        result.append(QVariantMap{
            {QStringLiteral("subjectKind"), device.subjectKind},
            {QStringLiteral("subjectId"), device.subjectId},
            {QStringLiteral("displayName"), device.displayName}
        });
    }
    return result;
}
QString Client::serviceInstanceUuid() const { return serviceInstanceUuid_; }
quint64 Client::serviceGeneration() const { return serviceGeneration_; }
quint64 Client::eventSequence() const { return eventSequence_; }
quint64 Client::inventoryGeneration() const { return inventoryGeneration_; }
quint64 Client::capabilityGeneration() const { return capabilityGeneration_; }

void Client::refresh()
{
    if (!connection_.isConnected()) {
        ++eventEpoch_;
        clearSnapshot(QStringLiteral("DISCONNECTED"));
        return;
    }
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }

    requestInFlight_ = true;
    refreshPending_ = false;
    auto *watcher = new QDBusPendingCallWatcher(hardwareProxy_.ListDevices(), this);
    const quint64 requestOwnerEpoch = ownerEpoch_;
    const quint64 requestEventEpoch = eventEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestOwnerEpoch, requestEventEpoch] {
                finishRefresh(watcher, requestOwnerEpoch, requestEventEpoch);
            });
}

void Client::onServiceOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++ownerEpoch_;
    ++eventEpoch_;
    requestInFlight_ = false;
    refreshPending_ = false;
    eventInstanceUuid_.clear();
    eventServiceGeneration_ = 0;
    lastObservedEventSequence_ = 0;
    lastHardwareEventSequence_ = 0;
    hardwareEventSeen_ = false;
    clearSnapshot(newOwner.isEmpty() ? QStringLiteral("DISCONNECTED")
                                    : QStringLiteral("RECONCILING"));
    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void Client::onEventPublished(const QString &uuid, qulonglong generation,
                              qulonglong sequence, const QString &subjectKind,
                              const QString &subjectId)
{
    acceptEventIdentity(uuid, generation, sequence, subjectKind, subjectId, false);
}

bool Client::acceptEventIdentity(const QString &uuid, quint64 generation, quint64 sequence,
                                 const QString &subjectKind, const QString &subjectId,
                                 bool hardwareSpecific)
{
    if (!isCanonicalUuid(uuid) || generation == 0 || sequence == 0
        || !(hardwareSpecific ? isStableHardwareSubject(subjectKind, subjectId)
                              : isStableSubject(subjectKind, subjectId))) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedEventSequence_ = 0;
        invalidateAndRefresh(QStringLiteral("INVALID_EVENT"));
        return false;
    }

    const bool sameOwner = eventInstanceUuid_ == uuid && eventServiceGeneration_ == generation;
    if (sameOwner && hardwareSpecific && available_
        && uuid == serviceInstanceUuid_ && generation == serviceGeneration_
        && sequence <= snapshotEventSequence_) {
        return false;
    }
    if (sameOwner && hardwareSpecific && hardwareEventSeen_
        && sequence <= lastHardwareEventSequence_) {
        return false;
    }
    if (sameOwner && !hardwareSpecific && sequence <= lastObservedEventSequence_) {
        return false;
    }

    const bool snapshotOwnerMismatch = available_
        && (uuid != serviceInstanceUuid_ || generation != serviceGeneration_);
    const bool eventOwnerChanged = !eventInstanceUuid_.isEmpty() && !sameOwner;
    const bool baselineKnown = sameOwner
        || (available_ && uuid == serviceInstanceUuid_ && generation == serviceGeneration_);
    const quint64 baseline = sameOwner ? lastObservedEventSequence_ : eventSequence_;
    const bool sequenceGap = baselineKnown && sequence > baseline
        && sequence - baseline > 1;
    eventInstanceUuid_ = uuid;
    eventServiceGeneration_ = generation;
    if (sequence > lastObservedEventSequence_) {
        lastObservedEventSequence_ = sequence;
    }
    if (hardwareSpecific) {
        hardwareEventSeen_ = true;
        lastHardwareEventSequence_ = qMax(lastHardwareEventSequence_, sequence);
    }
    ++eventEpoch_;

    if (snapshotOwnerMismatch || eventOwnerChanged || sequenceGap) {
        invalidateAndRefresh(snapshotOwnerMismatch || eventOwnerChanged
                                 ? QStringLiteral("RECONCILING")
                                 : QStringLiteral("EVENT_GAP"));
        return false;
    }
    if (hardwareSpecific) {
        invalidateAndRefresh(QStringLiteral("RECONCILING"));
        return true;
    }
    if (!available_) {
        refresh();
        return false;
    }
    if (sequence > eventSequence_) {
        eventSequence_ = sequence;
        emit snapshotChanged();
    }
    return true;
}

void Client::onInventoryChanged(const QString &uuid, qulonglong generation,
                                qulonglong sequence, const QString &subjectKind,
                                const QString &subjectId, qulonglong inventoryGeneration,
                                qulonglong capabilityGeneration)
{
    if (inventoryGeneration == 0 || capabilityGeneration == 0) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedEventSequence_ = 0;
        invalidateAndRefresh(QStringLiteral("INVALID_EVENT"));
        return;
    }
    acceptEventIdentity(uuid, generation, sequence, subjectKind, subjectId, true);
}

void Client::onCapabilityGraphChanged(const QString &uuid, qulonglong generation,
                                      qulonglong sequence, const QString &subjectKind,
                                      const QString &subjectId, qulonglong inventoryGeneration,
                                      qulonglong capabilityGeneration)
{
    if (inventoryGeneration == 0 || capabilityGeneration == 0) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedEventSequence_ = 0;
        invalidateAndRefresh(QStringLiteral("INVALID_EVENT"));
        return;
    }
    acceptEventIdentity(uuid, generation, sequence, subjectKind, subjectId, true);
}

void Client::clearSnapshot(const QString &status)
{
    available_ = false;
    status_ = status;
    devices_.clear();
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    eventSequence_ = 0;
    snapshotEventSequence_ = 0;
    inventoryGeneration_ = 0;
    capabilityGeneration_ = 0;
    emit snapshotChanged();
}

void Client::invalidateAndRefresh(const QString &status)
{
    clearSnapshot(status);
    if (requestInFlight_) {
        refreshPending_ = true;
    } else {
        refresh();
    }
}

void Client::finishRefresh(QDBusPendingCallWatcher *watcher, quint64 requestOwnerEpoch,
                           quint64 requestEventEpoch)
{
    QDBusPendingReply<Reply, QList<Device>> pending = *watcher;
    watcher->deleteLater();
    if (requestOwnerEpoch != ownerEpoch_) {
        return;
    }

    requestInFlight_ = false;
    QString error;
    const Reply reply = pending.isError() ? Reply{} : pending.argumentAt<0>();
    const QList<Device> devices = pending.isError() ? QList<Device>{} : pending.argumentAt<1>();
    const bool valid = !pending.isError() && reply.isValid(&error)
        && reply.code == QLatin1String("OK") && reply.snapshotValid
        && reply.subjectKind == QLatin1String("PLATFORM")
        && reply.subjectId == QLatin1String("platform")
        && isCanonicalUuid(reply.serviceInstanceUuid) && reply.serviceGeneration != 0
        && isDeviceSnapshotValid(devices);

    bool coversObservedEvents = true;
    if (requestEventEpoch != eventEpoch_) {
        coversObservedEvents = !eventInstanceUuid_.isEmpty()
            && reply.serviceInstanceUuid == eventInstanceUuid_
            && reply.serviceGeneration == eventServiceGeneration_
            && reply.eventSequence >= lastObservedEventSequence_;
    }
    if (!valid || !coversObservedEvents) {
        clearSnapshot(pending.isError() ? QStringLiteral("UNAVAILABLE")
                                        : QStringLiteral("RECONCILING"));
        const bool retry = refreshPending_ || (valid && !coversObservedEvents);
        refreshPending_ = false;
        if (retry) {
            refresh();
        }
        return;
    }

    const bool identityChanged = available_
        && (serviceInstanceUuid_ != reply.serviceInstanceUuid
            || serviceGeneration_ != reply.serviceGeneration);
    if (identityChanged && requestEventEpoch == eventEpoch_) {
        clearSnapshot(QStringLiteral("RECONCILING"));
        refresh();
        return;
    }

    available_ = true;
    status_ = QStringLiteral("READY");
    devices_ = devices;
    serviceInstanceUuid_ = reply.serviceInstanceUuid;
    serviceGeneration_ = reply.serviceGeneration;
    eventSequence_ = reply.eventSequence;
    snapshotEventSequence_ = reply.eventSequence;
    inventoryGeneration_ = reply.inventoryGeneration;
    capabilityGeneration_ = reply.capabilityGeneration;
    eventInstanceUuid_ = reply.serviceInstanceUuid;
    eventServiceGeneration_ = reply.serviceGeneration;
    lastObservedEventSequence_ = reply.eventSequence;
    if (!hardwareEventSeen_ || lastHardwareEventSequence_ <= reply.eventSequence) {
        hardwareEventSeen_ = false;
        lastHardwareEventSequence_ = 0;
    }
    refreshPending_ = false;
    emit snapshotChanged();
}

} // namespace adrenalin::contracts::hardware1
