#pragma once

#include "session_database.h"
#include "interfaces/settings1_contract.h"
#include "interfaces/linux_hardware1_inventory_provider.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QThread>
#include <QSet>

#include <memory>

class SessionService final : public QObject, public Settings1Contract
{
    friend class SessionContractTest;

    Q_OBJECT
    Q_PROPERTY(QString initializationState READ initializationState NOTIFY initializationStateChanged)
    Q_PROPERTY(QString serviceInstanceUuid READ serviceInstanceUuid CONSTANT)
    Q_PROPERTY(qulonglong serviceGeneration READ serviceGeneration NOTIFY initializationStateChanged)
    Q_PROPERTY(qulonglong eventSequence READ eventSequence NOTIFY eventPublished)
    Q_PROPERTY(ushort apiMajor READ apiMajor CONSTANT)
    Q_PROPERTY(ushort apiMinor READ apiMinor CONSTANT)
    Q_PROPERTY(QString lastInitializationError READ lastInitializationError NOTIFY initializationStateChanged)
    Q_PROPERTY(QString InitializationState READ initializationState NOTIFY initializationStateChanged)
    Q_PROPERTY(QString ServiceInstanceUuid READ serviceInstanceUuid CONSTANT)
    Q_PROPERTY(qulonglong ServiceGeneration READ serviceGeneration NOTIFY initializationStateChanged)
    Q_PROPERTY(qulonglong EventSequence READ eventSequence NOTIFY eventPublished)
    Q_PROPERTY(ushort ApiMajor READ apiMajor CONSTANT)
    Q_PROPERTY(ushort ApiMinor READ apiMinor CONSTANT)
    Q_PROPERTY(QString LastInitializationError READ lastInitializationError NOTIFY initializationStateChanged)

public:
    enum class State { Starting, Recovering, Ready, Degraded, Failed };
    Q_ENUM(State)

    explicit SessionService(QString databasePath, QObject *parent = nullptr);
    ~SessionService() override;

    bool initialize();
    bool initializeAsync();
    void requestHardwareInventoryRefresh();
    void setHardwareObserverUnavailable();
    void requestHardwareObserverRecovery();
    QString initializationState() const override;
    QString serviceInstanceUuid() const override;
    qulonglong serviceGeneration() const override;
    qulonglong eventSequence() const;
    QString eventSubjectKind() const;
    QString eventSubjectId() const;
    ushort apiMajor() const override;
    ushort apiMinor() const override;
    QString lastInitializationError() const override;
    bool hardwareRefreshAvailable() const;
    bool hardwareObserverAvailable() const;
    bool hardwareSubjectDisconnected(const QString &kind, const QString &id) const;

    bool getProductTelemetryConsent(bool *enabled, quint64 *revision, QString *error);
    Settings1ReadResult getProductTelemetryConsent() override;
    Settings1WriteResult setProductTelemetryConsent(const QString &operationId, bool enabled,
                                                    quint64 expectedRevision) override;
#ifdef ADRENALIN_SESSION_HARDWARE1_TESTING
    void setHardware1SnapshotForTesting(
        const adrenalin::hardware::Hardware1Snapshot &snapshot);
    bool reconcileHardwareSnapshotForTesting(
        const adrenalin::hardware::Hardware1Snapshot &snapshot);
#endif
    adrenalin::contracts::hardware1::Reply listHardwareDevices(
        QList<adrenalin::contracts::hardware1::Device> *devices) const;
    adrenalin::contracts::hardware1::Reply getHardwareDeviceInfo(
        const QString &subjectKind, const QString &subjectId,
        adrenalin::contracts::hardware1::DeviceInfo *info) const;
    adrenalin::contracts::hardware1::Reply getHardwareCapabilityGraph(
        const QString &subjectKind, const QString &subjectId,
        QList<adrenalin::contracts::hardware1::Capability> *capabilities) const;

signals:
    void initializationStateChanged();
    void eventPublished();
    void eventSequenceExhausted();
    void ProductTelemetryConsentChanged(const QString &serviceInstanceUuid,
                                        qulonglong serviceGeneration,
                                        qulonglong eventSequence,
                                        const QString &subjectKind,
                                        const QString &subjectId,
                                        bool enabled,
                                        qulonglong revision);
    void InventoryChanged(const QString &service_instance_uuid, qulonglong service_generation,
                         qulonglong event_sequence, const QString &subject_kind,
                         const QString &subject_id, qulonglong inventory_generation,
                         qulonglong capability_generation);
    void CapabilityGraphChanged(const QString &service_instance_uuid, qulonglong service_generation,
                                qulonglong event_sequence, const QString &subject_kind,
                                const QString &subject_id, qulonglong inventory_generation,
                                qulonglong capability_generation);

private:
    qulonglong nextEventSequence(const QString &subjectKind, const QString &subjectId);
    void failEventSequenceExhausted();
    bool setState(State state, QString error = {});
    void logEvent(const QString &eventName, const QString &level,
                  const QString &detail = {}) const;
    bool prepareDatabaseRecovery();
    bool publishHardwareInitialization(adrenalin::hardware::Hardware1Snapshot snapshot);
    void startHardwareInventoryRefresh();
    static QString hardwareSubjectKey(const QString &kind, const QString &id);

    State state_ = State::Starting;
    QString serviceInstanceUuid_;
    QString lastInitializationError_;
    quint64 eventSequence_ = 0;
    QString eventSubjectKind_;
    QString eventSubjectId_;
    std::unique_ptr<SessionDatabase> database_;
    adrenalin::hardware::LinuxHardware1InventoryProvider hardware1Provider_;
    adrenalin::hardware::Hardware1Snapshot hardware1Snapshot_;
    QThread *hardwareInventoryThread_ = nullptr;
    bool hardwareInitializationComplete_ = false;
    bool hardwareRefreshPending_ = false;
    bool hardwareRefreshAvailable_ = true;
    bool hardwareObserverAvailable_ = true;
    bool hardwareObserverRecoveryPending_ = false;
    QSet<QString> disconnectedHardwareSubjects_;
#ifdef ADRENALIN_SESSION_HARDWARE1_TESTING
    bool hardware1SnapshotInjectedForTesting_ = false;
#endif
};
