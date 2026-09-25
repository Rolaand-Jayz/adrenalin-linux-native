#include "interfaces/hardware1_contract_types.h"
#include "hardware1_adaptor.h"
#include "sessiond/session_service.h"
#include "session_identity.h"

#include <hardware1_interface.h>

#include <QDBusConnection>
#include <QDir>
#include <QScopeGuard>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
using namespace adrenalin::contracts::hardware1;

constexpr auto kCpuKind = "CPU_PACKAGE";
constexpr auto kCpuId = "cpu-package-integration-test";
constexpr auto kMissingGpuId = "gpu-pci-absent-integration-test";

adrenalin::hardware::Hardware1Snapshot validSnapshot()
{
    adrenalin::hardware::Hardware1Snapshot snapshot;
    snapshot.success = true;
    snapshot.inventoryGeneration = 1;
    snapshot.capabilityGeneration = 1;
    snapshot.devices.append({QString::fromLatin1(kCpuKind), QString::fromLatin1(kCpuId),
                             QStringLiteral("integration.cpu"), QStringLiteral("Integration CPU")});
    DeviceInfo info;
    info.subjectKind = QString::fromLatin1(kCpuKind);
    info.subjectId = QString::fromLatin1(kCpuId);
    info.identityEvidence = QStringLiteral("integration.cpu");
    info.displayName = QStringLiteral("Integration CPU");
    snapshot.deviceInfo.append(info);
    Capability capability;
    capability.subjectKind = info.subjectKind;
    capability.subjectId = info.subjectId;
    capability.capabilityId = QStringLiteral("cpu.metric.core_count");
    capability.supportState = QStringLiteral("UNKNOWN");
    snapshot.capabilities.append(capability);
    return snapshot;
}

