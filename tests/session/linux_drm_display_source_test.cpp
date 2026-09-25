#include "interfaces/linux_drm_display_source.h"

#include <QtTest>

#include <algorithm>
#include <cstdint>

using adrenalin::hardware::LinuxDrmDisplaySource;

class LinuxDrmDisplaySourceTest final : public QObject {
  Q_OBJECT

private:
  using RawConnector = LinuxDrmDisplaySource::RawConnector;
  using RawEnumeration = LinuxDrmDisplaySource::RawEnumeration;

  static QByteArray validEdid(std::uint32_t serial = 0x12345678) {
    QByteArray bytes(128, '\0');
    const unsigned char header[] = {0x00, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0x00};
    for (int i = 0; i < 8; ++i) {
      bytes[i] = static_cast<char>(header[i]);
    }
    // ABC manufacturer code, product code 0x1234 (little-endian).
    bytes[8] = static_cast<char>((1U << 2U) | (2U >> 3U));
    bytes[9] = static_cast<char>(((2U & 7U) << 5U) | 3U);
    bytes[10] = static_cast<char>(0x34);
    bytes[11] = static_cast<char>(0x12);
    bytes[12] = static_cast<char>(serial & 0xffU);
    bytes[13] = static_cast<char>((serial >> 8U) & 0xffU);
    bytes[14] = static_cast<char>((serial >> 16U) & 0xffU);
    bytes[15] = static_cast<char>((serial >> 24U) & 0xffU);
    bytes[16] = 12;
    bytes[17] = 34;
    bytes[18] = 1;
    bytes[19] = 4;
    bytes[126] = 0;
    unsigned int sum = 0;
    for (int i = 0; i < 127; ++i) {
      sum += static_cast<unsigned char>(bytes[i]);
    }
    bytes[127] = static_cast<char>((256U - (sum & 0xffU)) & 0xffU);
    return bytes;
  }

  static RawConnector display(
      const QString &gpu = QStringLiteral(
          "gpu.pci.v1."
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
      const QString &type = QStringLiteral("HDMI-A"), std::uint32_t typeId = 1,
      QByteArray edid = validEdid()) {
    return {gpu, type, typeId, true, std::move(edid)};
  }

  static auto inject(RawEnumeration raw) {
    return [raw = std::move(raw)] { return raw; };
  }

private slots:
  void validEmptyInventoryIsNotAnError() {
    const auto result =
        LinuxDrmDisplaySource::enumerateForTesting(inject({true, 0, {}}));
    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
    QVERIFY(result.displays.isEmpty());
  }

  void enumerationFailureIsDistinctFromEmpty() {
    const auto result =
        LinuxDrmDisplaySource::enumerateForTesting(inject({false, 0, {}}));
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.displays.isEmpty());
  }

  void malformedAndMismatchedCountsAreRejected() {
    QVERIFY(!LinuxDrmDisplaySource::enumerateForTesting(inject({true, 1, {}}))
                 .success);
    QVERIFY(!LinuxDrmDisplaySource::enumerateForTesting(inject({true, -1, {}}))
                 .success);
    QVERIFY(!LinuxDrmDisplaySource::enumerateForTesting(inject({true, 257, {}}))
                 .success);
    QVERIFY(!LinuxDrmDisplaySource::enumerateForTesting({}).success);
  }

