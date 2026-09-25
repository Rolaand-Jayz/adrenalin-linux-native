#include "linux_hardware1_inventory_provider.h"

#include "hardware1_registry.h"
#include "pci_gpu_subject_identity.h"

#include <QSet>
#include <QCryptographicHash>
#include <QRegularExpression>

#include <algorithm>
#include <limits>

namespace adrenalin::hardware {
namespace {
using namespace contracts::hardware1;

Hardware1Snapshot failure(const QString &message)
{
    Hardware1Snapshot result;
    result.error = message;
    return result;
}

bool sameDevice(const Device &left, const Device &right)
{
    return left.subjectKind == right.subjectKind && left.subjectId == right.subjectId
        && left.identityEvidence == right.identityEvidence;
}

bool sameInfo(const DeviceInfo &left, const DeviceInfo &right)
{
    return left.subjectKind == right.subjectKind && left.subjectId == right.subjectId
        && left.identityEvidence == right.identityEvidence
        && left.connectorIdentity == right.connectorIdentity
        && left.edidIdentityDigest == right.edidIdentityDigest;
}

bool sameValue(const Value &left, const Value &right)
{
    return left.kind == right.kind && left.booleanValue == right.booleanValue
        && left.signedValue == right.signedValue && left.unsignedValue == right.unsignedValue
        && left.realValue == right.realValue && left.enumValue == right.enumValue;
}

bool sameCapability(const Capability &left, const Capability &right)
{
    return left.subjectKind == right.subjectKind && left.subjectId == right.subjectId
        && left.capabilityId == right.capabilityId && left.supportState == right.supportState
        && left.providerId == right.providerId && left.evidenceCode == right.evidenceCode
        && left.failureCode == right.failureCode && left.unit == right.unit
        && sameValue(left.configuredValue, right.configuredValue)
        && sameValue(left.effectiveValue, right.effectiveValue)
        && sameValue(left.minimum, right.minimum) && sameValue(left.maximum, right.maximum)
        && sameValue(left.step, right.step) && left.allowedValues == right.allowedValues;
}

template <typename T, typename Compare>
bool sameList(const QList<T> &left, const QList<T> &right, Compare compare)
{
    return left.size() == right.size()
        && std::equal(left.cbegin(), left.cend(), right.cbegin(), compare);
}

bool sameInventory(const Hardware1Snapshot &left, const Hardware1Snapshot &right)
{
    return sameList(left.devices, right.devices, sameDevice)
        && sameList(left.deviceInfo, right.deviceInfo, sameInfo);
}

bool sameGraph(const Hardware1Snapshot &left, const Hardware1Snapshot &right)
{
    return sameList(left.capabilities, right.capabilities, sameCapability);
}

bool canonicalConnectorIdentity(const QString &value)
{
    static const QRegularExpression syntax(
        QStringLiteral("^[A-Z0-9]+(?:-[A-Z0-9]+)*-([1-9][0-9]*)$"));
    const auto match = syntax.match(value);
    if (!match.hasMatch()) {
        return false;
    }
    bool ok = false;
    const qulonglong index = match.captured(1).toULongLong(&ok, 10);
    return ok && index <= std::numeric_limits<std::uint32_t>::max()
        && QString::number(index) == match.captured(1);
}

bool canonicalSha256(const QString &value)
{
    static const QRegularExpression syntax(QStringLiteral("^[0-9a-f]{64}$"));
    return syntax.match(value).hasMatch();
}

QString expectedDisplaySubjectId(const QString &gpuSubjectId,
                                 const QString &connectorIdentity,
                                 const QString &edidIdentityDigest)
{
    constexpr auto nameSpace = "adrenalin-linux:subject:DISPLAY:v1";
    const QString material = QStringLiteral("%1|gpu=%2|connector=%3|edid=%4")
        .arg(QString::fromLatin1(nameSpace), gpuSubjectId,
             connectorIdentity, edidIdentityDigest);
    const QByteArray digest = QCryptographicHash::hash(material.toUtf8(),
                                                        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("display.v1.%1").arg(QString::fromLatin1(digest));
}

bool advance(std::uint64_t &generation)
{
    if (generation == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    ++generation;
    return true;
}

} // namespace

namespace {
Hardware1Snapshot composeEvidence(const Hardware1Evidence &evidence)
{
    if (!evidence.gpus.success) {
        return failure(evidence.gpus.error.isEmpty()
            ? QStringLiteral("libdrm GPU identity enumeration failed") : evidence.gpus.error);
    }
    if (!evidence.cpuPackages.success) {
        return failure(evidence.cpuPackages.error.isEmpty()
            ? QStringLiteral("hwloc CPU package enumeration failed") : evidence.cpuPackages.error);
    }
    if (evidence.cpuPackages.packages.isEmpty()) {
        return failure(QStringLiteral("hwloc returned an impossible successful empty CPU package inventory"));
    }
    if (!evidence.displays.success) {
        return failure(evidence.displays.error.isEmpty()
            ? QStringLiteral("libdrm display enumeration failed") : evidence.displays.error);
    }

    Hardware1Snapshot result;
    result.success = true;
    QSet<QString> subjects;
    QSet<QString> gpuSubjects;
    const auto addDevice = [&result, &subjects](const QString &kind, const QString &id,
                                               const QString &identityEvidence,
                                               const QString &name) -> bool {
        const QString key = kind + QLatin1Char(':') + id;
        if (id.isEmpty() || subjects.contains(key)) {
            return false;
        }
        subjects.insert(key);
        Device device;
        device.subjectKind = kind;
        device.subjectId = id;
        device.identityEvidence = identityEvidence;
        device.displayName = name;
        result.devices.push_back(device);
        DeviceInfo info;
        info.subjectKind = kind;
        info.subjectId = id;
        info.identityEvidence = identityEvidence;
        info.displayName = name;
        result.deviceInfo.push_back(info);
        return true;
    };

    for (const PciGpuIdentity &pciIdentity : evidence.gpus.devices) {
        const auto identity = gpuSubjectIdentity(pciIdentity);
        if (!identity || !addDevice(QStringLiteral("GPU_PCI"), identity->subjectId,
                                    QStringLiteral("evidence.libdrm.pci_identity"),
                                    QStringLiteral("AMD GPU"))) {
            return failure(QStringLiteral("GPU identity evidence is invalid or duplicated"));
        }
        gpuSubjects.insert(identity->subjectId);
        result.deviceInfo.last().manufacturer = QStringLiteral("AMD");
    }

    for (const CpuPackageIdentity &cpu : evidence.cpuPackages.packages) {
        if (!addDevice(QStringLiteral("CPU_PACKAGE"), cpu.subjectId,
                       QStringLiteral("evidence.hwloc.package_topology"),
                       QStringLiteral("CPU package"))) {
            return failure(QStringLiteral("CPU package identity evidence is invalid or duplicated"));
        }
        DeviceInfo &info = result.deviceInfo.last();
        info.manufacturer = cpu.vendorId;
        info.model = QStringLiteral("Family %1 Model %2")
            .arg(static_cast<qulonglong>(cpu.family))
            .arg(static_cast<qulonglong>(cpu.model));
    }

    for (const DrmDisplayIdentity &display : evidence.displays.displays) {
        if (!gpuSubjects.contains(display.gpuSubjectId)
            || display.subjectKind != QLatin1String("DISPLAY")
            || !canonicalConnectorIdentity(display.connectorIdentity)
            || !canonicalSha256(display.edidIdentityDigest)
            || display.subjectId != expectedDisplaySubjectId(
                display.gpuSubjectId, display.connectorIdentity, display.edidIdentityDigest)
            || !addDevice(QStringLiteral("DISPLAY"), display.subjectId,
                          QStringLiteral("evidence.libdrm.connector_edid_identity"),
                          QStringLiteral("Display"))) {
            return failure(QStringLiteral("display evidence references an absent GPU or has invalid identity"));
        }
        DeviceInfo &info = result.deviceInfo.last();
        info.connectorIdentity = display.connectorIdentity;
        info.edidIdentityDigest = display.edidIdentityDigest;
    }

    const auto &registry = capabilityRegistryV1();
    for (const Device &device : result.devices) {
        for (const CapabilityDefinition &definition : registry) {
            if (!definition.subjectKinds.contains(device.subjectKind)) {
                continue;
            }
            Capability capability;
            capability.subjectKind = device.subjectKind;
            capability.subjectId = device.subjectId;
            capability.capabilityId = definition.id;
            capability.supportState = QStringLiteral("UNKNOWN");
            QString validationError;
            if (!capability.isValid(&validationError)) {
                return failure(QStringLiteral("registered capability graph is invalid: %1")
                                   .arg(validationError));
            }
            result.capabilities.push_back(std::move(capability));
        }
    }
    for (const CapabilityDefinition &definition : registry) {
        if (!definition.subjectKinds.contains(QStringLiteral("PLATFORM"))) {
            continue;
        }
        Capability capability;
        capability.subjectKind = QStringLiteral("PLATFORM");
        capability.subjectId = QStringLiteral("platform");
        capability.capabilityId = definition.id;
        capability.supportState = QStringLiteral("UNKNOWN");
        if (!capability.isValid()) {
            return failure(QStringLiteral("reserved platform capability is invalid"));
        }
        result.capabilities.push_back(std::move(capability));
    }
    std::sort(result.devices.begin(), result.devices.end(), [](const Device &a, const Device &b) {
        return a.subjectKind == b.subjectKind ? a.subjectId < b.subjectId : a.subjectKind < b.subjectKind;
    });
    std::sort(result.deviceInfo.begin(), result.deviceInfo.end(), [](const DeviceInfo &a, const DeviceInfo &b) {
        return a.subjectKind == b.subjectKind ? a.subjectId < b.subjectId : a.subjectKind < b.subjectKind;
    });
    std::sort(result.capabilities.begin(), result.capabilities.end(), [](const Capability &a, const Capability &b) {
        if (a.subjectKind != b.subjectKind) return a.subjectKind < b.subjectKind;
        if (a.subjectId != b.subjectId) return a.subjectId < b.subjectId;
        return a.capabilityId < b.capabilityId;
    });
    return result;
}
} // namespace

#ifdef ADRENALIN_HARDWARE1_INVENTORY_TESTING
Hardware1Snapshot LinuxHardware1InventoryProvider::composeEvidenceForTesting(
    const Hardware1Evidence &evidence)
{
    return composeEvidence(evidence);
}
#endif

Hardware1Snapshot LinuxHardware1InventoryProvider::refresh()
{
    Hardware1Evidence evidence;
    evidence.gpus = LinuxDrmInventorySource::enumerate();
    if (evidence.gpus.success) {
        evidence.cpuPackages = LinuxCpuPackageSource::enumerate();
        if (evidence.cpuPackages.success) {
            evidence.displays = LinuxDrmDisplaySource::enumerate();
        }
    }
    return publish(composeEvidence(evidence));
}

Hardware1Snapshot LinuxHardware1InventoryProvider::publish(Hardware1Snapshot candidate)
{
    const Hardware1Snapshot empty;
    const Hardware1Snapshot &next = candidate.success ? candidate : empty;
    const bool firstSuccessfulReconciliation = !current_.success && candidate.success;
    const bool inventoryChanged = firstSuccessfulReconciliation || !sameInventory(current_, next);
    const bool graphChanged = firstSuccessfulReconciliation || !sameGraph(current_, next);
    if ((inventoryChanged && inventoryGeneration_ == std::numeric_limits<std::uint64_t>::max())
        || (graphChanged && capabilityGeneration_ == std::numeric_limits<std::uint64_t>::max())) {
        current_ = empty;
        return failure(QStringLiteral("Hardware1 snapshot generation exhausted"));
    }
    if (inventoryChanged) advance(inventoryGeneration_);
    if (graphChanged) advance(capabilityGeneration_);
    current_ = next;
    if (!candidate.success) {
        return candidate;
    }
    current_.inventoryGeneration = inventoryGeneration_;
    current_.capabilityGeneration = capabilityGeneration_;
    return current_;
}

#ifdef ADRENALIN_HARDWARE1_INVENTORY_TESTING
Hardware1Snapshot LinuxHardware1InventoryProvider::refreshWithEvidenceForTesting(
    const Hardware1Evidence &evidence)
{
    return publish(composeEvidence(evidence));
}
#endif

Hardware1Snapshot LinuxHardware1InventoryProvider::snapshot() const
{
    return current_;
}

} // namespace adrenalin::hardware
