#include "hardware1_contract_types.h"

#include "operation_result.h"

#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace adrenalin::contracts::hardware1 {
namespace {

constexpr auto kNone = "NONE";

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool isZeroValue(const Value &value)
{
    return !value.booleanValue && value.signedValue == 0 && value.unsignedValue == 0
        && value.realValue == 0.0 && value.enumValue.isEmpty();
}

bool isNumericKind(const QString &kind)
{
    return kind == QLatin1String("SIGNED_INTEGER")
        || kind == QLatin1String("UNSIGNED_INTEGER") || kind == QLatin1String("REAL");
}

bool isPositive(const Value &value)
{
    if (value.kind == QLatin1String("SIGNED_INTEGER")) {
        return value.signedValue > 0;
    }
    if (value.kind == QLatin1String("UNSIGNED_INTEGER")) {
        return value.unsignedValue > 0;
    }
    return value.kind == QLatin1String("REAL") && value.realValue > 0.0;
}

int compareNumeric(const Value &left, const Value &right)
{
    if (left.kind == QLatin1String("SIGNED_INTEGER")) {
        return left.signedValue < right.signedValue ? -1 : left.signedValue > right.signedValue;
    }
    if (left.kind == QLatin1String("UNSIGNED_INTEGER")) {
        return left.unsignedValue < right.unsignedValue ? -1
            : left.unsignedValue > right.unsignedValue;
    }
    return left.realValue < right.realValue ? -1 : left.realValue > right.realValue;
}

bool isIdentifier(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$"));
    return expression.match(value).hasMatch();
}

bool noValue(const Value &value)
{
    return value.kind == QLatin1String(kNone);
}

bool noValues(const Capability &capability)
{
    return noValue(capability.configuredValue) && noValue(capability.effectiveValue)
        && noValue(capability.minimum) && noValue(capability.maximum)
        && noValue(capability.step) && capability.allowedValues.isEmpty();
}

} // namespace

bool isValidSubjectKind(const QString &kind)
{
    return kind == QLatin1String("CPU_PACKAGE") || kind == QLatin1String("GPU_PCI")
        || kind == QLatin1String("DISPLAY") || kind == QLatin1String("PLATFORM");
}

bool Reply::isValid(QString *error) const
{
    if (!operationResultCodeFromName(code)) {
        return fail(error, QStringLiteral("reply has an unknown result code"));
    }
    if (humanMessageKey.isEmpty()) {
        return fail(error, QStringLiteral("reply has no human message key"));
    }
    if (!isValidSubjectKind(subjectKind)) {
        return fail(error, QStringLiteral("reply has an unknown subject kind"));
    }
    if (subjectId.isEmpty() && subjectKind != QLatin1String("PLATFORM")) {
        return fail(error, QStringLiteral("reply has no stable subject identity"));
    }
    if (subjectKind == QLatin1String("PLATFORM") && subjectId != QLatin1String("platform")) {
        return fail(error, QStringLiteral("platform reply has a non-canonical subject identity"));
    }
    if (snapshotValid) {
        if (serviceInstanceUuid.isEmpty() || serviceGeneration == 0 || inventoryGeneration == 0
            || capabilityGeneration == 0) {
            return fail(error, QStringLiteral("valid snapshot has an incomplete generation envelope"));
        }
    } else {
        const bool allowedCode = code == QLatin1String("BUSY")
            || code == QLatin1String("BACKEND_UNAVAILABLE");
        if (!allowedCode || retryable != (code == QLatin1String("BUSY"))
            || serviceInstanceUuid.isEmpty() || serviceGeneration == 0
            || inventoryGeneration != 0 || capabilityGeneration != 0) {
            return fail(error, QStringLiteral("invalid snapshot has inconsistent readiness fields"));
        }
    }
    return true;
}

bool Value::isValid(QString *error) const
{
    if (kind == QLatin1String(kNone)) {
        return isZeroValue(*this) || fail(error, QStringLiteral("NONE value carries a payload"));
    }
    if (kind == QLatin1String("BOOLEAN")) {
        return signedValue == 0 && unsignedValue == 0 && realValue == 0.0 && enumValue.isEmpty()
            ? true : fail(error, QStringLiteral("BOOLEAN value carries a mismatched payload"));
    }
    if (kind == QLatin1String("SIGNED_INTEGER")) {
        return !booleanValue && unsignedValue == 0 && realValue == 0.0 && enumValue.isEmpty()
            ? true : fail(error, QStringLiteral("SIGNED_INTEGER value carries a mismatched payload"));
    }
    if (kind == QLatin1String("UNSIGNED_INTEGER")) {
        return !booleanValue && signedValue == 0 && realValue == 0.0 && enumValue.isEmpty()
            ? true : fail(error, QStringLiteral("UNSIGNED_INTEGER value carries a mismatched payload"));
    }
    if (kind == QLatin1String("REAL")) {
        if (booleanValue || signedValue != 0 || unsignedValue != 0 || !enumValue.isEmpty()) {
            return fail(error, QStringLiteral("REAL value carries a mismatched payload"));
        }
        return std::isfinite(realValue) || fail(error, QStringLiteral("REAL value is not finite"));
    }
    if (kind == QLatin1String("ENUM")) {
        if (booleanValue || signedValue != 0 || unsignedValue != 0 || realValue != 0.0
            || !isIdentifier(enumValue)) {
            return fail(error, QStringLiteral("ENUM value is invalid or carries a mismatched payload"));
        }
        return true;
    }
    return fail(error, QStringLiteral("value has an unknown discriminant"));
}

