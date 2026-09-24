#include "interfaces/pci_gpu_subject_identity.h"

#include <QtTest>

using adrenalin::hardware::PciGpuIdentity;
using adrenalin::hardware::gpuSubjectIdentity;

class PciGpuSubjectIdentityTest final : public QObject
{
    Q_OBJECT

private:
    static PciGpuIdentity gpu(std::uint16_t device = 0x73bf,
                              QString bdf = QStringLiteral("0000:03:00.0"))
    {
        return {0x1002, device, std::move(bdf)};
    }

private slots:
    void sameValidatedIdentityProducesDeterministicTypedId()
    {
        const auto first = gpuSubjectIdentity(gpu());
        const auto second = gpuSubjectIdentity(gpu());

        QVERIFY(first.has_value());
        QVERIFY(second.has_value());
        QCOMPARE(first->subjectKind, QStringLiteral("GPU_PCI"));
        QCOMPARE(first->subjectId, second->subjectId);
        QVERIFY(first->subjectId.startsWith(QStringLiteral("gpu.pci.v1.")));
        QCOMPARE(first->subjectId.size(), 11 + 64);
    }

    void addressAndDeviceDifferencesProduceDistinctIds()
    {
        const auto baseline = gpuSubjectIdentity(gpu());
        const auto otherAddress = gpuSubjectIdentity(gpu(0x73bf,
                                                           QStringLiteral("0000:04:00.0")));
        const auto otherDevice = gpuSubjectIdentity(gpu(0x744c));

        QVERIFY(baseline && otherAddress && otherDevice);
        QVERIFY(baseline->subjectId != otherAddress->subjectId);
        QVERIFY(baseline->subjectId != otherDevice->subjectId);
    }

    void invalidPciIdentityIsRejected()
    {
        QVERIFY(!gpuSubjectIdentity({0, 0x73bf, QStringLiteral("0000:03:00.0")}));
        QVERIFY(!gpuSubjectIdentity({0x8086, 0x73bf, QStringLiteral("0000:03:00.0")}));
        QVERIFY(!gpuSubjectIdentity({0x1002, 0, QStringLiteral("0000:03:00.0")}));
        QVERIFY(!gpuSubjectIdentity(gpu(0x73bf, QStringLiteral("0000:03:20.0"))));
        QVERIFY(!gpuSubjectIdentity(gpu(0x73bf, QStringLiteral("0000:03:00.8"))));
        QVERIFY(!gpuSubjectIdentity(gpu(0x73bf, QStringLiteral("0000:3:00.0"))));
        QVERIFY(!gpuSubjectIdentity(gpu(0x73bf, QStringLiteral("0000:03:00.0\n"))));
    }

    void opaqueIdDoesNotContainRawBdf()
    {
        const QString bdf = QStringLiteral("0000:03:00.0");
        const auto result = gpuSubjectIdentity(gpu(0x73bf, bdf));

        QVERIFY(result.has_value());
        QVERIFY(!result->subjectId.contains(bdf));
        QCOMPARE(result->subjectId.size(), 75);
    }
};

QTEST_GUILESS_MAIN(PciGpuSubjectIdentityTest)
#include "pci_gpu_subject_identity_test.moc"
