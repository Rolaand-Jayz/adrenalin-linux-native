#include "settings1_mock.h"

using adrenalin::contracts::MutationResult;
using adrenalin::contracts::OperationResultCode;

Settings1ReadResult Settings1Mock::getProductTelemetryConsent()
{
    return {QStringLiteral("OK"), instanceUuid_, generation_, eventSequence_, enabled_, revision_};
}

Settings1WriteResult Settings1Mock::setProductTelemetryConsent(const QString &operationId,
                                                               bool enabled,
                                                               quint64 expectedRevision)
{
    MutationResult result;
    result.operationId = operationId;
    result.provider = QStringLiteral("session-settings");
    result.subjectId = QStringLiteral("product.telemetry_consent");
    if (operationId.isEmpty() || operationId.size() > 128) {
        result.code = OperationResultCode::InvalidArgument;
        result.humanMessageKey = QStringLiteral("settings.operation.invalid_argument");
        result.diagnosticMessage = QStringLiteral("Operation ID length is outside the supported range");
        result.revision = revision_;
        return {result};
    }
    const auto prior = operations_.constFind(operationId);
    if (prior != operations_.cend()) {
        if (prior->enabled != enabled || prior->expected != expectedRevision) {
            result.code = OperationResultCode::Conflict;
            result.humanMessageKey = QStringLiteral("settings.operation.conflict");
            result.diagnosticMessage = QStringLiteral("Operation ID was reused with different values");
            result.revision = revision_;
            return {result};
        }
        result.code = OperationResultCode::Ok;
        result.humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
        result.revision = prior->result;
        return {result};
    }
    if (expectedRevision != revision_) {
        result.code = OperationResultCode::StaleRevision;
        result.humanMessageKey = QStringLiteral("settings.operation.stale_revision");
        result.diagnosticMessage = QStringLiteral("Expected revision does not match current state");
        result.revision = revision_;
        return {result};
    }
    enabled_ = enabled;
    ++revision_;
    ++eventSequence_;
    operations_.insert(operationId, Operation{enabled, expectedRevision, revision_});
    result.code = OperationResultCode::Ok;
    result.humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
    result.revision = revision_;
    return {result};
}

ToastNotificationsReadResult Settings1Mock::getToastNotifications()
{
    if (!toastNotificationsConfigured_) {
        return {QStringLiteral("UNAVAILABLE"), {}, 0, 0, false, 0};
    }
    return {QStringLiteral("OK"), instanceUuid_, generation_, eventSequence_,
            toastNotificationsEnabled_, toastNotificationsRevision_};
}

Settings1WriteResult Settings1Mock::setToastNotifications(const QString &operationId,
                                                           bool enabled,
                                                           quint64 expectedRevision)
{
    using adrenalin::contracts::OperationResultCode;
    MutationResult result;
    result.operationId = operationId;
    result.provider = QStringLiteral("session-settings");
    result.subjectId = QStringLiteral("toast.notifications");
    if (operationId.isEmpty() || operationId.size() > 128) {
        result.code = OperationResultCode::InvalidArgument;
        result.humanMessageKey = QStringLiteral("settings.operation.invalid_argument");
        result.diagnosticMessage = QStringLiteral("Operation ID length is outside the supported range");
        result.revision = toastNotificationsRevision_;
        return {result};
    }
    const auto prior = toastOperations_.constFind(operationId);
    if (prior != toastOperations_.cend()) {
        if (prior->enabled != enabled || prior->expected != expectedRevision) {
            result.code = OperationResultCode::Conflict;
            result.humanMessageKey = QStringLiteral("settings.operation.conflict");
            result.diagnosticMessage = QStringLiteral("Operation ID was reused with different values");
            result.revision = toastNotificationsRevision_;
            return {result};
        }
        result.code = OperationResultCode::Ok;
        result.humanMessageKey = QStringLiteral("settings.toast_notifications.updated");
        result.revision = prior->result;
        return {result};
    }
    if (expectedRevision != toastNotificationsRevision_) {
        result.code = OperationResultCode::StaleRevision;
        result.humanMessageKey = QStringLiteral("settings.operation.stale_revision");
        result.diagnosticMessage = QStringLiteral("Expected revision does not match current state");
        result.revision = toastNotificationsRevision_;
        return {result};
    }
    if (enabled != toastNotificationsEnabled_) {
        toastNotificationsEnabled_ = enabled;
        toastNotificationsConfigured_ = true;
        ++toastNotificationsRevision_;
        ++eventSequence_;
    } else if (!toastNotificationsConfigured_) {
        toastNotificationsConfigured_ = true;
        ++toastNotificationsRevision_;
        ++eventSequence_;
    }
    toastOperations_.insert(operationId, Operation{enabled, expectedRevision,
                                                    toastNotificationsRevision_});
    result.code = OperationResultCode::Ok;
    result.humanMessageKey = QStringLiteral("settings.toast_notifications.updated");
    result.revision = toastNotificationsRevision_;
    return {result};
}
