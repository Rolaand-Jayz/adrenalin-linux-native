#pragma once

#include <QString>
#include <QVector>

#include <cstdint>
#include <functional>

namespace adrenalin::hardware {

struct PciGpuIdentity {
    std::uint16_t vendorId = 0;
    std::uint16_t deviceId = 0;
    // Transient physical-location evidence. Callers must not persist or expose
    // this value as a user-facing label.
    QString canonicalBdf;
};

struct PciGpuInventoryResult {
    bool success = false;
    QString error;
    QVector<PciGpuIdentity> devices;
};

class LinuxDrmInventorySource final {
public:
    // Enumeration failure is distinct from a successful empty inventory.
    static PciGpuInventoryResult enumerate();

#ifdef ADRENALIN_DRM_INVENTORY_SOURCE_TESTING
    struct RawDevice {
        bool pciBus = false;
        bool pciDevice = false;
        std::uint32_t domain = 0;
        std::uint32_t bus = 0;
        std::uint32_t device = 0;
        std::uint32_t function = 0;
        std::uint32_t vendorId = 0;
        std::uint32_t deviceId = 0;
    };

    struct RawEnumeration {
        bool success = false;
        int reportedCount = 0;
        QVector<RawDevice> records;
    };

    using TestEnumerator = std::function<RawEnumeration()>;
    static PciGpuInventoryResult enumerateForTesting(const TestEnumerator &enumerator);
#endif
};

} // namespace adrenalin::hardware
