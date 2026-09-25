#include "profiles1_client.h"
#include "profiles1_contract.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include <utility>

namespace adrenalin::contracts::profiles1 {

Client::Client(QString serviceName, QString objectPath, QString subjectKind, QString subjectId,
               const QDBusConnection &connection, QObject *parent)
    : QObject(parent), profileProxy_(serviceName, objectPath, connection, this),
      serviceProxy_(serviceName, objectPath, connection, this), connection_(connection),
      ownerWatcher_(serviceName, connection, QDBusServiceWatcher::WatchForOwnerChange, this),
      subjectKind_(std::move(subjectKind)), subjectId_(std::move(subjectId))
{
    registerMetaTypes();
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &Client::onOwnerChanged);
    connect(&serviceProxy_, &OrgAdrenalinlinuxSession1Service1Interface::EventPublished,
            this, &Client::onCommonEvent);
    connect(&profileProxy_, &OrgAdrenalinlinuxSession1Profiles1Interface::ProfileChanged,
            this, &Client::onProfileEvent);
    if (!isValidSubject(subjectKind_, subjectId_)) {
        clear(QStringLiteral("INVALID_ARGUMENT"));
        return;
    }
    refresh();
}

bool Client::ready() const { return ready_; }
QString Client::status() const { return status_; }
Profile Client::profile() const { return profile_; }
QString Client::serviceInstanceUuid() const { return serviceInstanceUuid_; }
quint64 Client::serviceGeneration() const { return serviceGeneration_; }
quint64 Client::eventSequence() const { return eventSequence_; }
QString Client::subjectKind() const { return subjectKind_; }
QString Client::subjectId() const { return subjectId_; }

void Client::refresh()
{
    if (!connection_.isConnected()) {
        clear(QStringLiteral("DISCONNECTED"));
        return;
    }
    if (inFlight_) {
        refreshPending_ = true;
        return;
    }
    requestSnapshot();
}

void Client::requestSnapshot()
{
    if (inFlight_) {
        refreshPending_ = true;
        return;
    }
    if (!connection_.isConnected()) return;
    inFlight_ = true;
    refreshPending_ = false;
    ready_ = false;
    status_ = QStringLiteral("RECONCILING");
    emit stateChanged();
    auto *watcher = new QDBusPendingCallWatcher(
        profileProxy_.ReadProfile(subjectKind_, subjectId_), this);
    const quint64 owner = ownerEpoch_;
    const quint64 event = eventEpoch_;
    const QString kind = subjectKind_;
    const QString id = subjectId_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, event, kind, id] {
                finishSnapshot(watcher, owner, event, kind, id);
            });
}

void Client::finishSnapshot(QDBusPendingCallWatcher *watcher, quint64 owner, quint64 event,
                            QString requestedKind, QString requestedId)
{
    QDBusPendingReply<QString, QString, qulonglong, qulonglong, Profile> reply = *watcher;
    watcher->deleteLater();
    if (owner != ownerEpoch_) return;
    inFlight_ = false;
    if (reply.isError()) {
        clear(QStringLiteral("UNAVAILABLE"));
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }

    ReadReply snapshot{reply.argumentAt<0>(), reply.argumentAt<1>(), reply.argumentAt<2>(),
                       reply.argumentAt<3>(), reply.argumentAt<4>()};
    QString error;
    if (!snapshot.isValidFor(requestedKind, requestedId, &error)) {
        clear(QStringLiteral("INVALID_RESPONSE"));
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }
    if (!connection_.isConnected()) {
        clear(QStringLiteral("DISCONNECTED"));
        return;
    }
    const bool observedCursorExists = !eventInstanceUuid_.isEmpty();
    const bool snapshotCoversObservedCursor = observedCursorExists
        && snapshot.serviceInstanceUuid == eventInstanceUuid_
        && snapshot.serviceGeneration == eventServiceGeneration_
        && snapshot.eventSequence >= lastObservedSequence_;
    if ((event != eventEpoch_ || refreshPending_ || observedCursorExists)
        && !snapshotCoversObservedCursor) {
        clear(QStringLiteral("RECONCILING"));
        refreshPending_ = false;
        requestSnapshot();
        return;
    }
    if (snapshot.code == QLatin1String("NOT_FOUND")) {
        profile_ = {};
    } else {
        profile_ = snapshot.profile;
    }
    serviceInstanceUuid_ = snapshot.serviceInstanceUuid;
    serviceGeneration_ = snapshot.serviceGeneration;
    eventSequence_ = snapshot.eventSequence;
    eventInstanceUuid_ = serviceInstanceUuid_;
    eventServiceGeneration_ = serviceGeneration_;
    lastObservedSequence_ = qMax(lastObservedSequence_, eventSequence_);
    ready_ = true;
    status_ = snapshot.code;
    emit profileChanged();
    emit stateChanged();
    if (refreshPending_) {
        refreshPending_ = false;
        requestSnapshot();
    }
}

