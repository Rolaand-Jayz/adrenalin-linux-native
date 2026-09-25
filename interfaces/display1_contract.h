#pragma once

#include "display1_contract_types.h"

namespace adrenalin::contracts::display1 {

class Contract
{
public:
    virtual ~Contract() = default;
    virtual ListReply listDisplays() const = 0;
    virtual StateReply getDisplayState(const QString &subjectId) const = 0;
    virtual ValidationReply validateDisplay(const QString &operationId,
                                            const QString &subjectId,
                                            quint64 expectedInventoryGeneration,
                                            quint64 expectedCapabilityGeneration,
                                            const QList<ControlChange> &changes) const = 0;
    virtual ApplyReply applyDisplay(const QString &operationId,
                                    const QString &subjectId,
                                    quint64 expectedInventoryGeneration,
                                    quint64 expectedCapabilityGeneration,
                                    const QList<ControlChange> &changes) = 0;
};

} // namespace adrenalin::contracts::display1
