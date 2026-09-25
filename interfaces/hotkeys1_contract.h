#pragma once

#include "hotkeys1_contract_types.h"
#include "operation_result.h"

namespace adrenalin::contracts::hotkeys1 {

struct UpdateReply final {
    MutationResult mutation;
    Action action;
    bool changed = false;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
};

class Contract
{
public:
    virtual ~Contract() = default;
    virtual Reply listActions(QList<Action> *actions) const = 0;
    virtual UpdateReply setBinding(const QString &actionId, const QString &configuredBinding,
                                   quint64 expectedRevision, const QString &operationId,
                                   const QString &parentWindowId) = 0;
};

} // namespace adrenalin::contracts::hotkeys1
