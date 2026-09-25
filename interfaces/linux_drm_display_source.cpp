#include "linux_drm_display_source.h"

#include "linux_drm_inventory_source.h"
#include "pci_gpu_subject_identity.h"

#include <QCryptographicHash>
#include <QSet>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace adrenalin::hardware {
namespace {

constexpr int kMaximumDrmRecords = 256;
constexpr int kMaximumEdidBytes = 128 * 33;
constexpr std::uint32_t kAmdVendorId = 0x1002;
constexpr char kDisplayNamespace[] = "adrenalin-linux:subject:DISPLAY:v1";
constexpr std::array<unsigned char, 8> kEdidHeader{
    {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}};

struct InternalRawConnector final {
  QString gpuSubjectId;
  QString connectorType;
  std::uint32_t connectorTypeId = 0;
  bool connected = false;
  QByteArray edid;
};

struct UniqueFd final {
  int value = -1;
  ~UniqueFd() {
    if (value >= 0)
      ::close(value);
  }
};

struct DrmDeviceList final {
  std::array<drmDevicePtr, kMaximumDrmRecords> devices{};
  ~DrmDeviceList() {
    for (drmDevicePtr &device : devices) {
      if (device != nullptr) {
        drmFreeDevice(&device);
      }
    }
  }
};

template <typename Pointer, void (*Free)(Pointer)> struct DrmObject final {
  Pointer value = nullptr;
  ~DrmObject() {
    if (value != nullptr)
      Free(value);
  }
  Pointer get() const { return value; }
};

using Resources = DrmObject<drmModeResPtr, drmModeFreeResources>;
using Connector = DrmObject<drmModeConnectorPtr, drmModeFreeConnector>;
using ObjectProperties =
    DrmObject<drmModeObjectPropertiesPtr, drmModeFreeObjectProperties>;
using Property = DrmObject<drmModePropertyPtr, drmModeFreeProperty>;
using PropertyBlob = DrmObject<drmModePropertyBlobPtr, drmModeFreePropertyBlob>;

bool validConnectorResourceArray(int count, const std::uint32_t *connectors) {
  return count >= 0 && count <= kMaximumDrmRecords &&
         (count == 0 || connectors != nullptr);
}

bool validPropertyArrays(std::uint32_t count, const std::uint32_t *properties,
                         const std::uint64_t *values) {
  return count <= 4096 &&
         (count == 0 || (properties != nullptr && values != nullptr));
}

bool validPropertyName(const char *name, std::size_t capacity) {
  return name != nullptr && capacity > 0 && name[0] != '\0' &&
         std::memchr(name, '\0', capacity) != nullptr;
}

DrmDisplayInventoryResult failure(const QString &message) {
  DrmDisplayInventoryResult result;
  result.error = message;
  return result;
}

QString canonicalConnectorIdentity(const QString &type, std::uint32_t typeId) {
  if (typeId == 0 || type.isEmpty() || type.size() > 32) {
    return {};
  }
  QString canonical;
  canonical.reserve(type.size() + 5);
  bool previousDash = true;
  for (const QChar input : type) {
    const ushort code = input.unicode();
    QChar output;
    if ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')) {
      output = QChar(code >= 'a' ? code - ('a' - 'A') : code);
      previousDash = false;
    } else if (code >= '0' && code <= '9') {
      output = input;
      previousDash = false;
    } else if (input == QLatin1Char('-') && !previousDash) {
      output = input;
      previousDash = true;
    } else {
      return {};
    }
    canonical.append(output);
  }
  if (previousDash) {
    return {};
  }
  canonical.append(QLatin1Char('-'));
  canonical.append(QString::number(typeId));
  return canonical;
}

bool validEdid(const QByteArray &edid) {
  if (edid.size() < 128 || edid.size() > kMaximumEdidBytes ||
      edid.size() % 128 != 0) {
    return false;
  }
  const auto *bytes = reinterpret_cast<const unsigned char *>(edid.constData());
  if (!std::equal(kEdidHeader.cbegin(), kEdidHeader.cend(), bytes)) {
    return false;
  }
  const int expectedBlocks = static_cast<int>(bytes[126]) + 1;
  if (expectedBlocks * 128 != edid.size()) {
    return false;
  }
  for (int block = 0; block < expectedBlocks; ++block) {
    unsigned int sum = 0;
    for (int index = 0; index < 128; ++index) {
      sum += bytes[block * 128 + index];
    }
    if ((sum & 0xffU) != 0) {
      return false;
    }
  }
  // EDID manufacturer code is three 5-bit letters. All-zero and reserved
  // values cannot provide safe identity evidence.
  const std::uint16_t manufacturer =
      (static_cast<std::uint16_t>(bytes[8]) << 8U) | bytes[9];
  for (int shift : {10, 5, 0}) {
    const auto letter = static_cast<unsigned>((manufacturer >> shift) & 0x1fU);
    if (letter == 0 || letter > 26) {
      return false;
    }
  }
  return true;
}