bool Capability::isValid(QString *error) const
{
    if (!isValidSubjectKind(subjectKind) || subjectId.isEmpty()) {
        return fail(error, QStringLiteral("capability has no typed stable subject"));
    }
    if (subjectKind == QLatin1String("PLATFORM") && subjectId != QLatin1String("platform")) {
        return fail(error, QStringLiteral("capability has a non-canonical platform subject"));
    }
    if (!isIdentifier(capabilityId)) {
        return fail(error, QStringLiteral("capability ID is not a stable token"));
    }
    const QStringList states{QStringLiteral("SUPPORTED"), QStringLiteral("UNSUPPORTED"),
                             QStringLiteral("UNKNOWN"), QStringLiteral("PROVIDER_UNAVAILABLE")};
    if (!states.contains(supportState)) {
        return fail(error, QStringLiteral("capability has an unknown support state"));
    }
    const QList<const Value *> values{&configuredValue, &effectiveValue, &minimum, &maximum, &step};
    for (const Value *value : values) {
        QString valueError;
        if (!value->isValid(&valueError)) {
            return fail(error, QStringLiteral("capability value is invalid: %1").arg(valueError));
        }
    }
    if (supportState == QLatin1String("SUPPORTED")) {
        if (!isIdentifier(providerId) || !isIdentifier(evidenceCode) || !failureCode.isEmpty()) {
            return fail(error, QStringLiteral("supported capability lacks provider/evidence or has a failure"));
        }
    } else if (supportState == QLatin1String("UNSUPPORTED")) {
        if (!isIdentifier(evidenceCode) || !providerId.isEmpty() || !failureCode.isEmpty()
            || !noValues(*this)) {
            return fail(error, QStringLiteral("unsupported capability carries support values or provider data"));
        }
    } else if (supportState == QLatin1String("UNKNOWN")) {
        if (!providerId.isEmpty() || !failureCode.isEmpty() || !noValues(*this)) {
            return fail(error, QStringLiteral("unknown capability carries unsupported state data"));
        }
    } else if ((!providerId.isEmpty() && !isIdentifier(providerId))
               || failureCode != QLatin1String("BACKEND_UNAVAILABLE")
               || !noValues(*this)) {
        return fail(error, QStringLiteral("unavailable provider capability has inconsistent state"));
    }
    if (!failureCode.isEmpty() && !operationResultCodeFromName(failureCode)) {
        return fail(error, QStringLiteral("capability has an unknown failure code"));
    }
    if (!allowedValues.isEmpty()) {
        QSet<QString> uniqueValues;
        for (const QString &allowedValue : allowedValues) {
            if (!isIdentifier(allowedValue) || uniqueValues.contains(allowedValue)) {
                return fail(error, QStringLiteral("allowed enum values are invalid or duplicated"));
            }
            uniqueValues.insert(allowedValue);
        }
    }
    if (configuredValue.kind == QLatin1String("ENUM")
        && !allowedValues.contains(configuredValue.enumValue)) {
        return fail(error, QStringLiteral("configured enum value is not in the allowed set"));
    }
    if (effectiveValue.kind == QLatin1String("ENUM")
        && !allowedValues.contains(effectiveValue.enumValue)) {
        return fail(error, QStringLiteral("effective enum value is not in the allowed set"));
    }

    const bool hasMinimum = !noValue(minimum);
    const bool hasMaximum = !noValue(maximum);
    const bool hasStep = !noValue(step);
    if (hasMinimum != hasMaximum || (hasStep && !hasMinimum)) {
        return fail(error, QStringLiteral("numeric range is partial"));
    }
    if (hasMinimum) {
        if (!isNumericKind(minimum.kind) || minimum.kind != maximum.kind
            || (hasStep && step.kind != minimum.kind)) {
            return fail(error, QStringLiteral("numeric range value kinds do not match"));
        }
        if (compareNumeric(minimum, maximum) > 0 || (hasStep && !isPositive(step))) {
            return fail(error, QStringLiteral("numeric range bounds or step are invalid"));
        }
    }

    QString valueKind;
    for (const Value *value : {&configuredValue, &effectiveValue}) {
        if (noValue(*value)) {
            continue;
        }
        if (valueKind.isEmpty()) {
            valueKind = value->kind;
        } else if (valueKind != value->kind) {
            return fail(error, QStringLiteral("configured and effective value kinds differ"));
        }
    }
    if (hasMinimum && !valueKind.isEmpty() && valueKind != minimum.kind) {
        return fail(error, QStringLiteral("range and value kinds differ"));
    }
    const QString effectiveKind = valueKind.isEmpty() ? minimum.kind : valueKind;
    if ((isNumericKind(effectiveKind) && !isIdentifier(unit))
        || ((effectiveKind == QLatin1String("BOOLEAN") || effectiveKind == QLatin1String("ENUM"))
            && !unit.isEmpty())) {
        return fail(error, QStringLiteral("unit does not match the capability value kind"));
    }
    if ((effectiveKind == QLatin1String("ENUM")) != !allowedValues.isEmpty()) {
        return fail(error, QStringLiteral("allowed enum values do not match the value kind"));
    }
    if ((effectiveKind == QLatin1String("BOOLEAN") || effectiveKind == QLatin1String("ENUM"))
        && hasMinimum) {
        return fail(error, QStringLiteral("non-numeric value kind carries a numeric range"));
    }
    return true;
}

