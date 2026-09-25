#include "notifications1_client.h"

#include "interfaces/notifications1_contract_types.h"
#include "interfaces/operation_result.h"
#include "session_identity.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>

#include <algorithm>
#include <utility>

using Notification = adrenalin::contracts::notifications1::Notification;

Notifications1Client::Notifications1Client(QObject *parent)
    : Notifications1Client(QDBusConnection::sessionBus(), parent)
{
}

Notifications1Client::Notifications1Client(const QDBusConnection &connection, QObject *parent)
    : QAbstractListModel(parent),
      proxy_(QString::fromLatin1(adrenalin::session1::serviceName),
             QString::fromLatin1(adrenalin::session1::objectPath), connection, this),
      ownerWatcher_(QString::fromLatin1(adrenalin::session1::serviceName), connection,
                    QDBusServiceWatcher::WatchForOwnerChange, this),
      readiness_(QString::fromLatin1(adrenalin::session1::serviceName),
                 QString::fromLatin1(adrenalin::session1::objectPath), connection, 1),
      retryTimer_(this)
{
    adrenalin::contracts::notifications1::registerMetaTypes();
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, [this] {
        if (requestInFlight_ || !readiness_.ready()) {
            return;
        }
        refresh();
    });
    connect(&readiness_, &ServiceReadinessClient::stateChanged,
            this, &Notifications1Client::updateReadiness);
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                onOwnerChanged(newOwner);
            });
    connect(&proxy_,
            &OrgAdrenalinlinuxSession1Notifications1Interface::NotificationsChanged,
            this, &Notifications1Client::onNotificationsChanged);
    updateReadiness();
}

int Notifications1Client::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : notifications_.size();
}

QVariant Notifications1Client::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= notifications_.size()) {
        return {};
    }
    const Notification &item = notifications_.at(index.row());
    switch (role) {
    case NotificationIdRole: return item.notificationId;
    case CategoryRole: return item.category;
    case CreatedAtUtcRole: return item.createdAtUtc;
    case TitleMessageKeyRole: return item.titleMessageKey;
    case BodyMessageKeyRole: return item.bodyMessageKey;
    case IsReadRole: return item.isRead;
    case CriticalRole: return item.critical;
    case ToastEligibleRole: return item.toastEligible;
    default: return {};
    }
}

QHash<int, QByteArray> Notifications1Client::roleNames() const
{
    return {{NotificationIdRole, "notificationId"}, {CategoryRole, "category"},
            {CreatedAtUtcRole, "createdAtUtc"}, {TitleMessageKeyRole, "titleMessageKey"},
            {BodyMessageKeyRole, "bodyMessageKey"}, {IsReadRole, "isRead"},
            {CriticalRole, "critical"}, {ToastEligibleRole, "toastEligible"}};
}

bool Notifications1Client::ready() const { return ready_ && readiness_.ready(); }
QString Notifications1Client::status() const { return status_; }
int Notifications1Client::count() const { return notifications_.size(); }
int Notifications1Client::unreadCount() const
{
    int count = 0;
    for (const Notification &item : notifications_) {
        if (!item.isRead) ++count;
    }
    return count;
}
qulonglong Notifications1Client::revision() const { return revision_; }
QString Notifications1Client::lastOperationCode() const { return lastOperationCode_; }

void Notifications1Client::refresh()
{
    if (!readiness_.ready()) {
        ready_ = false;
        setStatus(readiness_.status());
        return;
    }
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }
    requestSnapshot();
}

