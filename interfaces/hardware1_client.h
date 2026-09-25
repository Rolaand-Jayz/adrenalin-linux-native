#pragma once

#include <hardware1_interface.h>
#include <service1_interface.h>

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#include <QVariantList>

class QDBusPendingCallWatcher;

namespace adrenalin::contracts::hardware1 {

class Client final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY snapshotChanged)
    Q_PROPERTY(QString status READ status NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList deviceData READ deviceData NOTIFY snapshotChanged)

public:
    Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
           QObject *parent = nullptr);

    bool available() const;
    QString status() const;
    QList<Device> devices() const;
    QVariantList deviceData() const;
    QString serviceInstanceUuid() const;
    quint64 serviceGeneration() const;
    quint64 eventSequence() const;
    quint64 inventoryGeneration() const;
    quint64 capabilityGeneration() const;

    Q_INVOKABLE void refresh();

signals:
    void snapshotChanged();

private slots:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onEventPublished(const QString &serviceInstanceUuid, qulonglong serviceGeneration,
                          qulonglong eventSequence, const QString &subjectKind,
                          const QString &subjectId);
    void onInventoryChanged(const QString &serviceInstanceUuid, qulonglong serviceGeneration,
                            qulonglong eventSequence, const QString &subjectKind,
                            const QString &subjectId, qulonglong inventoryGeneration,
                            qulonglong capabilityGeneration);
    void onCapabilityGraphChanged(const QString &serviceInstanceUuid,
                                  qulonglong serviceGeneration, qulonglong eventSequence,
                                  const QString &subjectKind, const QString &subjectId,
                                  qulonglong inventoryGeneration,
                                  qulonglong capabilityGeneration);

private:
    void clearSnapshot(const QString &status);
    bool acceptEventIdentity(const QString &uuid, quint64 generation, quint64 sequence,
                             const QString &subjectKind, const QString &subjectId,
                             bool hardwareSpecific);
    void invalidateAndRefresh(const QString &status);
    void finishRefresh(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch,
                       quint64 eventEpoch);

    OrgAdrenalinlinuxSession1Hardware1Interface hardwareProxy_;
    OrgAdrenalinlinuxSession1Service1Interface serviceProxy_;
    QDBusConnection connection_;
    QDBusServiceWatcher ownerWatcher_;
    QString serviceName_;
    QString objectPath_;
    bool available_ = false;
    QString status_ = QStringLiteral("CONNECTING");
    QList<Device> devices_;
    QString serviceInstanceUuid_;
    quint64 serviceGeneration_ = 0;
    quint64 eventSequence_ = 0;
    quint64 snapshotEventSequence_ = 0;
    quint64 inventoryGeneration_ = 0;
    quint64 capabilityGeneration_ = 0;
    QString eventInstanceUuid_;
    quint64 eventServiceGeneration_ = 0;
    quint64 lastObservedEventSequence_ = 0;
    quint64 lastHardwareEventSequence_ = 0;
    bool hardwareEventSeen_ = false;
    bool requestInFlight_ = false;
    bool refreshPending_ = false;
    quint64 ownerEpoch_ = 0;
    quint64 eventEpoch_ = 0;
};

} // namespace adrenalin::contracts::hardware1
