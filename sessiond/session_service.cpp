#include "session_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QUuid>

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
    const bool ok = getProductTelemetryConsent(&result.enabled, &result.revision, nullptr);
    if (!ok) {
        result.resultCode = state_ == State::Ready ? QStringLiteral("STORAGE_FAILURE")
                                                    : QStringLiteral("NOT_READY");
    }
    return result;
}

QString SessionService::setProductTelemetryConsent(const QString &operationId, bool enabled,
                                                   quint64 expectedRevision,
                                                   quint64 *newRevision,
                                                   bool *replayed)
{
    if (state_ != State::Ready) {
        return QStringLiteral("NOT_READY");
    }
    bool stale = false;
    bool conflict = false;
    bool operationReplayed = false;
    QString error;
    if (!database_->updateProductTelemetryConsent(operationId, enabled, expectedRevision,
                                                   newRevision, &stale, &conflict,
                                                   &operationReplayed, &error)) {
        if (stale) return QStringLiteral("STALE_REVISION");
        if (conflict) return QStringLiteral("OPERATION_CONFLICT");
        if (error.startsWith(QStringLiteral("Operation ID"))) return QStringLiteral("INVALID_ARGUMENT");
        return QStringLiteral("STORAGE_FAILURE");
    }
    if (replayed != nullptr) {
        *replayed = operationReplayed;
    }
    if (!operationReplayed) {
        emit productTelemetryConsentChanged(enabled, newRevision != nullptr ? *newRevision : expectedRevision + 1);
        emit ProductTelemetryConsentChanged(enabled, newRevision != nullptr ? *newRevision : expectedRevision + 1);
    }
    return QStringLiteral("OK");
}

Settings1WriteResult SessionService::setProductTelemetryConsent(const QString &operationId,
                                                                bool enabled,
                                                                quint64 expectedRevision)
{
    Settings1WriteResult result;
    result.resultCode = setProductTelemetryConsent(operationId, enabled, expectedRevision,
                                                   &result.revision);
    return result;
}
