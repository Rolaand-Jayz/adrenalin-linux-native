#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include <cstdint>
#include <functional>

namespace adrenalin::hardware {

struct DrmDisplayIdentity final {
  QString subjectKind = QStringLiteral("DISPLAY");
  QString subjectId;
  QString gpuSubjectId;
  QString connectorIdentity;
  QString edidIdentityDigest;
};

struct DrmDisplayInventoryResult final {
  bool success = false;
  QString error;
  QVector<DrmDisplayIdentity> displays;
};

class LinuxDrmDisplaySource final {
public:
  // V1 IDs hash the owning opaque GPU subject ID, normalized connector type
  // and type index, and an EDID digest over manufacturer, product, and
  // manufacture week/year fields. EDID serial bytes and text descriptors are
  // excluded. IDs may change when connector or PCI topology evidence changes.
  // Reads connected display connector state from libdrm using only runtime
  // node names returned by drmGetDevices2. It never returns raw EDID data,
  // serial values, connector object IDs, or device node names.
  static DrmDisplayInventoryResult enumerate();

#ifdef ADRENALIN_DRM_DISPLAY_SOURCE_TESTING
  struct RawConnector final {
    QString gpuSubjectId;
    QString connectorType;
    std::uint32_t connectorTypeId = 0;
    bool connected = false;
    QByteArray edid;
  };

  struct RawEnumeration final {
    bool success = false;
    int reportedCount = 0;
    QVector<RawConnector> connectors;
  };

  using TestEnumerator = std::function<RawEnumeration()>;
  static DrmDisplayInventoryResult
  enumerateForTesting(const TestEnumerator &enumerator);

  // Pure identity transforms exposed only to focused contract tests.
  static QString normalizeConnectorIdentity(const QString &type,
                                            std::uint32_t typeId);
  static QString edidIdentityDigest(const QByteArray &edid);
  static bool validConnectorResourceArrayForTesting(
      int count, const std::uint32_t *connectors);
  static bool validPropertyArraysForTesting(
      std::uint32_t count, const std::uint32_t *properties,
      const std::uint64_t *values);
  static bool validPropertyNameForTesting(const char *name,
                                          std::size_t capacity);
#endif
};

} // namespace adrenalin::hardware
