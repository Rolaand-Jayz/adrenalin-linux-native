#include "toast_notifications_client.h"
#include "operation_result.h"
#include "session_identity.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>

#include <utility>

using adrenalin::contracts::OperationResultCode;
using adrenalin::contracts::operationResultCodeFromName;
using adrenalin::contracts::operationResultCodeName;

ToastNotificationsClient::ToastNotificationsClient(QObject *parent)
    : ToastNotificationsClient(QDBusConnection::sessionBus(), parent)
{
}

ToastNotificationsClient::ToastNotificationsClient(const QDBusConnection &connection,
                                                   QObject *parent)
    : QObject(parent),
      settingsProxy_(QString::fromLatin1(adrenalin::session1::serviceName),
                     QString::fromLatin1(adrenalin::session1::objectPath), connection, this),
      serviceProxy_(QString::fromLatin1(adrenalin::session1::serviceName),
                    QString::fromLatin1(adrenalin::session1::objectPath), connection, this),
      connection_(connection),
      ownerWatcher_(QString::fromLatin1(adrenalin::session1::serviceName), connection,
                    QDBusServiceWatcher::WatchForOwnerChange, this),
      readiness_(QString::fromLatin1(adrenalin::session1::serviceName),
                 QString::fromLatin1(adrenalin::session1::objectPath), connection, 1)
{
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &ToastNotificationsClient::onOwnerChanged);
    connect(&readiness_, &ServiceReadinessClient::stateChanged,
            this, &ToastNotificationsClient::onReadinessChanged);
    connect(&serviceProxy_, &OrgAdrenalinlinuxSession1Service1Interface::EventPublished,
            this, &ToastNotificationsClient::onCommonEvent);
    connect(&settingsProxy_, &OrgAdrenalinlinuxSession1Settings1Interface::ToastNotificationsChanged,
            this, &ToastNotificationsClient::onToastEvent);
    onReadinessChanged();
}

bool ToastNotificationsClient::ready() const
{
    return ready_ && readiness_.ready()
        && serviceInstanceUuid_ == readiness_.serviceInstanceUuid()
        && serviceGeneration_ == readiness_.serviceGeneration();
}

bool ToastNotificationsClient::configured() const { return configured_; }
bool ToastNotificationsClient::enabled() const { return enabled_; }
qulonglong ToastNotificationsClient::revision() const { return revision_; }
qulonglong ToastNotificationsClient::eventSequence() const { return eventSequence_; }
QString ToastNotificationsClient::serviceInstanceUuid() const { return serviceInstanceUuid_; }
qulonglong ToastNotificationsClient::serviceGeneration() const { return serviceGeneration_; }
QString ToastNotificationsClient::status() const { return status_; }
QString ToastNotificationsClient::lastOperationCode() const { return lastOperationCode_; }

void ToastNotificationsClient::refresh()
{
    if (!connection_.isConnected()) {
        clearSnapshot(QStringLiteral("DISCONNECTED"));
        resetEventOwner();
        return;
    }
    if (!readiness_.ready()) {
        clearSnapshot(readiness_.status());
        return;
    }
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }
    requestSnapshot();
}

void ToastNotificationsClient::requestSnapshot()
{
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }
    if (!connection_.isConnected() || !readiness_.ready()) return;
    requestInFlight_ = true;
    refreshPending_ = false;
    status_ = QStringLiteral("RECONCILING");
    emit stateChanged();
    auto *watcher = new QDBusPendingCallWatcher(settingsProxy_.GetToastNotifications(), this);
    const quint64 owner = ownerEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner] { finishSnapshot(watcher, owner); });
}

