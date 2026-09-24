#include <settings1_adaptor.h>

#include "interfaces/operation_result.h"
#include "session_service.h"

Settings1Adaptor::Settings1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Settings1Adaptor::~Settings1Adaptor() = default;

QString Settings1Adaptor::GetProductTelemetryConsent(bool &enabled, qulonglong &revision)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        enabled = false;
        revision = 0;
        return QStringLiteral("SERVICE_UNAVAILABLE");
    }
    const Settings1ReadResult result = service->getProductTelemetryConsent();
    enabled = result.enabled;
    revision = result.revision;
    return result.resultCode;
}

QString Settings1Adaptor::SetProductTelemetryConsent(const QString &operationId, bool enabled,
                                                      qulonglong expectedRevision,
                                                      QString &operationIdResult,
                                                      QString &humanMessageKey,
                                                      QString &diagnosticMessage,
                                                      bool &retryable,
                                                      QString &provider,
                                                      QString &subjectId,
                                                      qulonglong &newRevision)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        operationIdResult = operationId;
        humanMessageKey = QStringLiteral("service.unavailable");
        diagnosticMessage = QStringLiteral("Session service implementation is unavailable");
        retryable = true;
        provider = QStringLiteral("session-settings");
        subjectId = QStringLiteral("product.telemetry_consent");
        newRevision = expectedRevision;
        return adrenalin::contracts::operationResultCodeName(
            adrenalin::contracts::OperationResultCode::BackendUnavailable);
    }
    const Settings1WriteResult result = service->setProductTelemetryConsent(
        operationId, enabled, expectedRevision);
    operationIdResult = result.result.operationId;
    humanMessageKey = result.result.humanMessageKey;
    diagnosticMessage = result.result.diagnosticMessage;
    retryable = result.result.retryable;
    provider = result.result.provider;
    subjectId = result.result.subjectId;
    newRevision = result.result.revision;
    return adrenalin::contracts::operationResultCodeName(result.result.code);
}