void registerMetaTypes()
{
    qDBusRegisterMetaType<Reply>();
    qDBusRegisterMetaType<Device>();
    qDBusRegisterMetaType<QList<Device>>();
    qDBusRegisterMetaType<DeviceInfo>();
    qDBusRegisterMetaType<Value>();
    qDBusRegisterMetaType<Capability>();
    qDBusRegisterMetaType<QList<Capability>>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Reply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.humanMessageKey << reply.diagnosticMessage << reply.retryable
             << reply.provider << reply.subjectKind << reply.subjectId << reply.snapshotValid
             << reply.serviceInstanceUuid << reply.serviceGeneration << reply.inventoryGeneration
             << reply.capabilityGeneration;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Reply &reply)
{
    argument.beginStructure();
    argument >> reply.code >> reply.humanMessageKey >> reply.diagnosticMessage >> reply.retryable
             >> reply.provider >> reply.subjectKind >> reply.subjectId >> reply.snapshotValid
             >> reply.serviceInstanceUuid >> reply.serviceGeneration >> reply.inventoryGeneration
             >> reply.capabilityGeneration;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Device &device)
{
    argument.beginStructure();
    argument << device.subjectKind << device.subjectId << device.identityEvidence
             << device.displayName;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Device &device)
{
    argument.beginStructure();
    argument >> device.subjectKind >> device.subjectId >> device.identityEvidence
             >> device.displayName;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const DeviceInfo &info)
{
    argument.beginStructure();
    argument << info.subjectKind << info.subjectId << info.identityEvidence << info.displayName
             << info.manufacturer << info.model << info.driverName << info.driverVersion
             << info.pciAddress << info.connectorIdentity << info.edidIdentityDigest;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DeviceInfo &info)
{
    argument.beginStructure();
    argument >> info.subjectKind >> info.subjectId >> info.identityEvidence >> info.displayName
             >> info.manufacturer >> info.model >> info.driverName >> info.driverVersion
             >> info.pciAddress >> info.connectorIdentity >> info.edidIdentityDigest;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Value &value)
{
    argument.beginStructure();
    argument << value.kind << value.booleanValue << value.signedValue << value.unsignedValue
             << value.realValue << value.enumValue;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Value &value)
{
    argument.beginStructure();
    argument >> value.kind >> value.booleanValue >> value.signedValue >> value.unsignedValue
             >> value.realValue >> value.enumValue;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Capability &capability)
{
    argument.beginStructure();
    argument << capability.subjectKind << capability.subjectId << capability.capabilityId
             << capability.supportState << capability.providerId << capability.evidenceCode
             << capability.failureCode << capability.unit << capability.configuredValue
             << capability.effectiveValue << capability.minimum << capability.maximum
             << capability.step << capability.allowedValues;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Capability &capability)
{
    argument.beginStructure();
    argument >> capability.subjectKind >> capability.subjectId >> capability.capabilityId
             >> capability.supportState >> capability.providerId >> capability.evidenceCode
             >> capability.failureCode >> capability.unit >> capability.configuredValue
             >> capability.effectiveValue >> capability.minimum >> capability.maximum
             >> capability.step >> capability.allowedValues;
    argument.endStructure();
    return argument;
}

} // namespace adrenalin::contracts::hardware1
