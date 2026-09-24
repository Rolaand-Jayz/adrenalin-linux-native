#pragma once

#include <QString>

#include <optional>

namespace adrenalin::contracts {

enum class OperationResultCode {
    Ok,
    InvalidArgument,
    Unsupported,
    NotFound,
    PermissionDenied,
    AuthorizationCancelled,
    Cancelled,
    InteractionRequired,
    Busy,
    Conflict,
    StaleRevision,
    IncompatibleVersion,
    BackendUnavailable,
    BackendFailure,
    Timeout,
    DeviceDisconnected,
    StaleCapability,
    ValidationFailed,
    ApplyFailed,
    VerifyFailed,
    RollbackFailed,
    IoError,
    EncoderUnavailable,
    PortalDenied,
    AuthRequired,
    NetworkError,
    InternalError,
};

QString operationResultCodeName(OperationResultCode code);
std::optional<OperationResultCode> operationResultCodeFromName(const QString &name);

struct MutationResult {
    OperationResultCode code = OperationResultCode::InternalError;
    QString operationId;
    QString humanMessageKey;
    QString diagnosticMessage;
    bool retryable = false;
    QString provider;
    QString subjectId;
    quint64 revision = 0;
};

} // namespace adrenalin::contracts
