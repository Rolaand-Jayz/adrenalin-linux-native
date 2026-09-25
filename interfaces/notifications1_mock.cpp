#include "notifications1_mock.h"

#include <algorithm>
#include <limits>
#include <QRegularExpression>

namespace adrenalin::contracts::notifications1 {
using adrenalin::contracts::MutationResult;
using adrenalin::contracts::OperationResultCode;

Mock::Mock()
{
    notifications_.append({QStringLiteral("notification-contract-1"),
                           QStringLiteral("ERROR"),
                           QStringLiteral("2026-09-25T12:00:00.000Z"),
                           QStringLiteral("notification.error.title"),
                           QStringLiteral("notification.error.body"),
                           false, true, true});
    notifications_.append({QStringLiteral("notification-contract-2"),
                           QStringLiteral("GAME_DETECTED"),
                           QStringLiteral("2026-09-25T11:00:00.000Z"),
                           QStringLiteral("notification.game_detected.title"),
                           QStringLiteral("notification.game_detected.body"),
                           true, false, false});
}

ListReply Mock::listNotifications() const
{
    ListReply reply{QStringLiteral("OK"), serviceInstanceUuid_, serviceGeneration_, eventSequence_,
                    revision_, notifications_};
    return reply;
}

MarkReadReply Mock::markRead(const QString &notificationId, const QString &operationId,
                             quint64 expectedRevision)
{
    MarkReadReply reply;
    reply.serviceInstanceUuid = serviceInstanceUuid_;
    reply.serviceGeneration = serviceGeneration_;
    reply.eventSequence = eventSequence_;
    reply.mutation.operationId = operationId;
    reply.mutation.provider = QStringLiteral("notifications-contract-mock");
    reply.mutation.subjectId = QStringLiteral("platform");
    reply.mutation.revision = revision_;
    const auto setError = [&](OperationResultCode code, const QString &key, const QString &detail) {
        reply.mutation.code = code;
        reply.mutation.humanMessageKey = key;
        reply.mutation.diagnosticMessage = detail;
        return reply;
    };

    static const QRegularExpression idSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!idSyntax.match(operationId).hasMatch()) {
        return setError(OperationResultCode::InvalidArgument,
                        QStringLiteral("notifications.operation.invalid_argument"),
                        QStringLiteral("Operation ID is outside the supported range"));
    }
    if (!idSyntax.match(notificationId).hasMatch()) {
        return setError(OperationResultCode::InvalidArgument,
                        QStringLiteral("notifications.operation.invalid_argument"),
                        QStringLiteral("Notification ID is invalid"));
    }

    const auto prior = operations_.constFind(operationId);
    if (prior != operations_.cend()) {
        if (prior->notificationId != notificationId || prior->expectedRevision != expectedRevision) {
            return setError(OperationResultCode::Conflict,
                            QStringLiteral("notifications.operation.conflict"),
                            QStringLiteral("Operation ID was reused with different values"));
        }
        MarkReadReply replay = prior->reply;
        replay.changed = false;
        return replay;
    }

    auto found = std::find_if(notifications_.begin(), notifications_.end(),
        [&](const Notification &notification) {
            return notification.notificationId == notificationId;
        });
    if (found == notifications_.end()) {
        return setError(OperationResultCode::NotFound,
                        QStringLiteral("notifications.item.not_found"),
                        QStringLiteral("Notification is not present in this snapshot"));
    }
    if (expectedRevision != revision_) {
        return setError(OperationResultCode::StaleRevision,
                        QStringLiteral("notifications.operation.stale_revision"),
                        QStringLiteral("Expected revision does not match the current snapshot"));
    }
    if (found->isRead) {
        reply.mutation.code = OperationResultCode::Ok;
        reply.mutation.humanMessageKey = QStringLiteral("notifications.item.already_read");
        operations_.insert(operationId, {notificationId, expectedRevision, reply});
        return reply;
    }
    if (revision_ == std::numeric_limits<quint64>::max()
        || eventSequence_ == std::numeric_limits<quint64>::max()) {
        return setError(OperationResultCode::InternalError,
                        QStringLiteral("notifications.counter.exhausted"),
                        QStringLiteral("Notification revision or event sequence is exhausted"));
    }

    found->isRead = true;
    ++revision_;
    ++eventSequence_;
    reply.eventSequence = eventSequence_;
    reply.changed = true;
    reply.mutation.code = OperationResultCode::Ok;
    reply.mutation.humanMessageKey = QStringLiteral("notifications.item.marked_read");
    reply.mutation.revision = revision_;
    MarkReadReply cachedReply = reply;
    cachedReply.changed = false;
    operations_.insert(operationId, {notificationId, expectedRevision, cachedReply});
    return reply;
}

} // namespace adrenalin::contracts::notifications1
