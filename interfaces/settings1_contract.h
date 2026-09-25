#pragma once

#include "operation_result.h"
#include "service_readiness_contract.h"

#include <QString>

struct Settings1ReadResult {
    QString resultCode;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    bool enabled = false;
    quint64 revision = 0;
};

struct Settings1WriteResult {
    adrenalin::contracts::MutationResult result;
};

struct ToastNotificationsReadResult {
    QString resultCode;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    bool enabled = false;
    quint64 revision = 0;
};

class Settings1Contract : public ServiceReadinessContract
{
public:
    virtual ~Settings1Contract() = default;
    virtual Settings1ReadResult getProductTelemetryConsent() = 0;
    virtual Settings1WriteResult setProductTelemetryConsent(const QString &operationId,
                                                            bool enabled,
                                                            quint64 expectedRevision) = 0;
    virtual ToastNotificationsReadResult getToastNotifications() = 0;
    virtual Settings1WriteResult setToastNotifications(const QString &operationId,
                                                       bool enabled,
                                                       quint64 expectedRevision) = 0;
};
