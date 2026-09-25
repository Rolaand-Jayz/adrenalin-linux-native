#include "interfaces/linux_hardware1_inventory_provider.h"
#include "interfaces/hardware1_registry.h"
#include "interfaces/pci_gpu_subject_identity.h"

#include <QSet>
#include <QCryptographicHash>
#include <algorithm>
#include <QtTest>

using namespace adrenalin::hardware;
using namespace adrenalin::contracts::hardware1;

class LinuxHardware1InventoryProviderTest final : public QObject {
    Q_OBJECT

private:
    static QString displayId(const QString &gpuSubjectId, const QString &connector,
                            const QString &edidDigest)
    {
        const QString material = QStringLiteral(
            "adrenalin-linux:subject:DISPLAY:v1|gpu=%1|connector=%2|edid=%3")
            .arg(gpuSubjectId, connector, edidDigest);
        return QStringLiteral("display.v1.%1").arg(QString::fromLatin1(
            QCryptographicHash::hash(material.toUtf8(), QCryptographicHash::Sha256).toHex()));
    }

    static Hardware1Evidence evidence()
    {
        Hardware1Evidence result;
        PciGpuIdentity pci;
        pci.vendorId = 0x1002;
        pci.deviceId = 0x73df;
        pci.canonicalBdf = QStringLiteral("0000:03:00.0");
        result.gpus.success = true;
        result.gpus.devices.push_back(pci);
        PciGpuIdentity secondPci = pci;
        secondPci.deviceId = 0x73af;
        secondPci.canonicalBdf = QStringLiteral("0000:04:00.0");
        result.gpus.devices.push_back(secondPci);

        result.cpuPackages.success = true;
        CpuPackageIdentity cpu;
        cpu.subjectId = QStringLiteral("cpu-package-") + QString(64, QLatin1Char('a'));
        cpu.physicalPackageId = 3;
        cpu.vendorId = QStringLiteral("AuthenticAMD");
        cpu.architecture = QStringLiteral("x86_64");
        cpu.family = 25;
        cpu.model = 97;
        cpu.stepping = 2;
        result.cpuPackages.packages.push_back(cpu);
        CpuPackageIdentity secondCpu = cpu;
        secondCpu.subjectId = QStringLiteral("cpu-package-") + QString(64, QLatin1Char('e'));
        secondCpu.physicalPackageId = 7;
        secondCpu.vendorId = QStringLiteral("GenuineIntel");
        secondCpu.family = 6;
        secondCpu.model = 143;
        secondCpu.stepping = 8;
        result.cpuPackages.packages.push_back(secondCpu);

        const auto gpu = gpuSubjectIdentity(pci);
        const auto secondGpu = gpuSubjectIdentity(secondPci);
        Q_ASSERT(gpu.has_value());
        Q_ASSERT(secondGpu.has_value());
        result.displays.success = true;
        DrmDisplayIdentity display;
        display.gpuSubjectId = gpu->subjectId;
        display.connectorIdentity = QStringLiteral("DP-1");
        display.edidIdentityDigest = QString(64, QLatin1Char('c'));
        display.subjectId = displayId(display.gpuSubjectId, display.connectorIdentity,
                                      display.edidIdentityDigest);
        result.displays.displays.push_back(display);
        DrmDisplayIdentity secondDisplay = display;
        secondDisplay.gpuSubjectId = secondGpu->subjectId;
        secondDisplay.connectorIdentity = QStringLiteral("HDMI-A-2");
        secondDisplay.edidIdentityDigest = QString(64, QLatin1Char('9'));
        secondDisplay.subjectId = displayId(secondDisplay.gpuSubjectId,
                                            secondDisplay.connectorIdentity,
                                            secondDisplay.edidIdentityDigest);
        result.displays.displays.push_back(secondDisplay);
        return result;
    }

