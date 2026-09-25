#pragma once

#include "notifications1_contract_types.h"
#include "operation_result.h"

namespace adrenalin::contracts::notifications1 {

struct MarkReadReply final {
    MutationResult mutation;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    bool changed = false;
};

class Contract {
public:
    virtual ~Contract() = default;
    virtual ListReply listNotifications() const = 0;
    virtual MarkReadReply markRead(const QString &notificationId, const QString &operationId,
                                   quint64 expectedRevision) = 0;
};

} // namespace adrenalin::contracts::notifications1
