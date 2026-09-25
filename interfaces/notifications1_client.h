#pragma once

#include <notifications1_interface.h>
#include "service_readiness_client.h"

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QTimer>

class Notifications1Client final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY stateChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastOperationCode READ lastOperationCode NOTIFY stateChanged)

public:
    enum Role {
        NotificationIdRole = Qt::UserRole + 1,
        CategoryRole,
        CreatedAtUtcRole,
        TitleMessageKeyRole,
        BodyMessageKeyRole,
        IsReadRole,
        CriticalRole,
        ToastEligibleRole
    };
    Q_ENUM(Role)

    explicit Notifications1Client(QObject *parent = nullptr);
    explicit Notifications1Client(const QDBusConnection &connection, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool ready() const;
    QString status() const;
    int unreadCount() const;
    int count() const;
    qulonglong revision() const;
    QString lastOperationCode() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void markRead(const QString &notificationId);

signals:
    void stateChanged();
    void unreadCountChanged();
    void countChanged();

private:
    void updateReadiness();
    void onOwnerChanged(const QString &newOwner);
    void onNotificationsChanged(const QString &serviceInstanceUuid,
                                qulonglong serviceGeneration, qulonglong eventSequence,
                                const QString &subjectKind, const QString &subjectId,
                                qulonglong revision);
    void requestSnapshot();
    void requestMutation();
    void reconcile(const QString &status = QStringLiteral("RECONCILING"));
    void scheduleRetry();
    void setStatus(const QString &status);
    bool identityMatches(const QString &uuid, qulonglong generation) const;

    OrgAdrenalinlinuxSession1Notifications1Interface proxy_;
    QDBusServiceWatcher ownerWatcher_;
    ServiceReadinessClient readiness_;
    QList<adrenalin::contracts::notifications1::Notification> notifications_;
    QString status_ = QStringLiteral("CONNECTING");
    QString serviceInstanceUuid_;
    QString pendingNotificationId_;
    QString lastOperationCode_;
    QString pendingOperationId_;
    qulonglong serviceGeneration_ = 0;
    qulonglong eventSequence_ = 0;
    qulonglong revision_ = 0;
    qulonglong pendingExpectedRevision_ = 0;
    bool ready_ = false;
    bool requestInFlight_ = false;
    bool refreshPending_ = false;
    bool pendingOperationUncertain_ = false;
    QTimer retryTimer_;
    int retryAttempt_ = 0;
};
