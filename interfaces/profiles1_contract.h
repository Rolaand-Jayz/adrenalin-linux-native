#pragma once

#include "operation_result.h"
#include "profiles1_contract_types.h"

namespace adrenalin::contracts::profiles1 {

struct UpdateReply final {
    MutationResult mutation;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    bool changed = false;
};

class Contract {
public:
    virtual ~Contract() = default;
    virtual ReadReply readProfile(const QString &subjectKind, const QString &subjectId) const = 0;
    virtual UpdateReply updateProfile(const QString &subjectKind, const QString &subjectId,
                                      quint64 expectedRevision, const QString &operationId,
                                      const QVariantMap &settingsPatch) = 0;
};

} // namespace adrenalin::contracts::profiles1
