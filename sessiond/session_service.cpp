#include "session_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QUuid>

#include <limits>

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
    if (!setState(State::Recovering)) {
        return false;
    }
    QString error;
    if (!database_->initialize(&error)) {
        setState(State::Failed, error);
        logEvent(QStringLiteral("initialization_failed"), QStringLiteral("error"), error);
        return false;
    }
    if (!setState(State::Ready)) {
        return false;
    }
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
qulonglong SessionService::serviceGeneration() const
{
    // The service incarnation exists before SQLite recovery finishes. Keep the
    // invalid-snapshot envelope contract usable during STARTING/RECOVERING;
    // successful database recovery replaces this provisional floor with its
    // persisted, monotonically advanced generation before READY is published.
    return qMax<qulonglong>(1, database_->generation());
}
qulonglong SessionService::eventSequence() const { return eventSequence_; }
QString SessionService::eventSubjectKind() const { return eventSubjectKind_; }
QString SessionService::eventSubjectId() const { return eventSubjectId_; }
qulonglong SessionService::nextEventSequence(const QString &subjectKind, const QString &subjectId)
{
    if (eventSequence_ == std::numeric_limits<quint64>::max()) {
        return 0;
    }
    eventSubjectKind_ = subjectKind;
    eventSubjectId_ = subjectId;
    return ++eventSequence_;
}

void SessionService::failEventSequenceExhausted()
{
    state_ = State::Failed;
    lastInitializationError_ = QStringLiteral("Per-service event sequence exhausted");
    emit initializationStateChanged();
    emit eventSequenceExhausted();
    logEvent(QStringLiteral("event_sequence_exhausted"), QStringLiteral("error"),
             lastInitializationError_);
}

ushort SessionService::apiMajor() const { return 1; }
ushort SessionService::apiMinor() const { return 0; }
QString SessionService::lastInitializationError() const { return lastInitializationError_; }

bool SessionService::setState(State state, QString error)
{
    state_ = state;
    lastInitializationError_ = std::move(error);
    if (nextEventSequence(QStringLiteral("SERVICE"), QStringLiteral("service.readiness")) == 0) {
        failEventSequenceExhausted();
        return false;
    }
    emit eventPublished();
    emit initializationStateChanged();
    logEvent(QStringLiteral("state_changed"), state_ == State::Failed ? QStringLiteral("error")
                                                                       : QStringLiteral("info"),
             initializationState());
    return true;
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
    if (eventSequence_ == std::numeric_limits<quint64>::max()) {
        failEventSequenceExhausted();
        result.code = OperationResultCode::InternalError;
        result.humanMessageKey = QStringLiteral("service.event_sequence_exhausted");
        result.diagnosticMessage = QStringLiteral("No further sequenced events can be published");
        result.retryable = false;
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
        nextEventSequence(QStringLiteral("PREFERENCE"), result.subjectId);
        emit ProductTelemetryConsentChanged(serviceInstanceUuid(), serviceGeneration(),
                                            eventSequence_, eventSubjectKind_, result.subjectId,
                                            enabled, result.revision);
        emit eventPublished();
    }
    result.code = OperationResultCode::Ok;
    result.humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
    return writeResult;
}
