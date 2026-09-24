#include "session_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QUuid>

using adrenalin::contracts::OperationResultCode;

SessionService::SessionService(QString databasePath, QObject *parent)
    : QObject(parent), serviceInstanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      database_(std::make_unique<SessionDatabase>(std::move(databasePath)))
{
    logEvent(QStringLiteral("state_changed"), QStringLiteral("info"), initializationState());
}

SessionService::~SessionService() = default;

bool SessionService::initialize()
{
    setState(State::Recovering);
    QString error;
    if (!database_->initialize(&error)) {
        setState(State::Failed, error);
        logEvent(QStringLiteral("initialization_failed"), QStringLiteral("error"), error);
        return false;
    }
    setState(State::Ready);
    logEvent(QStringLiteral("service_ready"), QStringLiteral("info"));
    return true;
}

QString SessionService::initializationState() const
{
    switch (state_) {
    case State::Starting: return QStringLiteral("STARTING");
    case State::Recovering: return QStringLiteral("RECOVERING");
    case State::Ready: return QStringLiteral("READY");
    case State::Degraded: return QStringLiteral("DEGRADED");
    case State::Failed: return QStringLiteral("FAILED");
    }
    return QStringLiteral("FAILED");
}

QString SessionService::serviceInstanceUuid() const { return serviceInstanceUuid_; }
qulonglong SessionService::serviceGeneration() const { return database_->generation(); }
ushort SessionService::apiMajor() const { return 1; }
ushort SessionService::apiMinor() const { return 0; }
QString SessionService::lastInitializationError() const { return lastInitializationError_; }

void SessionService::setState(State state, QString error)
{
    state_ = state;
    lastInitializationError_ = std::move(error);
    emit initializationStateChanged();
    logEvent(QStringLiteral("state_changed"), state_ == State::Failed ? QStringLiteral("error")
                                                                       : QStringLiteral("info"),
             initializationState());
}

void SessionService::logEvent(const QString &eventName, const QString &level,
                              const QString &detail) const
{
    QJsonObject record{
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("level"), level},
        {QStringLiteral("event"), eventName},
        {QStringLiteral("service"), QStringLiteral("adrenalin-sessiond")},
        {QStringLiteral("service_instance_uuid"), serviceInstanceUuid_},
        {QStringLiteral("service_generation"), static_cast<qint64>(serviceGeneration())},
        {QStringLiteral("api_major"), apiMajor()},
        {QStringLiteral("api_minor"), apiMinor()}
    };
    if (!detail.isEmpty()) {
        record.insert(QStringLiteral("detail"), detail);
    }
    qInfo().noquote() << QJsonDocument(record).toJson(QJsonDocument::Compact);
}

bool SessionService::getProductTelemetryConsent(bool *enabled, quint64 *revision,
                                                QString *error)
{
    if (state_ != State::Ready) {
        if (error != nullptr) {
            *error = QStringLiteral("Service is not READY");
        }
        return false;
    }
    const auto value = database_->readProductTelemetryConsent(error);
    if (!value) {
        return false;
    }
    if (enabled != nullptr) {
        *enabled = value->enabled;
    }
    if (revision != nullptr) {
        *revision = value->revision;
    }
    return true;
}

Settings1ReadResult SessionService::getProductTelemetryConsent()
{
    Settings1ReadResult result;
    result.resultCode = QStringLiteral("OK");
    result.serviceInstanceUuid = serviceInstanceUuid();
    result.serviceGeneration = serviceGeneration();
    result.eventSequence = eventSequence_;
    const bool ok = getProductTelemetryConsent(&result.enabled, &result.revision, nullptr);
    if (!ok) {
        result.resultCode = state_ == State::Ready ? QStringLiteral("STORAGE_FAILURE")
                                                    : QStringLiteral("NOT_READY");
    }
    return result;
}

Settings1WriteResult SessionService::setProductTelemetryConsent(const QString &operationId,
                                                                bool enabled,
                                                                quint64 expectedRevision)
{
    Settings1WriteResult writeResult;
    auto &result = writeResult.result;
    result.operationId = operationId;
    result.provider = QStringLiteral("session-settings");
    result.subjectId = QStringLiteral("product.telemetry_consent");
    if (state_ != State::Ready) {
        result.code = OperationResultCode::BackendUnavailable;
        result.humanMessageKey = QStringLiteral("service.recovering");
        result.diagnosticMessage = QStringLiteral("Session service is not READY");
        result.retryable = true;
        return writeResult;
    }
    bool stale = false;
    bool conflict = false;
    bool operationReplayed = false;
    QString error;
    if (!database_->updateProductTelemetryConsent(operationId, enabled, expectedRevision,
                                                   &result.revision, &stale, &conflict,
                                                   &operationReplayed, &error)) {
        result.diagnosticMessage = error;
        if (stale) {
            result.code = OperationResultCode::StaleRevision;
            result.humanMessageKey = QStringLiteral("settings.operation.stale_revision");
        } else if (conflict) {
            result.code = OperationResultCode::Conflict;
            result.humanMessageKey = QStringLiteral("settings.operation.conflict");
        } else if (error.startsWith(QStringLiteral("Operation ID"))) {
            result.code = OperationResultCode::InvalidArgument;
            result.humanMessageKey = QStringLiteral("settings.operation.invalid_argument");
        } else {
            result.code = OperationResultCode::IoError;
            result.humanMessageKey = QStringLiteral("settings.operation.storage_failed");
        }
        return writeResult;
    }
    if (!operationReplayed) {
        ++eventSequence_;
        emit ProductTelemetryConsentChanged(serviceInstanceUuid(), serviceGeneration(),
                                            eventSequence_, result.subjectId, enabled,
                                            result.revision);
    }
    result.code = OperationResultCode::Ok;
    result.humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
    return writeResult;
}
