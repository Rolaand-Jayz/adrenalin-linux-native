#include <notifications1_adaptor.h>

#include "interfaces/operation_result.h"
#include "session_service.h"

Notifications1Adaptor::Notifications1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Notifications1Adaptor::~Notifications1Adaptor() = default;

QString Notifications1Adaptor::ListNotifications(QString &serviceInstanceUuid,
                                                  qulonglong &serviceGeneration,
                                                  qulonglong &eventSequence,
                                                  qulonglong &revision,
                                                  QList<adrenalin::contracts::notifications1::Notification> &notifications)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        serviceInstanceUuid.clear();
        serviceGeneration = 0;
        eventSequence = 0;
        revision = 0;
        notifications.clear();
        return QStringLiteral("BACKEND_UNAVAILABLE");
    }
    const auto reply = service->listNotifications();
    serviceInstanceUuid = reply.serviceInstanceUuid;
    serviceGeneration = reply.serviceGeneration;
    eventSequence = reply.eventSequence;
    revision = reply.revision;
    notifications = reply.notifications;
    return reply.code;
}

QString Notifications1Adaptor::MarkRead(const QString &notificationId, const QString &operationId,
                                         qulonglong expectedRevision, QString &operationIdResult,
                                         QString &humanMessageKey, QString &diagnosticMessage,
                                         bool &retryable, QString &provider, QString &subjectId,
                                         qulonglong &revision, bool &changed,
                                         QString &serviceInstanceUuid, qulonglong &serviceGeneration,
                                         qulonglong &eventSequence)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        operationIdResult = operationId;
        humanMessageKey = QStringLiteral("service.unavailable");
        diagnosticMessage = QStringLiteral("Session service implementation is unavailable");
        retryable = true;
        provider = QStringLiteral("session-notifications");
        subjectId = QStringLiteral("platform");
        revision = expectedRevision;
        changed = false;
        serviceInstanceUuid.clear();
        serviceGeneration = 0;
        eventSequence = 0;
        return QStringLiteral("BACKEND_UNAVAILABLE");
    }
    const auto reply = service->markRead(notificationId, operationId, expectedRevision);
    operationIdResult = reply.mutation.operationId;
    humanMessageKey = reply.mutation.humanMessageKey;
    diagnosticMessage = reply.mutation.diagnosticMessage;
    retryable = reply.mutation.retryable;
    provider = reply.mutation.provider;
    subjectId = reply.mutation.subjectId;
    revision = reply.mutation.revision;
    changed = reply.changed;
    serviceInstanceUuid = reply.serviceInstanceUuid;
    serviceGeneration = reply.serviceGeneration;
    eventSequence = reply.eventSequence;
    return adrenalin::contracts::operationResultCodeName(reply.mutation.code);
}
