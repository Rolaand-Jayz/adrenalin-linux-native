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

QString SessionService::hardwareSubjectKey(const QString &kind, const QString &id)
{
    return kind + QChar(u'\0') + id;
}

void SessionService::requestHardwareInventoryRefresh()
{
    if (state_ == State::Starting || state_ == State::Recovering) {
        hardwareRefreshPending_ = true;
        return;
    }
    if (hardwareInventoryThread_ != nullptr) {
        hardwareRefreshPending_ = true;
        return;
    }
    startHardwareInventoryRefresh();
}

void SessionService::setHardwareObserverUnavailable()
{
    if (!hardwareObserverAvailable_) {
        return;
    }
    if (state_ == State::Ready && hardware1Snapshot_.success
        && eventSequence_ == std::numeric_limits<quint64>::max()) {
        failEventSequenceExhausted();
        return;
    }
    hardwareObserverAvailable_ = false;
    hardwareObserverRecoveryPending_ = false;
    if (state_ == State::Ready && hardware1Snapshot_.success) {
        if (nextEventSequence(QStringLiteral("PLATFORM"), QStringLiteral("platform")) == 0) {
            failEventSequenceExhausted();
            return;
        }
        emit InventoryChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                              QStringLiteral("PLATFORM"), QStringLiteral("platform"),
                              hardware1Snapshot_.inventoryGeneration,
                              hardware1Snapshot_.capabilityGeneration);
        emit eventPublished();
    }
}

void SessionService::requestHardwareObserverRecovery()
{
    hardwareObserverRecoveryPending_ = true;
    requestHardwareInventoryRefresh();
}

void SessionService::startHardwareInventoryRefresh()
{
    if (hardwareInventoryThread_ != nullptr) {
        hardwareRefreshPending_ = true;
        return;
    }
    hardwareInventoryThread_ = QThread::create([this] {
        const auto snapshot = hardware1Provider_.refresh();
        QMetaObject::invokeMethod(this, [this, snapshot] {
            publishHardwareInitialization(snapshot);
        }, Qt::QueuedConnection);
    });
    hardwareInventoryThread_->start();
}

