#include "linux_cpu_package_source.h"

#include <QCryptographicHash>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cstring>
#include <limits>

#include <hwloc.h>

namespace adrenalin::hardware {
namespace {

constexpr int kMaximumCpuPackages = 256;
constexpr auto kUnknownVendor = "unknown";
constexpr auto kUnknownArchitecture = "unknown";

struct RawPackageEvidence {
    bool packageObject = false;
    bool topologyEvidencePresent = false;
    unsigned physicalPackageId = HWLOC_UNKNOWN_INDEX;
    QString architecture;
    QString vendorId;
    std::optional<std::uint32_t> family;
    std::optional<std::uint32_t> model;
    std::optional<std::uint32_t> stepping;
};

struct RawProcessorEvidence {
    unsigned processorOsIndex = HWLOC_UNKNOWN_INDEX;
    int packageAncestorCount = 0;
    std::optional<unsigned> packageOsIndex;
};

struct RawInventory {
    bool success = false;
    int reportedCount = 0;
    QVector<RawPackageEvidence> packages;
    int reportedOnlineProcessorCount = 0;
    QVector<RawProcessorEvidence> onlineProcessors;
};

CpuPackageInventoryResult failure(const QString &message)
{
    CpuPackageInventoryResult result;
    result.error = message;
    return result;
}

bool isSafeIdentityAtom(const QString &value)
{
    if (value.isEmpty() || value.size() > 64
        || value.compare(QString::fromLatin1(kUnknownVendor), Qt::CaseInsensitive) == 0
        || value.compare(QString::fromLatin1(kUnknownArchitecture), Qt::CaseInsensitive) == 0) {
        return false;
    }
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (!((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
              || (code >= '0' && code <= '9') || code == '_' || code == '-' || code == '.')) {
            return false;
        }
    }
    return true;
}

QByteArray canonicalArchitecture(const QString &architecture)
{
    return architecture.toLower().toLatin1();
}

void appendU32(QByteArray &output, std::uint32_t value)
{
    output.append(static_cast<char>((value >> 24) & 0xff));
    output.append(static_cast<char>((value >> 16) & 0xff));
    output.append(static_cast<char>((value >> 8) & 0xff));
    output.append(static_cast<char>(value & 0xff));
}

void appendString(QByteArray &output, const QByteArray &value)
{
    appendU32(output, static_cast<std::uint32_t>(value.size()));
    output.append(value);
}

QString makeSubjectId(const RawPackageEvidence &evidence)
{
    QByteArray canonical;
    appendString(canonical, QByteArrayLiteral("org.adrenalinlinux.cpu-package.v1"));
    appendString(canonical, canonicalArchitecture(evidence.architecture));
    appendString(canonical, evidence.vendorId.toLatin1());
    appendU32(canonical, *evidence.family);
    appendU32(canonical, *evidence.model);
    appendU32(canonical, *evidence.stepping);
    appendU32(canonical, evidence.physicalPackageId);
    return QStringLiteral("cpu-package-")
        + QString::fromLatin1(QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex());
}

CpuPackageInventoryResult validateEvidence(bool enumerationSucceeded, int reportedCount,
                                           const QVector<RawPackageEvidence> &packages,
                                           int reportedOnlineProcessorCount,
                                           const QVector<RawProcessorEvidence> &onlineProcessors)
{
    if (!enumerationSucceeded) {
        return failure(QStringLiteral("hwloc CPU package topology enumeration failed"));
    }
    if (reportedCount <= 0 || reportedCount > kMaximumCpuPackages
        || reportedCount != packages.size()) {
        return failure(QStringLiteral("hwloc returned an invalid CPU package count"));
    }

    CpuPackageInventoryResult result;
    result.success = true;
    QSet<unsigned> packageIds;
    QSet<QString> subjectIds;
    result.packages.reserve(packages.size());
    for (const RawPackageEvidence &package : packages) {
        if (!package.packageObject || !package.topologyEvidencePresent
            || package.physicalPackageId == HWLOC_UNKNOWN_INDEX
            || package.physicalPackageId > static_cast<unsigned>(std::numeric_limits<int>::max())
            || !isSafeIdentityAtom(package.architecture)
            || !isSafeIdentityAtom(package.vendorId)
            || !package.family.has_value() || *package.family == 0
            || !package.model.has_value() || !package.stepping.has_value()) {
            return failure(QStringLiteral("hwloc returned incomplete CPU package identity evidence"));
        }
        if (packageIds.contains(package.physicalPackageId)) {
            return failure(QStringLiteral("hwloc returned duplicate physical package identity evidence"));
        }
        packageIds.insert(package.physicalPackageId);

        CpuPackageIdentity identity;
        identity.subjectId = makeSubjectId(package);
        if (subjectIds.contains(identity.subjectId)) {
            return failure(QStringLiteral("CPU package identity token collision or contradictory evidence"));
        }
        subjectIds.insert(identity.subjectId);
        identity.physicalPackageId = package.physicalPackageId;
        identity.architecture = package.architecture.toLower();
        identity.vendorId = package.vendorId;
        identity.family = *package.family;
        identity.model = *package.model;
        identity.stepping = *package.stepping;
        result.packages.push_back(std::move(identity));
    }

    if (reportedOnlineProcessorCount <= 0
        || reportedOnlineProcessorCount != onlineProcessors.size()) {
        return failure(QStringLiteral("hwloc returned incomplete online CPU package coverage"));
    }
    QSet<unsigned> processorOsIndices;
    for (const RawProcessorEvidence &processor : onlineProcessors) {
        if (processor.processorOsIndex == HWLOC_UNKNOWN_INDEX
            || processor.packageAncestorCount != 1 || !processor.packageOsIndex.has_value()
            || !packageIds.contains(*processor.packageOsIndex)
            || processorOsIndices.contains(processor.processorOsIndex)) {
            return failure(QStringLiteral("an online CPU lacks one unambiguous enumerated package ancestor"));
        }
        processorOsIndices.insert(processor.processorOsIndex);
    }
    std::sort(result.packages.begin(), result.packages.end(),
              [](const CpuPackageIdentity &left, const CpuPackageIdentity &right) {
                  return left.subjectId < right.subjectId;
              });
    return result;
}

struct InfoValue {
    bool valid = true;
    QString value;
};

InfoValue uniqueInfoValue(hwloc_obj_t object, const char *name)
{
    InfoValue result;
    bool found = false;
    for (unsigned index = 0; index < object->infos_count; ++index) {
        const hwloc_info_s &info = object->infos[index];
        if (info.name == nullptr || std::strcmp(info.name, name) != 0) {
            continue;
        }
        if (found || info.value == nullptr) {
            result.valid = false;
            return result;
        }
        result.value = QString::fromLatin1(info.value);
        found = true;
    }
    if (!found || result.value.isEmpty()) {
        result.valid = false;
    }
    return result;
}

std::optional<std::uint32_t> parseUnsignedInfo(const QString &value)
{
    if (value.isEmpty()) {
        return std::nullopt;
    }
    std::uint32_t parsed = 0;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) {
            return std::nullopt;
        }
        const std::uint32_t digit = static_cast<std::uint32_t>(character.unicode() - '0');
        if (parsed > (std::numeric_limits<std::uint32_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        parsed = parsed * 10 + digit;
    }
    return parsed;
}

RawInventory enumerateWithHwloc()
{
    RawInventory raw;
    hwloc_topology_t topology = nullptr;
    if (hwloc_topology_init(&topology) < 0) {
        return raw;
    }
    if (hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_INCLUDE_DISALLOWED) < 0
        || hwloc_topology_load(topology) < 0) {
        hwloc_topology_destroy(topology);
        return raw;
    }

