#include "display1_client.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

namespace adrenalin::contracts::display1 {
namespace {
bool canonicalUuid(const QString &value)
{
    const QUuid uuid(value);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces).compare(value, Qt::CaseInsensitive) == 0;
}

bool validDisplayList(const ListReply &reply)
{
    QString error;
    if (!reply.isValid(&error) || reply.snapshot.code != QLatin1String("OK")
        || !reply.snapshot.snapshotValid || !canonicalUuid(reply.snapshot.serviceInstanceUuid)
        || reply.snapshot.serviceGeneration == 0
        || reply.snapshot.inventoryGeneration == 0 || reply.snapshot.capabilityGeneration == 0) {
        return false;
    }
    QSet<QString> ids;
    for (const Display &display : reply.displays) {
        if (display.identityEvidence.isEmpty() || ids.contains(display.subjectId)) return false;
        ids.insert(display.subjectId);
    }
    return true;
}

bool sameDisplay(const Display &a, const Display &b)
{
    return a.subjectKind == b.subjectKind && a.subjectId == b.subjectId
        && a.identityEvidence == b.identityEvidence && a.displayName == b.displayName;
}

} // namespace

Client::Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
               QObject *parent)
    : QObject(parent), displayProxy_(serviceName, objectPath, connection, this),
      hardwareProxy_(serviceName, objectPath, connection, this),
      serviceProxy_(serviceName, objectPath, connection, this), connection_(connection),
      ownerWatcher_(serviceName, connection, QDBusServiceWatcher::WatchForOwnerChange, this),
      serviceName_(std::move(serviceName)), objectPath_(std::move(objectPath))
{
    hardware1::registerMetaTypes();
    registerMetaTypes();
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &Client::onOwnerChanged);
    connect(&serviceProxy_, &OrgAdrenalinlinuxSession1Service1Interface::EventPublished,
            this, &Client::onCommonEvent);
    connect(&hardwareProxy_, &OrgAdrenalinlinuxSession1Hardware1Interface::InventoryChanged,
            this, &Client::onInventoryEvent);
    connect(&hardwareProxy_, &OrgAdrenalinlinuxSession1Hardware1Interface::CapabilityGraphChanged,
            this, &Client::onCapabilityEvent);
    connect(&displayProxy_, &OrgAdrenalinlinuxSession1Display1Interface::DisplayChanged,
            this, &Client::onDisplayEvent);
    refresh();
}

bool Client::available() const { return available_; }
QString Client::status() const { return status_; }
QList<DisplayState> Client::displays() const { return displays_; }
QString Client::serviceInstanceUuid() const { return serviceInstanceUuid_; }
quint64 Client::serviceGeneration() const { return serviceGeneration_; }
quint64 Client::eventSequence() const { return eventSequence_; }
quint64 Client::inventoryGeneration() const { return inventoryGeneration_; }
quint64 Client::capabilityGeneration() const { return capabilityGeneration_; }

void Client::refresh()
{
    if (!connection_.isConnected()) { ++eventEpoch_; clear(QStringLiteral("DISCONNECTED")); return; }
    if (inFlight_) { refreshPending_ = true; return; }
    inFlight_ = true;
    refreshPending_ = false;
    auto *watcher = new QDBusPendingCallWatcher(displayProxy_.ListDisplays(), this);
    const quint64 owner = ownerEpoch_, epoch = eventEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, epoch] { finishList(watcher, owner, epoch); });
}

void Client::onOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++ownerEpoch_; ++eventEpoch_; inFlight_ = false; refreshPending_ = false;
    recentEvents_.clear(); eventSequence_ = 0; lastObservedSequence_ = 0;
    eventInstanceUuid_.clear();
    eventServiceGeneration_ = 0;
    clear(newOwner.isEmpty() ? QStringLiteral("DISCONNECTED") : QStringLiteral("RECONCILING"));
    if (!newOwner.isEmpty()) refresh();
}