bool SessionService::publishHardwareInitialization(
    adrenalin::hardware::Hardware1Snapshot currentHardware)
{
    if (hardwareInventoryThread_ != nullptr) {
        hardwareInventoryThread_->wait();
        delete hardwareInventoryThread_;
        hardwareInventoryThread_ = nullptr;
    }
    if (!currentHardware.success) {
        if (!hardwareInitializationComplete_) {
            hardware1Snapshot_ = std::move(currentHardware);
            hardwareRefreshAvailable_ = false;
            setState(State::Failed, QStringLiteral("Hardware inventory evidence is unavailable"));
            logEvent(QStringLiteral("hardware_initialization_failed"), QStringLiteral("error"));
        } else {
            const bool wasAvailable = hardwareRefreshAvailable_;
            if (wasAvailable && eventSequence_ == std::numeric_limits<quint64>::max()) {
                failEventSequenceExhausted();
                return false;
            }
            hardwareRefreshAvailable_ = false;
            lastInitializationError_ = currentHardware.error.isEmpty()
                ? QStringLiteral("Hardware inventory evidence is unavailable") : currentHardware.error;
            emit initializationStateChanged();
            if (wasAvailable) {
                if (nextEventSequence(QStringLiteral("PLATFORM"), QStringLiteral("platform")) == 0) {
                    failEventSequenceExhausted();
                    return false;
                }
                emit InventoryChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                      QStringLiteral("PLATFORM"), QStringLiteral("platform"),
                                      hardware1Snapshot_.inventoryGeneration,
                                      hardware1Snapshot_.capabilityGeneration);
                emit eventPublished();
            }
            logEvent(QStringLiteral("hardware_refresh_failed"), QStringLiteral("error"),
                     lastInitializationError_);
        }
        if (hardwareRefreshPending_) {
            hardwareRefreshPending_ = false;
            startHardwareInventoryRefresh();
        }
        return false;
    }

    using Subject = QPair<QString, QString>;
    QList<Subject> inventorySubjects;
    QList<Subject> capabilitySubjects;
    auto appendUnique = [](QList<Subject> &subjects, const Subject &candidate) {
        if (!subjects.contains(candidate)) {
            subjects.append(candidate);
        }
    };
    const auto previousHardware = hardware1Snapshot_;
    const bool wasRefreshAvailable = hardwareRefreshAvailable_;
    const bool observerRecoveryPending = hardwareObserverRecoveryPending_;
    const bool inventoryChanged = !previousHardware.success || !wasRefreshAvailable
        || observerRecoveryPending
        || previousHardware.inventoryGeneration != currentHardware.inventoryGeneration;
    const bool capabilitiesChanged = !previousHardware.success || !wasRefreshAvailable
        || observerRecoveryPending
        || previousHardware.capabilityGeneration != currentHardware.capabilityGeneration;
    auto collectDevices = [&](const auto &devices) {
        for (const auto &device : devices) {
            appendUnique(inventorySubjects, {device.subjectKind, device.subjectId});
        }
    };
    auto collectCapabilities = [&](const auto &capabilities) {
        for (const auto &capability : capabilities) {
            appendUnique(capabilitySubjects, {capability.subjectKind, capability.subjectId});
        }
    };
    if (inventoryChanged) {
        collectDevices(previousHardware.devices);
        collectDevices(currentHardware.devices);
        if (inventorySubjects.isEmpty()) {
            inventorySubjects.append({QStringLiteral("PLATFORM"), QStringLiteral("platform")});
        }
    }
    if (capabilitiesChanged) {
        collectCapabilities(previousHardware.capabilities);
        collectCapabilities(currentHardware.capabilities);
        if (capabilitySubjects.isEmpty()) {
            capabilitySubjects.append({QStringLiteral("PLATFORM"), QStringLiteral("platform")});
        }
    }
    const quint64 hardwareEvents = static_cast<quint64>(inventorySubjects.size())
        + static_cast<quint64>(capabilitySubjects.size());
    const quint64 readinessEvents = state_ == State::Ready ? 0 : 1;
    if (hardwareEvents > std::numeric_limits<quint64>::max() - readinessEvents
        || eventSequence_ > std::numeric_limits<quint64>::max() - hardwareEvents - readinessEvents) {
        failEventSequenceExhausted();
        return false;
    }

    for (const auto &device : previousHardware.devices) {
        const Subject subject{device.subjectKind, device.subjectId};
        const bool remains = std::any_of(currentHardware.devices.cbegin(),
                                         currentHardware.devices.cend(),
            [&](const auto &current) {
                return current.subjectKind == subject.first && current.subjectId == subject.second;
            });
        if (!remains) {
            disconnectedHardwareSubjects_.insert(hardwareSubjectKey(subject.first, subject.second));
        }
    }
    for (const auto &device : currentHardware.devices) {
        disconnectedHardwareSubjects_.remove(hardwareSubjectKey(device.subjectKind, device.subjectId));
    }
    hardware1Snapshot_ = std::move(currentHardware);
    hardwareRefreshAvailable_ = true;
    if (observerRecoveryPending && !hardwareRefreshPending_) {
        hardwareObserverAvailable_ = true;
        hardwareObserverRecoveryPending_ = false;
    }
    const auto &snapshot = hardware1Snapshot_;
    if (!hardwareInitializationComplete_) {
        if (!setState(State::Ready)) {
            return false;
        }
        hardwareInitializationComplete_ = true;
    } else if (state_ != State::Ready) {
        if (!setState(State::Ready)) {
            return false;
        }
    }

    for (const auto &subject : inventorySubjects) {
        if (nextEventSequence(subject.first, subject.second) == 0) {
            failEventSequenceExhausted();
            return false;
        }
        emit InventoryChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                              subject.first, subject.second, snapshot.inventoryGeneration,
                              snapshot.capabilityGeneration);
        emit eventPublished();
    }
    for (const auto &subject : capabilitySubjects) {
        if (nextEventSequence(subject.first, subject.second) == 0) {
            failEventSequenceExhausted();
            return false;
        }
        emit CapabilityGraphChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                    subject.first, subject.second, snapshot.inventoryGeneration,
                                    snapshot.capabilityGeneration);
        emit eventPublished();
    }
    logEvent(QStringLiteral("service_ready"), QStringLiteral("info"));
    if (hardwareRefreshPending_) {
        hardwareRefreshPending_ = false;
        startHardwareInventoryRefresh();
    }
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
bool SessionService::hardwareRefreshAvailable() const { return hardwareRefreshAvailable_; }
bool SessionService::hardwareObserverAvailable() const { return hardwareObserverAvailable_; }
bool SessionService::hardwareSubjectDisconnected(const QString &kind, const QString &id) const
{
    return disconnectedHardwareSubjects_.contains(hardwareSubjectKey(kind, id));
}

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