void ToastNotificationsClient::finishSnapshot(QDBusPendingCallWatcher *watcher,
                                               quint64 ownerEpoch)
{
    QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, bool, qulonglong> reply = *watcher;
    watcher->deleteLater();
    if (ownerEpoch != ownerEpoch_) return;
    requestInFlight_ = false;
    if (reply.isError()) {
        clearSnapshot(QStringLiteral("UNAVAILABLE"));
        if (refreshPending_) {
            refreshPending_ = false;
            requestSnapshot();
        }
        return;
    }
    const QString code = reply.argumentAt<0>();
    const QString uuid = reply.argumentAt<1>();
    const qulonglong generation = reply.argumentAt<2>();
    const qulonglong sequence = reply.argumentAt<3>();
    const bool configured = reply.argumentAt<4>();
    const bool enabled = reply.argumentAt<5>();
    const qulonglong revision = reply.argumentAt<6>();
    if (code != QLatin1String("OK")) {
        const bool emptyErrorEnvelope = uuid.isEmpty() && generation == 0 && sequence == 0
            && !configured && !enabled && revision == 0;
        clearSnapshot(emptyErrorEnvelope ? code : QStringLiteral("INVALID_RESPONSE"));
        if (refreshPending_) {
            refreshPending_ = false;
            requestSnapshot();
        }
        return;
    }
    if (uuid.isEmpty() || generation == 0 || (!configured && (enabled || revision != 0))
        || (configured && revision == 0)) {
        clearSnapshot(QStringLiteral("INVALID_RESPONSE"));
        return;
    }
    if (!identityMatchesReadiness(uuid, generation)) {
        clearSnapshot(QStringLiteral("RECONCILING"));
        readiness_.refresh();
        return;
    }
    const bool observedOwnerMatches = eventInstanceUuid_.isEmpty()
        || (eventInstanceUuid_ == uuid && eventServiceGeneration_ == generation);
    if (!observedOwnerMatches || sequence < lastObservedSequence_) {
        clearSnapshot(QStringLiteral("EVENT_GAP"));
        return;
    }
    if (refreshPending_) {
        refreshPending_ = false;
        requestSnapshot();
        return;
    }

    serviceInstanceUuid_ = uuid;
    serviceGeneration_ = generation;
    eventSequence_ = sequence;
    eventInstanceUuid_ = uuid;
    eventServiceGeneration_ = generation;
    lastObservedSequence_ = qMax(lastObservedSequence_, sequence);
    configured_ = configured;
    enabled_ = configured && enabled;
    revision_ = revision;
    ready_ = true;
    status_ = configured ? QStringLiteral("READY") : QStringLiteral("UNCONFIGURED");
    emit stateChanged();
    if (!pendingOperationId_.isEmpty()) submitPendingMutation();
}

void ToastNotificationsClient::setEnabled(bool enabled)
{
    if (!ready() || requestInFlight_) return;
    if (!pendingOperationId_.isEmpty()) {
        if (enabled == pendingEnabled_) submitPendingMutation();
        return;
    }
    lastOperationCode_.clear();
    pendingOperationId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pendingEnabled_ = enabled;
    pendingExpectedRevision_ = revision_;
    pendingOperationUncertain_ = false;
    submitPendingMutation();
}

void ToastNotificationsClient::submitPendingMutation()
{
    if (pendingOperationId_.isEmpty() || requestInFlight_ || !ready()) return;
    requestInFlight_ = true;
    ready_ = false;
    status_ = pendingOperationUncertain_ ? QStringLiteral("RECONCILING")
                                         : QStringLiteral("SAVING");
    emit stateChanged();
    auto *watcher = new QDBusPendingCallWatcher(
        settingsProxy_.SetToastNotifications(pendingOperationId_, pendingEnabled_,
                                             pendingExpectedRevision_), this);
    const quint64 owner = ownerEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner] { finishMutation(watcher, owner); });
}

void ToastNotificationsClient::finishMutation(QDBusPendingCallWatcher *watcher,
                                               quint64 ownerEpoch)
{
    QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                       qulonglong> reply = *watcher;
    watcher->deleteLater();
    if (ownerEpoch != ownerEpoch_) return;
    requestInFlight_ = false;
    const bool reconcile = std::exchange(refreshPending_, false);
    if (reply.isError()) {
        pendingOperationUncertain_ = true;
        status_ = QStringLiteral("RECONCILING");
        emit stateChanged();
        if (reconcile) refresh(); else requestSnapshot();
        return;
    }
    const auto parsed = operationResultCodeFromName(reply.argumentAt<0>());
    const OperationResultCode code = parsed.value_or(OperationResultCode::InternalError);
    status_ = operationResultCodeName(code);
    if (code == OperationResultCode::Ok) {
        pendingOperationId_.clear();
        pendingOperationUncertain_ = false;
        lastOperationCode_.clear();
        refresh();
        return;
    }
    if (code == OperationResultCode::BackendUnavailable && reply.argumentAt<4>()) {
        pendingOperationUncertain_ = true;
        refresh();
        return;
    }
    pendingOperationId_.clear();
    pendingOperationUncertain_ = false;
    lastOperationCode_ = status_;
    ready_ = false;
    emit stateChanged();
    refresh();
}

void ToastNotificationsClient::onOwnerChanged(const QString &, const QString &,
                                              const QString &newOwner)
{
    ++ownerEpoch_;
    requestInFlight_ = false;
    refreshPending_ = false;
    resetEventOwner();
    clearSnapshot(newOwner.isEmpty() ? QStringLiteral("DISCONNECTED")
                                     : QStringLiteral("RECONCILING"));
    if (!newOwner.isEmpty()) readiness_.refresh();
}

