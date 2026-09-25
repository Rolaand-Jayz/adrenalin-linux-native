#include "session_service.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QStandardPaths>
#include <QMetaObject>
#include <QTimer>

#include "interfaces/operation_result.h"

#include <algorithm>
#include <QUuid>

#include <limits>


using adrenalin::contracts::OperationResultCode;

SessionService::SessionService(QString databasePath, QObject *parent)
    : QObject(parent), serviceInstanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      database_(std::make_unique<SessionDatabase>(std::move(databasePath)))
{
    logEvent(QStringLiteral("state_changed"), QStringLiteral("info"), initializationState());
}

SessionService::~SessionService()
{
    if (hardwareInventoryThread_ != nullptr) {
        hardwareInventoryThread_->wait();
        delete hardwareInventoryThread_;
        hardwareInventoryThread_ = nullptr;
    }
}

bool SessionService::prepareDatabaseRecovery()
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
    return true;
}

bool SessionService::initialize()
{
#ifndef ADRENALIN_SESSION_HARDWARE1_TESTING
    return initializeAsync();
#else
    if (!prepareDatabaseRecovery()) {
        return false;
    }
    auto snapshot = hardware1Snapshot_;
#ifdef ADRENALIN_SESSION_HARDWARE1_TESTING
    if (!hardware1SnapshotInjectedForTesting_)
#endif
    {
        QThread *worker = QThread::create([this, &snapshot] {
            snapshot = hardware1Provider_.refresh();
        });
        worker->start();
        worker->wait();
        delete worker;
    }
    return publishHardwareInitialization(std::move(snapshot));
#endif
}

bool SessionService::initializeAsync()
{
    if (!prepareDatabaseRecovery()) {
        return false;
    }
#ifdef ADRENALIN_SESSION_HARDWARE1_TESTING
    if (hardware1SnapshotInjectedForTesting_) {
        const auto snapshot = hardware1Snapshot_;
        hardware1Snapshot_ = {};
        QTimer::singleShot(75, this, [this, snapshot] {
            publishHardwareInitialization(snapshot);
        });
        return true;
    }
#endif
    if (hardwareInventoryThread_ != nullptr) {
        return false;
    }
    hardwareInventoryThread_ = QThread::create([this] {
        const auto snapshot = hardware1Provider_.refresh();
        QMetaObject::invokeMethod(this, [this, snapshot] {
            publishHardwareInitialization(snapshot);
        }, Qt::QueuedConnection);
    });
    hardwareInventoryThread_->start();
    return true;
}

bool SessionService::publishHardwareInitialization(
    adrenalin::hardware::Hardware1Snapshot currentHardware)
{
    if (hardwareInventoryThread_ != nullptr) {
        hardwareInventoryThread_->wait();
        delete hardwareInventoryThread_;
        hardwareInventoryThread_ = nullptr;
    }
    const auto previousHardware = hardware1Snapshot_;
    hardware1Snapshot_ = std::move(currentHardware);
    const auto &snapshot = hardware1Snapshot_;
    const bool inventoryChanged = snapshot.success
        && (!previousHardware.success
            || previousHardware.inventoryGeneration != snapshot.inventoryGeneration);
    const bool capabilitiesChanged = snapshot.success
        && (!previousHardware.success
            || previousHardware.capabilityGeneration != snapshot.capabilityGeneration);
    const quint64 initialHardwareEvents = static_cast<quint64>(inventoryChanged)
        + static_cast<quint64>(capabilitiesChanged);
    if (eventSequence_ > std::numeric_limits<quint64>::max() - 1 - initialHardwareEvents) {
        failEventSequenceExhausted();
        return false;
    }
    if (!snapshot.success) {
        setState(State::Failed, QStringLiteral("Hardware inventory evidence is unavailable"));
        logEvent(QStringLiteral("hardware_initialization_failed"), QStringLiteral("error"));
        return false;
    }
    if (!setState(State::Ready)) {
        return false;
    }
    if (snapshot.success) {
        if (inventoryChanged) {
            if (nextEventSequence(QStringLiteral("PLATFORM"), QStringLiteral("platform")) == 0) {
                failEventSequenceExhausted();
                return false;
            }
            emit InventoryChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                  QStringLiteral("PLATFORM"), QStringLiteral("platform"),
                                  snapshot.inventoryGeneration, snapshot.capabilityGeneration);
            emit eventPublished();
        }
        if (capabilitiesChanged) {
            if (nextEventSequence(QStringLiteral("PLATFORM"), QStringLiteral("platform")) == 0) {
                failEventSequenceExhausted();
                return false;
            }
            emit CapabilityGraphChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                        QStringLiteral("PLATFORM"), QStringLiteral("platform"),
                                        snapshot.inventoryGeneration,
                                        snapshot.capabilityGeneration);
            emit eventPublished();
        }
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

#ifdef ADRENALIN_SESSION_HARDWARE1_TESTING
void SessionService::setHardware1SnapshotForTesting(
    const adrenalin::hardware::Hardware1Snapshot &snapshot)
{
    hardware1Snapshot_ = snapshot;
    hardware1SnapshotInjectedForTesting_ = true;
}
#endif