bool SessionService::reconcileHardwareSnapshotForTesting(
    const adrenalin::hardware::Hardware1Snapshot &snapshot)
{
    return publishHardwareInitialization(snapshot);
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
    // Read methods execute on the serialized service thread, so this cursor
    // identifies the event history covered by the copied snapshot.
    reply.eventSequence = service->eventSequence();
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
    if (state == QLatin1String("FAILED") || !snapshot.success || !service->hardwareRefreshAvailable()
        || !service->hardwareObserverAvailable()) {
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
    const bool disconnected = service->hardwareSubjectDisconnected(kind, id);
    reply.code = disconnected ? QStringLiteral("DEVICE_DISCONNECTED") : QStringLiteral("NOT_FOUND");
    reply.humanMessageKey = disconnected ? QStringLiteral("hardware.subject.disconnected")
                                         : QStringLiteral("hardware.subject.notFound");
    reply.diagnosticMessage = disconnected
        ? QStringLiteral("The requested subject was removed from the current service inventory")
        : QStringLiteral("The requested subject is absent from the current inventory snapshot");
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

adrenalin::contracts::notifications1::ListReply SessionService::listNotifications() const
{
    using namespace adrenalin::contracts::notifications1;
    ListReply reply;
    if (state_ != State::Ready) {
        reply.code = QStringLiteral("BACKEND_UNAVAILABLE");
        return reply;
    }
    QString error;
    quint64 revision = 0;
    const auto notifications = database_->readNotifications(&revision, &error);
    if (!notifications.has_value()) {
        Q_UNUSED(error);
        reply.code = QStringLiteral("IO_ERROR");
        return reply;
    }
    reply.code = QStringLiteral("OK");
    reply.serviceInstanceUuid = serviceInstanceUuid();
    reply.serviceGeneration = serviceGeneration();
    reply.eventSequence = eventSequence_;
    reply.revision = revision;
    reply.notifications = *notifications;
    return reply;
}

adrenalin::contracts::notifications1::MarkReadReply SessionService::markRead(
    const QString &notificationId, const QString &operationId, quint64 expectedRevision)
{
    using namespace adrenalin::contracts;
    using namespace adrenalin::contracts::notifications1;
    MarkReadReply reply;
    reply.mutation.operationId = operationId;
    reply.mutation.provider = QStringLiteral("session-notifications");
    reply.mutation.subjectId = QStringLiteral("platform");
    reply.mutation.revision = expectedRevision;
    if (state_ != State::Ready) {
        reply.mutation.code = OperationResultCode::BackendUnavailable;
        reply.mutation.humanMessageKey = QStringLiteral("service.recovering");
        reply.mutation.diagnosticMessage = QStringLiteral("Session service is not READY");
        reply.mutation.retryable = true;
        return reply;
    }
    reply.serviceInstanceUuid = serviceInstanceUuid();
    reply.serviceGeneration = serviceGeneration();
    reply.eventSequence = eventSequence_;
    bool stale = false;
    bool conflict = false;
    bool notFound = false;
    bool replayed = false;
    bool changed = false;
    quint64 revision = expectedRevision;
    QString error;
    if (!database_->markNotificationRead(notificationId, operationId, expectedRevision,
                                          eventSequence_ != std::numeric_limits<quint64>::max(), &revision,
                                          &changed, &stale, &conflict, &notFound, &replayed, &error)) {
        reply.mutation.revision = revision;
        if (error == QStringLiteral("Event sequence is exhausted")) {
            failEventSequenceExhausted();
            reply.mutation.code = OperationResultCode::InternalError;
            reply.mutation.humanMessageKey = QStringLiteral("service.event_sequence_exhausted");
            reply.mutation.diagnosticMessage = error;
            return reply;
        }
        if (stale) {
            reply.mutation.code = OperationResultCode::StaleRevision;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.operation.stale_revision");
        } else if (conflict) {
            reply.mutation.code = OperationResultCode::Conflict;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.operation.conflict");
        } else if (notFound) {
            reply.mutation.code = OperationResultCode::NotFound;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.item.not_found");
        } else if (error.startsWith(QStringLiteral("Notification revision is exhausted"))) {
            reply.mutation.code = OperationResultCode::InternalError;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.counter.exhausted");
        } else if (error.startsWith(QStringLiteral("Notification ID"))
                   || error.startsWith(QStringLiteral("Notification revision"))) {
            reply.mutation.code = OperationResultCode::InvalidArgument;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.operation.invalid_argument");
        } else {
            reply.mutation.code = OperationResultCode::IoError;
            reply.mutation.humanMessageKey = QStringLiteral("notifications.operation.storage_failed");
        }
        reply.mutation.diagnosticMessage = error;
        return reply;
    }
    if (changed && !replayed) {
        nextEventSequence(QStringLiteral("NOTIFICATION"), QStringLiteral("platform"));
        emit NotificationsChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                  eventSubjectKind_, eventSubjectId_, revision);
        emit eventPublished();
    }
    reply.mutation.code = OperationResultCode::Ok;
    reply.mutation.humanMessageKey = changed
        ? QStringLiteral("notifications.item.marked_read")
        : QStringLiteral("notifications.item.already_read");
    reply.mutation.revision = revision;
    reply.changed = changed && !replayed;
    reply.serviceInstanceUuid = serviceInstanceUuid();
    reply.serviceGeneration = serviceGeneration();
    reply.eventSequence = eventSequence_;
    return reply;
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
    if (eventSequence_ > std::numeric_limits<quint64>::max() - 2) {
        failEventSequenceExhausted();
        result.code = OperationResultCode::InternalError;
        result.humanMessageKey = QStringLiteral("service.event_sequence_exhausted");
        result.diagnosticMessage = QStringLiteral("Two sequenced events are required for an atomic consent update");
        result.retryable = false;
        return writeResult;
    }
    adrenalin::contracts::notifications1::Notification notification;
    notification.notificationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    notification.category = QStringLiteral("SETTING_APPLIED");
    notification.createdAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    notification.titleMessageKey = QStringLiteral("settings.telemetry_consent.applied");
    notification.bodyMessageKey = enabled
        ? QStringLiteral("settings.telemetry_consent.enabled")
        : QStringLiteral("settings.telemetry_consent.disabled");
    notification.toastEligible = false;
    quint64 notificationRevision = 0;
    bool notificationInserted = false;
    bool stale = false;
    bool conflict = false;
    bool operationReplayed = false;
    QString error;
    if (!database_->updateProductTelemetryConsent(operationId, enabled, expectedRevision, notification,
                                                   &result.revision, &notificationRevision, &stale, &conflict,
                                                   &operationReplayed, &notificationInserted, &error)) {
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
        if (notificationInserted) {
            nextEventSequence(QStringLiteral("NOTIFICATION"), QStringLiteral("platform"));
            emit NotificationsChanged(serviceInstanceUuid(), serviceGeneration(), eventSequence_,
                                      eventSubjectKind_, eventSubjectId_, notificationRevision);
            emit eventPublished();
        }
    }
    result.code = OperationResultCode::Ok;
    result.humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
    return writeResult;
}
