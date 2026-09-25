#include "interfaces/hardware1_contract_types.h"
#include "interfaces/display1_contract_types.h"
#include "interfaces/profiles1_contract_types.h"
#include "display1_adaptor.h"
#include "hardware1_adaptor.h"
#include "profiles1_adaptor.h"
#include "sessiond/session_service.h"
#include "session_identity.h"

#include <hardware1_interface.h>
#include <display1_interface.h>
#include <profiles1_interface.h>

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
    info.manufacturer = QStringLiteral("AuthenticAMD");
    info.model = QStringLiteral("Family 25 Model 97");
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
        adrenalin::contracts::display1::registerMetaTypes();
        adrenalin::contracts::profiles1::registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
    }

    void displayIdentityReadAndSafeMutationRefusal()
    {
        constexpr auto displayId = "display-integration-test";
        QTemporaryDir dataDirectory;
        QVERIFY(dataDirectory.isValid());
        const QString databasePath = QDir(dataDirectory.path()).filePath(
            QStringLiteral("display-session.sqlite3"));
        SessionService service(databasePath);
        adrenalin::contracts::display1::ControlChange change;
        change.capabilityId = QStringLiteral("display.brightness");
        change.value.kind = QStringLiteral("REAL");
        change.value.realValue = 50.0;
        const QString operationId = QStringLiteral("2ecf452f-5e9f-47ad-9a02-8f69b7b940c5");
        const auto recovering = service.validateDisplay(operationId, QStringLiteral("pending"),
                                                         0, 0, {change});
        QVERIFY(recovering.isValid());
        QCOMPARE(recovering.code, QStringLiteral("BUSY"));
        const auto malformedWhileRecovering = service.validateDisplay(QStringLiteral("bad-id"),
            QStringLiteral("pending"), 0, 0, {change});
        QVERIFY(malformedWhileRecovering.isValid());
        QCOMPARE(malformedWhileRecovering.code, QStringLiteral("INVALID_ARGUMENT"));
        QVERIFY(malformedWhileRecovering.operationId.isEmpty());
        auto snapshot = validSnapshot();
        snapshot.devices.append({QStringLiteral("DISPLAY"), QString::fromLatin1(displayId),
                                 QStringLiteral("integration.display.edid"),
                                 QStringLiteral("Integration Display")});
        DeviceInfo info;
        info.subjectKind = QStringLiteral("DISPLAY");
        info.subjectId = QString::fromLatin1(displayId);
        info.identityEvidence = QStringLiteral("integration.display.edid");
        info.displayName = QStringLiteral("Integration Display");
        snapshot.deviceInfo.append(info);
        Capability capability;
        capability.subjectKind = info.subjectKind;
        capability.subjectId = info.subjectId;
        capability.capabilityId = QStringLiteral("display.brightness");
        capability.supportState = QStringLiteral("UNKNOWN");
        snapshot.capabilities.append(capability);
        service.setHardware1SnapshotForTesting(snapshot);
        Hardware1Adaptor hardwareAdaptor(&service);
        Display1Adaptor displayAdaptor(&service);
        QVERIFY(bus_.registerObject(QString::fromLatin1(adrenalin::session1::objectPath), &service,
                                    QDBusConnection::ExportAdaptors));
        QVERIFY(bus_.registerService(QString::fromLatin1(adrenalin::session1::serviceName)));
        const auto cleanup = qScopeGuard([this] {
            bus_.unregisterService(QString::fromLatin1(adrenalin::session1::serviceName));
            bus_.unregisterObject(QString::fromLatin1(adrenalin::session1::objectPath));
        });
        OrgAdrenalinlinuxSession1Display1Interface displayProxy(
            QString::fromLatin1(adrenalin::session1::serviceName),
            QString::fromLatin1(adrenalin::session1::objectPath), bus_);
        OrgAdrenalinlinuxSession1Hardware1Interface hardwareProxy(
            QString::fromLatin1(adrenalin::session1::serviceName),
            QString::fromLatin1(adrenalin::session1::objectPath), bus_);
        QVERIFY(displayProxy.isValid());
        QVERIFY(hardwareProxy.isValid());
        QSignalSpy displayEvents(&displayProxy,
            &OrgAdrenalinlinuxSession1Display1Interface::DisplayChanged);
        QSignalSpy hardwareEvents(&hardwareProxy,
            &OrgAdrenalinlinuxSession1Hardware1Interface::InventoryChanged);
        QSignalSpy hardwareCapabilityEvents(&hardwareProxy,
            &OrgAdrenalinlinuxSession1Hardware1Interface::CapabilityGraphChanged);
        QVERIFY(displayEvents.isValid());
        QVERIFY(hardwareEvents.isValid());
        QVERIFY(hardwareCapabilityEvents.isValid());
        QVERIFY(service.initializeAsync());
        QTRY_COMPARE_WITH_TIMEOUT(service.initializationState(), QStringLiteral("READY"), 2000);

        QTRY_COMPARE_WITH_TIMEOUT(displayEvents.count(), 2, 2000);
        auto displaysPending = displayProxy.ListDisplays();
        QTRY_VERIFY_WITH_TIMEOUT(displaysPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::ListReply> displaysPendingReply =
            displaysPending;
        QVERIFY2(!displaysPendingReply.isError(), qPrintable(displaysPendingReply.error().message()));
        const auto displays = displaysPendingReply.value();
        QVERIFY(displays.isValid());
        QCOMPARE(displays.snapshot.code, QStringLiteral("OK"));
        QCOMPARE(displays.displays.size(), 1);
        QCOMPARE(displays.displays.constFirst().subjectId, QString::fromLatin1(displayId));

        auto statePending = displayProxy.GetDisplayState(QString::fromLatin1(displayId));
        QTRY_VERIFY_WITH_TIMEOUT(statePending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::StateReply> statePendingReply =
            statePending;
        QVERIFY2(!statePendingReply.isError(), qPrintable(statePendingReply.error().message()));
        const auto state = statePendingReply.value();
        QVERIFY(state.isValid());
        QCOMPARE(state.display.displayName, QStringLiteral("Integration Display"));
        QCOMPARE(state.capabilities.size(), 1);
        QCOMPARE(state.capabilities.constFirst().supportState, QStringLiteral("UNKNOWN"));

        auto validationPending = displayProxy.ValidateDisplay(operationId,
            QString::fromLatin1(displayId), state.snapshot.inventoryGeneration,
            state.snapshot.capabilityGeneration, {change});
        QTRY_VERIFY_WITH_TIMEOUT(validationPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::ValidationReply> validationReply =
            validationPending;
        QVERIFY2(!validationReply.isError(), qPrintable(validationReply.error().message()));
        const auto validation = validationReply.value();
        QVERIFY(validation.isValid());
        QCOMPARE(validation.code, QStringLiteral("UNSUPPORTED"));
        QVERIFY(!validation.valid);
        QCOMPARE(validation.safetyClass, QStringLiteral("UNKNOWN"));

        auto staleInventoryPending = displayProxy.ValidateDisplay(operationId,
            QString::fromLatin1(displayId), state.snapshot.inventoryGeneration + 1,
            state.snapshot.capabilityGeneration, {change});
        QTRY_VERIFY_WITH_TIMEOUT(staleInventoryPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::ValidationReply>
            staleInventoryReply = staleInventoryPending;
        QVERIFY2(!staleInventoryReply.isError(),
                 qPrintable(staleInventoryReply.error().message()));
        QVERIFY(staleInventoryReply.value().isValid());
        QCOMPARE(staleInventoryReply.value().code, QStringLiteral("CONFLICT"));

        auto staleCapabilityPending = displayProxy.ValidateDisplay(operationId,
            QString::fromLatin1(displayId), state.snapshot.inventoryGeneration,
            state.snapshot.capabilityGeneration + 1, {change});
        QTRY_VERIFY_WITH_TIMEOUT(staleCapabilityPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::ValidationReply>
            staleCapabilityReply = staleCapabilityPending;
        QVERIFY2(!staleCapabilityReply.isError(),
                 qPrintable(staleCapabilityReply.error().message()));
        QVERIFY(staleCapabilityReply.value().isValid());
        QCOMPARE(staleCapabilityReply.value().code, QStringLiteral("STALE_CAPABILITY"));

        auto applyPending = displayProxy.ApplyDisplay(operationId, QString::fromLatin1(displayId),
            state.snapshot.inventoryGeneration, state.snapshot.capabilityGeneration, {change});
        QTRY_VERIFY_WITH_TIMEOUT(applyPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::ApplyReply> applyReply = applyPending;
        QVERIFY2(!applyReply.isError(), qPrintable(applyReply.error().message()));
        const auto applied = applyReply.value();
        QVERIFY(applied.isValid());
        QCOMPARE(applied.code, QStringLiteral("UNSUPPORTED"));
        QCOMPARE(applied.safetyRouteIntent, QStringLiteral("NONE"));
        QVERIFY(!applied.effectiveStateVerified);
        QCOMPARE(applied.revision, quint64(0));
        QCOMPARE(service.eventSequence(), state.snapshot.eventSequence);
        QCOMPARE(displayEvents.count(), 2);
        QCOMPARE(hardwareEvents.count(), 2);
        QCOMPARE(hardwareCapabilityEvents.count(), 2);
        QVariantList displayInventoryEvent;
        QVariantList hardwareDisplayInventoryEvent;
        QVariantList displayCapabilityEvent;
        QVariantList hardwareDisplayCapabilityEvent;
        for (const auto &event : displayEvents) {
            if (event.at(3).toString() == QLatin1String("DISPLAY")) {
                displayInventoryEvent = event;
                break;
            }
        }
        for (const auto &event : hardwareEvents) {
            if (event.at(3).toString() == QLatin1String("DISPLAY")) {
                hardwareDisplayInventoryEvent = event;
                break;
            }
        }
        for (const auto &event : displayEvents) {
            if (event.at(3).toString() == QLatin1String("DISPLAY")
                && event != displayInventoryEvent) {
                displayCapabilityEvent = event;
                break;
            }
        }
        for (const auto &event : hardwareCapabilityEvents) {
            if (event.at(3).toString() == QLatin1String("DISPLAY")) {
                hardwareDisplayCapabilityEvent = event;
                break;
            }
        }
        QVERIFY(!displayInventoryEvent.isEmpty());
        QVERIFY(!displayCapabilityEvent.isEmpty());
        QCOMPARE(displayInventoryEvent, hardwareDisplayInventoryEvent);
        QCOMPARE(displayCapabilityEvent, hardwareDisplayCapabilityEvent);
        auto stateAfterApplyPending = displayProxy.GetDisplayState(QString::fromLatin1(displayId));
        QTRY_VERIFY_WITH_TIMEOUT(stateAfterApplyPending.isFinished(), 2000);
        const QDBusPendingReply<adrenalin::contracts::display1::StateReply>
            stateAfterApplyReply = stateAfterApplyPending;
        QVERIFY2(!stateAfterApplyReply.isError(),
                 qPrintable(stateAfterApplyReply.error().message()));
        const auto stateAfterApply = stateAfterApplyReply.value();
        QVERIFY(stateAfterApply.isValid());
        QCOMPARE(stateAfterApply.snapshot.inventoryGeneration,
                 state.snapshot.inventoryGeneration);
        QCOMPARE(stateAfterApply.snapshot.capabilityGeneration,
                 state.snapshot.capabilityGeneration);
        QCOMPARE(stateAfterApply.capabilities.size(), state.capabilities.size());
        const auto &beforeCapability = state.capabilities.constFirst();
        const auto &afterCapability = stateAfterApply.capabilities.constFirst();
        QCOMPARE(afterCapability.subjectKind, beforeCapability.subjectKind);
        QCOMPARE(afterCapability.subjectId, beforeCapability.subjectId);
        QCOMPARE(afterCapability.capabilityId, beforeCapability.capabilityId);
        QCOMPARE(afterCapability.supportState, beforeCapability.supportState);
        QCOMPARE(afterCapability.providerId, beforeCapability.providerId);
        QCOMPARE(afterCapability.evidenceCode, beforeCapability.evidenceCode);
        QCOMPARE(afterCapability.failureCode, beforeCapability.failureCode);
        QCOMPARE(afterCapability.unit, beforeCapability.unit);
        QCOMPARE(afterCapability.allowedValues, beforeCapability.allowedValues);
        auto compareValue = [](const adrenalin::contracts::hardware1::Value &after,
                               const adrenalin::contracts::hardware1::Value &before) {
            QCOMPARE(after.kind, before.kind);
            QCOMPARE(after.booleanValue, before.booleanValue);
            QCOMPARE(after.signedValue, before.signedValue);
            QCOMPARE(after.unsignedValue, before.unsignedValue);
            QCOMPARE(after.realValue, before.realValue);
            QCOMPARE(after.enumValue, before.enumValue);
        };
        compareValue(afterCapability.configuredValue, beforeCapability.configuredValue);
        compareValue(afterCapability.effectiveValue, beforeCapability.effectiveValue);
        compareValue(afterCapability.minimum, beforeCapability.minimum);
        compareValue(afterCapability.maximum, beforeCapability.maximum);
        compareValue(afterCapability.step, beforeCapability.step);
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
        QCOMPARE(infoReply.argumentAt<1>().manufacturer, QStringLiteral("AuthenticAMD"));
        QCOMPARE(infoReply.argumentAt<1>().model, QStringLiteral("Family 25 Model 97"));

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

    void exportsPersistentProfilesWithSharedEventEnvelope()
    {
        QTemporaryDir dataDirectory;
        QVERIFY(dataDirectory.isValid());
        const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("profiles.sqlite3"));
        SessionService service(databasePath);
        service.setHardware1SnapshotForTesting(validSnapshot());
        Profiles1Adaptor adaptor(&service);
        QVERIFY(bus_.registerObject(QString::fromLatin1(adrenalin::session1::objectPath), &service,
                                    QDBusConnection::ExportAdaptors));
        QVERIFY(bus_.registerService(QString::fromLatin1(adrenalin::session1::serviceName)));
        const auto cleanup = qScopeGuard([this] {
            bus_.unregisterService(QString::fromLatin1(adrenalin::session1::serviceName));
            bus_.unregisterObject(QString::fromLatin1(adrenalin::session1::objectPath));
        });
        OrgAdrenalinlinuxSession1Profiles1Interface proxy(
            QString::fromLatin1(adrenalin::session1::serviceName),
            QString::fromLatin1(adrenalin::session1::objectPath), bus_);
        QVERIFY(proxy.isValid());
        QSignalSpy profileEvents(&proxy,
            &OrgAdrenalinlinuxSession1Profiles1Interface::ProfileChanged);
        QVERIFY(profileEvents.isValid());
        QVERIFY(service.initializeAsync());
        QTRY_COMPARE_WITH_TIMEOUT(service.initializationState(), QStringLiteral("READY"), 2000);

        auto missingPending = proxy.ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        QTRY_VERIFY_WITH_TIMEOUT(missingPending.isFinished(), 2000);
        const QDBusPendingReply<QString, QString, qulonglong, qulonglong,
                                 adrenalin::contracts::profiles1::Profile> missing = missingPending;
        QVERIFY2(!missing.isError(), qPrintable(missing.error().message()));
        QCOMPARE(missing.argumentAt<0>(), QStringLiteral("NOT_FOUND"));
        QVERIFY(!missing.argumentAt<1>().isEmpty());
        QVERIFY(missing.argumentAt<2>() > 0);

        const QString operationId = QStringLiteral("profiles-create-integration-1");
        const QVariantMap patch{{QStringLiteral("graphics.ris"), true},
                                {QStringLiteral("graphics.sharpening"), 70}};
        auto updatePending = proxy.UpdateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"),
            0, operationId, patch);
        QTRY_VERIFY_WITH_TIMEOUT(updatePending.isFinished(), 2000);
        const QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                                 qulonglong> update = updatePending;
        QVERIFY2(!update.isError(), qPrintable(update.error().message()));
        QCOMPARE(update.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(update.argumentAt<1>(), operationId);
        QCOMPARE(update.argumentAt<7>(), qulonglong(1));
        QTRY_COMPARE_WITH_TIMEOUT(profileEvents.count(), 1, 2000);
        QCOMPARE(profileEvents.constFirst().at(2).toULongLong(),
                 missing.argumentAt<3>() + 1);

        auto replayPending = proxy.UpdateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"),
            0, operationId, patch);
        QTRY_VERIFY_WITH_TIMEOUT(replayPending.isFinished(), 2000);
        const QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                                 qulonglong> replay = replayPending;
        QVERIFY2(!replay.isError(), qPrintable(replay.error().message()));
        QCOMPARE(replay.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(profileEvents.count(), 1);

        auto stalePending = proxy.UpdateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"),
            0, QStringLiteral("profiles-stale-integration-1"),
            QVariantMap{{QStringLiteral("graphics.ris"), false}});
        QTRY_VERIFY_WITH_TIMEOUT(stalePending.isFinished(), 2000);
        const QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                                 qulonglong> stale = stalePending;
        QVERIFY2(!stale.isError(), qPrintable(stale.error().message()));
        QCOMPARE(stale.argumentAt<0>(), QStringLiteral("STALE_REVISION"));

        auto readPending = proxy.ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        QTRY_VERIFY_WITH_TIMEOUT(readPending.isFinished(), 2000);
        const QDBusPendingReply<QString, QString, qulonglong, qulonglong,
                                 adrenalin::contracts::profiles1::Profile> read = readPending;
        QVERIFY2(!read.isError(), qPrintable(read.error().message()));
        QCOMPARE(read.argumentAt<0>(), QStringLiteral("OK"));
        const auto profile = read.argumentAt<4>();
        QVERIFY(profile.isValid());
        QCOMPARE(profile.revision, quint64(1));
        QCOMPARE(profile.settings.value(QStringLiteral("graphics.ris")).toBool(), true);
        QCOMPARE(profile.settings.value(QStringLiteral("graphics.sharpening")).toInt(), 70);
        QCOMPARE(read.argumentAt<3>(), profileEvents.constFirst().at(2).toULongLong());
        QCOMPARE(profileEvents.count(), 1);
    }

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
};

} // namespace

QTEST_GUILESS_MAIN(Hardware1SessionServiceTest)
#include "hardware1_session_service_test.moc"