void Client::onOwnerChanged(const QString &, const QString &, const QString &newOwner)
{
    ++ownerEpoch_;
    ++eventEpoch_;
    inFlight_ = false;
    refreshPending_ = false;
    eventInstanceUuid_.clear();
    eventServiceGeneration_ = 0;
    lastObservedSequence_ = 0;
    clear(newOwner.isEmpty() ? QStringLiteral("DISCONNECTED") : QStringLiteral("RECONCILING"));
    if (!newOwner.isEmpty()) requestSnapshot();
}

void Client::onCommonEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                           const QString &kind, const QString &id)
{
    observeEvent(uuid, generation, sequence, kind, id,
                 kind == subjectKind_ && id == subjectId_);
}

void Client::onProfileEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                            const QString &kind, const QString &id, qulonglong revision)
{
    if (revision == 0) {
        ++eventEpoch_;
        clear(QStringLiteral("INVALID_EVENT"));
        requestSnapshot();
        return;
    }
    observeEvent(uuid, generation, sequence, kind, id, true);
}

void Client::observeEvent(const QString &uuid, quint64 generation, quint64 sequence,
                          const QString &kind, const QString &id, bool profileSignal)
{
    if (!adrenalin::contracts::profiles1::isValidServiceIdentity(uuid, generation)
        || sequence == 0 || kind.isEmpty() || id.isEmpty()) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedSequence_ = 0;
        clear(QStringLiteral("INVALID_EVENT"));
        requestSnapshot();
        return;
    }
    if (profileSignal && !isValidSubject(kind, id)) {
        ++eventEpoch_;
        clear(QStringLiteral("INVALID_EVENT"));
        requestSnapshot();
        return;
    }
    const bool knownOwner = !eventInstanceUuid_.isEmpty();
    if (knownOwner && (uuid != eventInstanceUuid_ || generation != eventServiceGeneration_)) {
        eventInstanceUuid_ = uuid;
        eventServiceGeneration_ = generation;
        lastObservedSequence_ = sequence;
        ++eventEpoch_;
        clear(QStringLiteral("RECONCILING"));
        requestSnapshot();
        return;
    }
    if (knownOwner && sequence <= lastObservedSequence_) return;
    const bool gap = knownOwner && sequence > lastObservedSequence_
        && sequence - lastObservedSequence_ > 1;
    if (!knownOwner) {
        eventInstanceUuid_ = uuid;
        eventServiceGeneration_ = generation;
    }
    lastObservedSequence_ = qMax(lastObservedSequence_, sequence);
    if (gap) {
        ++eventEpoch_;
        clear(QStringLiteral("EVENT_GAP"));
        requestSnapshot();
        return;
    }
    const bool relevantProfile = profileSignal && kind == subjectKind_ && id == subjectId_;
    if (relevantProfile) {
        ++eventEpoch_;
        clear(QStringLiteral("RECONCILING"));
        requestSnapshot();
        return;
    }
    if (ready_ && uuid == serviceInstanceUuid_ && generation == serviceGeneration_) {
        eventSequence_ = sequence;
        emit stateChanged();
    }
}

void Client::clear(const QString &status)
{
    ready_ = false;
    status_ = status;
    profile_ = {};
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    eventSequence_ = 0;
    emit profileChanged();
    emit stateChanged();
}

} // namespace adrenalin::contracts::profiles1
