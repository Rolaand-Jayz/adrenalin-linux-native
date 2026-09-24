#include "operation_result.h"

#include <array>

namespace adrenalin::contracts {
namespace {

struct CodeName {
    OperationResultCode code;
    const char *name;
};

constexpr std::array codeNames{
    CodeName{OperationResultCode::Ok, "OK"},
    CodeName{OperationResultCode::InvalidArgument, "INVALID_ARGUMENT"},
    CodeName{OperationResultCode::Unsupported, "UNSUPPORTED"},
    CodeName{OperationResultCode::NotFound, "NOT_FOUND"},
    CodeName{OperationResultCode::PermissionDenied, "PERMISSION_DENIED"},
    CodeName{OperationResultCode::AuthorizationCancelled, "AUTHORIZATION_CANCELLED"},
    CodeName{OperationResultCode::Cancelled, "CANCELLED"},
    CodeName{OperationResultCode::InteractionRequired, "INTERACTION_REQUIRED"},
    CodeName{OperationResultCode::Busy, "BUSY"},
    CodeName{OperationResultCode::Conflict, "CONFLICT"},
    CodeName{OperationResultCode::StaleRevision, "STALE_REVISION"},
    CodeName{OperationResultCode::IncompatibleVersion, "INCOMPATIBLE_VERSION"},
    CodeName{OperationResultCode::BackendUnavailable, "BACKEND_UNAVAILABLE"},
    CodeName{OperationResultCode::BackendFailure, "BACKEND_FAILURE"},
    CodeName{OperationResultCode::Timeout, "TIMEOUT"},
    CodeName{OperationResultCode::DeviceDisconnected, "DEVICE_DISCONNECTED"},
    CodeName{OperationResultCode::StaleCapability, "STALE_CAPABILITY"},
    CodeName{OperationResultCode::ValidationFailed, "VALIDATION_FAILED"},
    CodeName{OperationResultCode::ApplyFailed, "APPLY_FAILED"},
    CodeName{OperationResultCode::VerifyFailed, "VERIFY_FAILED"},
    CodeName{OperationResultCode::RollbackFailed, "ROLLBACK_FAILED"},
    CodeName{OperationResultCode::IoError, "IO_ERROR"},
    CodeName{OperationResultCode::EncoderUnavailable, "ENCODER_UNAVAILABLE"},
    CodeName{OperationResultCode::PortalDenied, "PORTAL_DENIED"},
    CodeName{OperationResultCode::AuthRequired, "AUTH_REQUIRED"},
    CodeName{OperationResultCode::NetworkError, "NETWORK_ERROR"},
    CodeName{OperationResultCode::InternalError, "INTERNAL_ERROR"},
};

} // namespace

QString operationResultCodeName(OperationResultCode code)
{
    for (const auto &entry : codeNames) {
        if (entry.code == code) {
            return QString::fromLatin1(entry.name);
        }
    }
    return QStringLiteral("INTERNAL_ERROR");
}

std::optional<OperationResultCode> operationResultCodeFromName(const QString &name)
{
    for (const auto &entry : codeNames) {
        if (name == QLatin1StringView(entry.name)) {
            return entry.code;
        }
    }
    return std::nullopt;
}

} // namespace adrenalin::contracts
