#include <profiles1_adaptor.h>

#include "interfaces/operation_result.h"
#include "session_service.h"

Profiles1Adaptor::Profiles1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Profiles1Adaptor::~Profiles1Adaptor() = default;

QString Profiles1Adaptor::ReadProfile(
    const QString &subject_kind, const QString &subject_id,
    QString &service_instance_uuid, qulonglong &service_generation,
    qulonglong &event_sequence, adrenalin::contracts::profiles1::Profile &profile)
{
    const auto *service = static_cast<const SessionService *>(parent());
    const auto reply = service->readProfile(subject_kind, subject_id);
    service_instance_uuid = reply.serviceInstanceUuid;
    service_generation = reply.serviceGeneration;
    event_sequence = reply.eventSequence;
    profile = reply.profile;
    return reply.code;
}

QString Profiles1Adaptor::UpdateProfile(
    const QString &subject_kind, const QString &subject_id, qulonglong expected_revision,
    const QString &operation_id, const QVariantMap &settings_patch,
    QString &operation_id_result, QString &human_message_key,
    QString &diagnostic_message, bool &retryable, QString &provider,
    QString &result_subject_id, qulonglong &revision)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        operation_id_result = operation_id;
        human_message_key = QStringLiteral("service.unavailable");
        diagnostic_message = QStringLiteral("Session service implementation is unavailable");
        retryable = true;
        provider = QStringLiteral("session-profiles");
        result_subject_id = subject_id;
        revision = expected_revision;
        return adrenalin::contracts::operationResultCodeName(
            adrenalin::contracts::OperationResultCode::BackendUnavailable);
    }
    const auto outcome = service->updateProfile(subject_kind, subject_id, expected_revision,
                                                operation_id, settings_patch);
    const auto &mutation = outcome.mutation;
    operation_id_result = mutation.operationId;
    human_message_key = mutation.humanMessageKey;
    diagnostic_message = mutation.diagnosticMessage;
    retryable = mutation.retryable;
    provider = mutation.provider;
    result_subject_id = mutation.subjectId;
    revision = mutation.revision;
    return adrenalin::contracts::operationResultCodeName(mutation.code);
}
