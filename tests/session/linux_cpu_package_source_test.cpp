#include "interfaces/linux_cpu_package_source.h"

#include <QtTest>

#include <QRegularExpression>

using adrenalin::hardware::LinuxCpuPackageSource;

class LinuxCpuPackageSourceTest final : public QObject
{
    Q_OBJECT

private:
    using RawPackage = LinuxCpuPackageSource::RawPackage;
    using RawEnumeration = LinuxCpuPackageSource::RawEnumeration;

    static RawPackage package(std::uint32_t packageId, QString architecture = QStringLiteral("x86_64"),
                              QString vendor = QStringLiteral("AuthenticAMD"),
                              std::uint32_t family = 25, std::uint32_t model = 97,
                              std::uint32_t stepping = 2)
    {
        return {true, true, packageId, std::move(architecture), std::move(vendor),
                family, model, stepping};
    }

    static auto inject(RawEnumeration raw)
    {
        return [raw = std::move(raw)] { return raw; };
    }

private slots:
    void validPackageEvidenceCreatesOpaqueToken()
    {
        const auto result = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 1, {package(0)}}));
        QVERIFY(result.success);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.packages.size(), 1);
        const auto &cpu = result.packages.constFirst();
        QVERIFY(QRegularExpression(QStringLiteral("^cpu-package-[0-9a-f]{64}$"))
                    .match(cpu.subjectId).hasMatch());
        QCOMPARE(cpu.subjectId.size(), qsizetype(76));
        QCOMPARE(cpu.physicalPackageId, std::uint32_t(0));
        QCOMPARE(cpu.architecture, QStringLiteral("x86_64"));
        QCOMPARE(cpu.vendorId, QStringLiteral("AuthenticAMD"));
        QCOMPARE(cpu.family, std::uint32_t(25));
        QCOMPARE(cpu.model, std::uint32_t(97));
        QCOMPARE(cpu.stepping, std::uint32_t(2));
        QVERIFY(!cpu.subjectId.contains(QStringLiteral("AuthenticAMD")));
        QVERIFY(!cpu.subjectId.contains(QStringLiteral("x86_64")));
    }

    void emptyInventoryIsNotAResolvedCpuTopology()
    {
        const auto result = LinuxCpuPackageSource::enumerateForTesting(inject({true, 0, {}}));
        QVERIFY(!result.success);
        QVERIFY(result.packages.isEmpty());
        QVERIFY(!result.error.isEmpty());
    }

    void failedEnumerationIsDistinctFromEmpty()
    {
        const auto result = LinuxCpuPackageSource::enumerateForTesting(
            inject({false, 0, {}}));
        QVERIFY(!result.success);
        QVERIFY(result.packages.isEmpty());
    }

    void tokensAreStableUnderEnumerationReordering()
    {
        RawPackage first = package(3);
        RawPackage second = package(9, QStringLiteral("x86_64"), QStringLiteral("AuthenticAMD"),
                                    25, 97, 1);
        const auto forward = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {first, second}}));
        const auto reverse = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {second, first}}));
        QVERIFY(forward.success);
        QVERIFY(reverse.success);
        QCOMPARE(forward.packages.size(), 2);
        QCOMPARE(reverse.packages.size(), 2);
        QCOMPARE(forward.packages.at(0).subjectId, reverse.packages.at(0).subjectId);
        QCOMPARE(forward.packages.at(1).subjectId, reverse.packages.at(1).subjectId);
    }

    void packagePositionAndSignatureBothContributeToIdentity()
    {
        const auto base = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 1, {package(0)}}));
        const auto differentPackage = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 1, {package(1)}}));
        const auto differentSignature = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 1, {package(0, QStringLiteral("x86_64"),
                                     QStringLiteral("AuthenticAMD"), 25, 98, 2)}}));
        QVERIFY(base.success);
        QVERIFY(differentPackage.success);
        QVERIFY(differentSignature.success);
        QVERIFY(base.packages.constFirst().subjectId != differentPackage.packages.constFirst().subjectId);
        QVERIFY(base.packages.constFirst().subjectId != differentSignature.packages.constFirst().subjectId);
    }

    void equalCpuSignaturesInDifferentPackagesRemainDistinct()
    {
        const auto result = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {package(0), package(1)}}));
        QVERIFY(result.success);
        QCOMPARE(result.packages.size(), 2);
        QVERIFY(result.packages.at(0).subjectId != result.packages.at(1).subjectId);
    }

    void unknownOrMissingPackageTopologyIsRejected()
    {
        RawPackage unknownPackage = package(UINT32_MAX);
        RawPackage outOfRangePackage = package(static_cast<std::uint32_t>(INT32_MAX) + 1U);
        RawPackage missingTopology = package(0);
        missingTopology.topologyEvidencePresent = false;
        RawPackage wrongObject = package(0);
        wrongObject.packageObject = false;
        for (const RawPackage &invalid : {unknownPackage, outOfRangePackage,
                                          missingTopology, wrongObject}) {
            const auto result = LinuxCpuPackageSource::enumerateForTesting(
                inject({true, 1, {invalid}}));
            QVERIFY(!result.success);
            QVERIFY(result.packages.isEmpty());
        }
    }

    void missingOrMalformedPlatformCpuIdentityIsRejected()
    {
        RawPackage noArchitecture = package(0);
        noArchitecture.architecture.clear();
        RawPackage unknownArchitecture = package(0, QStringLiteral("unknown"));
        RawPackage unsafeArchitecture = package(0, QStringLiteral("x86/64"));
        RawPackage noVendor = package(0);
        noVendor.vendorId.clear();
        RawPackage unknownVendor = package(0, QStringLiteral("x86_64"), QStringLiteral("unknown"));
        RawPackage unsafeVendor = package(0, QStringLiteral("x86_64"), QStringLiteral("AMD CPU"));
        RawPackage noFamily = package(0);
        noFamily.family.reset();
        RawPackage invalidFamily = package(0, QStringLiteral("x86_64"), QStringLiteral("AuthenticAMD"),
                                           0, 97, 2);
        RawPackage noModel = package(0);
        noModel.model.reset();
        RawPackage noStepping = package(0);
        noStepping.stepping.reset();
        const QVector<RawPackage> invalidPackages{
            noArchitecture, unknownArchitecture, unsafeArchitecture, noVendor, unknownVendor,
            unsafeVendor, noFamily, invalidFamily, noModel, noStepping};
        for (const RawPackage &invalid : invalidPackages) {
            const auto result = LinuxCpuPackageSource::enumerateForTesting(
                inject({true, 1, {invalid}}));
            QVERIFY(!result.success);
            QVERIFY(result.packages.isEmpty());
        }
    }

    void duplicatePackageEvidenceIsRejectedFailClosed()
    {
        const auto identical = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {package(0), package(0)}}));
        QVERIFY(!identical.success);
        QVERIFY(identical.packages.isEmpty());

        const auto contradictory = LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {package(0), package(0, QStringLiteral("x86_64"),
                                                  QStringLiteral("AuthenticAMD"), 25, 97, 3)}}));
        QVERIFY(!contradictory.success);
        QVERIFY(contradictory.packages.isEmpty());
    }

    void countMismatchAndBoundsAreRejected()
    {
        QVERIFY(!LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 2, {package(0)}})).success);
        QVERIFY(!LinuxCpuPackageSource::enumerateForTesting(
            inject({true, 257, {}})).success);
        QVERIFY(!LinuxCpuPackageSource::enumerateForTesting(
            inject({true, -1, {}})).success);
    }

    void everyOnlineProcessorMustMapToExactlyOneEnumeratedPackage()
    {
        RawEnumeration valid{true, 2, {package(0), package(3)}};
        valid.reportedOnlineProcessorCount = 3;
        valid.onlineProcessors = {
            {0, 1, 0}, {1, 1, 0}, {8, 1, 3}
        };
        const auto resolved = LinuxCpuPackageSource::enumerateForTesting(
            inject(valid));
        QVERIFY(resolved.success);
        QCOMPARE(resolved.packages.size(), 2);

        QVector<RawEnumeration> invalidInventories;
        RawEnumeration missingPackage = valid;
        missingPackage.onlineProcessors[2].packageOsIndex = 9;
        invalidInventories.push_back(missingPackage);

        RawEnumeration noPackageAncestor = valid;
        noPackageAncestor.onlineProcessors[0].packageAncestorCount = 0;
        invalidInventories.push_back(noPackageAncestor);

        RawEnumeration ambiguousPackageAncestors = valid;
        ambiguousPackageAncestors.onlineProcessors[1].packageAncestorCount = 2;
        invalidInventories.push_back(ambiguousPackageAncestors);

        RawEnumeration duplicateProcessor = valid;
        duplicateProcessor.onlineProcessors[1].processorOsIndex = 0;
        invalidInventories.push_back(duplicateProcessor);

        RawEnumeration unknownProcessor = valid;
        unknownProcessor.onlineProcessors[1].processorOsIndex = UINT32_MAX;
        invalidInventories.push_back(unknownProcessor);

        RawEnumeration incompleteOnlineSet = valid;
        incompleteOnlineSet.reportedOnlineProcessorCount = 4;
        invalidInventories.push_back(incompleteOnlineSet);

        RawEnumeration countDisagreesWithOnlineSet = valid;
        countDisagreesWithOnlineSet.reportedOnlineProcessorCount = 2;
        invalidInventories.push_back(countDisagreesWithOnlineSet);

        RawEnumeration noOnlineProcessors = valid;
        noOnlineProcessors.reportedOnlineProcessorCount = 0;
        noOnlineProcessors.onlineProcessors.clear();
        invalidInventories.push_back(noOnlineProcessors);

        for (const RawEnumeration &invalid : invalidInventories) {
            const auto result = LinuxCpuPackageSource::enumerateForTesting(
                inject(invalid));
            QVERIFY(!result.success);
            QVERIFY(result.packages.isEmpty());
        }
    }

    void emptyTestSeamIsRejected()
    {
        QVERIFY(!LinuxCpuPackageSource::enumerateForTesting({}).success);
    }
};

QTEST_GUILESS_MAIN(LinuxCpuPackageSourceTest)
#include "linux_cpu_package_source_test.moc"