void Notifications1Client::requestSnapshot()
{
    if (requestInFlight_ || !readiness_.ready()) return;
    requestInFlight_ = true;
    ready_ = false;
    setStatus(QStringLiteral("RECONCILING"));
    auto *call = new QDBusPendingCallWatcher(proxy_.ListNotifications(), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        QDBusPendingReply<QString, QString, qulonglong, qulonglong, qulonglong,
                           QList<Notification>> reply = *call;
        call->deleteLater();
        requestInFlight_ = false;
        const bool queuedRefresh = std::exchange(refreshPending_, false);
        if (reply.isError()) {
            ready_ = false;
            setStatus(QStringLiteral("UNAVAILABLE"));
            scheduleRetry();
            if (queuedRefresh) refresh();
            return;
        }
        if (!readiness_.ready()) {
            reconcile(readiness_.status());
            return;
        }
        if (queuedRefresh) {
            reconcile();
            requestSnapshot();
            return;
        }
        if (reply.argumentAt<0>() != QLatin1String("OK")) {
            ready_ = false;
            setStatus(reply.argumentAt<0>());
            scheduleRetry();
            return;
        }
        const QString uuid = reply.argumentAt<1>();
        const qulonglong generation = reply.argumentAt<2>();
        const qulonglong sequence = reply.argumentAt<3>();
        const qulonglong snapshotRevision = reply.argumentAt<4>();
        const auto records = reply.argumentAt<5>();
        if (!identityMatches(uuid, generation) || sequence == 0 || snapshotRevision == 0) {
            reconcile();
            readiness_.refresh();
            scheduleRetry();
            return;
        }
        for (const Notification &item : records) {
            if (!item.isValid()) {
                reconcile(QStringLiteral("INVALID_RESPONSE"));
                return;
            }
        }
        const int oldUnreadCount = unreadCount();
        beginResetModel();
        notifications_ = records;
        endResetModel();
        emit countChanged();
        serviceInstanceUuid_ = uuid;
        serviceGeneration_ = generation;
        eventSequence_ = qMax(eventSequence_, sequence);
        revision_ = snapshotRevision;
        if (oldUnreadCount != unreadCount()) emit unreadCountChanged();
        if (!pendingOperationId_.isEmpty()) {
            requestMutation();
            return;
        }
        ready_ = true;
        retryAttempt_ = 0;
        setStatus(QStringLiteral("READY"));
    });
}

void Notifications1Client::markRead(const QString &notificationId)
{
    if (!ready() || requestInFlight_ || !pendingOperationId_.isEmpty()) return;
    const auto item = std::find_if(notifications_.cbegin(), notifications_.cend(),
                                   [&notificationId](const Notification &candidate) {
                                       return candidate.notificationId == notificationId;
                                   });
    if (item == notifications_.cend() || item->isRead) return;
    pendingNotificationId_ = notificationId;
    lastOperationCode_.clear();
    pendingOperationId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pendingExpectedRevision_ = revision_;
    pendingOperationUncertain_ = false;
    requestMutation();
}

void Notifications1Client::requestMutation()
{
    if (requestInFlight_ || pendingOperationId_.isEmpty()) return;
    if (!readiness_.ready() || !identityMatches(serviceInstanceUuid_, serviceGeneration_)) {
        reconcile();
        return;
    }
    requestInFlight_ = true;
    ready_ = false;
    setStatus(pendingOperationUncertain_ ? QStringLiteral("RECONCILING")
                                         : QStringLiteral("SAVING"));
    auto *call = new QDBusPendingCallWatcher(
        proxy_.MarkRead(pendingNotificationId_, pendingOperationId_, pendingExpectedRevision_), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                           qulonglong, bool, QString, qulonglong, qulonglong> reply = *call;
        call->deleteLater();
        requestInFlight_ = false;
        const bool queuedRefresh = std::exchange(refreshPending_, false);
        if (reply.isError()) {
            pendingOperationUncertain_ = true;
            ready_ = false;
            setStatus(QStringLiteral("RECONCILING"));
            scheduleRetry();
            if (queuedRefresh) refresh();
            return;
        }
        if (!readiness_.ready()) {
            pendingOperationUncertain_ = true;
            reconcile(readiness_.status());
            return;
        }
        const QString code = reply.argumentAt<0>();
        const QString uuid = reply.argumentAt<9>();
        const qulonglong generation = reply.argumentAt<10>();
        if (!identityMatches(uuid, generation)
            || uuid != serviceInstanceUuid_ || generation != serviceGeneration_) {
            pendingOperationUncertain_ = true;
            reconcile();
            readiness_.refresh();
            scheduleRetry();
            return;
        }
        lastOperationCode_ = code;
        if (code == QLatin1String("OK")) {
            lastOperationCode_.clear();
            pendingOperationId_.clear();
            pendingNotificationId_.clear();
            pendingOperationUncertain_ = false;
            refresh();
            return;
        }
        if (code == QLatin1String("STALE_REVISION") || code == QLatin1String("CONFLICT")) {
            pendingOperationId_.clear();
            pendingNotificationId_.clear();
            pendingOperationUncertain_ = false;
            reconcile(code);
            refresh();
            return;
        }
        if (code == QLatin1String("BACKEND_UNAVAILABLE") && reply.argumentAt<4>()) {
            pendingOperationUncertain_ = true;
            reconcile();
            scheduleRetry();
            return;
        }
        pendingOperationId_.clear();
        pendingNotificationId_.clear();
        pendingOperationUncertain_ = false;
        reconcile(code);
        refresh();
    });
}