void ToastNotificationsClient::onReadinessChanged()
{
    if (!readiness_.ready()) {
        if (ready_ || status_ != readiness_.status()) clearSnapshot(readiness_.status());
        return;
    }
    if (!ready_ || serviceInstanceUuid_ != readiness_.serviceInstanceUuid()
        || serviceGeneration_ != readiness_.serviceGeneration()) {
        refresh();
    }
}

void ToastNotificationsClient::onCommonEvent(const QString &uuid, qulonglong generation,
                                              qulonglong sequence, const QString &kind,
                                              const QString &id)
{
    observeEvent(uuid, generation, sequence, kind, id, false, false, 0);
}

void ToastNotificationsClient::onToastEvent(const QString &uuid, qulonglong generation,
                                             qulonglong sequence, const QString &kind,
                                             const QString &id, bool enabled,
                                             qulonglong revision)
{
    observeEvent(uuid, generation, sequence, kind, id, true, enabled, revision);
}

void ToastNotificationsClient::observeEvent(const QString &uuid, quint64 generation,
                                             quint64 sequence, const QString &kind,
                                             const QString &id, bool hasPreferenceValue,
                                             bool enabled, quint64 revision)
{
    if (uuid.isEmpty() || generation == 0 || sequence == 0 || kind.isEmpty() || id.isEmpty()) {
        clearSnapshot(QStringLiteral("INVALID_EVENT"));
        refresh();
        return;
    }
    const bool toastPreference = kind == QLatin1String("PREFERENCE")
        && id == QLatin1String("toast.notifications");
    if (hasPreferenceValue && (!toastPreference || revision == 0)) {
        clearSnapshot(QStringLiteral("INVALID_EVENT"));
        refresh();
        return;
    }
    if (!eventInstanceUuid_.isEmpty()
        && (eventInstanceUuid_ != uuid || eventServiceGeneration_ != generation)) {
        eventInstanceUuid_ = uuid;
        eventServiceGeneration_ = generation;
        lastObservedSequence_ = sequence;
        clearSnapshot(QStringLiteral("RECONCILING"));
        refresh();
        return;
    }
    if (eventInstanceUuid_.isEmpty()) {
        eventInstanceUuid_ = uuid;
        eventServiceGeneration_ = generation;
    }
    if (sequence < lastObservedSequence_) return;
    if (sequence == lastObservedSequence_) {
        if (hasPreferenceValue) {
            if (ready_ && configured_ && revision_ == revision && enabled_ == enabled) return;
            if (ready_ && revision == revision_ + 1) {
                configured_ = true;
                enabled_ = enabled;
                revision_ = revision;
                eventSequence_ = sequence;
                status_ = QStringLiteral("READY");
                emit stateChanged();
                return;
            }
            clearSnapshot(QStringLiteral("RECONCILING"));
            refresh();
        }
        return;
    }
    const bool gap = sequence - lastObservedSequence_ != 1;
    lastObservedSequence_ = sequence;
    if (requestInFlight_) refreshPending_ = true;
    if (gap) {
        clearSnapshot(QStringLiteral("EVENT_GAP"));
        refresh();
        return;
    }
    if (hasPreferenceValue) {
        const quint64 expectedRevision = configured_ ? revision_ + 1 : 1;
        if (!ready_ || revision != expectedRevision) {
            clearSnapshot(QStringLiteral("RECONCILING"));
            refresh();
            return;
        }
        configured_ = true;
        enabled_ = enabled;
        revision_ = revision;
        eventSequence_ = sequence;
        status_ = QStringLiteral("READY");
        emit stateChanged();
        return;
    }
    if (toastPreference) {
        clearSnapshot(QStringLiteral("RECONCILING"));
        refresh();
        return;
    }
    if (ready_ && identityMatchesReadiness(uuid, generation)) {
        eventSequence_ = sequence;
        emit stateChanged();
    }
}

void ToastNotificationsClient::clearSnapshot(const QString &status)
{
    ready_ = false;
    configured_ = false;
    enabled_ = false;
    revision_ = 0;
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    eventSequence_ = 0;
    status_ = status;
    emit stateChanged();
}

void ToastNotificationsClient::resetEventOwner()
{
    eventInstanceUuid_.clear();
    eventServiceGeneration_ = 0;
    lastObservedSequence_ = 0;
}

bool ToastNotificationsClient::identityMatchesReadiness(const QString &uuid,
                                                         quint64 generation) const
{
    return readiness_.ready() && uuid == readiness_.serviceInstanceUuid()
        && generation == readiness_.serviceGeneration();
}
