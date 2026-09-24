#include "settings1_mock.h"

Settings1ReadResult Settings1Mock::getProductTelemetryConsent()
{
    return {QStringLiteral("OK"), enabled_, revision_};
}

Settings1WriteResult Settings1Mock::setProductTelemetryConsent(const QString &operationId,
                                                               bool enabled,
                                                               quint64 expectedRevision)
{
    if (operationId.isEmpty() || operationId.size() > 128) {
        return {QStringLiteral("INVALID_ARGUMENT"), revision_};
    }
    const auto prior = operations_.constFind(operationId);
    if (prior != operations_.cend()) {
        if (prior->enabled != enabled || prior->expected != expectedRevision) {
            return {QStringLiteral("OPERATION_CONFLICT"), revision_};
        }
        return {QStringLiteral("OK"), prior->result};
    }
    if (expectedRevision != revision_) {
        return {QStringLiteral("STALE_REVISION"), revision_};
    }
    enabled_ = enabled;
    ++revision_;
    operations_.insert(operationId, Operation{enabled, expectedRevision, revision_});
    return {QStringLiteral("OK"), revision_};
}
