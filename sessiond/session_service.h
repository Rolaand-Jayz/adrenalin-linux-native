#pragma once

#include "session_database.h"
#include "interfaces/settings1_contract.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include <memory>

class SessionService final : public QObject, public Settings1Contract
{
    Q_OBJECT
    Q_PROPERTY(QString initializationState READ initializationState NOTIFY initializationStateChanged)
    Q_PROPERTY(QString serviceInstanceUuid READ serviceInstanceUuid CONSTANT)
    Q_PROPERTY(qulonglong serviceGeneration READ serviceGeneration NOTIFY initializationStateChanged)
    Q_PROPERTY(ushort apiMajor READ apiMajor CONSTANT)
    Q_PROPERTY(ushort apiMinor READ apiMinor CONSTANT)
    Q_PROPERTY(QString lastInitializationError READ lastInitializationError NOTIFY initializationStateChanged)
    Q_PROPERTY(QString InitializationState READ initializationState NOTIFY initializationStateChanged)
    Q_PROPERTY(QString ServiceInstanceUuid READ serviceInstanceUuid CONSTANT)
    Q_PROPERTY(qulonglong ServiceGeneration READ serviceGeneration NOTIFY initializationStateChanged)
    Q_PROPERTY(ushort ApiMajor READ apiMajor CONSTANT)
    Q_PROPERTY(ushort ApiMinor READ apiMinor CONSTANT)
    Q_PROPERTY(QString LastInitializationError READ lastInitializationError NOTIFY initializationStateChanged)

public:
    enum class State { Starting, Recovering, Ready, Degraded, Failed };
    Q_ENUM(State)

    explicit SessionService(QString databasePath, QObject *parent = nullptr);
    ~SessionService() override;

    bool initialize();
    QString initializationState() const override;
    QString serviceInstanceUuid() const override;
    qulonglong serviceGeneration() const override;
    ushort apiMajor() const override;
    ushort apiMinor() const override;
    QString lastInitializationError() const override;

    bool getProductTelemetryConsent(bool *enabled, quint64 *revision, QString *error);
    Settings1ReadResult getProductTelemetryConsent() override;
    Settings1WriteResult setProductTelemetryConsent(const QString &operationId, bool enabled,
                                                    quint64 expectedRevision) override;

signals:
    void initializationStateChanged();
    void ProductTelemetryConsentChanged(const QString &serviceInstanceUuid,
                                        qulonglong serviceGeneration,
                                        qulonglong eventSequence,
                                        const QString &subjectId,
                                        bool enabled,
                                        qulonglong revision);

private:
    void setState(State state, QString error = {});
    void logEvent(const QString &eventName, const QString &level,
                  const QString &detail = {}) const;

    State state_ = State::Starting;
    QString serviceInstanceUuid_;
    QString lastInitializationError_;
    quint64 eventSequence_ = 0;
    std::unique_ptr<SessionDatabase> database_;
};