QString canonicalEdidIdentityDigest(const QByteArray &edid) {
  if (!validEdid(edid)) {
    return {};
  }
  const auto *bytes = reinterpret_cast<const unsigned char *>(edid.constData());
  // Identity uses only EDID manufacturer, product, and manufacture-date
  // fields. It intentionally excludes the four-byte serial and all text
  // descriptors and extension payloads.
  QByteArray identity;
  identity.reserve(11);
  identity.append("edid-identity-v1", 16);
  identity.append(static_cast<char>(bytes[8]));
  identity.append(static_cast<char>(bytes[9]));
  identity.append(static_cast<char>(bytes[10]));
  identity.append(static_cast<char>(bytes[11]));
  identity.append(static_cast<char>(bytes[16]));
  identity.append(static_cast<char>(bytes[17]));
  return QString::fromLatin1(
      QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
}

bool isCanonicalGpuSubjectId(const QString &subjectId) {
  constexpr qsizetype prefixSize = 11; // "gpu.pci.v1."
  if (subjectId.size() != prefixSize + 64 ||
      !subjectId.startsWith(QStringLiteral("gpu.pci.v1."))) {
    return false;
  }
  for (qsizetype index = prefixSize; index < subjectId.size(); ++index) {
    const QChar character = subjectId.at(index);
    if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
          (character >= QLatin1Char('a') && character <= QLatin1Char('f')))) {
      return false;
    }
  }
  return true;
}

DrmDisplayInventoryResult
resolveConnectors(bool enumerationSucceeded, int reportedCount,
                  const QVector<InternalRawConnector> &connectors) {
  if (!enumerationSucceeded) {
    return failure(QStringLiteral("DRM connector enumeration failed"));
  }
  if (reportedCount < 0 || reportedCount > kMaximumDrmRecords ||
      reportedCount != connectors.size()) {
    return failure(
        QStringLiteral("DRM connector enumeration returned an invalid count"));
  }

  DrmDisplayInventoryResult result;
  result.success = true;
  QSet<QString> observedConnectors;
  QSet<QString> subjectIds;
  for (const InternalRawConnector &raw : connectors) {
    // Disconnected connector observations are not DISPLAY subjects.
    if (!raw.connected) {
      continue;
    }
    if (!isCanonicalGpuSubjectId(raw.gpuSubjectId)) {
      return failure(
          QStringLiteral("Connected DRM display has no resolved owning GPU"));
    }
    const QString connector =
        canonicalConnectorIdentity(raw.connectorType, raw.connectorTypeId);
    const QString edidDigest = canonicalEdidIdentityDigest(raw.edid);
    if (connector.isEmpty() || edidDigest.isEmpty()) {
      return failure(
          QStringLiteral("Connected DRM display identity evidence is invalid"));
    }
    const QString connectorKey =
        raw.gpuSubjectId + QLatin1Char('|') + connector;
    if (observedConnectors.contains(connectorKey)) {
      return failure(QStringLiteral(
          "Duplicate DRM connector identity evidence is ambiguous"));
    }
    observedConnectors.insert(connectorKey);

    const QString material = QStringLiteral("%1|gpu=%2|connector=%3|edid=%4")
                                 .arg(QString::fromLatin1(kDisplayNamespace),
                                      raw.gpuSubjectId, connector, edidDigest);
    const QString subjectId =
        QStringLiteral("display.v1.%1")
            .arg(QString::fromLatin1(
                QCryptographicHash::hash(material.toUtf8(),
                                         QCryptographicHash::Sha256)
                    .toHex()));
    if (subjectIds.contains(subjectId)) {
      return failure(QStringLiteral(
          "Duplicate DRM display subject identity is ambiguous"));
    }
    subjectIds.insert(subjectId);
    result.displays.push_back({QStringLiteral("DISPLAY"), subjectId,
                               raw.gpuSubjectId, connector, edidDigest});
  }
  std::sort(
      result.displays.begin(), result.displays.end(),
      [](const DrmDisplayIdentity &left, const DrmDisplayIdentity &right) {
        return left.subjectId < right.subjectId;
      });
  return result;
}

