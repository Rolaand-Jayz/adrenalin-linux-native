#pragma once

#include "operation_result.h"
#include "profiles1_contract_types.h"

#include <optional>

namespace adrenalin::contracts::profiles1 {

struct ProfileChangedEvent final {
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    QString subjectKind;
    QString subjectId;
    quint64 revision = 0;

    bool isValid(QString *error = nullptr) const;
};

bool isValidServiceIdentity(const QString &serviceInstanceUuid, quint64 serviceGeneration);

inline bool isValidEventEnvelope(const QString &serviceInstanceUuid, quint64 serviceGeneration,
                                 quint64 eventSequence)
{
    return isValidServiceIdentity(serviceInstanceUuid, serviceGeneration) && eventSequence != 0;
}

// Local fixture outcome. Only mutation is returned over UpdateProfile; event
// is present only when this call freshly publishes a changed profile.
struct UpdateOutcome final {
    MutationResult mutation;
    std::optional<ProfileChangedEvent> event;
};

class Contract {
public:
    virtual ~Contract() = default;
    virtual ReadReply readProfile(const QString &subjectKind, const QString &subjectId) const = 0;
    virtual UpdateOutcome updateProfile(const QString &subjectKind, const QString &subjectId,
                                        quint64 expectedRevision, const QString &operationId,
                                        const QVariantMap &settingsPatch) = 0;
};

} // namespace adrenalin::contracts::profiles1
