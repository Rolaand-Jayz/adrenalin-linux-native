#include "interfaces/linux_drm_inventory_source.h"

#include <QtTest>

using adrenalin::hardware::LinuxDrmInventorySource;

class LinuxDrmInventorySourceTest final : public QObject
{
    Q_OBJECT

private:
    using RawDevice = LinuxDrmInventorySource::RawDevice;
    using RawEnumeration = LinuxDrmInventorySource::RawEnumeration;

    static RawDevice amdDevice(std::uint32_t bus = 3, std::uint32_t slot = 0)
    {
        return {true, true, 0, bus, slot, 0, 0x1002, 0x73bf};
    }

    static auto inject(RawEnumeration raw)
    {
        return [raw = std::move(raw)] { return raw; };
    }

private slots:
    void validEmptyInventoryIsNotAnError()
    {
        const auto result = LinuxDrmInventorySource::enumerateForTesting(inject({true, 0, {}}));
        QVERIFY(result.success);
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.devices.isEmpty());
    }

    void enumerationFailureIsDistinctFromEmpty()
    {
        const auto result = LinuxDrmInventorySource::enumerateForTesting(inject({false, 0, {}}));
        QVERIFY(!result.success);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.devices.isEmpty());
    }

    void nonPciRecordsAreNotTreatedAsPci()
    {
        RawDevice nonPci{false, false, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                         UINT32_MAX, UINT32_MAX, UINT32_MAX};
        const auto result = LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {nonPci}}));
        QVERIFY(result.success);
        QVERIFY(result.devices.isEmpty());
    }

    void amdPciIdentityHasCanonicalTransientBdf()
    {
        const auto result = LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {amdDevice()}}));
        QVERIFY(result.success);
        QCOMPARE(result.devices.size(), 1);
        QCOMPARE(result.devices.constFirst().vendorId, std::uint16_t(0x1002));
        QCOMPARE(result.devices.constFirst().deviceId, std::uint16_t(0x73bf));
        QCOMPARE(result.devices.constFirst().canonicalBdf, QStringLiteral("0000:03:00.0"));
    }

    void nonAmdPciDevicesAreExcluded()
    {
        RawDevice other = amdDevice();
        other.vendorId = 0x8086;
        const auto result = LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {other}}));
        QVERIFY(result.success);
        QVERIFY(result.devices.isEmpty());
    }

    void identicalDuplicateBdfEvidenceIsRejected()
    {
        const auto result = LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 2, {amdDevice(), amdDevice()}}));
        QVERIFY(!result.success);
        QVERIFY(result.devices.isEmpty());
    }

    void conflictingDuplicateIsRejected()
    {
        RawDevice conflict = amdDevice();
        conflict.deviceId = 0x744c;
        const auto result = LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 2, {amdDevice(), conflict}}));
        QVERIFY(!result.success);
        QVERIFY(result.devices.isEmpty());
    }

    void malformedPciDataIsRejected()
    {
        RawDevice missingDeviceInfo = amdDevice();
        missingDeviceInfo.pciDevice = false;
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {missingDeviceInfo}})).success);

        RawDevice invalidSlot = amdDevice();
        invalidSlot.device = 32;
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {invalidSlot}})).success);

        RawDevice invalidId = amdDevice();
        invalidId.vendorId = 0x10000;
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {invalidId}})).success);
    }

    void countOverflowAndMismatchAreRejected()
    {
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 257, {}})).success);
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, 1, {}})).success);
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting(
            inject({true, -1, {}})).success);
    }

    void emptyTestSeamIsRejected()
    {
        QVERIFY(!LinuxDrmInventorySource::enumerateForTesting({}).success);
    }
};

QTEST_GUILESS_MAIN(LinuxDrmInventorySourceTest)
#include "linux_drm_inventory_source_test.moc"
