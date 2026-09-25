#include "hardware1_registry.h"

#include <algorithm>

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
    {QStringLiteral("gpu.clock.maximum"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.fan.control"), {QStringLiteral("GPU_PCI")}},
    {QStringLiteral("gpu.voltage.manual"), {QStringLiteral("GPU_PCI")}},

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

} // namespace adrenalin::contracts::hardware1
