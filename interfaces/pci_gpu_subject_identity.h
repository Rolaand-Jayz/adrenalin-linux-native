#pragma once

#include "linux_drm_inventory_source.h"

#include <QString>

#include <cstdint>
#include <optional>

namespace adrenalin::hardware {

struct GpuSubjectIdentity final {
    QString subjectKind = QStringLiteral("GPU_PCI");
    QString subjectId;
};

// The ID is stable only while the GPU keeps the same PCI address and PCI
// vendor/device IDs. Moving the device to another slot or changing PCI
// topology changes the ID; it is not a hardware-serial identity. The PCI BDF
// is used only as input evidence and is never included verbatim in the ID.
std::optional<GpuSubjectIdentity> gpuSubjectIdentity(const PciGpuIdentity &identity);

} // namespace adrenalin::hardware
