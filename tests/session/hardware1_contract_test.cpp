#include "interfaces/hardware1_mock.h"
#include "interfaces/hardware1_contract_types.h"
#include "interfaces/hardware1_registry.h"

#include <hardware1_interface.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QFile>
#include <QDBusVirtualObject>
#include <QTest>

#include <limits>
#include <memory>

namespace {
constexpr auto kServiceName = "org.adrenalinlinux.Hardware1ContractTest";
constexpr auto kObjectPath = "/org/adrenalinlinux/Hardware1ContractTest";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Hardware1";

using namespace adrenalin::contracts::hardware1;

class HardwareFixture final : public QDBusVirtualObject
{
public:
    explicit HardwareFixture(QObject *parent = nullptr) : QDBusVirtualObject(parent) {}

    QString introspect(const QString &) const override
    {
        QFile schema(QStringLiteral(":/hardware1/org.adrenalinlinux.Session1.Hardware1.xml"));
        if (!schema.open(QIODevice::ReadOnly)) {
            return QStringLiteral("<node/>");
        }
        return QString::fromUtf8(schema.readAll());
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() != QLatin1String(kInterfaceName)) {
            return false;
        }
        if (message.member() == QLatin1String("ListDevices")) {
            QList<Device> devices;
            const Reply reply = mock_.listDevices(&devices);
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(reply), QVariant::fromValue(devices)}));
        }
        if (message.member() == QLatin1String("GetDeviceInfo")) {
            const QString kind = message.arguments().value(0).toString();
            const QString id = message.arguments().value(1).toString();
            DeviceInfo info;
            const Reply reply = mock_.getDeviceInfo(kind, id, &info);
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(reply), QVariant::fromValue(info)}));
        }
        if (message.member() == QLatin1String("GetCapabilityGraph")) {
            const QString kind = message.arguments().value(0).toString();
            const QString id = message.arguments().value(1).toString();
            QList<Capability> capabilities;
            const Reply reply = mock_.getCapabilityGraph(kind, id, &capabilities);
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(reply), QVariant::fromValue(capabilities)}));
        }
        return false;
    }

private:
    Mock mock_;
};

