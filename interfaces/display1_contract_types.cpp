#include "display1_contract_types.h"

#include "operation_result.h"
#include "hardware1_registry.h"

#include <QRegularExpression>
namespace adrenalin::contracts::display1 {
namespace {

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool validUuid(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
                       "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return expression.match(value).hasMatch();
}

bool validResultFields(const QString &code, const QString &operationId,
                       const QString &humanMessageKey, const QString &subjectId,
                       QString *error)
{
    if (!operationResultCodeFromName(code)) {
        return fail(error, QStringLiteral("unknown operation result code"));
    }
    const bool invalidArgument = code == QLatin1String("INVALID_ARGUMENT");
    if ((!operationId.isEmpty() && !validUuid(operationId))
        || (operationId.isEmpty() && !invalidArgument)) {
        return fail(error, QStringLiteral("operation ID is not a canonical UUID"));
    }
    if (humanMessageKey.isEmpty() || (subjectId.isEmpty() && !invalidArgument)) {
        return fail(error, QStringLiteral("result is missing its message key or subject ID"));
    }
    return true;
}

bool validEnvelope(const QString &serviceInstanceUuid, quint64 serviceGeneration,
                   quint64 inventoryGeneration, quint64 capabilityGeneration,
                   bool snapshotValid, const QString &code, bool retryable,
                   QString *error)
{
    if (!validUuid(serviceInstanceUuid) || serviceGeneration == 0) {
        return fail(error, QStringLiteral("display result has an incomplete service identity"));
    }
    if (snapshotValid) {
        if (inventoryGeneration == 0 || capabilityGeneration == 0) {
            return fail(error, QStringLiteral("valid display result has an incomplete generation envelope"));
        }
        return true;
    }
    const bool allowedCode = code == QLatin1String("BUSY")
        || code == QLatin1String("BACKEND_UNAVAILABLE")
        || code == QLatin1String("INVALID_ARGUMENT");
    if (!allowedCode || retryable != (code == QLatin1String("BUSY"))
        || inventoryGeneration != 0 || capabilityGeneration != 0) {
        return fail(error, QStringLiteral("invalid display snapshot has inconsistent readiness fields"));
    }
    return true;
}

} // namespace

bool ControlChange::isValid(QString *error) const
{
    static const QRegularExpression capabilityId(
        QStringLiteral("^display\\.[a-z0-9]+(?:[._-][a-z0-9]+)*$"));
    if (!capabilityId.match(this->capabilityId).hasMatch()) {
        return fail(error, QStringLiteral("control change has an invalid display capability ID"));
    }
    if (!hardware1::capabilityAppliesToV1(this->capabilityId,
                                         QStringLiteral("DISPLAY"))) {
        return fail(error, QStringLiteral("control change references an unregistered display capability"));
    }
    return value.isValid(error);
}

bool ListReply::isValid(QString *error) const
{
    if (!snapshot.isValid(error)) {
        return false;
    }
    if (snapshot.snapshotValid && snapshot.subjectKind != QLatin1String("PLATFORM")) {
        return fail(error, QStringLiteral("display list must use the platform snapshot subject"));
    }
    for (const Display &display : displays) {
        if (display.subjectKind != QLatin1String("DISPLAY") || display.subjectId.isEmpty()) {
            return fail(error, QStringLiteral("display list contains a non-display subject"));
        }
    }
    if (snapshot.code != QLatin1String("OK") && !displays.isEmpty()) {
        return fail(error, QStringLiteral("failed display list contains records"));
    }
    return true;
}

bool StateReply::isValid(QString *error) const
{
    if (!snapshot.isValid(error)) {
        return false;
    }
    if (!snapshot.snapshotValid) {
        if (!display.subjectId.isEmpty() || !capabilities.isEmpty()) {
            return fail(error, QStringLiteral("invalid display snapshot contains stale payload"));
        }
        return true;
    }
    if (snapshot.code != QLatin1String("OK")) {
        if (!display.subjectId.isEmpty() || !capabilities.isEmpty()) {
            return fail(error, QStringLiteral("failed display read contains stale payload"));
        }
        return true;
    }
    if (display.subjectKind != QLatin1String("DISPLAY")
        || display.subjectId != snapshot.subjectId) {
        return fail(error, QStringLiteral("display state identity does not match its envelope"));
    }
    for (const Capability &capability : capabilities) {
        if (capability.subjectKind != QLatin1String("DISPLAY")
            || capability.subjectId != display.subjectId || !capability.isValid(error)) {
            return fail(error, QStringLiteral("display state contains an invalid capability record"));
        }
    }
    return true;
}

bool ValidationReply::isValid(QString *error) const
{
    if (!validResultFields(code, operationId, humanMessageKey, subjectId, error)
        || !validEnvelope(serviceInstanceUuid, serviceGeneration, inventoryGeneration,
                          capabilityGeneration, snapshotValid, code, retryable, error)) {
        return false;
    }
    if (safetyClass != QLatin1String("SAFE")
        && safetyClass != QLatin1String("OUTPUT_RISKING")
        && safetyClass != QLatin1String("UNKNOWN")) {
        return fail(error, QStringLiteral("unknown display safety classification"));
    }
    if (valid != (code == QLatin1String("OK"))) {
        return fail(error, QStringLiteral("validation success flag disagrees with result code"));
    }
    if (!snapshotValid && valid) {
        return fail(error, QStringLiteral("validation cannot succeed without a valid capability snapshot"));
    }
    if (valid && safetyClass == QLatin1String("UNKNOWN")) {
        return fail(error, QStringLiteral("valid display changes require a known safety classification"));
    }
    return true;
}

bool ApplyReply::isValid(QString *error) const
{
    if (!validResultFields(code, operationId, humanMessageKey, subjectId, error)
        || !validEnvelope(serviceInstanceUuid, serviceGeneration, inventoryGeneration,
                          capabilityGeneration, snapshotValid, code, retryable, error)) {
        return false;
    }
    if (safetyRouteIntent != QLatin1String("SAFE_DIRECT_INTENT")
        && safetyRouteIntent != QLatin1String("DISPLAY_GUARD_INTENT")
        && safetyRouteIntent != QLatin1String("NONE")) {
        return fail(error, QStringLiteral("unknown display safety route intent"));
    }
    if (effectiveStateVerified && code != QLatin1String("OK")) {
        return fail(error, QStringLiteral("failed display apply claims verified success"));
    }
    if (code == QLatin1String("OK")) {
        if (!snapshotValid || revision == 0 || eventSequence == 0 || !effectiveStateVerified
            || (safetyRouteIntent != QLatin1String("SAFE_DIRECT_INTENT")
                && safetyRouteIntent != QLatin1String("DISPLAY_GUARD_INTENT"))) {
            return fail(error, QStringLiteral("successful display apply lacks verified state, revision, event, or a concrete safety intent"));
        }
    }
    return true;
}

void registerMetaTypes()
{
    hardware1::registerMetaTypes();
    qDBusRegisterMetaType<ControlChange>();
    qDBusRegisterMetaType<QList<ControlChange>>();
    qDBusRegisterMetaType<ListReply>();
    qDBusRegisterMetaType<StateReply>();
    qDBusRegisterMetaType<ValidationReply>();
    qDBusRegisterMetaType<ApplyReply>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const ControlChange &change)
{
    argument.beginStructure();
    argument << change.capabilityId << change.value;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ControlChange &change)
{
    argument.beginStructure();
    argument >> change.capabilityId >> change.value;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ListReply &reply)
{
    argument.beginStructure();
    argument << reply.snapshot << reply.displays;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ListReply &reply)
{
    argument.beginStructure();
    argument >> reply.snapshot >> reply.displays;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const StateReply &reply)
{
    argument.beginStructure();
    argument << reply.snapshot << reply.display << reply.capabilities;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, StateReply &reply)
{
    argument.beginStructure();
    argument >> reply.snapshot >> reply.display >> reply.capabilities;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ValidationReply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.operationId << reply.humanMessageKey
             << reply.diagnosticMessage << reply.retryable << reply.provider << reply.subjectId
             << reply.serviceInstanceUuid << reply.serviceGeneration << reply.snapshotValid
             << reply.inventoryGeneration << reply.capabilityGeneration << reply.valid
             << reply.safetyClass
             << reply.validationMessageKeys;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ValidationReply &reply)
{
    argument.beginStructure();
    argument >> reply.code >> reply.operationId >> reply.humanMessageKey
             >> reply.diagnosticMessage >> reply.retryable >> reply.provider >> reply.subjectId
             >> reply.serviceInstanceUuid >> reply.serviceGeneration >> reply.snapshotValid
             >> reply.inventoryGeneration >> reply.capabilityGeneration >> reply.valid
             >> reply.safetyClass
             >> reply.validationMessageKeys;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ApplyReply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.operationId << reply.humanMessageKey
             << reply.diagnosticMessage << reply.retryable << reply.provider << reply.subjectId
             << reply.revision << reply.serviceInstanceUuid << reply.serviceGeneration
             << reply.eventSequence
             << reply.snapshotValid << reply.inventoryGeneration << reply.capabilityGeneration
             << reply.safetyRouteIntent
             << reply.effectiveStateVerified;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ApplyReply &reply)
{
    argument.beginStructure();
    argument >> reply.code >> reply.operationId >> reply.humanMessageKey
             >> reply.diagnosticMessage >> reply.retryable >> reply.provider >> reply.subjectId
             >> reply.revision >> reply.serviceInstanceUuid >> reply.serviceGeneration
             >> reply.eventSequence
             >> reply.snapshotValid >> reply.inventoryGeneration >> reply.capabilityGeneration
             >> reply.safetyRouteIntent
             >> reply.effectiveStateVerified;
    argument.endStructure();
    return argument;
}

} // namespace adrenalin::contracts::display1
