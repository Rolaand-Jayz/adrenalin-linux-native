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
        QCOMPARE(listed.eventSequence, service.eventSequence());
        QCOMPARE(devicesReply.argumentAt<1>().size(), 1);

        auto infoPending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind), QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(infoPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> infoReply = infoPending;
        QVERIFY2(!infoReply.isError(), qPrintable(infoReply.error().message()));
        QCOMPARE(infoReply.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(infoReply.argumentAt<0>().eventSequence, listed.eventSequence);
        QCOMPARE(infoReply.argumentAt<1>().displayName, QStringLiteral("Integration CPU"));

        auto graphPending = proxy.GetCapabilityGraph(QString::fromLatin1(kCpuKind),
                                                     QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(graphPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Capability>> graphReply = graphPending;
        QVERIFY2(!graphReply.isError(), qPrintable(graphReply.error().message()));
        QCOMPARE(graphReply.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(graphReply.argumentAt<0>().eventSequence, listed.eventSequence);
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

        QCOMPARE(inventoryChanged.constFirst().at(3).toString(), QString::fromLatin1(kCpuKind));
        QCOMPARE(inventoryChanged.constFirst().at(4).toString(), QString::fromLatin1(kCpuId));
        QCOMPARE(graphChanged.constFirst().at(3).toString(), QString::fromLatin1(kCpuKind));
        QCOMPARE(graphChanged.constFirst().at(4).toString(), QString::fromLatin1(kCpuId));

        auto removedSnapshot = validSnapshot();
        removedSnapshot.inventoryGeneration = 2;
        removedSnapshot.capabilityGeneration = 2;
        removedSnapshot.devices.clear();
        removedSnapshot.deviceInfo.clear();
        removedSnapshot.capabilities.clear();
        QVERIFY(service.reconcileHardwareSnapshotForTesting(removedSnapshot));
        QTRY_COMPARE_WITH_TIMEOUT(inventoryChanged.count(), 2, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(graphChanged.count(), 2, 2000);
        QCOMPARE(inventoryChanged.last().at(3).toString(), QString::fromLatin1(kCpuKind));
        QCOMPARE(inventoryChanged.last().at(4).toString(), QString::fromLatin1(kCpuId));
        QCOMPARE(inventoryChanged.last().at(2).toULongLong(), quint64(5));

        auto removedListPending = proxy.ListDevices();
        QTRY_VERIFY_WITH_TIMEOUT(removedListPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Device>> removedList = removedListPending;
        QVERIFY2(!removedList.isError(), qPrintable(removedList.error().message()));
        QCOMPARE(removedList.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(removedList.argumentAt<0>().eventSequence, service.eventSequence());
        QVERIFY(removedList.argumentAt<1>().isEmpty());

        auto disconnectedInfoPending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind),
                                                            QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(disconnectedInfoPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> disconnectedInfo = disconnectedInfoPending;
        QVERIFY2(!disconnectedInfo.isError(), qPrintable(disconnectedInfo.error().message()));
        QCOMPARE(disconnectedInfo.argumentAt<0>().code, QStringLiteral("DEVICE_DISCONNECTED"));
        QVERIFY(disconnectedInfo.argumentAt<0>().snapshotValid);
        QCOMPARE(disconnectedInfo.argumentAt<0>().inventoryGeneration, quint64(2));
        QCOMPARE(disconnectedInfo.argumentAt<0>().eventSequence, removedList.argumentAt<0>().eventSequence);
        QCOMPARE(disconnectedInfo.argumentAt<1>().subjectId, QString());

        auto disconnectedGraphPending = proxy.GetCapabilityGraph(QString::fromLatin1(kCpuKind),
                                                                  QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(disconnectedGraphPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, QList<Capability>> disconnectedGraph = disconnectedGraphPending;
        QVERIFY2(!disconnectedGraph.isError(), qPrintable(disconnectedGraph.error().message()));
        QCOMPARE(disconnectedGraph.argumentAt<0>().code, QStringLiteral("DEVICE_DISCONNECTED"));
        QVERIFY(disconnectedGraph.argumentAt<1>().isEmpty());

        auto stillUnknownPending = proxy.GetDeviceInfo(QStringLiteral("GPU_PCI"),
                                                        QStringLiteral("never-observed-gpu"));
        QTRY_VERIFY_WITH_TIMEOUT(stillUnknownPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> stillUnknown = stillUnknownPending;
        QVERIFY2(!stillUnknown.isError(), qPrintable(stillUnknown.error().message()));
        QCOMPARE(stillUnknown.argumentAt<0>().code, QStringLiteral("NOT_FOUND"));

        adrenalin::hardware::Hardware1Snapshot unavailable;
        unavailable.error = QStringLiteral("temporary refresh failure");
        QVERIFY(!service.reconcileHardwareSnapshotForTesting(unavailable));
        QTRY_COMPARE_WITH_TIMEOUT(inventoryChanged.count(), 3, 2000);
        QCOMPARE(inventoryChanged.last().at(3).toString(), QStringLiteral("PLATFORM"));
        QCOMPARE(inventoryChanged.last().at(4).toString(), QStringLiteral("platform"));
        QCOMPARE(inventoryChanged.last().at(2).toULongLong(), quint64(7));
        auto failedRefreshPending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind),
                                                         QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(failedRefreshPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> failedRefresh = failedRefreshPending;
        QVERIFY2(!failedRefresh.isError(), qPrintable(failedRefresh.error().message()));
        QCOMPARE(failedRefresh.argumentAt<0>().code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(!failedRefresh.argumentAt<0>().snapshotValid);
        QCOMPARE(failedRefresh.argumentAt<1>().subjectId, QString());

        auto reappearedSnapshot = validSnapshot();
        reappearedSnapshot.inventoryGeneration = 3;
        reappearedSnapshot.capabilityGeneration = 3;
        QVERIFY(service.reconcileHardwareSnapshotForTesting(reappearedSnapshot));
        QTRY_COMPARE_WITH_TIMEOUT(inventoryChanged.count(), 4, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(graphChanged.count(), 3, 2000);
        QCOMPARE(inventoryChanged.last().at(2).toULongLong(), quint64(8));
        QCOMPARE(graphChanged.last().at(2).toULongLong(), quint64(9));
        auto reappearedPending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind),
                                                      QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(reappearedPending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> reappeared = reappearedPending;
        QVERIFY2(!reappeared.isError(), qPrintable(reappeared.error().message()));
        QCOMPARE(reappeared.argumentAt<0>().code, QStringLiteral("OK"));
        QCOMPARE(reappeared.argumentAt<1>().displayName, QStringLiteral("Integration CPU"));

        service.setHardwareObserverUnavailable();
        QTRY_COMPARE_WITH_TIMEOUT(inventoryChanged.count(), 5, 2000);
        QCOMPARE(inventoryChanged.last().at(3).toString(), QStringLiteral("PLATFORM"));
        QCOMPARE(inventoryChanged.last().at(2).toULongLong(), quint64(10));
        auto observerUnavailablePending = proxy.GetDeviceInfo(QString::fromLatin1(kCpuKind),
                                                               QString::fromLatin1(kCpuId));
        QTRY_VERIFY_WITH_TIMEOUT(observerUnavailablePending.isFinished(), 2000);
        const QDBusPendingReply<Reply, DeviceInfo> observerUnavailable = observerUnavailablePending;
        QVERIFY2(!observerUnavailable.isError(), qPrintable(observerUnavailable.error().message()));
        QCOMPARE(observerUnavailable.argumentAt<0>().code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(observerUnavailable.argumentAt<1>().subjectId.isEmpty());
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
