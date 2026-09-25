#include "display1_mock.h"

#include "hardware1_registry.h"

#include <QSet>
#include <QRegularExpression>

#include <algorithm>

namespace adrenalin::contracts::display1 {
namespace {

using hardware1::Reply;

constexpr auto kDisplayId = "display-test-0";
constexpr auto kServiceUuid = "123e4567-e89b-12d3-a456-426614174000";

bool validOperationId(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return expression.match(value).hasMatch();
}

Reply makeSnapshot(const QString &subjectKind, const QString &subjectId,
                   quint64 inventoryGeneration = 5, quint64 capabilityGeneration = 9)
{
    Reply reply;
    reply.code = QStringLiteral("OK");
    reply.humanMessageKey = QStringLiteral("display.read.ok");
    reply.subjectKind = subjectKind;
    reply.subjectId = subjectId;
    reply.snapshotValid = true;
    reply.serviceInstanceUuid = QString::fromLatin1(kServiceUuid);
    reply.serviceGeneration = 2;
    reply.inventoryGeneration = inventoryGeneration;
    reply.capabilityGeneration = capabilityGeneration;
    return reply;
}

Reply failedSnapshot(const QString &code, const QString &key,
                     const QString &subjectKind, const QString &subjectId,
                     quint64 inventoryGeneration = 5, quint64 capabilityGeneration = 9)
{
    Reply reply = makeSnapshot(subjectKind, subjectId, inventoryGeneration,
                               capabilityGeneration);
    reply.code = code;
    reply.humanMessageKey = key;
    reply.diagnosticMessage = QStringLiteral("Display1 test fixture rejected the request");
    return reply;
}

Reply notReadySnapshot(const QString &code, const QString &subjectKind,
                       const QString &subjectId)
{
    Reply reply;
    reply.code = code;
    reply.humanMessageKey = code == QLatin1String("BUSY")
        ? QStringLiteral("service.recovering")
        : QStringLiteral("display.backendUnavailable");
    reply.diagnosticMessage = QStringLiteral("Display1 fixture has no complete snapshot");
    reply.retryable = code == QLatin1String("BUSY");
    reply.subjectKind = subjectKind;
    reply.subjectId = subjectId;
    reply.snapshotValid = false;
    reply.serviceInstanceUuid = QString::fromLatin1(kServiceUuid);
    reply.serviceGeneration = 2;
    return reply;
}

bool sameChanges(const QList<ControlChange> &left, const QList<ControlChange> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (qsizetype index = 0; index < left.size(); ++index) {
        const ControlChange &a = left.at(index);
        const ControlChange &b = right.at(index);
        if (a.capabilityId != b.capabilityId || a.value.kind != b.value.kind
            || a.value.booleanValue != b.value.booleanValue
            || a.value.signedValue != b.value.signedValue
            || a.value.unsignedValue != b.value.unsignedValue
            || a.value.realValue != b.value.realValue || a.value.enumValue != b.value.enumValue) {
            return false;
        }
    }
    return true;
}

bool outputRisking(const QList<ControlChange> &changes)
{
    return std::any_of(changes.cbegin(), changes.cend(), [](const ControlChange &change) {
        return change.capabilityId == QLatin1String("display.custom_resolution");
    });
}

hardware1::Value realValue(double value)
{
    hardware1::Value result;
    result.kind = QStringLiteral("REAL");
    result.realValue = value;
    return result;
}

QStringList validateChanges(const QList<ControlChange> &changes)
{
    QStringList failures;
    if (changes.isEmpty()) {
        failures.append(QStringLiteral("display.validation.noChanges"));
        return failures;
    }
    QSet<QString> seen;
    for (const ControlChange &change : changes) {
        QString error;
        if (!change.isValid(&error)) {
            failures.append(QStringLiteral("display.validation.invalidChange"));
            continue;
        }
        if (!hardware1::capabilityAppliesToV1(change.capabilityId,
                                               QStringLiteral("DISPLAY"))) {
            failures.append(QStringLiteral("display.validation.unknownCapability"));
        }
        if (seen.contains(change.capabilityId)) {
            failures.append(QStringLiteral("display.validation.duplicateCapability"));
        }
        seen.insert(change.capabilityId);
        if (change.capabilityId != QLatin1String("display.brightness")) {
            failures.append(QStringLiteral("display.validation.providerUnavailable"));
        }
        if (change.capabilityId == QLatin1String("display.brightness")
            && (change.value.kind != QLatin1String("REAL")
                || change.value.realValue < 0.0 || change.value.realValue > 100.0)) {
            failures.append(QStringLiteral("display.validation.valueOutOfRange"));
        }
    }
    return failures;
}

ValidationReply validationResult(const QString &code, const QString &operationId,
                                 const QString &subjectId, const QString &safetyClass,
                                 const QStringList &messageKeys,
                                 quint64 inventoryGeneration = 5,
                                 quint64 capabilityGeneration = 9,
                                 bool snapshotValid = true)
{
    ValidationReply reply;
    reply.code = code;
    reply.operationId = operationId;
    reply.humanMessageKey = code == QLatin1String("OK")
        ? QStringLiteral("display.validation.ok")
        : QStringLiteral("display.validation.failed");
    reply.diagnosticMessage = code == QLatin1String("OK")
        ? QString() : QStringLiteral("Display1 test fixture rejected validation");
    reply.retryable = code == QLatin1String("BUSY");
    reply.subjectId = subjectId;
    reply.serviceInstanceUuid = QString::fromLatin1(kServiceUuid);
    reply.serviceGeneration = 2;
    reply.snapshotValid = snapshotValid;
    reply.inventoryGeneration = inventoryGeneration;
    reply.capabilityGeneration = capabilityGeneration;
    reply.valid = code == QLatin1String("OK");
    reply.safetyClass = safetyClass;
    reply.validationMessageKeys = messageKeys;
    return reply;
}

ApplyReply applyResult(const QString &code, const QString &operationId,
                       const QString &subjectId, const QString &route,
                       bool verified, quint64 revision,
                       quint64 eventSequence,
                       quint64 inventoryGeneration = 5,
                       quint64 capabilityGeneration = 9,
                       bool snapshotValid = true)
{
    ApplyReply reply;
    reply.code = code;
    reply.operationId = operationId;
    reply.humanMessageKey = code == QLatin1String("OK")
        ? QStringLiteral("display.apply.ok") : QStringLiteral("display.apply.failed");
    reply.diagnosticMessage = code == QLatin1String("OK")
        ? QString() : QStringLiteral("Display1 test fixture rejected the request");
    reply.retryable = code == QLatin1String("BUSY");
    reply.subjectId = subjectId;
    reply.revision = revision;
    reply.serviceInstanceUuid = QString::fromLatin1(kServiceUuid);
    reply.serviceGeneration = 2;
    reply.eventSequence = eventSequence;
    reply.snapshotValid = snapshotValid;
    reply.inventoryGeneration = inventoryGeneration;
    reply.capabilityGeneration = capabilityGeneration;
    reply.safetyRouteIntent = route;
    reply.effectiveStateVerified = verified;
    return reply;
}

} // namespace

Mock::Mock()
{
    Capability brightness;
    brightness.subjectKind = QStringLiteral("DISPLAY");
    brightness.subjectId = QString::fromLatin1(kDisplayId);
    brightness.capabilityId = QStringLiteral("display.brightness");
    brightness.supportState = QStringLiteral("SUPPORTED");
    brightness.providerId = QStringLiteral("provider.test.fixture");
    brightness.evidenceCode = QStringLiteral("evidence.test.observed_range");
    brightness.unit = QStringLiteral("percent");
    brightness.configuredValue = realValue(50.0);
    brightness.effectiveValue = realValue(50.0);
    brightness.minimum = realValue(0.0);
    brightness.maximum = realValue(100.0);
    brightness.step = realValue(1.0);
    capabilities_.append(brightness);

    Capability customResolution;
    customResolution.subjectKind = QStringLiteral("DISPLAY");
    customResolution.subjectId = QString::fromLatin1(kDisplayId);
    customResolution.capabilityId = QStringLiteral("display.custom_resolution");
    customResolution.supportState = QStringLiteral("UNKNOWN");
    capabilities_.append(customResolution);
}

QString Mock::testDisplaySubjectId()
{
    return QString::fromLatin1(kDisplayId);
}

ListReply Mock::listDisplays() const
{
    ListReply reply;
    if (readinessState_ == ReadinessState::Ready) {
        reply.snapshot = makeSnapshot(QStringLiteral("PLATFORM"), QStringLiteral("platform"),
                                      inventoryGeneration_, capabilityGeneration_);
        reply.displays.append({QStringLiteral("DISPLAY"), QString::fromLatin1(kDisplayId),
                               QStringLiteral("test.display.edid"), QStringLiteral("Test Display")});
    } else {
        const QString code = readinessState_ == ReadinessState::Recovering
            ? QStringLiteral("BUSY") : QStringLiteral("BACKEND_UNAVAILABLE");
        reply.snapshot = notReadySnapshot(code, QStringLiteral("PLATFORM"),
                                          QStringLiteral("platform"));
    }
    return reply;
}

StateReply Mock::getDisplayState(const QString &subjectId) const
{
    StateReply reply;
    if (readinessState_ != ReadinessState::Ready) {
        const QString code = readinessState_ == ReadinessState::Recovering
            ? QStringLiteral("BUSY") : QStringLiteral("BACKEND_UNAVAILABLE");
        reply.snapshot = notReadySnapshot(code, QStringLiteral("DISPLAY"), subjectId);
        return reply;
    }
    if (subjectId != QLatin1String(kDisplayId)) {
        reply.snapshot = failedSnapshot(QStringLiteral("NOT_FOUND"),
                                        QStringLiteral("hardware.subject.notFound"),
                                        QStringLiteral("DISPLAY"), subjectId,
                                        inventoryGeneration_, capabilityGeneration_);
        return reply;
    }
    reply.snapshot = makeSnapshot(QStringLiteral("DISPLAY"), subjectId,
                                  inventoryGeneration_, capabilityGeneration_);
    reply.display = {QStringLiteral("DISPLAY"), QString::fromLatin1(kDisplayId),
                     QStringLiteral("test.display.edid"), QStringLiteral("Test Display")};
    reply.capabilities = capabilities_;
    return reply;
}

ValidationReply Mock::validateDisplay(const QString &operationId, const QString &subjectId,
                                      quint64 expectedInventoryGeneration,
                                      quint64 expectedCapabilityGeneration,
                                      const QList<ControlChange> &changes) const
{
    if (!validOperationId(operationId) || subjectId.isEmpty()) {
        return validationResult(QStringLiteral("INVALID_ARGUMENT"),
                                validOperationId(operationId) ? operationId : QString(), subjectId,
                                QStringLiteral("UNKNOWN"),
                                {QStringLiteral("display.validation.invalidRequest")},
                                inventoryGeneration_, capabilityGeneration_);
    }
    if (readinessState_ != ReadinessState::Ready) {
        const QString code = readinessState_ == ReadinessState::Recovering
            ? QStringLiteral("BUSY") : QStringLiteral("BACKEND_UNAVAILABLE");
        return validationResult(code, operationId, subjectId, QStringLiteral("UNKNOWN"),
                                {QStringLiteral("display.validation.serviceNotReady")},
                                0, 0, false);
    }
    if (subjectId != QLatin1String(kDisplayId)) {
        return validationResult(QStringLiteral("NOT_FOUND"), operationId, subjectId,
                                QStringLiteral("UNKNOWN"),
                                {QStringLiteral("display.validation.subjectUnavailable")},
                                inventoryGeneration_, capabilityGeneration_);
    }
    if (expectedInventoryGeneration != inventoryGeneration_) {
        return validationResult(QStringLiteral("CONFLICT"), operationId, subjectId,
                                QStringLiteral("UNKNOWN"),
                                {QStringLiteral("display.validation.staleInventory")},
                                inventoryGeneration_, capabilityGeneration_);
    }
    if (expectedCapabilityGeneration != capabilityGeneration_) {
        return validationResult(QStringLiteral("STALE_CAPABILITY"), operationId, subjectId,
                                QStringLiteral("UNKNOWN"),
                                {QStringLiteral("display.validation.staleCapability")},
                                inventoryGeneration_, capabilityGeneration_);
    }
    const QStringList failures = validateChanges(changes);
    if (!failures.isEmpty()) {
        return validationResult(QStringLiteral("UNSUPPORTED"), operationId, subjectId,
                                outputRisking(changes) ? QStringLiteral("OUTPUT_RISKING")
                                                       : QStringLiteral("UNKNOWN"),
                                failures, inventoryGeneration_, capabilityGeneration_);
    }
    return validationResult(QStringLiteral("OK"), operationId, subjectId,
                            QStringLiteral("SAFE"), {}, inventoryGeneration_,
                            capabilityGeneration_);
}

ApplyReply Mock::applyDisplay(const QString &operationId, const QString &subjectId,
                              quint64 expectedInventoryGeneration,
                              quint64 expectedCapabilityGeneration,
                              const QList<ControlChange> &changes)
{
    const bool risky = outputRisking(changes);
    const QString route = risky ? QStringLiteral("DISPLAY_GUARD_INTENT")
                                : QStringLiteral("SAFE_DIRECT_INTENT");
    if (!validOperationId(operationId) || subjectId.isEmpty()) {
        return applyResult(QStringLiteral("INVALID_ARGUMENT"),
                           validOperationId(operationId) ? operationId : QString(), subjectId,
                           QStringLiteral("NONE"), false, revision_, eventSequence_,
                           inventoryGeneration_, capabilityGeneration_);
    }
    if (readinessState_ != ReadinessState::Ready) {
        const QString code = readinessState_ == ReadinessState::Recovering
            ? QStringLiteral("BUSY") : QStringLiteral("BACKEND_UNAVAILABLE");
        return applyResult(code, operationId, subjectId, QStringLiteral("NONE"), false,
                           revision_, 0, 0, 0, false);
    }
    const auto previous = operations_.constFind(operationId);
    if (previous != operations_.cend()) {
        if (previous->subjectId == subjectId
            && previous->inventoryGeneration == expectedInventoryGeneration
            && previous->capabilityGeneration == expectedCapabilityGeneration
            && sameChanges(previous->changes, changes)) {
            return previous->reply;
        }
        return applyResult(QStringLiteral("CONFLICT"), operationId, subjectId,
                           route, false, revision_, eventSequence_, inventoryGeneration_,
                           capabilityGeneration_);
    }
    const ValidationReply validation = validateDisplay(operationId, subjectId,
                                                       expectedInventoryGeneration,
                                                       expectedCapabilityGeneration, changes);
    if (!validation.valid) {
        return applyResult(validation.code, validation.operationId, validation.subjectId,
                           route, false, revision_, eventSequence_, inventoryGeneration_,
                           capabilityGeneration_);
    }

    ++revision_;
    ++eventSequence_;
    for (const ControlChange &change : changes) {
        for (Capability &capability : capabilities_) {
            if (capability.capabilityId == change.capabilityId) {
                capability.configuredValue = change.value;
                capability.effectiveValue = change.value;
            }
        }
    }
    ApplyReply reply = applyResult(QStringLiteral("OK"), operationId, subjectId, route,
                                   true, revision_, eventSequence_, inventoryGeneration_,
                                   capabilityGeneration_ + 1);
    Operation operation;
    operation.subjectId = subjectId;
    operation.inventoryGeneration = expectedInventoryGeneration;
    operation.capabilityGeneration = expectedCapabilityGeneration;
    operation.changes = changes;
    operation.reply = reply;
    operations_.insert(operationId, operation);
    ++capabilityGeneration_;
    return reply;
}

} // namespace adrenalin::contracts::display1