namespace {
using namespace adrenalin::contracts::hardware1;

using adrenalin::hardware::Hardware1Snapshot;

Reply hardwareReply(const SessionService *service, const Hardware1Snapshot &snapshot,
                    const QString &kind, const QString &id)
{
    Reply reply;
    reply.subjectKind = kind;
    reply.subjectId = id;
    reply.serviceInstanceUuid = service->serviceInstanceUuid();
    reply.serviceGeneration = service->serviceGeneration();
    reply.provider = QStringLiteral("linux-hardware1-inventory");
    const QString state = service->initializationState();
    if (state == QLatin1String("STARTING") || state == QLatin1String("RECOVERING")) {
        reply.code = QStringLiteral("BUSY");
        reply.humanMessageKey = QStringLiteral("service.recovering");
        reply.diagnosticMessage = QStringLiteral("Hardware inventory reconciliation is in progress");
        reply.retryable = true;
        reply.snapshotValid = false;
        return reply;
    }
    if (state == QLatin1String("FAILED") || !snapshot.success) {
        reply.code = QStringLiteral("BACKEND_UNAVAILABLE");
        reply.humanMessageKey = QStringLiteral("hardware.snapshot.unavailable");
        reply.diagnosticMessage = snapshot.error.isEmpty()
            ? QStringLiteral("Hardware inventory evidence is unavailable") : snapshot.error;
        reply.snapshotValid = false;
        return reply;
    }
    reply.code = QStringLiteral("OK");
    reply.humanMessageKey = QStringLiteral("hardware.read.ok");
    reply.snapshotValid = true;
    reply.inventoryGeneration = snapshot.inventoryGeneration;
    reply.capabilityGeneration = snapshot.capabilityGeneration;
    return reply;
}

Reply invalidHardwareArgument(const SessionService *service, const Hardware1Snapshot &snapshot)
{
    Reply reply = hardwareReply(service, snapshot, QStringLiteral("PLATFORM"),
                                QStringLiteral("platform"));
    if (reply.snapshotValid) {
        reply.code = QStringLiteral("INVALID_ARGUMENT");
        reply.humanMessageKey = QStringLiteral("operation.invalidArgument");
        reply.diagnosticMessage = QStringLiteral("The Hardware1 subject selector is invalid");
    }
    return reply;
}

bool validSelector(const QString &kind, const QString &id)
{
    return isValidSubjectKind(kind) && !id.isEmpty()
        && (kind != QLatin1String("PLATFORM") || id == QLatin1String("platform"));
}

Reply notFoundReply(const SessionService *service, const Hardware1Snapshot &snapshot,
                    const QString &kind, const QString &id)
{
    Reply reply = hardwareReply(service, snapshot, kind, id);
    reply.code = QStringLiteral("NOT_FOUND");
    reply.humanMessageKey = QStringLiteral("hardware.subject.notFound");
    reply.diagnosticMessage = QStringLiteral("The requested subject is absent from the current inventory snapshot");
    return reply;
}
} // namespace

adrenalin::contracts::hardware1::Reply SessionService::listHardwareDevices(
    QList<adrenalin::contracts::hardware1::Device> *devices) const
{
    using namespace adrenalin::contracts::hardware1;
    const Hardware1Snapshot snapshot = hardware1Snapshot_;
    if (devices == nullptr) {
        return invalidHardwareArgument(this, snapshot);
    }
    *devices = {};
    Reply reply = hardwareReply(this, snapshot, QStringLiteral("PLATFORM"), QStringLiteral("platform"));
    if (reply.snapshotValid) {
        *devices = snapshot.devices;
    }
    return reply;
}

adrenalin::contracts::hardware1::Reply SessionService::getHardwareDeviceInfo(
    const QString &subjectKind, const QString &subjectId,
    adrenalin::contracts::hardware1::DeviceInfo *info) const
{
    using namespace adrenalin::contracts::hardware1;
    const Hardware1Snapshot snapshot = hardware1Snapshot_;
    if (info == nullptr) {
        return invalidHardwareArgument(this, snapshot);
    }
    *info = {};
    if (!validSelector(subjectKind, subjectId)) {
        return invalidHardwareArgument(this, snapshot);
    }
    Reply reply = hardwareReply(this, snapshot, subjectKind, subjectId);
    if (!reply.snapshotValid) {
        return reply;
    }
    if (subjectKind == QLatin1String("PLATFORM")) {
        info->subjectKind = subjectKind;
        info->subjectId = subjectId;
        info->identityEvidence = QStringLiteral("contract.platform.reserved_id");
        info->displayName = QStringLiteral("Platform");
        return reply;
    }
    const auto found = std::find_if(snapshot.deviceInfo.cbegin(), snapshot.deviceInfo.cend(),
        [&](const DeviceInfo &candidate) {
            return candidate.subjectKind == subjectKind && candidate.subjectId == subjectId;
        });
    if (found == snapshot.deviceInfo.cend()) {
        return notFoundReply(this, snapshot, subjectKind, subjectId);
    }
    *info = *found;
    return reply;
}

adrenalin::contracts::hardware1::Reply SessionService::getHardwareCapabilityGraph(
    const QString &subjectKind, const QString &subjectId,
    QList<adrenalin::contracts::hardware1::Capability> *capabilities) const
{
    using namespace adrenalin::contracts::hardware1;
    const Hardware1Snapshot snapshot = hardware1Snapshot_;
    if (capabilities == nullptr) {
        return invalidHardwareArgument(this, snapshot);
    }
    *capabilities = {};
    if (!validSelector(subjectKind, subjectId)) {
        return invalidHardwareArgument(this, snapshot);
    }
    Reply reply = hardwareReply(this, snapshot, subjectKind, subjectId);
    if (!reply.snapshotValid) {
        return reply;
    }
    const bool exists = subjectKind == QLatin1String("PLATFORM")
        ? true : std::any_of(snapshot.devices.cbegin(), snapshot.devices.cend(),
            [&](const Device &device) {
                return device.subjectKind == subjectKind && device.subjectId == subjectId;
            });
    if (!exists) {
        return notFoundReply(this, snapshot, subjectKind, subjectId);
    }
    for (const Capability &capability : snapshot.capabilities) {
        if (capability.subjectKind == subjectKind && capability.subjectId == subjectId) {
            capabilities->append(capability);
        }
    }
    return reply;
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
