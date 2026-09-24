#include "linux_drm_inventory_source.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <xf86drm.h>

namespace adrenalin::hardware {
namespace {

constexpr int kMaximumDrmRecords = 256;
constexpr std::uint32_t kAmdVendorId = 0x1002;

struct RawPciRecord {
    bool pciBus = false;
    bool pciDevice = false;
    std::uint32_t domain = 0;
    std::uint32_t bus = 0;
    std::uint32_t device = 0;
    std::uint32_t function = 0;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
};

PciGpuInventoryResult failure(const QString &message)
{
    PciGpuInventoryResult result;
    result.error = message;
    return result;
}

PciGpuInventoryResult validateRecords(bool enumerationSucceeded, int reportedCount,
                                      const QVector<RawPciRecord> &records)
{
    if (!enumerationSucceeded) {
        return failure(QStringLiteral("libdrm device enumeration failed"));
    }
    if (reportedCount < 0 || reportedCount > kMaximumDrmRecords
        || reportedCount != records.size()) {
        return failure(QStringLiteral("libdrm returned an invalid device count"));
    }

    PciGpuInventoryResult result;
    result.success = true;
    for (const RawPciRecord &record : records) {
        // Other DRM bus types are valid libdrm records but cannot identify a PCI GPU.
        if (!record.pciBus) {
            continue;
        }
        if (!record.pciDevice || record.domain > std::numeric_limits<std::uint16_t>::max()
            || record.bus > 0xff || record.device > 0x1f || record.function > 0x7
            || record.vendorId > std::numeric_limits<std::uint16_t>::max()
            || record.deviceId > std::numeric_limits<std::uint16_t>::max()
            || record.vendorId == 0 || record.deviceId == 0) {
            return failure(QStringLiteral("libdrm returned malformed PCI identity data"));
        }
        if (record.vendorId != kAmdVendorId) {
            continue;
        }

        const QString bdf = QStringLiteral("%1:%2:%3.%4")
            .arg(record.domain, 4, 16, QLatin1Char('0'))
            .arg(record.bus, 2, 16, QLatin1Char('0'))
            .arg(record.device, 2, 16, QLatin1Char('0'))
            .arg(record.function, 1, 16, QLatin1Char('0'))
            .toLower();
        const auto duplicate = std::find_if(result.devices.cbegin(), result.devices.cend(),
            [&bdf](const PciGpuIdentity &identity) { return identity.canonicalBdf == bdf; });
        if (duplicate != result.devices.cend()) {
            if (duplicate->vendorId != record.vendorId
                || duplicate->deviceId != record.deviceId) {
                return failure(QStringLiteral("libdrm returned conflicting duplicate PCI identities"));
            }
            return failure(QStringLiteral("libdrm returned ambiguous duplicate PCI identity evidence"));
        }
        result.devices.push_back({static_cast<std::uint16_t>(record.vendorId),
                                  static_cast<std::uint16_t>(record.deviceId), bdf});
    }
    std::sort(result.devices.begin(), result.devices.end(),
        [](const PciGpuIdentity &left, const PciGpuIdentity &right) {
            return left.canonicalBdf < right.canonicalBdf;
        });
    return result;
}

struct DrmDeviceList final {
    std::array<drmDevicePtr, kMaximumDrmRecords> devices{};
    ~DrmDeviceList()
    {
        // drmGetDevices2 may have filled part of the output array even when a
        // concurrent inventory change makes its result unusable.
        for (drmDevicePtr &device : devices) {
            if (device != nullptr) {
                drmFreeDevice(&device);
            }
        }
    }
};

PciGpuInventoryResult enumerateWithLibdrm()
{
    const int count = drmGetDevices2(0, nullptr, 0);
    if (count < 0) {
        return failure(QStringLiteral("libdrm device enumeration failed"));
    }
    if (count > kMaximumDrmRecords) {
        return failure(QStringLiteral("libdrm device count exceeds the supported bound"));
    }
    if (count == 0) {
        PciGpuInventoryResult result;
        result.success = true;
        return result;
    }

    DrmDeviceList allocated;
    const int found = drmGetDevices2(0, allocated.devices.data(), count);
    if (found < 0 || found != count) {
        return failure(QStringLiteral("libdrm device inventory changed during enumeration"));
    }

    QVector<RawPciRecord> records;
    records.reserve(found);
    for (int index = 0; index < found; ++index) {
        const drmDevicePtr device = allocated.devices[static_cast<std::size_t>(index)];
        if (device == nullptr) {
            return failure(QStringLiteral("libdrm returned a null device record"));
        }
        RawPciRecord record;
        record.pciBus = device->bustype == DRM_BUS_PCI;
        if (record.pciBus) {
            record.pciDevice = device->businfo.pci != nullptr
                && device->deviceinfo.pci != nullptr;
            if (record.pciDevice) {
                record.domain = device->businfo.pci->domain;
                record.bus = device->businfo.pci->bus;
                record.device = device->businfo.pci->dev;
                record.function = device->businfo.pci->func;
                record.vendorId = device->deviceinfo.pci->vendor_id;
                record.deviceId = device->deviceinfo.pci->device_id;
            }
        }
        records.push_back(record);
    }
    return validateRecords(true, found, records);
}

} // namespace

PciGpuInventoryResult LinuxDrmInventorySource::enumerate()
{
    return enumerateWithLibdrm();
}

#ifdef ADRENALIN_DRM_INVENTORY_SOURCE_TESTING
PciGpuInventoryResult LinuxDrmInventorySource::enumerateForTesting(
    const TestEnumerator &enumerator)
{
    if (!enumerator) {
        return failure(QStringLiteral("test enumerator is empty"));
    }
    const RawEnumeration raw = enumerator();
    QVector<RawPciRecord> records;
    records.reserve(raw.records.size());
    for (const RawDevice &record : raw.records) {
        records.push_back({record.pciBus, record.pciDevice, record.domain, record.bus,
                           record.device, record.function, record.vendorId, record.deviceId});
    }
    return validateRecords(raw.success, raw.reportedCount, records);
}
#endif

} // namespace adrenalin::hardware