    static int expectedForKind(const QString &kind)
    {
        int count = 0;
        for (const CapabilityDefinition &definition : capabilityRegistryV1()) {
            if (definition.subjectKinds.contains(kind)) ++count;
        }
        return count;
    }

private slots:
    void composesVerifiedSubjectsAndCompleteUnknownGraph()
    {
        const Hardware1Snapshot snapshot = LinuxHardware1InventoryProvider::composeEvidenceForTesting(evidence());
        QVERIFY(snapshot.success);
        QCOMPARE(snapshot.devices.size(), 6);
        QCOMPARE(snapshot.deviceInfo.size(), 6);

        QSet<QString> kinds;
        for (const Device &device : snapshot.devices) kinds.insert(device.subjectKind);
        QVERIFY(kinds.contains(QStringLiteral("GPU_PCI")));
        QVERIFY(kinds.contains(QStringLiteral("CPU_PACKAGE")));
        QVERIFY(kinds.contains(QStringLiteral("DISPLAY")));
        QVERIFY(!kinds.contains(QStringLiteral("PLATFORM")));

        int gpuCount = 0;
        int cpuCount = 0;
        int displayCount = 0;
        int gpuSubjects = 0;
        int cpuSubjects = 0;
        int displaySubjects = 0;
        for (const Device &device : snapshot.devices) {
            if (device.subjectKind == QStringLiteral("GPU_PCI")) ++gpuSubjects;
            else if (device.subjectKind == QStringLiteral("CPU_PACKAGE")) ++cpuSubjects;
            else if (device.subjectKind == QStringLiteral("DISPLAY")) ++displaySubjects;
        }
        int platformCount = 0;
        for (const Capability &capability : snapshot.capabilities) {
            QVERIFY(capability.isValid());
            QCOMPARE(capability.supportState, QStringLiteral("UNKNOWN"));
            QVERIFY(capability.providerId.isEmpty());
            QVERIFY(capability.evidenceCode.isEmpty());
            QVERIFY(capability.configuredValue.kind == QStringLiteral("NONE"));
            if (capability.subjectKind == QStringLiteral("GPU_PCI")) ++gpuCount;
            else if (capability.subjectKind == QStringLiteral("CPU_PACKAGE")) ++cpuCount;
            else if (capability.subjectKind == QStringLiteral("DISPLAY")) ++displayCount;
            else if (capability.subjectKind == QStringLiteral("PLATFORM")) ++platformCount;
        }
        QCOMPARE(gpuCount, expectedForKind(QStringLiteral("GPU_PCI")) * gpuSubjects);
        QCOMPARE(cpuCount, expectedForKind(QStringLiteral("CPU_PACKAGE")) * cpuSubjects);
        QCOMPARE(displayCount, expectedForKind(QStringLiteral("DISPLAY")) * displaySubjects);
        QCOMPARE(platformCount, expectedForKind(QStringLiteral("PLATFORM")));

        const auto gpuInfo = std::find_if(snapshot.deviceInfo.cbegin(), snapshot.deviceInfo.cend(),
            [](const DeviceInfo &info) { return info.subjectKind == QStringLiteral("GPU_PCI"); });
        QVERIFY(gpuInfo != snapshot.deviceInfo.cend());
        QVERIFY(gpuInfo->pciAddress.isEmpty());
        QVERIFY(gpuInfo->model.isEmpty());
        QVERIFY(gpuInfo->driverName.isEmpty());

        const auto cpuInfo = std::find_if(snapshot.deviceInfo.cbegin(), snapshot.deviceInfo.cend(),
            [](const DeviceInfo &info) {
                return info.subjectKind == QStringLiteral("CPU_PACKAGE")
                    && info.subjectId.endsWith(QLatin1Char('a'));
            });
        const auto secondCpuInfo = std::find_if(snapshot.deviceInfo.cbegin(), snapshot.deviceInfo.cend(),
            [](const DeviceInfo &info) {
                return info.subjectKind == QStringLiteral("CPU_PACKAGE")
                    && info.subjectId.endsWith(QLatin1Char('e'));
            });
        QVERIFY(cpuInfo != snapshot.deviceInfo.cend());
        QVERIFY(secondCpuInfo != snapshot.deviceInfo.cend());
        QCOMPARE(cpuInfo->manufacturer, QStringLiteral("AuthenticAMD"));
        QCOMPARE(cpuInfo->model, QStringLiteral("Family 25 Model 97"));
        QCOMPARE(secondCpuInfo->manufacturer, QStringLiteral("GenuineIntel"));
        QCOMPARE(secondCpuInfo->model, QStringLiteral("Family 6 Model 143"));
        QVERIFY(!cpuInfo->model.contains(QStringLiteral("Stepping")));
        QVERIFY(!secondCpuInfo->model.contains(QStringLiteral("Stepping")));
    }

    void cpuIdentityChangesAdvanceInventoryAndCapabilityGenerations()
    {
        LinuxHardware1InventoryProvider provider;
        Hardware1Evidence input = evidence();
        const Hardware1Snapshot initial = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(initial.success);
        const Hardware1Snapshot unchanged = provider.refreshWithEvidenceForTesting(input);
        QCOMPARE(unchanged.inventoryGeneration, initial.inventoryGeneration);
        QCOMPARE(unchanged.capabilityGeneration, initial.capabilityGeneration);

        input.cpuPackages.packages[0].model += 1;
        input.cpuPackages.packages[0].subjectId = QStringLiteral("cpu-package-")
            + QString(64, QLatin1Char('b'));
        const Hardware1Snapshot changed = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(changed.success);
        QCOMPARE(changed.inventoryGeneration, initial.inventoryGeneration + 1);
        QCOMPARE(changed.capabilityGeneration, initial.capabilityGeneration + 1);
    }

