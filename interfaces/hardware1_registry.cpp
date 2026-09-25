#include "hardware1_registry.h"

#include <algorithm>
#include <QHash>

namespace adrenalin::contracts::hardware1 {
namespace {

const QList<CapabilityDefinition> kRegistry{
    // GPU metrics explicitly named by the audited performance requirements.
    {QStringLiteral("gpu.metric.utilization"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.clock"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.vram_clock"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.board_power"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.edge_temperature"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.junction_temperature"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.fan_speed"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.vram_use"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.voltage"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.metric.vram_temperature"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.clock.minimum"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.clock.maximum"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.fan.control"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.fan.zero_rpm"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.fan.maximum"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.fan.curve"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.voltage.manual"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.voltage.offset"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.voltage.curve"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.vram.tuning"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.vram.frequency"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.vram.memory_timing"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.power.tuning"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.power.limit"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.tuning.mode"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.tuning.preset"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.tuning.manual"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.stress_test"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.variable_graphics_memory"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("platform.smart_access_memory"), {QStringLiteral("PLATFORM")}},

    // CPU package and platform scopes are stated separately by the spec.
    {QStringLiteral("cpu.metric.utilization"), {QStringLiteral("CPU_PACKAGE")}},
    {QStringLiteral("cpu.metric.frequency"), {QStringLiteral("CPU_PACKAGE")}},
    {QStringLiteral("cpu.metric.temperature"), {QStringLiteral("CPU_PACKAGE")}},
    {QStringLiteral("platform.metric.system_ram"), {QStringLiteral("PLATFORM")}},

    // Display controls/capabilities are explicitly per-display in the spec.
    {QStringLiteral("display.freesync_adaptive_sync"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.virtual_super_resolution"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.gpu_scaling"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.scaling_mode"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.hdmi_scaling"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.integer_scaling"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.custom_color"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.color_temperature"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.brightness"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.hue"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.contrast"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.saturation"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.color_enhancement"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.color_deficiency_correction"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.color_depth"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.pixel_format"), {QStringLiteral("DISPLAY")}},
    {QStringLiteral("display.custom_resolution"), {QStringLiteral("DISPLAY")}},
};

const QStringList kProviderIds{
    QStringLiteral("provider.hwloc.cpu_topology"),
    QStringLiteral("provider.libdrm.display_inventory"),
    QStringLiteral("provider.libdrm.pci_inventory"),
    QStringLiteral("provider.test.fixture"),
    QStringLiteral("test.provider.gpu"),
    QStringLiteral("test.provider.telemetry")
};

const QStringList kEvidenceCodes{
    QStringLiteral("evidence.hwloc.package_topology"),
    QStringLiteral("evidence.libdrm.connector_edid_identity"),
    QStringLiteral("evidence.libdrm.pci_identity"),
    QStringLiteral("evidence.test.no_control"),
    QStringLiteral("evidence.test.observed_range"),
    QStringLiteral("test.observed.range"),
    QStringLiteral("test.provider.no-control"),
    QStringLiteral("test.provider.observed.range")
};

const QStringList kUnits{
    QStringLiteral("bytes"),
    QStringLiteral("bits_per_color_channel"),
    QStringLiteral("celsius"),
    QStringLiteral("degrees"),
    QStringLiteral("kelvin"),
    QStringLiteral("megahertz"),
    QStringLiteral("millivolts"),
    QStringLiteral("percent"),
    QStringLiteral("rpm"),
    QStringLiteral("watts")
};

const QHash<QString, QStringList> kUnitsByCapability{
    {QStringLiteral("gpu.metric.utilization"), {QStringLiteral("percent")}},
    {QStringLiteral("gpu.metric.clock"), {QStringLiteral("megahertz")}},
    {QStringLiteral("gpu.metric.vram_clock"), {QStringLiteral("megahertz")}},
    {QStringLiteral("gpu.metric.board_power"), {QStringLiteral("watts")}},
    {QStringLiteral("gpu.metric.edge_temperature"), {QStringLiteral("celsius")}},
    {QStringLiteral("gpu.metric.junction_temperature"), {QStringLiteral("celsius")}},
    {QStringLiteral("gpu.metric.fan_speed"), {QStringLiteral("rpm")}},
    {QStringLiteral("gpu.metric.vram_use"), {QStringLiteral("bytes")}},
    {QStringLiteral("gpu.metric.voltage"), {QStringLiteral("millivolts")}},
    {QStringLiteral("gpu.metric.vram_temperature"), {QStringLiteral("celsius")}},
    {QStringLiteral("gpu.clock.minimum"), {QStringLiteral("megahertz")}},
    {QStringLiteral("gpu.clock.maximum"), {QStringLiteral("megahertz")}},
    {QStringLiteral("gpu.fan.maximum"), {QStringLiteral("percent")}},
    {QStringLiteral("gpu.voltage.manual"), {QStringLiteral("millivolts")}},
    {QStringLiteral("gpu.voltage.offset"), {QStringLiteral("millivolts")}},
    {QStringLiteral("gpu.voltage.curve"), {QStringLiteral("millivolts")}},
    {QStringLiteral("gpu.vram.frequency"), {QStringLiteral("megahertz")}},
    {QStringLiteral("gpu.power.limit"), {QStringLiteral("watts")}},
    {QStringLiteral("cpu.metric.utilization"), {QStringLiteral("percent")}},
    {QStringLiteral("cpu.metric.frequency"), {QStringLiteral("megahertz")}},
    {QStringLiteral("cpu.metric.temperature"), {QStringLiteral("celsius")}},
    {QStringLiteral("platform.metric.system_ram"), {QStringLiteral("bytes")}},
    {QStringLiteral("display.hdmi_scaling"), {QStringLiteral("percent")}},
    {QStringLiteral("display.color_temperature"), {QStringLiteral("kelvin")}},
    {QStringLiteral("display.brightness"), {QStringLiteral("percent")}},
    {QStringLiteral("display.hue"), {QStringLiteral("degrees")}},
    {QStringLiteral("display.contrast"), {QStringLiteral("percent")}},
    {QStringLiteral("display.saturation"), {QStringLiteral("percent")}},
    {QStringLiteral("display.color_depth"), {QStringLiteral("bits_per_color_channel")}}
};

const QHash<QString, QStringList> kEnumValuesByCapability{
    {QStringLiteral("gpu.tuning.mode"),
     {QStringLiteral("default"), QStringLiteral("undervolt_gpu"),
      QStringLiteral("overclock_gpu"), QStringLiteral("overclock_vram"),
      QStringLiteral("manual")}},
    {QStringLiteral("gpu.tuning.preset"),
     {QStringLiteral("quiet"), QStringLiteral("balanced"), QStringLiteral("rage")}},
    {QStringLiteral("display.pixel_format"),
     {QStringLiteral("ycbcr_444"), QStringLiteral("ycbcr_422"),
      QStringLiteral("ycbcr_420"), QStringLiteral("rgb_444_limited"),
      QStringLiteral("rgb_444_full")}}
};

} // namespace

const QList<CapabilityDefinition> &capabilityRegistryV1()
{
    return kRegistry;
}

const CapabilityDefinition *findCapabilityV1(const QString &id)
{
    const auto it = std::find_if(kRegistry.cbegin(), kRegistry.cend(),
                                 [&id](const CapabilityDefinition &definition) {
                                     return definition.id == id;
                                 });
    return it == kRegistry.cend() ? nullptr : &*it;
}

bool capabilityAppliesToV1(const QString &id, const QString &subjectKind)
{
    const auto *definition = findCapabilityV1(id);
    return definition && definition->subjectKinds.contains(subjectKind);
}

QStringList subjectKindsForCapabilityV1(const QString &id)
{
    const auto *definition = findCapabilityV1(id);
    return definition ? definition->subjectKinds : QStringList{};
}

bool isProviderIdV1(const QString &id)
{
    return kProviderIds.contains(id);
}

bool isEvidenceCodeV1(const QString &code)
{
    return kEvidenceCodes.contains(code);
}

const QStringList &unitRegistryV1()
{
    return kUnits;
}

bool isUnitV1(const QString &unit)
{
    return kUnits.contains(unit);
}

bool unitAppliesToV1(const QString &capabilityId, const QString &unit)
{
    const auto it = kUnitsByCapability.constFind(capabilityId);
    return it != kUnitsByCapability.cend() && it->contains(unit);
}

bool enumValueAppliesToV1(const QString &capabilityId, const QString &value)
{
    const auto it = kEnumValuesByCapability.constFind(capabilityId);
    return it != kEnumValuesByCapability.cend() && it->contains(value);
}

const QStringList &providerIdRegistryV1()
{
    return kProviderIds;
}

const QStringList &evidenceCodeRegistryV1()
{
    return kEvidenceCodes;
}

} // namespace adrenalin::contracts::hardware1
