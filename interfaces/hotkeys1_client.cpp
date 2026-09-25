#include "hotkeys1_client.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>

#include <algorithm>

namespace adrenalin::contracts::hotkeys1 {
namespace {
constexpr auto kInterface = "org.adrenalinlinux.Session1.Hotkeys1";
constexpr auto kServiceInterface = "org.adrenalinlinux.Session1.Service1";
}

Client::Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
               QObject *parent)
    : QObject(parent), connection_(connection), serviceName_(std::move(serviceName)),
      objectPath_(std::move(objectPath)),
      ownerWatcher_(serviceName_, connection_, QDBusServiceWatcher::WatchForOwnerChange, this),
      readiness_(serviceName_, objectPath_, connection_, 1)
{
    registerMetaTypes();
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged,
            this, &Client::onOwnerChanged);
    connect(&readiness_, &ServiceReadinessClient::stateChanged,
            this, &Client::onReadinessChanged);
    connection_.connect(serviceName_, objectPath_, QString::fromLatin1(kServiceInterface),
                        QStringLiteral("EventPublished"), this,
                        SLOT(onCommonEvent(QString,qulonglong,qulonglong,QString,QString)));
    connection_.connect(serviceName_, objectPath_, QString::fromLatin1(kInterface),
                        QStringLiteral("HotkeyChanged"), this,
                        SLOT(onHotkeyChanged(QString,qulonglong,qulonglong,QString,QString,qulonglong)));
    if (!connection_.isConnected()) {
        clear(QStringLiteral("DISCONNECTED"));
    } else if (readiness_.ready()) {
        refresh();
    }
}

bool Client::ready() const { return ready_ && readiness_.ready(); }
QString Client::status() const { return status_; }
QList<Action> Client::actions() const { return actions_; }
QString Client::serviceInstanceUuid() const { return serviceInstanceUuid_; }
quint64 Client::serviceGeneration() const { return serviceGeneration_; }
quint64 Client::eventSequence() const { return eventSequence_; }
quint64 Client::revision() const { return revision_; }

void Client::refresh()
{
    if (!connection_.isConnected()) {
        clear(QStringLiteral("DISCONNECTED"));
        return;
    }
    if (!readiness_.ready()) {
        clear(readiness_.status());
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
    if (!connection_.isConnected() || !readiness_.ready()) return;
    inFlight_ = true;
    refreshPending_ = false;
    ready_ = false;
    status_ = QStringLiteral("RECONCILING");
    emit stateChanged();
    QDBusMessage request = QDBusMessage::createMethodCall(
        serviceName_, objectPath_, QString::fromLatin1(kInterface), QStringLiteral("ListHotkeys"));
    auto *watcher = new QDBusPendingCallWatcher(connection_.asyncCall(request), this);
    const quint64 owner = ownerEpoch_;
    const quint64 event = eventEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, owner, event] { finishSnapshot(watcher, owner, event); });
}

void Client::finishSnapshot(QDBusPendingCallWatcher *watcher, quint64 owner, quint64 event)
{
    QDBusPendingReply<Reply, QList<Action>> reply = *watcher;
    watcher->deleteLater();
    if (owner != ownerEpoch_) return;
    inFlight_ = false;
    if (reply.isError()) {
        clear(QStringLiteral("UNAVAILABLE"));
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }
    const Reply snapshot = reply.argumentAt<0>();
    const QList<Action> actions = reply.argumentAt<1>();
    QString error;
    if (!isValidSnapshot(snapshot, actions, &error)) {
        clear(QStringLiteral("INVALID_RESPONSE"));
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }
    if (!snapshot.snapshotValid) {
        clear(snapshot.code);
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }
    if (!readiness_.ready() || snapshot.serviceInstanceUuid != readiness_.serviceInstanceUuid()
        || snapshot.serviceGeneration != readiness_.serviceGeneration()) {
        clear(QStringLiteral("INVALID_RESPONSE"));
        if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
        return;
    }
    const bool hasObservedOwner = !eventInstanceUuid_.isEmpty();
    const bool coversEvents = hasObservedOwner && snapshot.serviceInstanceUuid == eventInstanceUuid_
        && snapshot.serviceGeneration == eventServiceGeneration_
        && snapshot.eventSequence >= lastObservedSequence_;
    if ((event != eventEpoch_ || refreshPending_ || hasObservedOwner) && !coversEvents) {
        ready_ = false;
        status_ = QStringLiteral("RECONCILING");
        emit stateChanged();
        refreshPending_ = false;
        requestSnapshot();
        return;
    }
    actions_ = actions;
    serviceInstanceUuid_ = snapshot.serviceInstanceUuid;
    serviceGeneration_ = snapshot.serviceGeneration;
    eventSequence_ = snapshot.eventSequence;
    revision_ = snapshot.revision;
    eventInstanceUuid_ = serviceInstanceUuid_;
    eventServiceGeneration_ = serviceGeneration_;
    lastObservedSequence_ = qMax(lastObservedSequence_, eventSequence_);
    ready_ = true;
    status_ = snapshot.code;
    emit actionsChanged();
    emit stateChanged();
    if (refreshPending_) { refreshPending_ = false; requestSnapshot(); }
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
    if (!newOwner.isEmpty()) {
        readiness_.refresh();
    }
}