    void rejectsCrossSourceMismatchWithoutPartialData()
    {
        Hardware1Evidence input = evidence();
        input.displays.displays[0].gpuSubjectId = QStringLiteral("gpu.pci.v1.") + QString(64, QLatin1Char('d'));
        const Hardware1Snapshot snapshot = LinuxHardware1InventoryProvider::composeEvidenceForTesting(input);
        QVERIFY(!snapshot.success);
        QVERIFY(snapshot.devices.isEmpty());
        QVERIFY(snapshot.deviceInfo.isEmpty());
        QVERIFY(snapshot.capabilities.isEmpty());

        Hardware1Evidence malformed = evidence();
        malformed.displays.displays[0].connectorIdentity = QStringLiteral("dp-1");
        QVERIFY(!LinuxHardware1InventoryProvider::composeEvidenceForTesting(malformed).success);
        malformed = evidence();
        malformed.displays.displays[0].edidIdentityDigest[0] = QLatin1Char('A');
        QVERIFY(!LinuxHardware1InventoryProvider::composeEvidenceForTesting(malformed).success);
        malformed = evidence();
        malformed.displays.displays[0].subjectId = QStringLiteral("display.v1.")
            + QString(64, QLatin1Char('a'));
        QVERIFY(!LinuxHardware1InventoryProvider::composeEvidenceForTesting(malformed).success);
    }

    void rejectsAnyFailedSourceAndDistinguishesSuccessfulEmptyInventory()
    {
        Hardware1Evidence empty;
        empty.gpus.success = true;
        empty.cpuPackages.success = true;
        CpuPackageIdentity cpu;
        cpu.subjectId = QStringLiteral("cpu-package-") + QString(64, QLatin1Char('a'));
        empty.cpuPackages.packages.push_back(cpu);
        empty.displays.success = true;
        const Hardware1Snapshot successfulEmpty = LinuxHardware1InventoryProvider::composeEvidenceForTesting(empty);
        QVERIFY(successfulEmpty.success);
        QVERIFY(std::none_of(successfulEmpty.devices.cbegin(), successfulEmpty.devices.cend(),
            [](const Device &device) { return device.subjectKind == QStringLiteral("GPU_PCI")
                || device.subjectKind == QStringLiteral("DISPLAY"); }));
        QCOMPARE(successfulEmpty.devices.size(), 1);
        QCOMPARE(successfulEmpty.capabilities.size(), expectedForKind(QStringLiteral("CPU_PACKAGE"))
                 + expectedForKind(QStringLiteral("PLATFORM")));

        const auto expectFailure = [](const Hardware1Evidence &input) {
            const Hardware1Snapshot failed = LinuxHardware1InventoryProvider::composeEvidenceForTesting(input);
            return !failed.success && failed.devices.isEmpty() && failed.deviceInfo.isEmpty()
                && failed.capabilities.isEmpty();
        };
        Hardware1Evidence failedGpu = empty;
        failedGpu.gpus.success = false;
        QVERIFY(expectFailure(failedGpu));
        Hardware1Evidence failedCpu = empty;
        failedCpu.cpuPackages.success = false;
        QVERIFY(expectFailure(failedCpu));
        Hardware1Evidence failedDisplay = empty;
        failedDisplay.displays.success = false;
        QVERIFY(expectFailure(failedDisplay));
        Hardware1Evidence impossibleEmptyCpu = empty;
        impossibleEmptyCpu.cpuPackages.packages.clear();
        QVERIFY(expectFailure(impossibleEmptyCpu));
    }

