#pragma once

#include "hardware1_contract_types.h"

namespace adrenalin::contracts::hardware1 {

class Contract
{
public:
    virtual ~Contract() = default;
    virtual Reply listDevices(QList<Device> *devices) const = 0;
    virtual Reply getDeviceInfo(const QString &subjectKind, const QString &subjectId,
                                DeviceInfo *info) const = 0;
    virtual Reply getCapabilityGraph(const QString &subjectKind, const QString &subjectId,
                                     QList<Capability> *capabilities) const = 0;
};

} // namespace adrenalin::contracts::hardware1