void Client::onReadinessChanged()
{
    if (!readiness_.ready()) {
        if (ready_ || status_ != readiness_.status()) clear(readiness_.status());
        return;
    }
    if (serviceInstanceUuid_ != readiness_.serviceInstanceUuid()
        || serviceGeneration_ != readiness_.serviceGeneration()) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedSequence_ = 0;
        clear(QStringLiteral("RECONCILING"));
        requestSnapshot();
    } else if (!ready_) {
        requestSnapshot();
    }
}

void Client::onCommonEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                           const QString &kind, const QString &id)
{
    observeEvent(uuid, generation, sequence, kind, id, false);
}

void Client::onHotkeyChanged(const QString &uuid, qulonglong generation, qulonglong sequence,
                             const QString &kind, const QString &id, qulonglong revision)
{
    if (revision == 0) {
        // Even an invalid family payload carries a cursor that must fence any
        // snapshot already in flight. The snapshot remains the recovery source.
        observeEvent(uuid, generation, sequence, kind, id, true);
        return;
    }
    observeEvent(uuid, generation, sequence, kind, id, true);
}

void Client::observeEvent(const QString &uuid, quint64 generation, quint64 sequence,
                          const QString &kind, const QString &id, bool specific)
{
    const QUuid parsed(uuid);
    const auto validEventText = [](const QString &value, qsizetype maximum) {
        return !value.isEmpty() && value.size() <= maximum
            && !value.contains(QChar::Null)
            && std::none_of(value.cbegin(), value.cend(), [](QChar character) {
                   return character.category() == QChar::Other_Control;
               });
    };
    if (parsed.isNull() || parsed.toString(QUuid::WithoutBraces) != uuid || generation == 0
        || sequence == 0 || !validEventText(kind, 64) || !validEventText(id, 256)
        || (specific && (kind != QLatin1String("HOTKEY_ACTION") || !isValidActionId(id)))) {
        ++eventEpoch_;
        eventInstanceUuid_.clear();
        eventServiceGeneration_ = 0;
        lastObservedSequence_ = 0;
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
    if (inFlight_) refreshPending_ = true;
    const bool gap = knownOwner && sequence - lastObservedSequence_ > 1;
    if (!knownOwner) {
        eventInstanceUuid_ = uuid;
        eventServiceGeneration_ = generation;
    }
    lastObservedSequence_ = sequence;
    const bool relevant = specific || kind == QLatin1String("HOTKEY_ACTION");
    if (gap || relevant) {
        ++eventEpoch_;
        clear(gap ? QStringLiteral("EVENT_GAP") : QStringLiteral("RECONCILING"));
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
    const bool hadActions = !actions_.isEmpty();
    ready_ = false;
    status_ = status;
    actions_.clear();
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    eventSequence_ = 0;
    revision_ = 0;
    if (hadActions) emit actionsChanged();
    emit stateChanged();
}

} // namespace adrenalin::contracts::hotkeys1