class Hardware1SessionServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
    }

    void exportsProviderBackedReadsAndTypedFailureEnvelopes()
    {
        QTemporaryDir dataDirectory;
        QVERIFY(dataDirectory.isValid());
        const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
        SessionService service(databasePath);
        service.setHardware1SnapshotForTesting(validSnapshot());
        Hardware1Adaptor adaptor(&service);
        QVERIFY(bus_.registerObject(QString::fromLatin1(adrenalin::session1::objectPath), &service,
                                    QDBusConnection::ExportAdaptors));
        QVERIFY(bus_.registerService(QString::fromLatin1(adrenalin::session1::serviceName)));
        const auto cleanup = qScopeGuard([this] {
            bus_.unregisterService(QString::fromLatin1(adrenalin::session1::serviceName));
            bus_.unregisterObject(QString::fromLatin1(adrenalin::session1::objectPath));
        });

        OrgAdrenalinlinuxSession1Hardware1Interface proxy(
            QString::fromLatin1(adrenalin::session1::serviceName),
            QString::fromLatin1(adrenalin::session1::objectPath), bus_);
        QVERIFY(proxy.isValid());
        QSignalSpy inventoryChanged(&proxy,
            &OrgAdrenalinlinuxSession1Hardware1Interface::InventoryChanged);
        QSignalSpy graphChanged(&proxy,
            &OrgAdrenalinlinuxSession1Hardware1Interface::CapabilityGraphChanged);
        QVERIFY(inventoryChanged.isValid());
        QVERIFY(graphChanged.isValid());

        QVERIFY(service.initializeAsync());
        QCOMPARE(service.initializationState(), QStringLiteral("RECOVERING"));
        auto busyPending = proxy.ListDevices();
        QTRY_VERIFY_WITH_TIMEOUT(busyPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Device>> busyReply = busyPending;
        QVERIFY2(!busyReply.isError(), qPrintable(busyReply.error().message()));
        const Reply busy = busyReply.argumentAt<0>();
        QVERIFY(busy.isValid());
        QCOMPARE(busy.code, QStringLiteral("BUSY"));
        QVERIFY(busy.retryable);
        QVERIFY(busyReply.argumentAt<1>().isEmpty());

        QTRY_COMPARE_WITH_TIMEOUT(service.initializationState(), QStringLiteral("READY"), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(inventoryChanged.count(), 1, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(graphChanged.count(), 1, 2000);

        auto devicesPending = proxy.ListDevices();
        QTRY_VERIFY_WITH_TIMEOUT(devicesPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Device>> devicesReply = devicesPending;
        QVERIFY2(!devicesReply.isError(), qPrintable(devicesReply.error().message()));
        const Reply listed = devicesReply.argumentAt<0>();
        QVERIFY(listed.isValid());
        QCOMPARE(listed.code, QStringLiteral("OK"));
        QCOMPARE(listed.inventoryGeneration, quint64(1));
        QCOMPARE(devicesReply.argumentAt<1>().size(), 1);

        auto infoPending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind), QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(infoPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> infoReply = infoPending;
        QVERIFY2(!infoReply.isError(), qPrintable(infoReply.error().message()));
        QCOMPARE(infoReply.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(infoReply.argumentAt<1>().displayName, QStringLiteral("Integration CPU"));

        auto graphPending = proxy.GetCapabilityGraph(QString::fromLatin1(kCpuKind),
                                                     QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(graphPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Capability>> graphReply = graphPending;
        QVERIFY2(!graphReply.isError(), qPrintable(graphReply.error().message()));
        QCOMPARE(graphReply.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(graphReply.argumentAt<1>().size(), 1);
        QCOMPARE(graphReply.argumentAt<1>().constFirst().capabilityId,
                 QStringLiteral("cpu.metric.core_count"));

        auto platformInfoPending = proxy.GetDeviceInfo(QStringLiteral("PLATFORM"),
                                                         QStringLiteral("platform"));
        QTRY_VERIFY_WITH_TIMEOUT(platformInfoPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> platformInfoReply = platformInfoPending;
        QVERIFY2(!platformInfoReply.isError(), qPrintable(platformInfoReply.error().message()));
        QCOMPARE(platformInfoReply.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(platformInfoReply.argumentAt<1>().subjectKind, QStringLiteral("PLATFORM"));
        QCOMPARE(platformInfoReply.argumentAt<1>().subjectId, QStringLiteral("platform"));
        QCOMPARE(platformInfoReply.argumentAt<1>().displayName, QStringLiteral("Platform"));

        auto missingPending = proxy.GetDeviceInfo(QStringLiteral("GPU_PCI"),
                                                   QString::fromLatin1(kMissingGpuId));
        QTRY_VERIFY_WITH_TIMEOUT(missingPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> missingReply = missingPending;
        QVERIFY2(!missingReply.isError(), qPrintable(missingReply.error().message()));
        QCOMPARE(missingReply.argumentAt<0>().code, QStringLiteral("NOT_FOUND"));
        QVERIFY(missingReply.argumentAt<0>().isValid());
        QCOMPARE(missingReply.argumentAt<1>().subjectId, QString());

        auto invalidPending = proxy.GetCapabilityGraph(QStringLiteral("INVALID"),
                                                        QStringLiteral("unstable"));
        QTRY_VERIFY_WITH_TIMEOUT(invalidPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Capability>> invalidReply = invalidPending;
        QVERIFY2(!invalidReply.isError(), qPrintable(invalidReply.error().message()));
        QCOMPARE(invalidReply.argumentAt<0>().code, QStringLiteral("INVALID_ARGUMENT"));
        QVERIFY(invalidReply.argumentAt<0>().isValid());
        QVERIFY(invalidReply.argumentAt<1>().isEmpty());

        QCOMPARE(inventoryChanged.constFirst().at(2).toULongLong(), quint64(3));
        QCOMPARE(graphChanged.constFirst().at(2).toULongLong(), quint64(4));
    }

    void invalidProviderSnapshotReturnsTypedNoPayloadEnvelope()
    {
        QTemporaryDir dataDirectory;
        QVERIFY(dataDirectory.isValid());
        const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
        SessionService service(databasePath);
        adrenalin::hardware::Hardware1Snapshot failed;
        failed.error = QStringLiteral("Test source evidence is unavailable");
        service.setHardware1SnapshotForTesting(failed);
        Hardware1Adaptor adaptor(&service);
        QVERIFY(bus_.registerObject(QString::fromLatin1(adrenalin::session1::objectPath), &service,
                                    QDBusConnection::ExportAdaptors));
        QVERIFY(bus_.registerService(QString::fromLatin1(adrenalin::session1::serviceName)));
        const auto cleanup = qScopeGuard([this] {
            bus_.unregisterService(QString::fromLatin1(adrenalin::session1::serviceName));
            bus_.unregisterObject(QString::fromLatin1(adrenalin::session1::objectPath));
        });
        OrgAdrenalinlinuxSession1Hardware1Interface proxy(
            QString::fromLatin1(adrenalin::session1::serviceName),
            QString::fromLatin1(adrenalin::session1::objectPath), bus_);
        QVERIFY(proxy.isValid());
        QVERIFY(service.initializeAsync());
        auto pending = proxy.ListDevices();
        QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Device>> result = pending;
        QVERIFY2(!result.isError(), qPrintable(result.error().message()));
        const Reply reply = result.argumentAt<0>();
        QVERIFY(reply.isValid());
        QCOMPARE(reply.code, QStringLiteral("BUSY"));
        QVERIFY(reply.retryable);
        QVERIFY(result.argumentAt<1>().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(service.initializationState(), QStringLiteral("FAILED"), 2000);
        auto unavailablePending = proxy.ListDevices();
        QTRY_VERIFY_WITH_TIMEOUT(unavailablePending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Device>> unavailable = unavailablePending;
        QVERIFY2(!unavailable.isError(), qPrintable(unavailable.error().message()));
        const Reply unavailableReply = unavailable.argumentAt<0>();
        QVERIFY(unavailableReply.isValid());
        QCOMPARE(unavailableReply.code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(!unavailableReply.snapshotValid);
        QCOMPARE(unavailableReply.inventoryGeneration, quint64(0));
        QCOMPARE(unavailableReply.capabilityGeneration, quint64(0));
        QVERIFY(unavailable.argumentAt<1>().isEmpty());
    }

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
};

} // namespace

QTEST_GUILESS_MAIN(Hardware1SessionServiceTest)
#include "hardware1_session_service_test.moc"
