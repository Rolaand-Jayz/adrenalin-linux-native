#include "interfaces/hardware1_client.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

#include <memory>
#include <utility>

namespace {
using namespace adrenalin::contracts::hardware1;
constexpr auto kService = "org.adrenalinlinux.Hardware1ClientTest";
constexpr auto kPath = "/org/adrenalinlinux/Hardware1ClientTest";
constexpr auto kHardwareInterface = "org.adrenalinlinux.Session1.Hardware1";
constexpr auto kServiceInterface = "org.adrenalinlinux.Session1.Service1";

class HardwareFixture final : public QDBusVirtualObject
{
public:
    explicit HardwareFixture(QObject *parent = nullptr) : QDBusVirtualObject(parent)
    {
        devices_ = {{QStringLiteral("GPU_PCI"), QStringLiteral("gpu.pci.test-0"),
                     QStringLiteral("pci:0000:03:00.0"), QStringLiteral("Fixture GPU")}};
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node>"
            "<interface name='org.adrenalinlinux.Session1.Hardware1'>"
            "<method name='ListDevices'><arg direction='out' type='(sssbsssbstttt)'/>"
            "<arg direction='out' type='a(ssss)'/></method>"
            "<signal name='InventoryChanged'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/><arg type='t'/><arg type='t'/></signal>"
            "<signal name='CapabilityGraphChanged'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/><arg type='t'/><arg type='t'/></signal>"
            "</interface>"
            "<interface name='org.adrenalinlinux.Session1.Service1'>"
            "<signal name='EventPublished'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/></signal>"
            "</interface></node>");
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() != QLatin1String(kHardwareInterface)
            || message.member() != QLatin1String("ListDevices")) {
            return false;
        }
        ++listCount_;
        if (holdNextList_) {
            holdNextList_ = false;
            pendingList_ = message;
            pendingReply_ = makeReply();
            pendingDevices_ = devices_;
            return true;
        }
        return sendListReply(message, connection, makeReply(), devices_);
    }

    bool publishCommon(const QDBusConnection &connection, quint64 sequence,
                       const QString &subjectKind = QStringLiteral("SERVICE"),
                       const QString &subjectId = QStringLiteral("service.readiness"))
    {
        eventSequence_ = sequence;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kServiceInterface),
            QStringLiteral("EventPublished"));
        signal << uuid_ << QVariant::fromValue<qulonglong>(generation_)
               << QVariant::fromValue<qulonglong>(sequence) << subjectKind << subjectId;
        return connection.send(signal);
    }

    bool publishInventory(const QDBusConnection &connection, quint64 sequence)
    {
        eventSequence_ = sequence;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kHardwareInterface),
            QStringLiteral("InventoryChanged"));
        signal << uuid_ << QVariant::fromValue<qulonglong>(generation_)
               << QVariant::fromValue<qulonglong>(sequence) << QStringLiteral("GPU_PCI")
               << QStringLiteral("gpu.pci.test-0") << QVariant::fromValue<qulonglong>(6)
               << QVariant::fromValue<qulonglong>(10);
        return connection.send(signal);
    }

    bool publishCapabilityGraph(const QDBusConnection &connection, quint64 sequence)
    {
        eventSequence_ = sequence;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kHardwareInterface),
            QStringLiteral("CapabilityGraphChanged"));
        signal << uuid_ << QVariant::fromValue<qulonglong>(generation_)
               << QVariant::fromValue<qulonglong>(sequence) << QStringLiteral("GPU_PCI")
               << QStringLiteral("gpu.pci.test-0") << QVariant::fromValue<qulonglong>(6)
               << QVariant::fromValue<qulonglong>(10);
        return connection.send(signal);
    }

    bool releasePendingList(const QDBusConnection &connection)
    {
        if (pendingList_.path().isEmpty()) {
            return false;
        }
        const QDBusMessage request = std::exchange(pendingList_, QDBusMessage{});
        const Reply reply = std::exchange(pendingReply_, Reply{});
        const QList<Device> devices = std::exchange(pendingDevices_, QList<Device>{});
        return sendListReply(request, connection, reply, devices);
    }

    void setGeneration(quint64 generation) { generation_ = generation; }
    void setUuid(const QString &uuid) { uuid_ = uuid; }
    void setCursor(quint64 sequence) { eventSequence_ = sequence; }
    void setDevices(QList<Device> devices) { devices_ = std::move(devices); }
    void holdNextList() { holdNextList_ = true; }
    bool hasPendingList() const { return !pendingList_.path().isEmpty(); }
    int listCount() const { return listCount_; }
    QString uuid() const { return uuid_; }