void Client::onCommonEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                           const QString &kind, const QString &id)
{
    acceptEvent({uuid, generation, kind, id, 0, 0, true}, sequence,
                kind == QLatin1String("DISPLAY"));
}
void Client::onInventoryEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                              const QString &kind, const QString &id, qulonglong inventory,
                              qulonglong capability)
{
    acceptEvent({uuid, generation, kind, id, inventory, capability, false}, sequence,
                kind == QLatin1String("DISPLAY") || kind == QLatin1String("PLATFORM"));
}
void Client::onCapabilityEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                               const QString &kind, const QString &id, qulonglong inventory,
                               qulonglong capability)
{
    onInventoryEvent(uuid, generation, sequence, kind, id, inventory, capability);
}
void Client::onDisplayEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                            const QString &kind, const QString &id, qulonglong inventory,
                            qulonglong capability)
{
    onInventoryEvent(uuid, generation, sequence, kind, id, inventory, capability);
}

void Client::acceptEvent(const EventRecord &event, quint64 sequence, bool relevant)
{
    static const QRegularExpression kindSyntax(QStringLiteral("^[A-Z][A-Z0-9_]{0,63}$"));
    if (!canonicalUuid(event.uuid) || event.generation == 0 || sequence == 0
        || !kindSyntax.match(event.kind).hasMatch() || event.id.isEmpty()
        || (!event.common && (event.inventory == 0 || event.capability == 0))) {
        recentEvents_.clear(); eventInstanceUuid_.clear(); eventServiceGeneration_ = 0;
        lastObservedSequence_ = 0;
        ++eventEpoch_; invalidate(QStringLiteral("INVALID_EVENT")); return;
    }
    const bool snapshotOwnerMismatch = available_
        && (event.uuid != serviceInstanceUuid_ || event.generation != serviceGeneration_);
    const bool eventOwnerMismatch = !eventInstanceUuid_.isEmpty()
        && (event.uuid != eventInstanceUuid_ || event.generation != eventServiceGeneration_);
    if (snapshotOwnerMismatch || eventOwnerMismatch) {
        recentEvents_.clear();
        eventInstanceUuid_ = event.uuid; eventServiceGeneration_ = event.generation;
        ++eventEpoch_;
        lastObservedSequence_ = sequence;
        invalidate(QStringLiteral("RECONCILING"));
        return;
    }
    const auto prior = recentEvents_.constFind(sequence);
    if (prior != recentEvents_.cend()) {
        const EventRecord &p = prior.value();
        const bool sameBase = p.uuid == event.uuid && p.generation == event.generation
            && p.kind == event.kind && p.id == event.id;
        const bool generationsAgree = p.common || event.common
            || (p.inventory == event.inventory && p.capability == event.capability);
        if (!sameBase || !generationsAgree) {
            ++eventEpoch_; invalidate(QStringLiteral("INVALID_EVENT")); return;
        }
        if (!p.common && !event.common && p.inventory != event.inventory) {
            ++eventEpoch_; invalidate(QStringLiteral("INVALID_EVENT")); return;
        }
        EventRecord merged = event.common ? p : event;
        merged.common = p.common || event.common;
        merged.refreshIssued = p.refreshIssued || event.refreshIssued;
        recentEvents_[sequence] = merged;
        if (relevant && !merged.refreshIssued) {
            merged.refreshIssued = true; recentEvents_[sequence] = merged;
            ++eventEpoch_; invalidate(QStringLiteral("RECONCILING"));
        }
        return;
    }
    const bool knownOwner = !eventInstanceUuid_.isEmpty();
    const quint64 baseline = lastObservedSequence_;
    EventRecord first = event;
    first.refreshIssued = relevant;
    recentEvents_.insert(sequence, first);
    eventInstanceUuid_ = event.uuid; eventServiceGeneration_ = event.generation;
    while (recentEvents_.size() > 256) recentEvents_.erase(recentEvents_.begin());
    if (knownOwner && sequence <= baseline) return;
    const bool gap = knownOwner && sequence > baseline && sequence - baseline > 1;
    lastObservedSequence_ = qMax(lastObservedSequence_, sequence);
    if (gap) { ++eventEpoch_; invalidate(QStringLiteral("EVENT_GAP")); return; }
    if (relevant) { ++eventEpoch_; invalidate(QStringLiteral("RECONCILING")); return; }
    if (!available_) {
        refresh();
    } else {
        eventSequence_ = lastObservedSequence_;
        emit snapshotChanged();
    }
}

void Client::clear(const QString &status)
{
    available_ = false; status_ = status; displays_.clear(); serviceInstanceUuid_.clear();
    eventSequence_ = 0;
    serviceGeneration_ = inventoryGeneration_ = capabilityGeneration_ = 0;
    emit snapshotChanged();
}
void Client::invalidate(const QString &status)
{
    clear(status);
    if (inFlight_) refreshPending_ = true; else refresh();
}

