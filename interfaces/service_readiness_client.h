#pragma once

#include <service1_interface.h>

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

class ServiceReadinessClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool compatible READ compatible NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString initializationState READ initializationState NOTIFY stateChanged)
    Q_PROPERTY(QString serviceInstanceUuid READ serviceInstanceUuid NOTIFY stateChanged)
    Q_PROPERTY(qulonglong serviceGeneration READ serviceGeneration NOTIFY stateChanged)
    Q_PROPERTY(ushort apiMajor READ apiMajor NOTIFY stateChanged)
    Q_PROPERTY(ushort apiMinor READ apiMinor NOTIFY stateChanged)
    Q_PROPERTY(QString lastInitializationError READ lastInitializationError NOTIFY stateChanged)

public:
    ServiceReadinessClient(QString serviceName, QString objectPath,
                           const QDBusConnection &connection, ushort expectedApiMajor,
                           QObject *parent = nullptr);

    bool available() const;
    bool compatible() const;
    bool ready() const;
    QString status() const;
    QString initializationState() const;
    QString serviceInstanceUuid() const;
    qulonglong serviceGeneration() const;
    qulonglong eventSequence() const;
    ushort apiMajor() const;
    ushort apiMinor() const;
    QString lastInitializationError() const;

    Q_INVOKABLE void refresh();

signals:
    void stateChanged();

private slots:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onEventPublished(const QString &serviceInstanceUuid, qulonglong serviceGeneration,
                          qulonglong eventSequence, const QString &subjectKind,
                          const QString &subjectId);

private:
    void clearSnapshot(const QString &status);
    void applySnapshot(const QVariantMap &properties);
    void finishRefresh(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch);

    OrgAdrenalinlinuxSession1Service1Interface proxy_;
    QDBusConnection connection_;
    QDBusServiceWatcher ownerWatcher_;
    QString serviceName_;
    QString objectPath_;
    ushort expectedApiMajor_ = 0;
    bool available_ = false;
    bool compatible_ = false;
    QString status_ = QStringLiteral("CONNECTING");
    QString initializationState_;
    QString serviceInstanceUuid_;
    qulonglong serviceGeneration_ = 0;
    qulonglong eventSequence_ = 0;
    QString lastEventInstanceUuid_;
    qulonglong lastEventServiceGeneration_ = 0;
    qulonglong lastEventSequence_ = 0;
    ushort apiMajor_ = 0;
    ushort apiMinor_ = 0;
    QString lastInitializationError_;
    bool requestInFlight_ = false;
    bool refreshPending_ = false;
    quint64 ownerEpoch_ = 0;
};