private:
    Reply makeReply() const
    {
        Reply reply;
        reply.code = QStringLiteral("OK");
        reply.humanMessageKey = QStringLiteral("hardware.read.ok");
        reply.provider = QStringLiteral("hardware1-client-fixture");
        reply.subjectKind = QStringLiteral("PLATFORM");
        reply.subjectId = QStringLiteral("platform");
        reply.snapshotValid = true;
        reply.serviceInstanceUuid = uuid_;
        reply.serviceGeneration = generation_;
        reply.inventoryGeneration = inventoryGeneration_;
        reply.capabilityGeneration = capabilityGeneration_;
        reply.eventSequence = eventSequence_;
        return reply;
    }

    static bool sendListReply(const QDBusMessage &request, const QDBusConnection &connection,
                              const Reply &reply, const QList<Device> &devices)
    {
        return connection.send(request.createReply({QVariant::fromValue(reply),
                                                    QVariant::fromValue(devices)}));
    }

    QString uuid_ = QStringLiteral("123e4567-e89b-42d3-a456-426614174000");
    quint64 generation_ = 3;
    quint64 eventSequence_ = 10;
    quint64 inventoryGeneration_ = 5;
    quint64 capabilityGeneration_ = 9;
    QList<Device> devices_;
    bool holdNextList_ = false;
    int listCount_ = 0;
    QDBusMessage pendingList_;
    Reply pendingReply_;
    QList<Device> pendingDevices_;
};

class PrivateClientConnection final
{
public:
    PrivateClientConnection()
        : name_(QStringLiteral("hardware1-client-%1")
                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))),
          connection_(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name_))
    {
    }

    ~PrivateClientConnection() { QDBusConnection::disconnectFromBus(name_); }
    const QDBusConnection &connection() const { return connection_; }

private:
    QString name_;
    QDBusConnection connection_;
};

} // namespace

class Hardware1ClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void unrelatedSequentialAndDuplicateEventsAdvanceWithoutRefresh();
    void eventGapForcesAuthoritativeRefresh();
    void hardwareSignalsRefreshAndDeduplicateWithCommonEnvelope();
    void changedGenerationAndOwnerLossInvalidateSnapshots();
    void eventDuringSnapshotRejectsStaleReplyAndRefetches();

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<PrivateClientConnection> clientBus_;
    std::unique_ptr<HardwareFixture> fixture_;
    std::unique_ptr<Client> client_;
};

void Hardware1ClientTest::init()
{
    bus_ = QDBusConnection::sessionBus();
    clientBus_ = std::make_unique<PrivateClientConnection>();
    QVERIFY(bus_.isConnected());
    QVERIFY(clientBus_->connection().isConnected());
    fixture_ = std::make_unique<HardwareFixture>();
    QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kPath), fixture_.get()));
    QVERIFY(bus_.registerService(QString::fromLatin1(kService)));
    client_ = std::make_unique<Client>(QString::fromLatin1(kService),
                                       QString::fromLatin1(kPath), clientBus_->connection());
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
}

void Hardware1ClientTest::cleanup()
{
    client_.reset();
    if (fixture_) {
        bus_.unregisterObject(QString::fromLatin1(kPath));
    }
    fixture_.reset();
    bus_.unregisterService(QString::fromLatin1(kService));
    clientBus_.reset();
}

void Hardware1ClientTest::unrelatedSequentialAndDuplicateEventsAdvanceWithoutRefresh()
{
    const int originalReads = fixture_->listCount();
    QVERIFY(fixture_->publishCommon(bus_, 11));
    QTRY_COMPARE(client_->eventSequence(), quint64(11));
    QVERIFY(client_->available());
    QCOMPARE(fixture_->listCount(), originalReads);

    QVERIFY(fixture_->publishCommon(bus_, 11));
    QTest::qWait(30);
    QVERIFY(client_->available());
    QCOMPARE(client_->eventSequence(), quint64(11));
    QCOMPARE(fixture_->listCount(), originalReads);
}