    void failureClearsPublishedDataAndGenerationsTrackContent()
    {
        LinuxHardware1InventoryProvider provider;
        Hardware1Evidence input = evidence();
        const Hardware1Snapshot first = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(first.success);
        QCOMPARE(first.inventoryGeneration, std::uint64_t{1});
        QCOMPARE(first.capabilityGeneration, std::uint64_t{1});

        const Hardware1Snapshot stable = provider.refreshWithEvidenceForTesting(input);
        QCOMPARE(stable.inventoryGeneration, first.inventoryGeneration);
        QCOMPARE(stable.capabilityGeneration, first.capabilityGeneration);

        std::reverse(input.gpus.devices.begin(), input.gpus.devices.end());
        std::reverse(input.cpuPackages.packages.begin(), input.cpuPackages.packages.end());
        std::reverse(input.displays.displays.begin(), input.displays.displays.end());
        const Hardware1Snapshot reordered = provider.refreshWithEvidenceForTesting(input);
        QCOMPARE(reordered.inventoryGeneration, first.inventoryGeneration);
        QCOMPARE(reordered.capabilityGeneration, first.capabilityGeneration);

        PciGpuIdentity addedGpu;
        addedGpu.vendorId = 0x1002;
        addedGpu.deviceId = 0x744c;
        addedGpu.canonicalBdf = QStringLiteral("0000:05:00.0");
        input.gpus.devices.push_back(addedGpu);
        const Hardware1Snapshot changed = provider.refreshWithEvidenceForTesting(input);
        QCOMPARE(changed.inventoryGeneration, first.inventoryGeneration + 1);
        QCOMPARE(changed.capabilityGeneration, first.capabilityGeneration + 1);

        input.displays.success = false;
        const Hardware1Snapshot failed = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(!failed.success);
        QCOMPARE(failed.inventoryGeneration, std::uint64_t{0});
        QCOMPARE(failed.capabilityGeneration, std::uint64_t{0});
        const Hardware1Snapshot invalidSnapshot = provider.snapshot();
        QVERIFY(!invalidSnapshot.success);
        QCOMPARE(invalidSnapshot.inventoryGeneration, std::uint64_t{0});
        QCOMPARE(invalidSnapshot.capabilityGeneration, std::uint64_t{0});
        QVERIFY(provider.snapshot().devices.isEmpty());
        QVERIFY(provider.snapshot().deviceInfo.isEmpty());
        QVERIFY(provider.snapshot().capabilities.isEmpty());
        QCOMPARE(failed.inventoryGeneration, std::uint64_t{0});
        QCOMPARE(failed.capabilityGeneration, std::uint64_t{0});
        input.displays.success = true;
        const Hardware1Snapshot recovered = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(recovered.success);
        QVERIFY(recovered.inventoryGeneration > changed.inventoryGeneration);
        QVERIFY(recovered.capabilityGeneration > changed.capabilityGeneration);
        QCOMPARE(provider.snapshot().inventoryGeneration, recovered.inventoryGeneration);
        QCOMPARE(provider.snapshot().capabilityGeneration, recovered.capabilityGeneration);
    }

    void successfulSubjectRemovalReconcilesInventoryAndCapabilityGraph()
    {
        LinuxHardware1InventoryProvider provider;
        Hardware1Evidence input = evidence();
        const Hardware1Snapshot initial = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(initial.success);

        const auto removedGpuIdentity = gpuSubjectIdentity(input.gpus.devices[1]);
        QVERIFY(removedGpuIdentity.has_value());
        const QString removedGpuId = removedGpuIdentity->subjectId;
        const QString removedDisplayId = input.displays.displays[1].subjectId;
        const auto retainedGpuIdentity = gpuSubjectIdentity(input.gpus.devices[0]);
        QVERIFY(retainedGpuIdentity.has_value());
        const QString retainedGpuId = retainedGpuIdentity->subjectId;
        const QString retainedDisplayId = input.displays.displays[0].subjectId;

        input.gpus.devices.removeAt(1);
        input.displays.displays.removeAt(1);
        const Hardware1Snapshot reconciled = provider.refreshWithEvidenceForTesting(input);

        QVERIFY(reconciled.success);
        QCOMPARE(reconciled.inventoryGeneration, initial.inventoryGeneration + 1);
        QCOMPARE(reconciled.capabilityGeneration, initial.capabilityGeneration + 1);
        QVERIFY(std::none_of(reconciled.devices.cbegin(), reconciled.devices.cend(),
            [&removedGpuId, &removedDisplayId](const Device &device) {
                return (device.subjectKind == QStringLiteral("GPU_PCI")
                        && device.subjectId == removedGpuId)
                    || (device.subjectKind == QStringLiteral("DISPLAY")
                        && device.subjectId == removedDisplayId);
            }));
        QVERIFY(std::none_of(reconciled.deviceInfo.cbegin(), reconciled.deviceInfo.cend(),
            [&removedGpuId, &removedDisplayId](const DeviceInfo &info) {
                return (info.subjectKind == QStringLiteral("GPU_PCI")
                        && info.subjectId == removedGpuId)
                    || (info.subjectKind == QStringLiteral("DISPLAY")
                        && info.subjectId == removedDisplayId);
            }));
        QVERIFY(std::none_of(reconciled.capabilities.cbegin(), reconciled.capabilities.cend(),
            [&removedGpuId, &removedDisplayId](const Capability &capability) {
                return (capability.subjectKind == QStringLiteral("GPU_PCI")
                        && capability.subjectId == removedGpuId)
                    || (capability.subjectKind == QStringLiteral("DISPLAY")
                        && capability.subjectId == removedDisplayId);
            }));
        QVERIFY(std::any_of(reconciled.devices.cbegin(), reconciled.devices.cend(),
            [&retainedGpuId](const Device &device) {
                return device.subjectKind == QStringLiteral("GPU_PCI")
                    && device.subjectId == retainedGpuId;
            }));
        QVERIFY(std::any_of(reconciled.devices.cbegin(), reconciled.devices.cend(),
            [&retainedDisplayId](const Device &device) {
                return device.subjectKind == QStringLiteral("DISPLAY")
                    && device.subjectId == retainedDisplayId;
            }));
        QVERIFY(std::any_of(reconciled.capabilities.cbegin(), reconciled.capabilities.cend(),
            [](const Capability &capability) {
                return capability.subjectKind == QStringLiteral("PLATFORM")
                    && capability.subjectId == QStringLiteral("platform");
            }));

        const Hardware1Snapshot stable = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(stable.success);
        QCOMPARE(stable.inventoryGeneration, reconciled.inventoryGeneration);
        QCOMPARE(stable.capabilityGeneration, reconciled.capabilityGeneration);
    }

