#include "../../interfaces/telemetry1_abi_v1.h"
#include "../../interfaces/telemetry1_fixture_mock.h"
#include "../../interfaces/telemetry1_types.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QDBusVirtualObject>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>

using namespace adrenalin::contracts::telemetry1;
using namespace adrenalin::telemetry1::abi_v1;

constexpr auto kObjectPath = "/org/adrenalinlinux/Session";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Telemetry1";

class EventReceiver final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
public slots:
    void receive(const QString &uuid, quint64 serviceGeneration, quint64 eventSequence,
                 const QString &subjectKind, const QString &subjectId,
                 quint64 producerGeneration, quint64 metricGeneration,
                 quint64 subjectGeneration) {
        emit observed(uuid, serviceGeneration, eventSequence, subjectKind, subjectId,
                      producerGeneration, metricGeneration, subjectGeneration);
    }
signals:
    void observed(QString, quint64, quint64, QString, QString, quint64, quint64, quint64);
};

class TelemetryObject final : public QDBusVirtualObject {
public:
    explicit TelemetryObject(FixtureMock &mock) : m_mock(mock) {}
    bool publishDefinitionsChanged(const QDBusConnection &connection) const {
        const auto result = m_mock.openResult();
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
            QStringLiteral("DefinitionsChanged"));
        signal.setArguments({result.serviceInstanceUuid, QVariant::fromValue<quint64>(1),
            QVariant::fromValue<quint64>(27), QStringLiteral("PLATFORM"),
            QStringLiteral("platform"), QVariant::fromValue<quint64>(result.producerGeneration),
            QVariant::fromValue<quint64>(result.metricDefinitionGeneration + 1),
            QVariant::fromValue<quint64>(result.subjectDefinitionGeneration)});
        return connection.send(signal);
    }
    QString introspect(const QString &) const override {
        QFile schema(QStringLiteral(":/telemetry/org.adrenalinlinux.Session1.Telemetry1.xml"));
        if (!schema.open(QIODevice::ReadOnly)) return {};
        return QString::fromUtf8(schema.readAll());
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
        if (message.member() != QLatin1String("OpenStream") || message.arguments().size() != 1)
            return false;
        const auto requested = message.arguments().constFirst().toUInt();
        if (!m_mock.isValid()) return false;
        if (requested != kMajor) {
            OpenResult rejected;
            rejected.code = QStringLiteral("INCOMPATIBLE_VERSION");
            rejected.diagnostic = QStringLiteral("requested ABI major is unsupported");
            rejected.serviceInstanceUuid = m_mock.openResult().serviceInstanceUuid;
            rejected.serviceGeneration = 1;
            const auto reply = message.createReply({
                QVariant::fromValue(rejected),
                QVariant::fromValue(QList<QDBusUnixFileDescriptor>()),
                QVariant::fromValue(QList<MetricDefinition>()),
                QVariant::fromValue(QList<SubjectDefinition>())
            });
            return connection.send(reply);
        }
        const auto reply = message.createReply({
            QVariant::fromValue(m_mock.openResult()),
            QVariant::fromValue(QList<QDBusUnixFileDescriptor>{m_mock.readOnlyHandle()}),
            QVariant::fromValue(m_mock.metrics()),
            QVariant::fromValue(m_mock.subjects())
        });
        return connection.send(reply);
    }
private:
    FixtureMock &m_mock;
};