    const hwloc_obj_t machine = hwloc_get_root_obj(topology);
    const InfoValue architecture = machine == nullptr
        ? InfoValue{false, {}} : uniqueInfoValue(machine, "Architecture");
    const int packageCount = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PACKAGE);
    if (!architecture.valid || !isSafeIdentityAtom(architecture.value)
        || machine == nullptr || machine->cpuset == nullptr
        || hwloc_bitmap_iszero(machine->cpuset)
        || packageCount <= 0 || packageCount > kMaximumCpuPackages) {
        hwloc_topology_destroy(topology);
        return raw;
    }

    raw.reportedCount = packageCount;
    raw.packages.reserve(packageCount);
    for (int index = 0; index < packageCount; ++index) {
        const hwloc_obj_t current = hwloc_get_obj_by_type(
            topology, HWLOC_OBJ_PACKAGE, static_cast<unsigned>(index));
        if (current == nullptr || current->cpuset == nullptr
            || hwloc_bitmap_iszero(current->cpuset)) {
            hwloc_topology_destroy(topology);
            return RawInventory{};
        }
        for (int previousIndex = 0; previousIndex < index; ++previousIndex) {
            const hwloc_obj_t previous = hwloc_get_obj_by_type(
                topology, HWLOC_OBJ_PACKAGE, static_cast<unsigned>(previousIndex));
            if (previous == nullptr || previous->cpuset == nullptr
                || hwloc_bitmap_intersects(current->cpuset, previous->cpuset)) {
                hwloc_topology_destroy(topology);
                return RawInventory{};
            }
        }
    }
    QHash<unsigned, hwloc_obj_t> packageObjectsByOsIndex;
    for (int index = 0; index < packageCount; ++index) {
        const hwloc_obj_t object = hwloc_get_obj_by_type(
            topology, HWLOC_OBJ_PACKAGE, static_cast<unsigned>(index));
        if (object == nullptr || object->os_index == HWLOC_UNKNOWN_INDEX
            || object->cpuset == nullptr
            || packageObjectsByOsIndex.contains(object->os_index)) {
            hwloc_topology_destroy(topology);
            return RawInventory{};
        }
        packageObjectsByOsIndex.insert(object->os_index, object);
        RawPackageEvidence evidence;
        evidence.packageObject = object->type == HWLOC_OBJ_PACKAGE;
        evidence.topologyEvidencePresent = object->cpuset != nullptr
            && !hwloc_bitmap_iszero(object->cpuset);
        evidence.physicalPackageId = object->os_index;
        evidence.architecture = architecture.value;
        const InfoValue vendor = uniqueInfoValue(object, "CPUVendor");
        const InfoValue family = uniqueInfoValue(object, "CPUFamilyNumber");
        const InfoValue model = uniqueInfoValue(object, "CPUModelNumber");
        const InfoValue stepping = uniqueInfoValue(object, "CPUStepping");
        if (vendor.valid) {
            evidence.vendorId = vendor.value;
        }
        if (family.valid) {
            evidence.family = parseUnsignedInfo(family.value);
        }
        if (model.valid) {
            evidence.model = parseUnsignedInfo(model.value);
        }
        if (stepping.valid) {
            evidence.stepping = parseUnsignedInfo(stepping.value);
        }
        raw.packages.push_back(std::move(evidence));
    }

