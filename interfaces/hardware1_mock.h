#pragma once

#include "hardware1_contract.h"

namespace adrenalin::contracts::hardware1 {

class Mock final : public Contract
{
public:
    Reply listDevices(QList<Device> *devices) const override;
    Reply getDeviceInfo(const QString &subjectKind, const QString &subjectId,
                        DeviceInfo *info) const override;
    Reply getCapabilityGraph(const QString &subjectKind, const QString &subjectId,
                             QList<Capability> *capabilities) const override;

    static QString testGpuSubjectId();
};

} // namespace adrenalin::contracts::hardware1
