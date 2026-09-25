#include "profiles1_contract_types.h"
#include "profiles1_contract.h"

#include "operation_result.h"

#include <QRegularExpression>
#include <cmath>

namespace adrenalin::contracts::profiles1 {
namespace {

bool fail(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool isScalarValue(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::QString:
        return true;
    case QMetaType::Double:
        return std::isfinite(value.toDouble());
    default:
        return false;
    }
}

} // namespace

bool isValidSubject(const QString &subjectKind, const QString &subjectId)
{
    static const QRegularExpression gameId(
        QStringLiteral("^[a-z][a-z0-9+.-]*:[A-Za-z0-9][A-Za-z0-9._:/-]{0,254}$"));
    if (subjectKind == QLatin1String("GLOBAL")) {
        return subjectId == QLatin1String("global");
    }
    return subjectKind == QLatin1String("GAME") && gameId.match(subjectId).hasMatch();
}

bool isValidServiceIdentity(const QString &serviceInstanceUuid, quint64 serviceGeneration)
{
    static const QRegularExpression uuid(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-8][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$"));
    return uuid.match(serviceInstanceUuid).hasMatch() && serviceGeneration != 0;
}

bool isValidSettings(const QVariantMap &settings, QString *error)
{
    static const QRegularExpression keySyntax(QStringLiteral("^[a-z][a-z0-9_.-]{0,127}$"));
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        if (!keySyntax.match(it.key()).hasMatch()) {
            return fail(error, QStringLiteral("Profile setting key is outside the stable token syntax"));
        }
        if (!isScalarValue(it.value())) {
            return fail(error, QStringLiteral("Profile settings accept only finite scalar wire values"));
        }
    }
    return true;
}

bool Profile::isValid(QString *error) const
{
    static const QRegularExpression uuid(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-8][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$"));
    if (!uuid.match(profileId).hasMatch()) {
        return fail(error, QStringLiteral("Profile identity must be a UUID"));
    }
    if (!isValidSubject(subjectKind, subjectId)) {
        return fail(error, QStringLiteral("Profile subject identity is invalid"));
    }
    if (!presetId.isEmpty()) {
        return fail(error, QStringLiteral("Preset identity is reference-gated and cannot be asserted by this contract"));
    }
    if (referenceState != QLatin1String("REFERENCE_GATED")) {
        return fail(error, QStringLiteral("Profile state semantics must remain reference-gated in this contract"));
    }
    if (revision == 0) {
        return fail(error, QStringLiteral("Profile revision must be nonzero"));
    }
    return isValidSettings(settings, error);
}

bool ReadReply::isValid(QString *error) const
{
    if (!adrenalin::contracts::operationResultCodeFromName(code).has_value()) {
        return fail(error, QStringLiteral("Profile result code is outside the canonical result vocabulary"));
    }
    const bool hasEnvelope = isValidServiceIdentity(serviceInstanceUuid, serviceGeneration);
    if (code == QLatin1String("OK")) {
        if (!hasEnvelope) {
            return fail(error, QStringLiteral("Successful profile reads require a valid service event cursor"));
        }
        if (!profile.isValid(error)) {
            return false;
        }
        return true;
    }
    if (code == QLatin1String("NOT_FOUND")) {
        if (!hasEnvelope) {
            return fail(error, QStringLiteral("Authoritative NOT_FOUND reads require a valid service event cursor"));
        }
    } else if (hasEnvelope || !serviceInstanceUuid.isEmpty() || serviceGeneration != 0 || eventSequence != 0) {
        return fail(error, QStringLiteral("Unavailable or invalid profile reads must use an empty cursor envelope"));
    }
    if (!profile.profileId.isEmpty() || !profile.subjectKind.isEmpty() || !profile.subjectId.isEmpty()
        || !profile.presetId.isEmpty() || !profile.referenceState.isEmpty() || profile.revision != 0
        || !profile.settings.isEmpty()) {
        return fail(error, QStringLiteral("Failed profile reads must not carry partial profile state"));
    }
    return true;
}

bool ReadReply::isValidFor(const QString &subjectKind, const QString &subjectId, QString *error) const
{
    if (!isValid(error)) {
        return false;
    }
    if (!isValidSubject(subjectKind, subjectId)) {
        return code == QLatin1String("INVALID_ARGUMENT")
            ? true : fail(error, QStringLiteral("Invalid requested profile identity requires INVALID_ARGUMENT"));
    }
    if (code == QLatin1String("INVALID_ARGUMENT")) {
        return fail(error, QStringLiteral("Valid requested profile identity cannot return INVALID_ARGUMENT"));
    }
    if (code == QLatin1String("OK")
        && (profile.subjectKind != subjectKind || profile.subjectId != subjectId)) {
        return fail(error, QStringLiteral("Profile result identity does not match the requested subject"));
    }
    return true;
}

void registerMetaTypes()
{
    qRegisterMetaType<Profile>();
    qDBusRegisterMetaType<Profile>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Profile &profile)
{
    argument.beginStructure();
    argument << profile.profileId << profile.subjectKind << profile.subjectId << profile.presetId
             << profile.referenceState << profile.revision << profile.settings;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Profile &profile)
{
    argument.beginStructure();
    argument >> profile.profileId >> profile.subjectKind >> profile.subjectId >> profile.presetId
             >> profile.referenceState >> profile.revision >> profile.settings;
    argument.endStructure();
    return argument;
}

} // namespace adrenalin::contracts::profiles1
