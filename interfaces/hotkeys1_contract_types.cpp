#include "hotkeys1_contract_types.h"

#include <QRegularExpression>
#include <QUuid>

#include <algorithm>

namespace adrenalin::contracts::hotkeys1 {
namespace {

bool fail(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool boundedText(const QString &value, qsizetype maximum)
{
    return value.size() <= maximum && !value.contains(QChar::Null)
        && std::none_of(value.cbegin(), value.cend(), [](QChar character) {
               return character.category() == QChar::Other_Control;
           });
}

} // namespace

bool isValidActionId(const QString &actionId)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$"));
    return actionId.size() <= 128 && expression.match(actionId).hasMatch();
}

bool isValidBindingText(const QString &binding)
{
    return boundedText(binding, 256);
}

bool isValidParentWindowId(const QString &parentWindowId)
{
    return boundedText(parentWindowId, 4096);
}

QStringList registrationStates()
{
    return {QStringLiteral("UNBOUND"), QStringLiteral("ACTIVE"),
            QStringLiteral("CONFLICT"), QStringLiteral("DENIED"),
            QStringLiteral("INTERACTION_REQUIRED"), QStringLiteral("UNSUPPORTED"),
            QStringLiteral("FAILED")};
}

bool Action::isValid(QString *error) const
{
    if (!isValidActionId(actionId)) {
        return fail(error, QStringLiteral("action ID is invalid"));
    }
    static const QRegularExpression messageKeyExpression(
        QStringLiteral("^[a-z][a-z0-9]*(?:[._-][a-z0-9]+)*$"));
    if (!messageKeyExpression.match(labelMessageKey).hasMatch()
        || !messageKeyExpression.match(descriptionMessageKey).hasMatch()) {
        return fail(error, QStringLiteral("action message keys are invalid"));
    }
    if (!isValidBindingText(configuredBinding) || !isValidBindingText(effectiveBinding)) {
        return fail(error, QStringLiteral("binding text is invalid"));
    }
    if (!registrationStates().contains(registrationState)) {
        return fail(error, QStringLiteral("registration state is unsupported"));
    }
    if (!boundedText(provider, 128)) {
        return fail(error, QStringLiteral("provider is invalid"));
    }
    if (!errorCode.isEmpty() && !operationResultCodeFromName(errorCode).has_value()) {
        return fail(error, QStringLiteral("error code is unsupported"));
    }
    if (registrationState == QLatin1String("ACTIVE")
        && (effectiveBinding.isEmpty() || provider.isEmpty() || !errorCode.isEmpty())) {
        return fail(error, QStringLiteral("active binding lacks effective provider state"));
    }
    if (registrationState == QLatin1String("UNBOUND")
        && (!configuredBinding.isEmpty() || !effectiveBinding.isEmpty())) {
        return fail(error, QStringLiteral("unbound action contains a binding"));
    }
    return true;
}

bool Reply::isValid(QString *error) const
{
    if (!operationResultCodeFromName(code).has_value()) {
        return fail(error, QStringLiteral("reply result code is unsupported"));
    }
    if (!boundedText(humanMessageKey, 256) || !boundedText(diagnosticMessage, 2048)
        || !boundedText(provider, 128) || !boundedText(subjectKind, 64)
        || !boundedText(subjectId, 128)) {
        return fail(error, QStringLiteral("reply text field is invalid"));
    }
    if (snapshotValid) {
        const QUuid uuid(serviceInstanceUuid);
        if (code != QLatin1String("OK") || uuid.isNull()
            || uuid.toString(QUuid::WithoutBraces) != serviceInstanceUuid
            || serviceGeneration == 0 || eventSequence == 0 || revision == 0) {
            return fail(error, QStringLiteral("valid snapshot metadata is inconsistent"));
        }
    } else if (code == QLatin1String("OK") || !serviceInstanceUuid.isEmpty()
               || serviceGeneration != 0 || eventSequence != 0 || revision != 0) {
        return fail(error, QStringLiteral("invalid snapshot has inconsistent cursor metadata"));
    }
    return true;
}

bool isValidSnapshot(const Reply &reply, const QList<Action> &actions, QString *error)
{
    if (!reply.isValid(error)) {
        return false;
    }
    if (!reply.snapshotValid) {
        return actions.isEmpty() || fail(error, QStringLiteral("failed snapshot contains actions"));
    }
    QStringList seen;
    for (const Action &action : actions) {
        if (!action.isValid(error)) {
            return false;
        }
        if (seen.contains(action.actionId)) {
            return fail(error, QStringLiteral("snapshot contains a duplicate action ID"));
        }
        seen.append(action.actionId);
    }
    return true;
}

void registerMetaTypes()
{
    qRegisterMetaType<Action>();
    qRegisterMetaType<QList<Action>>();
    qRegisterMetaType<Reply>();
    qDBusRegisterMetaType<Action>();
    qDBusRegisterMetaType<QList<Action>>();
    qDBusRegisterMetaType<Reply>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Action &action)
{
    argument.beginStructure();
    argument << action.actionId << action.labelMessageKey << action.descriptionMessageKey
             << action.configuredBinding << action.effectiveBinding << action.registrationState
             << action.provider << action.errorCode;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Action &action)
{
    argument.beginStructure();
    argument >> action.actionId >> action.labelMessageKey >> action.descriptionMessageKey
             >> action.configuredBinding >> action.effectiveBinding >> action.registrationState
             >> action.provider >> action.errorCode;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const Reply &reply)
{
    argument.beginStructure();
    argument << reply.code << reply.humanMessageKey << reply.diagnosticMessage
             << reply.retryable << reply.provider << reply.subjectKind << reply.subjectId
             << reply.snapshotValid << reply.serviceInstanceUuid
             << static_cast<qulonglong>(reply.serviceGeneration)
             << static_cast<qulonglong>(reply.eventSequence)
             << static_cast<qulonglong>(reply.revision);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Reply &reply)
{
    qulonglong serviceGeneration = 0;
    qulonglong eventSequence = 0;
    qulonglong revision = 0;
    argument.beginStructure();
    argument >> reply.code >> reply.humanMessageKey >> reply.diagnosticMessage
             >> reply.retryable >> reply.provider >> reply.subjectKind >> reply.subjectId
             >> reply.snapshotValid >> reply.serviceInstanceUuid >> serviceGeneration
             >> eventSequence >> revision;
    argument.endStructure();
    reply.serviceGeneration = serviceGeneration;
    reply.eventSequence = eventSequence;
    reply.revision = revision;
    return argument;
}

} // namespace adrenalin::contracts::hotkeys1