std::optional<QByteArray> readEdidBlob(int fd,
                                       const drmModeConnectorPtr connector) {
  ObjectProperties objectProperties{drmModeObjectGetProperties(
      fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR)};
  if (objectProperties.get() == nullptr ||
      !validPropertyArrays(objectProperties.get()->count_props,
                           objectProperties.get()->props,
                           objectProperties.get()->prop_values)) {
    return std::nullopt;
  }

  std::optional<std::uint64_t> edidBlobId;
  for (std::uint32_t index = 0; index < objectProperties.get()->count_props;
       ++index) {
    Property property{
        drmModeGetProperty(fd, objectProperties.get()->props[index])};
    if (property.get() == nullptr ||
        !validPropertyName(property.get()->name,
                           sizeof(property.get()->name))) {
      return std::nullopt;
    }
    if (std::strcmp(property.get()->name, "EDID") == 0) {
      if ((property.get()->flags & DRM_MODE_PROP_BLOB) == 0 ||
          edidBlobId.has_value()) {
        return std::nullopt;
      }
      edidBlobId = objectProperties.get()->prop_values[index];
    }
  }
  if (!edidBlobId.has_value() || *edidBlobId == 0 ||
      *edidBlobId > std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }

  PropertyBlob blob{
      drmModeGetPropertyBlob(fd, static_cast<std::uint32_t>(*edidBlobId))};
  if (blob.get() == nullptr || blob.get()->data == nullptr ||
      blob.get()->length > static_cast<std::uint32_t>(kMaximumEdidBytes)) {
    return std::nullopt;
  }
  // Only a temporary copy is made for field extraction and hashing.
  return QByteArray(static_cast<const char *>(blob.get()->data),
                    static_cast<qsizetype>(blob.get()->length));
}

QString connectorTypeName(std::uint32_t type) {
  const char *name = drmModeGetConnectorTypeName(type);
  return name == nullptr ? QString{} : QString::fromLatin1(name);
}