    void successfulDisplayRemovalRetainsParentGpuAndPlatformCapabilities()
    {
        LinuxHardware1InventoryProvider provider;
        Hardware1Evidence input = evidence();
        const Hardware1Snapshot initial = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(initial.success);

        const auto parentGpuIdentity = gpuSubjectIdentity(input.gpus.devices[1]);
        QVERIFY(parentGpuIdentity.has_value());
        const QString parentGpuId = parentGpuIdentity->subjectId;
        const QString removedDisplayId = input.displays.displays[1].subjectId;
        input.displays.displays.removeAt(1);

        const Hardware1Snapshot reconciled = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(reconciled.success);
        QCOMPARE(reconciled.inventoryGeneration, initial.inventoryGeneration + 1);
        QCOMPARE(reconciled.capabilityGeneration, initial.capabilityGeneration + 1);
        QVERIFY(std::any_of(reconciled.devices.cbegin(), reconciled.devices.cend(),
            [&parentGpuId](const Device &device) {
                return device.subjectKind == QStringLiteral("GPU_PCI")
                    && device.subjectId == parentGpuId;
            }));
        QVERIFY(std::none_of(reconciled.devices.cbegin(), reconciled.devices.cend(),
            [&removedDisplayId](const Device &device) {
                return device.subjectKind == QStringLiteral("DISPLAY")
                    && device.subjectId == removedDisplayId;
            }));
        QVERIFY(std::none_of(reconciled.deviceInfo.cbegin(), reconciled.deviceInfo.cend(),
            [&removedDisplayId](const DeviceInfo &info) {
                return info.subjectKind == QStringLiteral("DISPLAY")
                    && info.subjectId == removedDisplayId;
            }));
        QVERIFY(std::none_of(reconciled.capabilities.cbegin(), reconciled.capabilities.cend(),
            [&removedDisplayId](const Capability &capability) {
                return capability.subjectKind == QStringLiteral("DISPLAY")
                    && capability.subjectId == removedDisplayId;
            }));
        QVERIFY(std::any_of(reconciled.capabilities.cbegin(), reconciled.capabilities.cend(),
            [&parentGpuId](const Capability &capability) {
                return capability.subjectKind == QStringLiteral("GPU_PCI")
                    && capability.subjectId == parentGpuId;
            }));
        QVERIFY(std::any_of(reconciled.capabilities.cbegin(), reconciled.capabilities.cend(),
            [](const Capability &capability) {
                return capability.subjectKind == QStringLiteral("PLATFORM")
                    && capability.subjectId == QStringLiteral("platform");
            }));

        const Hardware1Snapshot stable = provider.refreshWithEvidenceForTesting(input);
        QVERIFY(stable.success);
        QCOMPARE(stable.inventoryGeneration, reconciled.inventoryGeneration);
        QCOMPARE(stable.capabilityGeneration, reconciled.capabilityGeneration);
    }
};

QTEST_GUILESS_MAIN(LinuxHardware1InventoryProviderTest)
#include "linux_hardware1_inventory_provider_test.moc"