class TelemetryPrivateBusTest final : public QObject {
    Q_OBJECT
private slots:
    void openReturnsReadOnlyHandleAndGenerationBoundDefinitions() {
        registerMetaTypes();
        FixtureMock mock;
        QVERIFY(mock.isValid());
        QCOMPARE(::fcntl(mock.readerDescriptor(), F_GETFL) & O_ACCMODE, O_RDONLY);
        auto connection = QDBusConnection::sessionBus();
        QVERIFY(connection.isConnected());
        constexpr auto service = "org.adrenalinlinux.TelemetryContractFixture";
        const QString path = QString::fromLatin1(kObjectPath);
        const QString interfaceName = QString::fromLatin1(kInterfaceName);
        TelemetryObject object(mock);
        QVERIFY(connection.registerService(QString::fromLatin1(service)));
        QVERIFY(connection.registerVirtualObject(path, &object, QDBusConnection::SingleNode));
        EventReceiver eventReceiver;
        QSignalSpy events(&eventReceiver, &EventReceiver::observed);
        QVERIFY(events.isValid());
        QVERIFY(connection.connect(QString::fromLatin1(service), path, interfaceName,
            QStringLiteral("DefinitionsChanged"), &eventReceiver,
            SLOT(receive(QString,quint64,quint64,QString,QString,quint64,quint64,quint64))));
        QDBusInterface client(QString::fromLatin1(service), path, interfaceName, connection);
        const auto reply = client.call(QStringLiteral("OpenStream"), QVariant::fromValue<quint16>(kMajor));
        QVERIFY2(reply.type() == QDBusMessage::ReplyMessage, qPrintable(reply.errorMessage()));
        QCOMPARE(reply.arguments().size(), 4);
        const auto result = qdbus_cast<OpenResult>(reply.arguments().at(0));
        QCOMPARE(result.code, QStringLiteral("OK"));
        QCOMPARE(result.abiMajor, kMajor);
        QCOMPARE(result.mappedSize, quint64(kMappedSize));
        QCOMPARE(result.eventSequenceCursor, quint64(26));
        QVERIFY(result.serviceGeneration > 0 && result.producerGeneration > 0);
        QVERIFY(result.metricDefinitionGeneration > 0 && result.subjectDefinitionGeneration > 0);
        const auto handles = qdbus_cast<QList<QDBusUnixFileDescriptor>>(reply.arguments().at(1));
        QCOMPARE(handles.size(), 1);
        const auto handle = handles.constFirst();
        QVERIFY(handle.isValid());
        QCOMPARE(::fcntl(handle.fileDescriptor(), F_GETFL) & O_ACCMODE, O_RDONLY);
        void *writable = ::mmap(nullptr, kMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                                handle.fileDescriptor(), 0);
        QVERIFY(writable == MAP_FAILED);
        void *mapped = ::mmap(nullptr, kMappedSize, PROT_READ, MAP_SHARED,
                              handle.fileDescriptor(), 0);
        QVERIFY(mapped != MAP_FAILED);
        const auto *header = static_cast<const Header *>(mapped);
        QCOMPARE(std::memcmp(header->magic, kMagic, sizeof(kMagic)), 0);
        QCOMPARE(header->abi_major, result.abiMajor);
        QCOMPARE(header->mapped_size, result.mappedSize);
        QCOMPARE(header->service_generation, result.serviceGeneration);
        QCOMPARE(header->producer_generation, result.producerGeneration);
        QCOMPARE(header->metric_definition_generation, result.metricDefinitionGeneration);
        QCOMPARE(header->subject_definition_generation, result.subjectDefinitionGeneration);
        const auto *sample = reinterpret_cast<const Slot *>(
            static_cast<const char *>(mapped) + kHeaderSize);
        QCOMPARE(sample->sequence_guard, std::uint64_t(2));
        QVERIFY((sample->sequence_guard & 1U) == 0U);
        QCOMPARE(sample->sample_sequence, std::uint64_t(1));
        QCOMPARE(sample->state, static_cast<std::uint32_t>(SampleState::Valid));
        QCOMPARE(sample->encoding,
                 static_cast<std::uint32_t>(MetricEncoding::UnsignedMicroUnits));
        const auto metrics = qdbus_cast<QList<MetricDefinition>>(reply.arguments().at(2));
        const auto subjects = qdbus_cast<QList<SubjectDefinition>>(reply.arguments().at(3));
        QCOMPARE(metrics.size(), 1);
        QCOMPARE(metrics.constFirst().metricId, QStringLiteral("gpu.fixture.utilization"));
        QCOMPARE(subjects.size(), 1);
        QCOMPARE(subjects.constFirst().subjectKind, QStringLiteral("GPU_PCI"));
        QVERIFY(!subjects.constFirst().subjectId.isEmpty());
        QVERIFY((::fcntl(mock.readerDescriptor(), F_GETFD) & FD_CLOEXEC) != 0);
        QVERIFY((::fcntl(handle.fileDescriptor(), F_GETFD) & FD_CLOEXEC) != 0);

        QVERIFY(object.publishDefinitionsChanged(connection));
        QTRY_COMPARE(events.size(), 1);
        const auto event = events.takeFirst();
        QCOMPARE(event.size(), 8);
        QCOMPARE(event.at(0).toString(), result.serviceInstanceUuid);
        QCOMPARE(event.at(1).toULongLong(), result.serviceGeneration);
        QCOMPARE(event.at(2).toULongLong(), qulonglong(27));
        QCOMPARE(event.at(3).toString(), QStringLiteral("PLATFORM"));
        QCOMPARE(event.at(4).toString(), QStringLiteral("platform"));
        QCOMPARE(event.at(5).toULongLong(), result.producerGeneration);
        QCOMPARE(event.at(6).toULongLong(), result.metricDefinitionGeneration + 1);
        QCOMPARE(event.at(7).toULongLong(), result.subjectDefinitionGeneration);
        ::munmap(mapped, kMappedSize);
        const auto incompatible = client.call(QStringLiteral("OpenStream"),
                                              QVariant::fromValue<quint16>(kMajor + 1));
        QVERIFY(incompatible.type() == QDBusMessage::ReplyMessage);
        const auto rejected = qdbus_cast<OpenResult>(incompatible.arguments().at(0));
        QCOMPARE(rejected.code, QStringLiteral("INCOMPATIBLE_VERSION"));
        QCOMPARE(qdbus_cast<QList<QDBusUnixFileDescriptor>>(incompatible.arguments().at(1)).size(), 0);
        QCOMPARE(qdbus_cast<QList<MetricDefinition>>(incompatible.arguments().at(2)).size(), 0);
        connection.unregisterObject(path);
        connection.unregisterService(QString::fromLatin1(service));
    }
};

QTEST_GUILESS_MAIN(TelemetryPrivateBusTest)
#include "telemetry1_contract_test.moc"
