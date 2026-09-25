#include "interfaces/display1_client.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QtTest>
#include <QUuid>

#include <memory>

namespace {
using namespace adrenalin::contracts;
using namespace adrenalin::contracts::display1;
constexpr auto kService = "org.adrenalinlinux.Display1ClientTest";
constexpr auto kPath = "/org/adrenalinlinux/Display1ClientTest";
constexpr auto kDisplay = "display.test-0";
constexpr auto kDisplayInterface = "org.adrenalinlinux.Session1.Display1";
constexpr auto kHardwareInterface = "org.adrenalinlinux.Session1.Hardware1";
constexpr auto kServiceInterface = "org.adrenalinlinux.Session1.Service1";

class Fixture final : public QDBusVirtualObject
{
public:
    using QDBusVirtualObject::QDBusVirtualObject;
    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node>"
            "<interface name='org.adrenalinlinux.Session1.Display1'>"
            "<method name='ListDisplays'><arg direction='out' type='((sssbsssbstttt)a(ssss))'/></method>"
            "<method name='GetDisplayState'><arg direction='in' type='s'/><arg direction='out' type='((sssbsssbstttt)(ssss)a(ssssssss(sbxtds)(sbxtds)(sbxtds)(sbxtds)(sbxtds)as))'/></method>"
            "<signal name='DisplayChanged'><arg type='s'/><arg type='t'/><arg type='t'/><arg type='s'/><arg type='s'/><arg type='t'/><arg type='t'/></signal>"
            "</interface><interface name='org.adrenalinlinux.Session1.Hardware1'>"
            "<signal name='InventoryChanged'><arg type='s'/><arg type='t'/><arg type='t'/><arg type='s'/><arg type='s'/><arg type='t'/><arg type='t'/></signal>"
            "<signal name='CapabilityGraphChanged'><arg type='s'/><arg type='t'/><arg type='t'/><arg type='s'/><arg type='s'/><arg type='t'/><arg type='t'/></signal>"
            "</interface><interface name='org.adrenalinlinux.Session1.Service1'>"
            "<signal name='EventPublished'><arg type='s'/><arg type='t'/><arg type='t'/><arg type='s'/><arg type='s'/></signal>"
            "</interface></node>");
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != QLatin1String(kDisplayInterface)) return false;
        if (message.member() == QLatin1String("ListDisplays")) {
            ++listCalls;
            ListReply reply; reply.snapshot = makeReply(true);
            reply.displays = {{QStringLiteral("DISPLAY"), QString::fromLatin1(kDisplay),
                               QStringLiteral("gpu=fixture;connector=DP-1;edid=test"),
                               QStringLiteral("Fixture display")}};
            return connection.send(message.createReply({QVariant::fromValue(reply)}));
        }
        if (message.member() == QLatin1String("GetDisplayState")) {
            ++stateCalls;
            StateReply reply; reply.snapshot = makeReply(false);
            reply.snapshot.subjectKind = QStringLiteral("DISPLAY");
            reply.snapshot.subjectId = message.arguments().value(0).toString();
            reply.display = {QStringLiteral("DISPLAY"), reply.snapshot.subjectId,
                             QStringLiteral("gpu=fixture;connector=DP-1;edid=test"),
                             QStringLiteral("Fixture display")};
            if (failState) {
                reply.snapshot.code = QStringLiteral("NOT_FOUND");
                reply.display = {};
                reply.capabilities.clear();
            }
            if (holdState) { pendingState = message; savedState = reply; return true; }
            return connection.send(message.createReply({QVariant::fromValue(reply)}));
        }
        return false;
    }
    hardware1::Reply makeReply(bool platform) const
    {
        hardware1::Reply reply; reply.code = QStringLiteral("OK");
        reply.humanMessageKey = QStringLiteral("display.read.ok");
        reply.provider = QStringLiteral("fixture");
        reply.subjectKind = platform ? QStringLiteral("PLATFORM") : QStringLiteral("DISPLAY");
        reply.subjectId = platform ? QStringLiteral("platform") : QString::fromLatin1(kDisplay);
        reply.snapshotValid = true; reply.serviceInstanceUuid = uuid;
        reply.serviceGeneration = generation; reply.inventoryGeneration = inventory;
        reply.capabilityGeneration = capability; reply.eventSequence = cursor;
        return reply;
    }
    bool emitEvent(const QDBusConnection &connection, const QString &interface,
                   const QString &member, quint64 sequence, bool common = false,
                   quint64 inv = 5, quint64 cap = 9)
    {
        cursor = sequence;
        QDBusMessage signal = QDBusMessage::createSignal(QString::fromLatin1(kPath), interface,
                                                          member);
        signal << uuid << QVariant::fromValue<qulonglong>(generation)
               << QVariant::fromValue<qulonglong>(sequence) << QStringLiteral("DISPLAY")
               << QString::fromLatin1(kDisplay);
        if (!common) signal << QVariant::fromValue<qulonglong>(inv)
                            << QVariant::fromValue<qulonglong>(cap);
        return connection.send(signal);
    }
    bool emitUnrelatedCommon(const QDBusConnection &connection, quint64 sequence)
    {
        cursor = sequence;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kServiceInterface),
            QStringLiteral("EventPublished"));
        signal << uuid << QVariant::fromValue<qulonglong>(generation)
               << QVariant::fromValue<qulonglong>(sequence) << QStringLiteral("SERVICE")
               << QStringLiteral("service.readiness");
        return connection.send(signal);
    }
    bool releaseHeldState(const QDBusConnection &connection)
    {
        if (pendingState.path().isEmpty()) return false;
        return connection.send(pendingState.createReply({QVariant::fromValue(savedState)}));
    }
    QString uuid = QStringLiteral("123e4567-e89b-42d3-a456-426614174000");
    quint64 generation = 2, inventory = 5, capability = 9, cursor = 10;
    int listCalls = 0, stateCalls = 0;
    bool holdState = false;
    bool failState = false;
    QDBusMessage pendingState;
    StateReply savedState;
};

