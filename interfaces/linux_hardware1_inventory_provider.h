#pragma once

#include "hardware1_contract_types.h"
#include "linux_cpu_package_source.h"
#include "linux_drm_display_source.h"
#include "linux_drm_inventory_source.h"

#include <QList>
#include <QString>

#include <cstdint>

namespace adrenalin::hardware {

struct Hardware1Evidence final {
    PciGpuInventoryResult gpus;
    CpuPackageInventoryResult cpuPackages;
    DrmDisplayInventoryResult displays;
};

struct Hardware1Snapshot final {
    bool success = false;
    QString error;
    QList<contracts::hardware1::Device> devices;
    QList<contracts::hardware1::DeviceInfo> deviceInfo;
    QList<contracts::hardware1::Capability> capabilities;
    std::uint64_t inventoryGeneration = 0;
    std::uint64_t capabilityGeneration = 0;
};

// Composes one all-or-nothing Hardware1 inventory from the real Linux evidence
// sources. Test-only evidence entry points are excluded from the production
// build; production callers use refresh(), which always invokes real providers.
class LinuxHardware1InventoryProvider final {
public:
    Hardware1Snapshot refresh();
    Hardware1Snapshot snapshot() const;

#ifdef ADRENALIN_HARDWARE1_INVENTORY_TESTING
    static Hardware1Snapshot composeEvidenceForTesting(const Hardware1Evidence &evidence);
    Hardware1Snapshot refreshWithEvidenceForTesting(const Hardware1Evidence &evidence);
#endif

private:
    Hardware1Snapshot publish(Hardware1Snapshot candidate);
    Hardware1Snapshot current_;
    std::uint64_t inventoryGeneration_ = 0;
    std::uint64_t capabilityGeneration_ = 0;
};

} // namespace adrenalin::hardware