bool Client::sameSnapshot(const hardware1::Reply &a, const hardware1::Reply &b) const
{
    return a.serviceInstanceUuid == b.serviceInstanceUuid
        && a.serviceGeneration == b.serviceGeneration && a.eventSequence == b.eventSequence
        && a.inventoryGeneration == b.inventoryGeneration
        && a.capabilityGeneration == b.capabilityGeneration;
}

void Client::finishList(QDBusPendingCallWatcher *watcher, quint64 owner, quint64 epoch)
{
    QDBusPendingReply<ListReply> pending = *watcher; watcher->deleteLater();
    if (owner != ownerEpoch_) return;
    if (pending.isError() || !validDisplayList(pending.value())) {
        inFlight_ = false; clear(QStringLiteral("UNAVAILABLE"));
        if (refreshPending_) { refreshPending_ = false; refresh(); }
        return;
    }
    const ListReply list = pending.value();
    const bool coversEvents = epoch == eventEpoch_
        || (!eventInstanceUuid_.isEmpty()
            && list.snapshot.serviceInstanceUuid == eventInstanceUuid_
            && list.snapshot.serviceGeneration == eventServiceGeneration_
            && list.snapshot.eventSequence >= lastObservedSequence_);
    if (!coversEvents) {
        inFlight_ = false; clear(QStringLiteral("RECONCILING")); refreshPending_ = false; refresh(); return;
    }
    requestState(list, 0, {}, owner, epoch);
}

void Client::requestState(const ListReply &list, int index, QList<DisplayState> states,
                          quint64 owner, quint64 epoch)
{
    if (index >= list.displays.size()) {
        inFlight_ = false;
        const bool coversEvents = epoch == eventEpoch_
            || (!eventInstanceUuid_.isEmpty()
                && list.snapshot.serviceInstanceUuid == eventInstanceUuid_
                && list.snapshot.serviceGeneration == eventServiceGeneration_
                && list.snapshot.eventSequence >= lastObservedSequence_);
        if (owner != ownerEpoch_ || !coversEvents || refreshPending_) {
            clear(QStringLiteral("RECONCILING")); refreshPending_ = false; refresh(); return;
        }
        available_ = true; status_ = QStringLiteral("READY"); displays_ = std::move(states);
        serviceInstanceUuid_ = list.snapshot.serviceInstanceUuid;
        serviceGeneration_ = list.snapshot.serviceGeneration;
        eventSequence_ = list.snapshot.eventSequence;
        lastObservedSequence_ = list.snapshot.eventSequence;
        inventoryGeneration_ = list.snapshot.inventoryGeneration;
        capabilityGeneration_ = list.snapshot.capabilityGeneration;
        eventInstanceUuid_ = serviceInstanceUuid_; eventServiceGeneration_ = serviceGeneration_;
        emit snapshotChanged(); return;
    }
    auto *watcher = new QDBusPendingCallWatcher(displayProxy_.GetDisplayState(
        list.displays.at(index).subjectId), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, list, index, states = std::move(states), owner, epoch]() mutable {
                finishState(watcher, list, index, std::move(states), owner, epoch);
            });
}

void Client::finishState(QDBusPendingCallWatcher *watcher, ListReply list, int index,
                         QList<DisplayState> states, quint64 owner, quint64 epoch)
{
    QDBusPendingReply<StateReply> pending = *watcher; watcher->deleteLater();
    if (owner != ownerEpoch_) return;
    QString error;
    const bool validReply = !pending.isError() && pending.value().isValid(&error)
        && pending.value().snapshot.code == QLatin1String("OK")
        && sameSnapshot(list.snapshot, pending.value().snapshot)
        && pending.value().display.subjectId == list.displays.at(index).subjectId
        && sameDisplay(pending.value().display, list.displays.at(index));
    if (!validReply) {
        const bool retry = refreshPending_ || epoch != eventEpoch_;
        inFlight_ = false;
        clear(QStringLiteral("RECONCILING"));
        refreshPending_ = false;
        if (retry) refresh();
        return;
    }
    states.append({pending.value().display, pending.value().capabilities});
    requestState(list, index + 1, std::move(states), owner, epoch);
}

} // namespace adrenalin::contracts::display1