DrmDisplayInventoryResult enumerateFromLibdrm() {
  const int count = drmGetDevices2(0, nullptr, 0);
  if (count < 0 || count > kMaximumDrmRecords) {
    return failure(QStringLiteral(
        "libdrm device enumeration failed or exceeded its bound"));
  }
  if (count == 0) {
    DrmDisplayInventoryResult empty;
    empty.success = true;
    return empty;
  }

  DrmDeviceList devices;
  const int found = drmGetDevices2(0, devices.devices.data(), count);
  if (found < 0 || found != count) {
    return failure(
        QStringLiteral("libdrm device inventory changed during enumeration"));
  }

  QVector<InternalRawConnector> observations;
  QSet<QString> gpuSubjects;
  for (int deviceIndex = 0; deviceIndex < found; ++deviceIndex) {
    const drmDevicePtr device =
        devices.devices[static_cast<std::size_t>(deviceIndex)];
    if (device == nullptr) {
      return failure(QStringLiteral("libdrm returned a null device record"));
    }
    if (device->bustype != DRM_BUS_PCI) {
      continue;
    }
    if (device->businfo.pci == nullptr || device->deviceinfo.pci == nullptr) {
      return failure(QStringLiteral("libdrm returned malformed PCI identity data"));
    }
    if (device->deviceinfo.pci->vendor_id != kAmdVendorId) {
      continue;
    }
    const auto *bus = device->businfo.pci;
    const auto *pciDevice = device->deviceinfo.pci;
    if (bus->domain > std::numeric_limits<std::uint16_t>::max() ||
        bus->dev > 0x1f || bus->func > 0x7 ||
        pciDevice->device_id == 0 ||
        pciDevice->device_id > std::numeric_limits<std::uint16_t>::max()) {
      return failure(QStringLiteral(
          "AMD DRM device returned malformed PCI identity data"));
    }
    PciGpuIdentity pci;
    pci.vendorId =
        static_cast<std::uint16_t>(device->deviceinfo.pci->vendor_id);
    pci.deviceId =
        static_cast<std::uint16_t>(device->deviceinfo.pci->device_id);
    pci.canonicalBdf =
        QStringLiteral("%1:%2:%3.%4")
            .arg(device->businfo.pci->domain, 4, 16, QLatin1Char('0'))
            .arg(device->businfo.pci->bus, 2, 16, QLatin1Char('0'))
            .arg(device->businfo.pci->dev, 2, 16, QLatin1Char('0'))
            .arg(device->businfo.pci->func, 1, 16, QLatin1Char('0'))
            .toLower();
    const auto gpu = gpuSubjectIdentity(pci);
    if (!gpu.has_value() || gpuSubjects.contains(gpu->subjectId) ||
        device->nodes == nullptr || device->available_nodes == 0 ||
        (device->available_nodes & (1 << DRM_NODE_PRIMARY)) == 0 ||
        device->nodes[DRM_NODE_PRIMARY] == nullptr) {
      return failure(QStringLiteral("AMD DRM device lacks valid or unique "
                                    "primary-node identity evidence"));
    }
    gpuSubjects.insert(gpu->subjectId);

    UniqueFd node{
        ::open(device->nodes[DRM_NODE_PRIMARY], O_RDONLY | O_CLOEXEC)};
    if (node.value < 0) {
      return failure(QStringLiteral(
          "AMD DRM primary node is unavailable for read-only inventory"));
    }
    Resources resources{drmModeGetResources(node.value)};
    if (resources.get() == nullptr ||
        !validConnectorResourceArray(resources.get()->count_connectors,
                                     resources.get()->connectors)) {
      return failure(
          QStringLiteral("AMD DRM connector resource enumeration failed"));
    }
    for (int connectorIndex = 0;
         connectorIndex < resources.get()->count_connectors; ++connectorIndex) {
      Connector connector{drmModeGetConnectorCurrent(
          node.value, resources.get()->connectors[connectorIndex])};
      if (connector.get() == nullptr) {
        return failure(QStringLiteral("AMD DRM connector state query failed"));
      }
      if (connector.get()->connection != DRM_MODE_CONNECTED) {
        continue;
      }
      const auto edid = readEdidBlob(node.value, connector.get());
      if (!edid.has_value()) {
        return failure(QStringLiteral(
            "Connected DRM connector EDID identity is unavailable"));
      }
      observations.push_back(
          {gpu->subjectId, connectorTypeName(connector.get()->connector_type),
           connector.get()->connector_type_id, true, *edid});
      if (observations.size() > kMaximumDrmRecords) {
        return failure(QStringLiteral(
            "AMD DRM display count exceeds its supported bound"));
      }
    }
  }
  return resolveConnectors(true, observations.size(), observations);
}

} // namespace

DrmDisplayInventoryResult LinuxDrmDisplaySource::enumerate() {
  return enumerateFromLibdrm();
}

#ifdef ADRENALIN_DRM_DISPLAY_SOURCE_TESTING
DrmDisplayInventoryResult
LinuxDrmDisplaySource::enumerateForTesting(const TestEnumerator &enumerator) {
  if (!enumerator) {
    return failure(QStringLiteral("test enumerator is empty"));
  }
  const RawEnumeration raw = enumerator();
  QVector<InternalRawConnector> records;
  records.reserve(raw.connectors.size());
  for (const LinuxDrmDisplaySource::RawConnector &connector : raw.connectors) {
    records.push_back({connector.gpuSubjectId, connector.connectorType,
                       connector.connectorTypeId, connector.connected,
                       connector.edid});
  }
  return resolveConnectors(raw.success, raw.reportedCount, records);
}

QString
LinuxDrmDisplaySource::normalizeConnectorIdentity(const QString &type,
                                                  std::uint32_t typeId) {
  return canonicalConnectorIdentity(type, typeId);
}

QString LinuxDrmDisplaySource::edidIdentityDigest(const QByteArray &edid) {
  return canonicalEdidIdentityDigest(edid);
}

bool LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
    int count, const std::uint32_t *connectors) {
  return validConnectorResourceArray(count, connectors);
}

bool LinuxDrmDisplaySource::validPropertyArraysForTesting(
    std::uint32_t count, const std::uint32_t *properties,
    const std::uint64_t *values) {
  return validPropertyArrays(count, properties, values);
}

bool LinuxDrmDisplaySource::validPropertyNameForTesting(
    const char *name, std::size_t capacity) {
  return validPropertyName(name, capacity);
}
#endif

} // namespace adrenalin::hardware