void Hardware1ClientTest::eventGapForcesAuthoritativeRefresh()
{
    fixture_->setCursor(13);
    const int originalReads = fixture_->listCount();
    QVERIFY(fixture_->publishCommon(bus_, 13));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_COMPARE(fixture_->listCount(), originalReads + 1);
    QCOMPARE(client_->eventSequence(), quint64(13));
}

void Hardware1ClientTest::hardwareSignalsRefreshAndDeduplicateWithCommonEnvelope()
{
    const int originalReads = fixture_->listCount();
    fixture_->holdNextList();
    fixture_->setCursor(11);
    fixture_->setDevices({
        {QStringLiteral("GPU_PCI"), QStringLiteral("gpu.pci.test-0"),
         QStringLiteral("pci:0000:03:00.0"), QStringLiteral("Fixture GPU")},
        {QStringLiteral("DISPLAY"), QStringLiteral("display.test-0"),
         QStringLiteral("edid:fixture"), QStringLiteral("Fixture Display")}});
    QVERIFY(fixture_->publishInventory(bus_, 11));
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->hasPendingList(), 3000);
    QVERIFY(!client_->available());
    QVERIFY(fixture_->publishCommon(bus_, 11, QStringLiteral("GPU_PCI"),
                                    QStringLiteral("gpu.pci.test-0")));
    QVERIFY(fixture_->releasePendingList(bus_));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_COMPARE(fixture_->listCount(), originalReads + 1);
    QCOMPARE(client_->devices().size(), 2);
    QCOMPARE(client_->eventSequence(), quint64(11));

    const int afterInventory = fixture_->listCount();
    fixture_->holdNextList();
    fixture_->setCursor(12);
    QVERIFY(fixture_->publishCapabilityGraph(bus_, 12));
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->hasPendingList(), 3000);
    QVERIFY(!client_->available());
    QVERIFY(fixture_->releasePendingList(bus_));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QCOMPARE(fixture_->listCount(), afterInventory + 1);
    QCOMPARE(client_->eventSequence(), quint64(12));
}

void Hardware1ClientTest::changedGenerationAndOwnerLossInvalidateSnapshots()
{
    fixture_->setGeneration(4);
    fixture_->setCursor(1);
    const int originalReads = fixture_->listCount();
    QVERIFY(fixture_->publishCommon(bus_, 1));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_COMPARE(fixture_->listCount(), originalReads + 1);
    QCOMPARE(client_->serviceGeneration(), quint64(4));
    QCOMPARE(client_->eventSequence(), quint64(1));

    const QString previousInstance = client_->serviceInstanceUuid();
    QVERIFY(bus_.unregisterService(QString::fromLatin1(kService)));
    QTRY_VERIFY_WITH_TIMEOUT(!client_->available(), 3000);
    QVERIFY(client_->devices().isEmpty());
    fixture_->setUuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
    fixture_->setGeneration(1);
    fixture_->setCursor(0);
    QVERIFY(bus_.registerService(QString::fromLatin1(kService)));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QVERIFY(client_->serviceInstanceUuid() != previousInstance);
    QCOMPARE(client_->serviceGeneration(), quint64(1));
}

void Hardware1ClientTest::eventDuringSnapshotRejectsStaleReplyAndRefetches()
{
    const int readsBeforeNewClient = fixture_->listCount();
    client_.reset();
    fixture_->holdNextList();
    client_ = std::make_unique<Client>(QString::fromLatin1(kService),
                                       QString::fromLatin1(kPath), clientBus_->connection());
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->hasPendingList(), 3000);

    fixture_->setCursor(11);
    fixture_->setDevices({
        {QStringLiteral("GPU_PCI"), QStringLiteral("gpu.pci.test-0"),
         QStringLiteral("pci:0000:03:00.0"), QStringLiteral("Fresh Fixture GPU")}});
    QVERIFY(fixture_->publishCommon(bus_, 11));
    QVERIFY(fixture_->releasePendingList(bus_));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_COMPARE(fixture_->listCount(), readsBeforeNewClient + 2);
    QCOMPARE(client_->eventSequence(), quint64(11));
    QCOMPARE(client_->devices().constFirst().displayName, QStringLiteral("Fresh Fixture GPU"));
}

QTEST_GUILESS_MAIN(Hardware1ClientTest)
#include "hardware1_client_test.moc"