    const int onlineProcessorCount = hwloc_bitmap_weight(machine->cpuset);
    if (onlineProcessorCount <= 0) {
        hwloc_topology_destroy(topology);
        return RawInventory{};
    }
    raw.reportedOnlineProcessorCount = onlineProcessorCount;
    raw.onlineProcessors.reserve(onlineProcessorCount);
    for (int processorOsIndex = hwloc_bitmap_first(machine->cpuset);
         processorOsIndex != -1;
         processorOsIndex = hwloc_bitmap_next(machine->cpuset, processorOsIndex)) {
        if (processorOsIndex < 0) {
            hwloc_topology_destroy(topology);
            return RawInventory{};
        }
        const hwloc_obj_t processor = hwloc_get_pu_obj_by_os_index(
            topology, static_cast<unsigned>(processorOsIndex));
        if (processor == nullptr || processor->type != HWLOC_OBJ_PU) {
            hwloc_topology_destroy(topology);
            return RawInventory{};
        }
        int packageAncestorCount = 0;
        hwloc_obj_t packageAncestor = nullptr;
        for (hwloc_obj_t ancestor = processor->parent; ancestor != nullptr;
             ancestor = ancestor->parent) {
            if (ancestor->type == HWLOC_OBJ_PACKAGE) {
                ++packageAncestorCount;
                packageAncestor = ancestor;
            }
        }
        if (packageAncestorCount != 1 || packageAncestor == nullptr
            || !packageObjectsByOsIndex.contains(packageAncestor->os_index)
            || !hwloc_bitmap_isset(packageAncestor->cpuset,
                                   static_cast<unsigned>(processorOsIndex))) {
            hwloc_topology_destroy(topology);
            return RawInventory{};
        }
        raw.onlineProcessors.push_back({static_cast<unsigned>(processorOsIndex),
                                        packageAncestorCount, packageAncestor->os_index});
    }
    hwloc_topology_destroy(topology);
    raw.success = true;
    return raw;
}

} // namespace

CpuPackageInventoryResult LinuxCpuPackageSource::enumerate()
{
    const RawInventory raw = enumerateWithHwloc();
    return validateEvidence(raw.success, raw.reportedCount, raw.packages,
                            raw.reportedOnlineProcessorCount, raw.onlineProcessors);
}

#ifdef ADRENALIN_CPU_PACKAGE_SOURCE_TESTING
CpuPackageInventoryResult LinuxCpuPackageSource::enumerateForTesting(
    const TestEnumerator &enumerator)
{
    if (!enumerator) {
        return failure(QStringLiteral("test enumerator is empty"));
    }
    const RawEnumeration raw = enumerator();
    QVector<RawPackageEvidence> packages;
    packages.reserve(raw.packages.size());
    for (const RawPackage &package : raw.packages) {
        RawPackageEvidence evidence;
        evidence.packageObject = package.packageObject;
        evidence.topologyEvidencePresent = package.topologyEvidencePresent;
        evidence.physicalPackageId = package.physicalPackageId.value_or(HWLOC_UNKNOWN_INDEX);
        evidence.architecture = package.architecture;
        evidence.vendorId = package.vendorId;
        evidence.family = package.family;
        evidence.model = package.model;
        evidence.stepping = package.stepping;
        packages.push_back(std::move(evidence));
    }
    QVector<RawProcessorEvidence> onlineProcessors;
    int reportedOnlineProcessorCount = raw.reportedOnlineProcessorCount;
    if (reportedOnlineProcessorCount < 0) {
        onlineProcessors.reserve(raw.packages.size());
        unsigned processorOsIndex = 0;
        for (const RawPackage &package : raw.packages) {
            if (package.physicalPackageId.has_value()) {
                onlineProcessors.push_back({processorOsIndex++, 1, package.physicalPackageId});
            }
        }
        reportedOnlineProcessorCount = onlineProcessors.size();
    } else {
        onlineProcessors.reserve(raw.onlineProcessors.size());
        for (const RawProcessor &processor : raw.onlineProcessors) {
            onlineProcessors.push_back({processor.processorOsIndex,
                                        processor.packageAncestorCount,
                                        processor.packageOsIndex});
        }
    }
    return validateEvidence(raw.success, raw.reportedCount, packages,
                            reportedOnlineProcessorCount, onlineProcessors);
}
#endif

} // namespace adrenalin::hardware
