#pragma once

#include <QString>
#include <QVector>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace adrenalin::hardware {

struct CpuPackageIdentity {
    QString subjectId;
    // Local kernel topology evidence. It is part of token derivation, but it is
    // not a user-facing label and must not be exposed on IPC.
    std::uint32_t physicalPackageId = 0;
    QString architecture;
    QString vendorId;
    std::uint32_t family = 0;
    std::uint32_t model = 0;
    std::uint32_t stepping = 0;
};

struct CpuPackageInventoryResult {
    bool success = false;
    QString error;
    QVector<CpuPackageIdentity> packages;
};

class LinuxCpuPackageSource final {
public:
    // Enumerates physical package topology through hwloc. Enumeration failure
    // is distinct from a successfully observed inventory.
    static CpuPackageInventoryResult enumerate();

#ifdef ADRENALIN_CPU_PACKAGE_SOURCE_TESTING
    struct RawPackage {
        bool packageObject = true;
        bool topologyEvidencePresent = true;
        std::optional<std::uint32_t> physicalPackageId;
        QString architecture;
        QString vendorId;
        std::optional<std::uint32_t> family;
        std::optional<std::uint32_t> model;
        std::optional<std::uint32_t> stepping;
    };

    struct RawProcessor {
        std::uint32_t processorOsIndex = 0;
        int packageAncestorCount = 0;
        std::optional<std::uint32_t> packageOsIndex;
    };

    struct RawEnumeration {
        RawEnumeration() = default;
        RawEnumeration(bool enumerationSuccess, int packageCount,
                       QVector<RawPackage> packageEvidence,
                       int onlineProcessorCount = -1,
                       QVector<RawProcessor> processorEvidence = {})
            : success(enumerationSuccess), reportedCount(packageCount),
              packages(std::move(packageEvidence)),
              reportedOnlineProcessorCount(onlineProcessorCount),
              onlineProcessors(std::move(processorEvidence))
        {
        }

        bool success = false;
        int reportedCount = 0;
        QVector<RawPackage> packages;
        int reportedOnlineProcessorCount = -1;
        QVector<RawProcessor> onlineProcessors;
    };

    using TestEnumerator = std::function<RawEnumeration()>;
    static CpuPackageInventoryResult enumerateForTesting(const TestEnumerator &enumerator);
#endif
};

} // namespace adrenalin::hardware
