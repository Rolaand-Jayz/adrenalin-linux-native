#pragma once

#include "operation_result.h"

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

class Settings1Contract
{
public:
    virtual ~Settings1Contract() = default;
    virtual QString initializationState() const = 0;
    virtual QString serviceInstanceUuid() const = 0;
    virtual quint64 serviceGeneration() const = 0;
    virtual ushort apiMajor() const = 0;
    virtual ushort apiMinor() const = 0;
    virtual Settings1ReadResult getProductTelemetryConsent() = 0;
    virtual Settings1WriteResult setProductTelemetryConsent(const QString &operationId,
                                                            bool enabled,
                                                            quint64 expectedRevision) = 0;
};