void Notifications1Client::onNotificationsChanged(const QString &uuid,
                                                    qulonglong generation,
                                                    qulonglong sequence,
                                                    const QString &subjectKind,
                                                    const QString &subjectId,
                                                    qulonglong eventRevision)
{
    Q_UNUSED(subjectId)
    if (subjectKind != QLatin1String("NOTIFICATION") || sequence == 0
        || eventRevision == 0 || !identityMatches(uuid, generation)) {
        reconcile();
        readiness_.refresh();
        scheduleRetry();
        return;
    }
    // EventSequence is shared by all session interfaces and can legitimately skip.
    eventSequence_ = qMax(eventSequence_, sequence);
    if (requestInFlight_) refreshPending_ = true;
    else refresh();
}

void Notifications1Client::onOwnerChanged(const QString &newOwner)
{
    if (newOwner.isEmpty()) {
        ready_ = false;
        beginResetModel();
        notifications_.clear();
        endResetModel();
        serviceInstanceUuid_.clear();
        serviceGeneration_ = 0;
        eventSequence_ = 0;
        revision_ = 0;
        emit unreadCountChanged();
        emit countChanged();
        setStatus(QStringLiteral("UNAVAILABLE"));
        return;
    }
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    eventSequence_ = 0;
    revision_ = 0;
    reconcile();
    readiness_.refresh();
}

void Notifications1Client::updateReadiness()
{
    if (!readiness_.ready()) {
        ready_ = false;
        setStatus(readiness_.status());
        return;
    }
    if (!identityMatches(serviceInstanceUuid_, serviceGeneration_)) {
        reconcile();
        refresh();
    }
}

void Notifications1Client::reconcile(const QString &newStatus)
{
    ready_ = false;
    setStatus(newStatus);
}

void Notifications1Client::scheduleRetry()
{
    if (retryTimer_.isActive()) return;
    constexpr int initialDelayMs = 100;
    constexpr int maximumDelayMs = 2000;
    const int delayMs = qMin(initialDelayMs * (1 << qMin(retryAttempt_, 5)), maximumDelayMs);
    ++retryAttempt_;
    retryTimer_.start(delayMs);
}

void Notifications1Client::setStatus(const QString &newStatus)
{
    if (status_ != newStatus) {
        status_ = newStatus;
        emit stateChanged();
    } else {
        emit stateChanged();
    }
}

bool Notifications1Client::identityMatches(const QString &uuid, qulonglong generation) const
{
    return readiness_.ready() && !uuid.isEmpty() && generation != 0
        && uuid == readiness_.serviceInstanceUuid()
        && generation == readiness_.serviceGeneration();
}