class Hardware1ContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
        QVERIFY(bus_.registerService(QString::fromLatin1(kServiceName)));
        fixture_ = std::make_unique<HardwareFixture>();
        QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kObjectPath), fixture_.get()));
        proxy_ = std::make_unique<OrgAdrenalinlinuxSession1Hardware1Interface>(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), bus_);
        QVERIFY(proxy_->isValid());
    }

    void cleanupTestCase()
    {
        proxy_.reset();
        if (fixture_) {
            bus_.unregisterObject(QString::fromLatin1(kObjectPath));
        }
        fixture_.reset();
        bus_.unregisterService(QString::fromLatin1(kServiceName));
    }

    void wireTypesAndIntrospectionMatchTheVersionedSchema()
    {
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Reply>())),
                 QByteArray("(sssbsssbsttt)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Device>())),
                 QByteArray("(ssss)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<DeviceInfo>())),
                 QByteArray("(sssssssssss)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Value>())),
                 QByteArray("(sbxtds)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Capability>())),
                 QByteArray("(ssssssss(sbxtds)(sbxtds)(sbxtds)(sbxtds)(sbxtds)as)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<Device>>())),
                 QByteArray("a(ssss)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<Capability>>())),
                 QByteArray("a(ssssssss(sbxtds)(sbxtds)(sbxtds)(sbxtds)(sbxtds)as)"));

        const QDBusMessage request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
        const QDBusMessage response = bus_.call(request);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage,
                 qPrintable(response.errorMessage()));
        const QString schema = response.arguments().value(0).toString();
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"reply\" type=\"(sssbsssbsttt)\" direction=\"out\"/>")));
        const QString completeEventEnvelope = QStringLiteral(
            "\n      <arg name=\"service_instance_uuid\" type=\"s\"/>"
            "\n      <arg name=\"service_generation\" type=\"t\"/>"
            "\n      <arg name=\"event_sequence\" type=\"t\"/>"
            "\n      <arg name=\"subject_kind\" type=\"s\"/>"
            "\n      <arg name=\"subject_id\" type=\"s\"/>"
            "\n      <arg name=\"inventory_generation\" type=\"t\"/>"
            "\n      <arg name=\"capability_generation\" type=\"t\"/>");
        QVERIFY(schema.contains(QStringLiteral("<signal name=\"InventoryChanged\">")
                                + completeEventEnvelope));
        QVERIFY(schema.contains(QStringLiteral("<signal name=\"CapabilityGraphChanged\">")
                                + completeEventEnvelope));
    }

    void failedMockReadsClearExistingPayloads()
    {
        Mock mock;
        DeviceInfo info;
        QCOMPARE(mock.getDeviceInfo(QStringLiteral("GPU_PCI"), Mock::testGpuSubjectId(), &info).code,
                 QStringLiteral("OK"));
        QVERIFY(!info.displayName.isEmpty());
        const Reply missingInfo = mock.getDeviceInfo(QStringLiteral("GPU_PCI"),
                                                     QStringLiteral("unknown-subject"), &info);
        QCOMPARE(missingInfo.code, QStringLiteral("NOT_FOUND"));
        QVERIFY(info.subjectKind.isEmpty());
        QVERIFY(info.subjectId.isEmpty());
        QVERIFY(info.identityEvidence.isEmpty());
        QVERIFY(info.displayName.isEmpty());
        QVERIFY(info.manufacturer.isEmpty());
        QVERIFY(info.model.isEmpty());
        QVERIFY(info.driverName.isEmpty());
        QVERIFY(info.driverVersion.isEmpty());
        QVERIFY(info.pciAddress.isEmpty());
        QVERIFY(info.connectorIdentity.isEmpty());
        QVERIFY(info.edidIdentityDigest.isEmpty());

        QList<Capability> capabilities;
        QCOMPARE(mock.getCapabilityGraph(QStringLiteral("GPU_PCI"), Mock::testGpuSubjectId(),
                                         &capabilities).code, QStringLiteral("OK"));
        QVERIFY(!capabilities.isEmpty());
        const Reply missingGraph = mock.getCapabilityGraph(QStringLiteral("GPU_PCI"),
                                                           QStringLiteral("unknown-subject"),
                                                           &capabilities);
        QCOMPARE(missingGraph.code, QStringLiteral("NOT_FOUND"));
        QVERIFY(capabilities.isEmpty());
    }

    void listDevicesRoundTripsTypedIdentityAndEnvelope()
    {
        auto pending = proxy_->ListDevices();
        pending.waitForFinished();
        QVERIFY2(!pending.isError(), qPrintable(pending.error().message()));

        const Reply reply = pending.argumentAt<0>();
        const QList<Device> devices = pending.argumentAt<1>();
        QVERIFY(reply.isValid());
        QCOMPARE(reply.code, QStringLiteral("OK"));
        QCOMPARE(reply.humanMessageKey, QStringLiteral("hardware.read.ok"));
        QVERIFY(reply.diagnosticMessage.isEmpty());
        QVERIFY(!reply.retryable);
        QVERIFY(reply.provider.isEmpty());
        QCOMPARE(reply.subjectKind, QStringLiteral("PLATFORM"));
        QCOMPARE(reply.subjectId, QStringLiteral("platform"));
        QVERIFY(reply.snapshotValid);
        QCOMPARE(reply.serviceInstanceUuid,
                 QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
        QCOMPARE(reply.serviceGeneration, quint64(2));
        QCOMPARE(reply.inventoryGeneration, quint64(5));
        QCOMPARE(reply.capabilityGeneration, quint64(9));
        QCOMPARE(devices.size(), 3);
        QCOMPARE(devices.at(0).subjectKind, QStringLiteral("CPU_PACKAGE"));
        QCOMPARE(devices.at(0).subjectId, QStringLiteral("cpu-package-test-0"));
        QCOMPARE(devices.at(0).identityEvidence, QStringLiteral("test.cpu.package"));
        QCOMPARE(devices.at(0).displayName, QStringLiteral("Test CPU Package"));
        QCOMPARE(devices.at(1).subjectKind, QStringLiteral("GPU_PCI"));
        QCOMPARE(devices.at(1).subjectId, Mock::testGpuSubjectId());
        QCOMPARE(devices.at(1).identityEvidence, QStringLiteral("test.gpu.pci"));
        QCOMPARE(devices.at(1).displayName, QStringLiteral("Test Radeon GPU"));
        QCOMPARE(devices.at(2).subjectKind, QStringLiteral("DISPLAY"));
        QCOMPARE(devices.at(2).subjectId, QStringLiteral("display-test-0"));
        QCOMPARE(devices.at(2).identityEvidence, QStringLiteral("test.display.edid"));
        QCOMPARE(devices.at(2).displayName, QStringLiteral("Test Display"));
    }

    void deviceInfoRoundTripsStaticFieldsWithoutRawIdentityMaterial()
    {
        auto pending = proxy_->GetDeviceInfo(QStringLiteral("GPU_PCI"),
                                             Mock::testGpuSubjectId());
        pending.waitForFinished();
        QVERIFY2(!pending.isError(), qPrintable(pending.error().message()));

        const Reply reply = pending.argumentAt<0>();
        const DeviceInfo info = pending.argumentAt<1>();
        QVERIFY(reply.isValid());
        QCOMPARE(info.subjectKind, QStringLiteral("GPU_PCI"));
        QCOMPARE(info.subjectId, Mock::testGpuSubjectId());
        QCOMPARE(info.identityEvidence, QStringLiteral("test.gpu.pci"));
        QCOMPARE(info.displayName, QStringLiteral("Test Radeon GPU"));
        QCOMPARE(info.manufacturer, QStringLiteral("Test Vendor"));
        QCOMPARE(info.model, QStringLiteral("Test Graphics Device"));
        QCOMPARE(info.driverName, QStringLiteral("test.amdgpu"));
        QCOMPARE(info.driverVersion, QStringLiteral("test-version"));
        QCOMPARE(info.pciAddress, QStringLiteral("0000:03:00.0"));
        QVERIFY(info.connectorIdentity.isEmpty());
        QVERIFY(info.edidIdentityDigest.isEmpty());
        QVERIFY(!info.edidIdentityDigest.contains(QLatin1String("serial")));
    }

    void capabilityGraphRoundTripsClosedValuesAndTruthStates()
    {
        auto pending = proxy_->GetCapabilityGraph(QStringLiteral("GPU_PCI"),
                                                  Mock::testGpuSubjectId());
        pending.waitForFinished();
        QVERIFY2(!pending.isError(), qPrintable(pending.error().message()));

        const Reply reply = pending.argumentAt<0>();
        const QList<Capability> capabilities = pending.argumentAt<1>();
        QVERIFY(reply.isValid());
        QCOMPARE(capabilities.size(), 4);
        const Capability frequency = capabilities.at(0);
        QVERIFY2(frequency.isValid(), "frequency record must remain valid after D-Bus marshalling");
        QCOMPARE(frequency.subjectKind, QStringLiteral("GPU_PCI"));
        QCOMPARE(frequency.subjectId, Mock::testGpuSubjectId());
        QCOMPARE(frequency.capabilityId, QStringLiteral("gpu.clock.maximum"));
        QCOMPARE(frequency.supportState, QStringLiteral("SUPPORTED"));
        QCOMPARE(frequency.providerId, QStringLiteral("test.provider.gpu"));
        QCOMPARE(frequency.evidenceCode, QStringLiteral("test.observed.range"));
        QVERIFY(frequency.failureCode.isEmpty());
        QVERIFY(!frequency.configuredValue.booleanValue);
        QCOMPARE(frequency.configuredValue.signedValue, qint64(0));
        QCOMPARE(frequency.configuredValue.kind, QStringLiteral("UNSIGNED_INTEGER"));
        QCOMPARE(frequency.configuredValue.unsignedValue, quint64(2400));
        QVERIFY(qFuzzyIsNull(frequency.configuredValue.realValue));
        QVERIFY(frequency.configuredValue.enumValue.isEmpty());
        QCOMPARE(frequency.effectiveValue.kind, QStringLiteral("UNSIGNED_INTEGER"));
        QCOMPARE(frequency.effectiveValue.unsignedValue, quint64(2250));
        QCOMPARE(frequency.minimum.kind, QStringLiteral("UNSIGNED_INTEGER"));
        QCOMPARE(frequency.minimum.unsignedValue, quint64(500));
        QCOMPARE(frequency.maximum.kind, QStringLiteral("UNSIGNED_INTEGER"));
        QCOMPARE(frequency.maximum.unsignedValue, quint64(3000));
        QCOMPARE(frequency.step.kind, QStringLiteral("UNSIGNED_INTEGER"));
        QCOMPARE(frequency.step.unsignedValue, quint64(1));
        QCOMPARE(frequency.unit, QStringLiteral("megahertz"));
        QVERIFY(frequency.allowedValues.isEmpty());

        const Capability unknown = capabilities.at(1);
        QCOMPARE(unknown.supportState, QStringLiteral("UNKNOWN"));
        QCOMPARE(unknown.configuredValue.kind, QStringLiteral("NONE"));
        QCOMPARE(unknown.effectiveValue.kind, QStringLiteral("NONE"));
        QCOMPARE(unknown.minimum.kind, QStringLiteral("NONE"));
        QCOMPARE(unknown.maximum.kind, QStringLiteral("NONE"));
        QCOMPARE(unknown.step.kind, QStringLiteral("NONE"));
        QVERIFY(unknown.allowedValues.isEmpty());
        QVERIFY(unknown.isValid());

        const Capability unsupported = capabilities.at(2);
        QCOMPARE(unsupported.supportState, QStringLiteral("UNSUPPORTED"));
        QCOMPARE(unsupported.evidenceCode, QStringLiteral("test.provider.no-control"));
        QVERIFY(unsupported.providerId.isEmpty());
        QVERIFY(unsupported.isValid());

        const Capability unavailable = capabilities.at(3);
        QCOMPARE(unavailable.supportState, QStringLiteral("PROVIDER_UNAVAILABLE"));
        QCOMPARE(unavailable.providerId, QStringLiteral("test.provider.telemetry"));
        QCOMPARE(unavailable.failureCode, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(unavailable.isValid());
    }

    void invalidCapabilityValuesFailClosed()
    {
        Capability capability;
        capability.subjectKind = QStringLiteral("GPU_PCI");
        capability.subjectId = Mock::testGpuSubjectId();
        capability.capabilityId = QStringLiteral("gpu.clock.maximum");
        capability.supportState = QStringLiteral("SUPPORTED");
        capability.providerId = QStringLiteral("provider.gpu");
        capability.evidenceCode = QStringLiteral("provider.observed");
        capability.unit = QStringLiteral("megahertz");
        capability.configuredValue.kind = QStringLiteral("REAL");
        capability.configuredValue.realValue = std::numeric_limits<double>::infinity();
        QString error;
        QVERIFY(!capability.isValid(&error));
        QVERIFY(!error.isEmpty());
    }

    void partialRangesAndUnsupportedValuesFailClosed()
    {
        Capability capability;
        capability.subjectKind = QStringLiteral("GPU_PCI");
        capability.subjectId = Mock::testGpuSubjectId();
        capability.capabilityId = QStringLiteral("gpu.clock.maximum");
        capability.supportState = QStringLiteral("SUPPORTED");
        capability.providerId = QStringLiteral("provider.gpu");
        capability.evidenceCode = QStringLiteral("provider.observed");
        capability.unit = QStringLiteral("megahertz");
        capability.minimum.kind = QStringLiteral("UNSIGNED_INTEGER");
        capability.minimum.unsignedValue = 500;
        QString error;
        QVERIFY(!capability.isValid(&error));
        QVERIFY(error.contains(QStringLiteral("partial")));

        capability = {};
        capability.subjectKind = QStringLiteral("GPU_PCI");
        capability.subjectId = Mock::testGpuSubjectId();
        capability.capabilityId = QStringLiteral("gpu.voltage.manual");
        capability.supportState = QStringLiteral("UNSUPPORTED");
        capability.evidenceCode = QStringLiteral("provider.no-control");
        capability.configuredValue.kind = QStringLiteral("REAL");
        capability.configuredValue.realValue = 1.0;
        QVERIFY(!capability.isValid(&error));
    }

    void enumValuesRequireAClosedAllowedSet()
    {
        Capability capability;
        capability.subjectKind = QStringLiteral("GPU_PCI");
        capability.subjectId = Mock::testGpuSubjectId();
        capability.capabilityId = QStringLiteral("gpu.profile.mode");
        capability.supportState = QStringLiteral("SUPPORTED");
        capability.providerId = QStringLiteral("provider.gpu");
        capability.evidenceCode = QStringLiteral("provider.observed");
        capability.configuredValue.kind = QStringLiteral("ENUM");
        capability.configuredValue.enumValue = QStringLiteral("balanced");
        capability.allowedValues = {QStringLiteral("quiet"), QStringLiteral("balanced")};
        QVERIFY(capability.isValid());

        capability.allowedValues = {QStringLiteral("quiet")};
        QVERIFY(!capability.isValid());
    }

    void versionedCapabilityRegistryHasUniqueExplicitScopes()
    {
        const auto &registry = capabilityRegistryV1();
        QVERIFY(registry.size() >= 30);
        QSet<QString> ids;
        for (const auto &definition : registry) {
            QVERIFY(!definition.id.isEmpty());
            QVERIFY2(!ids.contains(definition.id), qPrintable(definition.id));
            ids.insert(definition.id);
            QVERIFY(!definition.subjectKinds.isEmpty());
            QSet<QString> scopes;
            for (const QString &kind : definition.subjectKinds) {
                QVERIFY(isValidSubjectKind(kind));
                QVERIFY(!scopes.contains(kind));
                scopes.insert(kind);
            }
            QCOMPARE(findCapabilityV1(definition.id), &definition);
        }
        QCOMPARE(ids.size(), registry.size());
        QVERIFY(capabilityAppliesToV1(QStringLiteral("gpu.metric.utilization"), QStringLiteral("GPU_PCI")));
        QVERIFY(capabilityAppliesToV1(QStringLiteral("cpu.metric.temperature"), QStringLiteral("CPU_PACKAGE")));
        QVERIFY(capabilityAppliesToV1(QStringLiteral("platform.metric.system_ram"), QStringLiteral("PLATFORM")));
        QVERIFY(capabilityAppliesToV1(QStringLiteral("display.brightness"), QStringLiteral("DISPLAY")));
        QVERIFY(!capabilityAppliesToV1(QStringLiteral("display.brightness"), QStringLiteral("GPU_PCI")));
        QVERIFY(!capabilityAppliesToV1(QStringLiteral("game.metric.fps"), QStringLiteral("GPU_PCI")));
        QVERIFY(!capabilityAppliesToV1(QStringLiteral("unknown.metric"), QStringLiteral("PLATFORM")));
        QVERIFY(subjectKindsForCapabilityV1(QStringLiteral("unknown.metric")).isEmpty());
    }

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<HardwareFixture> fixture_;
    std::unique_ptr<OrgAdrenalinlinuxSession1Hardware1Interface> proxy_;
};

} // namespace

QTEST_GUILESS_MAIN(Hardware1ContractTest)
#include "hardware1_contract_test.moc"