class PrivateConnection final
{
public:
    PrivateConnection() : name(QStringLiteral("display-client-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces))),
        connection(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name)) {}
    ~PrivateConnection() { QDBusConnection::disconnectFromBus(name); }
    QString name; QDBusConnection connection;
};
}

class Display1ClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void coherentSnapshotAndMirroredEventDeduplication();
    void commonEventGapRefreshesAndDuplicateDoesNot();
    void staleInFlightReplyIsRejected();
    void ownerChangeClearsAndReconciles();
    void serviceGenerationRolloverClearsEventDeduplication();
    void persistentStateFailureDoesNotRetryLoop();
private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<PrivateConnection> clientBus_;
    std::unique_ptr<Fixture> fixture_;
    std::unique_ptr<Client> client_;
};

void Display1ClientTest::init()
{
    bus_ = QDBusConnection::sessionBus();
    clientBus_ = std::make_unique<PrivateConnection>();
    hardware1::registerMetaTypes(); display1::registerMetaTypes();
    QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kPath), (fixture_ = std::make_unique<Fixture>()).get()));
    QVERIFY(bus_.registerService(QString::fromLatin1(kService)));
    client_ = std::make_unique<Client>(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                       clientBus_->connection);
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QCOMPARE(client_->displays().size(), 1);
}
void Display1ClientTest::cleanup()
{
    client_.reset();
    if (fixture_) bus_.unregisterObject(QString::fromLatin1(kPath));
    fixture_.reset(); bus_.unregisterService(QString::fromLatin1(kService)); clientBus_.reset();
}
void Display1ClientTest::coherentSnapshotAndMirroredEventDeduplication()
{
    QCOMPARE(client_->inventoryGeneration(), quint64(5));
    const int reads = fixture_->listCalls;
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kHardwareInterface),
                                QStringLiteral("InventoryChanged"), 11));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kDisplayInterface),
                                QStringLiteral("DisplayChanged"), 11));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kServiceInterface),
                                QStringLiteral("EventPublished"), 11, true));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->listCalls >= reads + 1, 3000);
    QTest::qWait(30);
    QCOMPARE(fixture_->listCalls, reads + 1);

    const int secondReads = fixture_->listCalls;
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kServiceInterface),
                                QStringLiteral("EventPublished"), 12, true));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kHardwareInterface),
                                QStringLiteral("CapabilityGraphChanged"), 12));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kDisplayInterface),
                                QStringLiteral("DisplayChanged"), 12));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->listCalls >= secondReads + 1, 3000);
    QTest::qWait(30); QCOMPARE(fixture_->listCalls, secondReads + 1);

    const int thirdReads = fixture_->listCalls;
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kDisplayInterface),
                                QStringLiteral("DisplayChanged"), 13));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kServiceInterface),
                                QStringLiteral("EventPublished"), 13, true));
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kHardwareInterface),
                                QStringLiteral("InventoryChanged"), 13));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->listCalls >= thirdReads + 1, 3000);
    QTest::qWait(30); QCOMPARE(fixture_->listCalls, thirdReads + 1);
}
void Display1ClientTest::commonEventGapRefreshesAndDuplicateDoesNot()
{
    const int reads = fixture_->listCalls;
    QVERIFY(fixture_->emitUnrelatedCommon(bus_, 11));
    QTRY_COMPARE_WITH_TIMEOUT(client_->eventSequence(), quint64(11), 3000);
    QCOMPARE(fixture_->listCalls, reads);
    QVERIFY(fixture_->emitUnrelatedCommon(bus_, 11));
    QTest::qWait(30); QCOMPARE(fixture_->listCalls, reads);
    QVERIFY(fixture_->emitUnrelatedCommon(bus_, 13));
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->listCalls > reads, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
}
void Display1ClientTest::staleInFlightReplyIsRejected()
{
    fixture_->holdState = true; const int reads = fixture_->listCalls;
    client_->refresh(); QTRY_VERIFY_WITH_TIMEOUT(!fixture_->pendingState.path().isEmpty(), 3000);
    QVERIFY(fixture_->emitEvent(bus_, QString::fromLatin1(kDisplayInterface),
                                QStringLiteral("DisplayChanged"), client_->eventSequence() + 1,
                                false, 6, 10));
    QTRY_VERIFY_WITH_TIMEOUT(!client_->available(), 3000);
    QCOMPARE(client_->eventSequence(), quint64(0));
    QCOMPARE(client_->inventoryGeneration(), quint64(0));
    QCOMPARE(client_->capabilityGeneration(), quint64(0));
    fixture_->holdState = false;
    QVERIFY(fixture_->releaseHeldState(bus_)); fixture_->pendingState = {};
    QTRY_VERIFY_WITH_TIMEOUT(fixture_->listCalls > reads, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
}
void Display1ClientTest::ownerChangeClearsAndReconciles()
{
    const QString prior = client_->serviceInstanceUuid();
    bus_.unregisterService(QString::fromLatin1(kService));
    QTRY_VERIFY_WITH_TIMEOUT(!client_->available(), 3000);
    fixture_->uuid = QStringLiteral("223e4567-e89b-42d3-a456-426614174000");
    QVERIFY(bus_.registerService(QString::fromLatin1(kService)));
    QTRY_VERIFY_WITH_TIMEOUT(client_->available(), 3000);
    QVERIFY(client_->serviceInstanceUuid() != prior);
}

void Display1ClientTest::serviceGenerationRolloverClearsEventDeduplication()
{
    const int reads = fixture_->listCalls;
    fixture_->holdState = true;
    client_->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!fixture_->pendingState.path().isEmpty(), 3000);
    fixture_->generation = 3;
    QVERIFY(fixture_->emitUnrelatedCommon(bus_, 1));
    QTRY_VERIFY_WITH_TIMEOUT(!client_->available(), 3000);
    fixture_->holdState = false;
    QVERIFY(fixture_->releaseHeldState(bus_));
    fixture_->pendingState = {};
    QTRY_COMPARE_WITH_TIMEOUT(client_->serviceGeneration(), quint64(3), 3000);
    QVERIFY(client_->available());
    QCOMPARE(client_->eventSequence(), quint64(1));
    QTRY_COMPARE_WITH_TIMEOUT(fixture_->listCalls, reads + 2, 3000);

    QVERIFY(fixture_->emitUnrelatedCommon(bus_, 2));
    QTRY_COMPARE_WITH_TIMEOUT(client_->eventSequence(), quint64(2), 3000);
    QCOMPARE(client_->serviceGeneration(), quint64(3));
    QCOMPARE(fixture_->listCalls, reads + 2);
}

void Display1ClientTest::persistentStateFailureDoesNotRetryLoop()
{
    fixture_->failState = true;
    const int reads = fixture_->listCalls;
    const int states = fixture_->stateCalls;
    client_->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client_->available(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(fixture_->listCalls, reads + 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(fixture_->stateCalls, states + 1, 3000);
    QTest::qWait(100);
    QCOMPARE(fixture_->listCalls, reads + 1);
    QCOMPARE(fixture_->stateCalls, states + 1);
}

QTEST_GUILESS_MAIN(Display1ClientTest)
#include "display1_client_test.moc"
