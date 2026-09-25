#include "hotkeys1_mock.h"

#include <QStringList>
#include <QUuid>

namespace adrenalin::contracts::hotkeys1 {
namespace {

MutationResult result(OperationResultCode code, const QString &operationId,
                      const QString &subjectId, quint64 revision)
{
    MutationResult mutation;
    mutation.code = code;
    mutation.operationId = operationId;
    mutation.humanMessageKey = code == OperationResultCode::Ok
        ? QString{} : QStringLiteral("hotkeys.operation_failed");
    mutation.provider = QStringLiteral("mock.hotkeys");
    mutation.subjectId = subjectId;
    mutation.revision = revision;
    return mutation;
}

QString requestKey(const QString &actionId, const QString &binding, quint64 expectedRevision,
                   const QString &parentWindowId)
{
    return QStringList{actionId, binding, QString::number(expectedRevision), parentWindowId}
        .join(QChar::Null);
}

} // namespace

Mock::Mock()
    : serviceInstanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    action_.actionId = QStringLiteral("overlay.toggle");
    action_.labelMessageKey = QStringLiteral("hotkeys.action.overlay_toggle");
    action_.descriptionMessageKey = QStringLiteral("hotkeys.action.overlay_toggle.description");
    action_.registrationState = QStringLiteral("UNBOUND");
}

Reply Mock::listActions(QList<Action> *actions) const
{
    Reply reply;
    if (actions == nullptr) {
        reply.code = QStringLiteral("INVALID_ARGUMENT");
        return reply;
    }
    *actions = {action_};
    reply.code = QStringLiteral("OK");
    reply.snapshotValid = true;
    reply.serviceInstanceUuid = serviceInstanceUuid_;
    reply.serviceGeneration = serviceGeneration_;
    reply.eventSequence = eventSequence_;
    reply.revision = revision_;
    return reply;
}

UpdateReply Mock::setBinding(const QString &actionId, const QString &configuredBinding,
                             quint64 expectedRevision, const QString &operationId,
                             const QString &parentWindowId)
{
    const QString key = requestKey(actionId, configuredBinding, expectedRevision, parentWindowId);
    const auto priorRequest = operationRequests_.constFind(operationId);
    if (priorRequest != operationRequests_.cend()) {
        if (priorRequest.value() != key) {
            return {result(OperationResultCode::Conflict, operationId, actionId, revision_),
                    action_, false, serviceInstanceUuid_, serviceGeneration_, eventSequence_};
        }
        UpdateReply replay = completedOperations_.value(operationId);
        replay.changed = false;
        return replay;
    }

    if (!isValidActionId(actionId) || actionId != action_.actionId
        || !isValidBindingText(configuredBinding) || !isValidParentWindowId(parentWindowId)
        || QUuid(operationId).isNull()
        || QUuid(operationId).toString(QUuid::WithoutBraces) != operationId) {
        return {result(OperationResultCode::InvalidArgument, operationId, actionId, revision_),
                action_, false, serviceInstanceUuid_, serviceGeneration_, eventSequence_};
    }
    operationRequests_.insert(operationId, key);
    if (expectedRevision != revision_) {
        UpdateReply reply{result(OperationResultCode::StaleRevision, operationId,
                                 actionId, revision_),
                          action_, false, serviceInstanceUuid_, serviceGeneration_, eventSequence_};
        completedOperations_.insert(operationId, reply);
        return reply;
    }

    action_.configuredBinding = configuredBinding;
    action_.effectiveBinding = configuredBinding;
    action_.registrationState = configuredBinding.isEmpty()
        ? QStringLiteral("UNBOUND") : QStringLiteral("ACTIVE");
    action_.provider = configuredBinding.isEmpty() ? QString{} : QStringLiteral("mock.hotkeys");
    action_.errorCode.clear();
    ++revision_;
    ++eventSequence_;

    UpdateReply reply{result(OperationResultCode::Ok, operationId, actionId, revision_),
                      action_, true, serviceInstanceUuid_, serviceGeneration_, eventSequence_};
    completedOperations_.insert(operationId, reply);
    return reply;
}

} // namespace adrenalin::contracts::hotkeys1