  void malformedLibdrmArraysAndPropertyNamesAreRejected() {
    const std::uint32_t connectorId = 1;
    const std::uint64_t propertyValue = 1;
    QVERIFY(LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
        0, nullptr));
    QVERIFY(LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
        1, &connectorId));
    QVERIFY(!LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
        -1, nullptr));
    QVERIFY(!LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
        1, nullptr));
    QVERIFY(!LinuxDrmDisplaySource::validConnectorResourceArrayForTesting(
        257, &connectorId));

    QVERIFY(LinuxDrmDisplaySource::validPropertyArraysForTesting(
        0, nullptr, nullptr));
    QVERIFY(LinuxDrmDisplaySource::validPropertyArraysForTesting(
        1, &connectorId, &propertyValue));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyArraysForTesting(
        1, nullptr, &propertyValue));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyArraysForTesting(
        1, &connectorId, nullptr));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyArraysForTesting(
        4097, &connectorId, &propertyValue));

    const char terminated[] = "EDID";
    const char unterminated[] = {'E', 'D', 'I', 'D'};
    QVERIFY(LinuxDrmDisplaySource::validPropertyNameForTesting(
        terminated, sizeof(terminated)));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyNameForTesting(
        nullptr, sizeof(terminated)));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyNameForTesting(
        "", 1));
    QVERIFY(!LinuxDrmDisplaySource::validPropertyNameForTesting(
        unterminated, sizeof(unterminated)));
  }

  void connectorIdentityIsNormalizedAndValidated() {
    QCOMPARE(LinuxDrmDisplaySource::normalizeConnectorIdentity(
                 QStringLiteral("HDMI-A"), 2),
             QStringLiteral("HDMI-A-2"));
    QCOMPARE(LinuxDrmDisplaySource::normalizeConnectorIdentity(
                 QStringLiteral("edp"), 1),
             QStringLiteral("EDP-1"));
    QVERIFY(LinuxDrmDisplaySource::normalizeConnectorIdentity(
                QStringLiteral("../card0"), 1)
                .isEmpty());
    QVERIFY(LinuxDrmDisplaySource::normalizeConnectorIdentity(
                QStringLiteral("DP-"), 1)
                .isEmpty());
    QVERIFY(LinuxDrmDisplaySource::normalizeConnectorIdentity(
                QStringLiteral("DP"), 0)
                .isEmpty());
  }

  void edidIdentityDigestUsesNonSerialIdentityFieldsOnly() {
    const QString first =
        LinuxDrmDisplaySource::edidIdentityDigest(validEdid(0x11111111));
    const QString second =
        LinuxDrmDisplaySource::edidIdentityDigest(validEdid(0xeeeeeeee));
    QVERIFY(!first.isEmpty());
    QCOMPARE(first, second);
    QVERIFY(first.size() == 64);

    QByteArray changedProduct = validEdid();
    changedProduct[10] = static_cast<char>(0x35);
    unsigned int sum = 0;
    for (int i = 0; i < 127; ++i) {
      sum += static_cast<unsigned char>(changedProduct[i]);
    }
    changedProduct[127] = static_cast<char>((256U - (sum & 0xffU)) & 0xffU);
    QVERIFY(first != LinuxDrmDisplaySource::edidIdentityDigest(changedProduct));
  }

  void invalidEdidNeverCreatesDisplayIdentity() {
    QByteArray invalid = validEdid();
    invalid[127] ^= 1;
    QVERIFY(LinuxDrmDisplaySource::edidIdentityDigest(invalid).isEmpty());
    QVERIFY(
        !LinuxDrmDisplaySource::enumerateForTesting(
             inject({true,
                     1,
                     {display(QStringLiteral("gpu.pci.v1."
                                             "0123456789abcdef0123456789abcdef0"
                                             "123456789abcdef0123456789abcdef"),
                              QStringLiteral("DP"), 1, invalid)}}))
             .success);

    QByteArray invalidManufacturer = validEdid();
    invalidManufacturer[8] = 0;
    invalidManufacturer[9] = 0;
    unsigned int sum = 0;
    for (int i = 0; i < 127; ++i) {
      sum += static_cast<unsigned char>(invalidManufacturer[i]);
    }
    invalidManufacturer[127] =
        static_cast<char>((256U - (sum & 0xffU)) & 0xffU);
    QVERIFY(LinuxDrmDisplaySource::edidIdentityDigest(invalidManufacturer)
                .isEmpty());
  }

  void malformedOwnerIdentityIsRejected() {
    const auto result = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true, 1, {display(QStringLiteral("gpu.pci.not-a-token"))}}));
    QVERIFY(!result.success);
    QVERIFY(result.displays.isEmpty());
  }

  void duplicateConnectorEvidenceIsRejectedRegardlessOfEdid() {
    const auto result = LinuxDrmDisplaySource::enumerateForTesting(inject(
        {true,
         2,
         {display(),
          display(QStringLiteral("gpu.pci.v1."
                                 "0123456789abcdef0123456789abcdef0123456789abc"
                                 "def0123456789abcdef"),
                  QStringLiteral("HDMI-A"), 1, validEdid(0x87654321))}}));
    QVERIFY(!result.success);
    QVERIFY(result.displays.isEmpty());
  }

  void sameEdidOnDistinctConnectorsRemainsDistinct() {
    const auto result = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true,
                2,
                {display(QStringLiteral("gpu.pci.v1."
                                        "0123456789abcdef0123456789abcdef012345"
                                        "6789abcdef0123456789abcdef"),
                         QStringLiteral("DP"), 1),
                 display(QStringLiteral("gpu.pci.v1."
                                        "0123456789abcdef0123456789abcdef012345"
                                        "6789abcdef0123456789abcdef"),
                         QStringLiteral("DP"), 2)}}));
    QVERIFY(result.success);
    QCOMPARE(result.displays.size(), 2);
    QVERIFY(result.displays.at(0).subjectId != result.displays.at(1).subjectId);
  }

  void disconnectedConnectorsDoNotBecomeSubjects() {
    RawConnector disconnected = display();
    disconnected.connected = false;
    disconnected.edid.clear();
    const auto result = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true, 1, {disconnected}}));
    QVERIFY(result.success);
    QVERIFY(result.displays.isEmpty());
  }

  void displayTokenBindsOwnerConnectorAndEdidIdentity() {
    const auto baseline = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true, 1, {display()}}));
    const auto otherGpu = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true,
                1,
                {display(QStringLiteral("gpu.pci.v1."
                                        "abcdef0123456789abcdef0123456789abcdef"
                                        "0123456789abcdef0123456789"))}}));
    const auto otherConnector = LinuxDrmDisplaySource::enumerateForTesting(
        inject({true,
                1,
                {display(QStringLiteral("gpu.pci.v1."
                                        "0123456789abcdef0123456789abcdef012345"
                                        "6789abcdef0123456789abcdef"),
                         QStringLiteral("DP"), 1)}}));
    QVERIFY(baseline.success && otherGpu.success && otherConnector.success);
    QCOMPARE(baseline.displays.size(), 1);
    QVERIFY(baseline.displays.constFirst().subjectId !=
            otherGpu.displays.constFirst().subjectId);
    QVERIFY(baseline.displays.constFirst().subjectId !=
            otherConnector.displays.constFirst().subjectId);
    QVERIFY(baseline.displays.constFirst().subjectId.startsWith(
        QStringLiteral("display.v1.")));
    QVERIFY(!baseline.displays.constFirst().subjectId.contains(QStringLiteral(
        "gpu.pci.v1."
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")));
    QVERIFY(!baseline.displays.constFirst().edidIdentityDigest.contains(
        QByteArray("11111111")));

    const auto alternateSerial =
        LinuxDrmDisplaySource::enumerateForTesting(inject(
            {true,
             1,
             {display(QStringLiteral("gpu.pci.v1."
                                     "0123456789abcdef0123456789abcdef012345678"
                                     "9abcdef0123456789abcdef"),
                      QStringLiteral("HDMI-A"), 1, validEdid(0xeeeeeeee))}}));
    QVERIFY(alternateSerial.success);
    QCOMPARE(baseline.displays.constFirst().subjectId,
             alternateSerial.displays.constFirst().subjectId);
  }
};

QTEST_GUILESS_MAIN(LinuxDrmDisplaySourceTest)
#include "linux_drm_display_source_test.moc"
