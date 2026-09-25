#include "interfaces/hotkeys1_client.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QtTest>
#include <QUuid>

#include <utility>

using namespace adrenalin::contracts::hotkeys1;

namespace {
constexpr auto kName = "org.adrenalinlinux.Hotkeys1ClientTest";
constexpr auto kPath = "/org/adrenalinlinux/Hotkeys1ClientTest";
constexpr auto kService = "org.adrenalinlinux.Session1.Service1";
constexpr auto kHotkeys = "org.adrenalinlinux.Session1.Hotkeys1";

class Fixture final : public QDBusVirtualObject
{
public:
    Fixture() : uuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
        actions_.append(Action{QStringLiteral("game.capture"), QStringLiteral("hotkey.capture.label"),
            QStringLiteral("hotkey.capture.description"), QStringLiteral("Ctrl+Shift+O"),
            QStringLiteral("Ctrl+Shift+O"), QStringLiteral("ACTIVE"), QStringLiteral("test"), {}});
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.freedesktop.DBus.Properties'>"
            "<method name='GetAll'><arg direction='in' type='s'/><arg direction='out' type='a{sv}'/></method>"
            "</interface><interface name='org.adrenalinlinux.Session1.Service1'/>"
            "<interface name='org.adrenalinlinux.Session1.Hotkeys1'>"
            "<method name='ListHotkeys'><arg direction='out' type='(sssbsssbsttt)'/>"
            "<arg direction='out' type='a(ssssssss)'/></method>"
            "<signal name='HotkeyChanged'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/><arg type='t'/></signal>"
            "</interface></node>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() == QLatin1String("org.freedesktop.DBus.Properties")
            && message.member() == QLatin1String("GetAll")) {
            const QVariantMap props{
                {QStringLiteral("InitializationState"), QStringLiteral("READY")},
                {QStringLiteral("ServiceInstanceUuid"), uuid_},
                {QStringLiteral("ServiceGeneration"), QVariant::fromValue<qulonglong>(generation_)},
                {QStringLiteral("EventSequence"), QVariant::fromValue<qulonglong>(sequence_)},
                {QStringLiteral("ApiMajor"), QVariant::fromValue<ushort>(1)},
                {QStringLiteral("ApiMinor"), QVariant::fromValue<ushort>(0)},
                {QStringLiteral("LastInitializationError"), QString{}}
            };
            return connection.send(message.createReply(QVariantList{QVariant::fromValue(props)}));
        }
        if (message.interface() == QLatin1String(kHotkeys)
            && message.member() == QLatin1String("ListHotkeys")) {
            ++reads_;
            const QVariantList args{QVariant::fromValue(reply()), QVariant::fromValue(actions_)};
            if (holdNext_) {
                holdNext_ = false;
                pending_ = message;
                pendingArgs_ = args;
                return true;
            }
            if (failNext_) {
                failNext_ = false;
                return connection.send(message.createErrorReply(
                    QStringLiteral("org.adrenalinlinux.Session1.Error.Unavailable"),
                    QStringLiteral("fixture failure")));
            }
            return connection.send(message.createReply(args));
        }
        return false;
    }

    int reads() const { return reads_; }
    void holdNext() { holdNext_ = true; }
    void failNext() { failNext_ = true; }
    bool pending() const { return !pending_.path().isEmpty(); }
    bool release(const QDBusConnection &connection, bool fail = false)
    {
        if (!pending()) return false;
        const QDBusMessage request = std::exchange(pending_, QDBusMessage{});
        const QVariantList args = std::exchange(pendingArgs_, QVariantList{});
        return connection.send(fail
            ? request.createErrorReply(QStringLiteral("org.adrenalinlinux.Session1.Error.Unavailable"),
                                       QStringLiteral("held fixture failure"))
            : request.createReply(args));
    }
    void publish(const QDBusConnection &connection, quint64 sequence,
                 QString kind = QStringLiteral("SETTINGS"), QString id = QStringLiteral("settings.global"),
                 bool specific = false)
    {
        sequence_ = sequence;
        QDBusMessage common = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kService), QStringLiteral("EventPublished"));
        common << uuid_ << qulonglong(generation_) << qulonglong(sequence_) << kind << id;
        connection.send(common);
        if (specific) {
            QDBusMessage changed = QDBusMessage::createSignal(
                QString::fromLatin1(kPath), QString::fromLatin1(kHotkeys), QStringLiteral("HotkeyChanged"));
            changed << uuid_ << qulonglong(generation_) << qulonglong(sequence_)
                    << kind << id << qulonglong(revision_);
            connection.send(changed);
        }
    }
    void publishInvalidRevision(const QDBusConnection &connection, quint64 sequence)
    {
        sequence_ = sequence;
        QDBusMessage changed = QDBusMessage::createSignal(
            QString::fromLatin1(kPath), QString::fromLatin1(kHotkeys), QStringLiteral("HotkeyChanged"));
        changed << uuid_ << qulonglong(generation_) << qulonglong(sequence_)
                << QStringLiteral("HOTKEY_ACTION") << QStringLiteral("game.capture")
                << qulonglong(0);
        connection.send(changed);
    }
    void changeGeneration(const QDBusConnection &connection)
    {
        uuid_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ++generation_;
        sequence_ = 1;
        publish(connection, sequence_);
    }

private:
    Reply reply() const
    {
        Reply value;
        value.code = QStringLiteral("OK");
        value.snapshotValid = true;
        value.serviceInstanceUuid = uuid_;
        value.serviceGeneration = generation_;
        value.eventSequence = sequence_;
        value.revision = revision_;
        return value;
    }

    QString uuid_;
    quint64 generation_ = 1;
    quint64 sequence_ = 5;
    quint64 revision_ = 1;
    QList<Action> actions_;
    int reads_ = 0;
    bool holdNext_ = false;
    bool failNext_ = false;
    QDBusMessage pending_;
    QVariantList pendingArgs_;
};

class Bus final
{
public:
    Bus()
        : service(QDBusConnection::sessionBus()),
          client(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
              QStringLiteral("hotkeys-client-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))))
    {
        registerObject = service.registerVirtualObject(QString::fromLatin1(kPath), &fixture);
        registerService = registerObject && service.registerService(QString::fromLatin1(kName));
    }
    ~Bus()
    {
        if (registerService) service.unregisterService(QString::fromLatin1(kName));
        if (registerObject) service.unregisterObject(QString::fromLatin1(kPath));
        QDBusConnection::disconnectFromBus(client.name());
    }
    bool valid() const { return registerObject && registerService && client.isConnected(); }
    QDBusConnection service;
    QDBusConnection client;
    Fixture fixture;
    bool registerObject = false;
    bool registerService = false;
};
}

class Hotkeys1ClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void initialSnapshotAndSequentialUnrelatedEvents();
    void duplicatesAreIgnoredAndGapsRefresh();
    void relevantEventRefreshesAndGenerationRolloverReconciles();
    void eventDuringHeldReadFencesOldSnapshot();
    void failedHeldReadRetriesAfterObservedEvent();
    void invalidRevisionEventFencesHeldSnapshot();
    void ownerRolloverRejectsOldReply();
};

void Hotkeys1ClientTest::initialSnapshotAndSequentialUnrelatedEvents()
{
    Bus bus;
    QVERIFY(bus.valid());
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.actions().size(), 1);
    QCOMPARE(client.eventSequence(), quint64(5));
    const int before = bus.fixture.reads();
    bus.fixture.publish(bus.service, 6);
    QTRY_COMPARE_WITH_TIMEOUT(client.eventSequence(), quint64(6), 3000);
    QCOMPARE(bus.fixture.reads(), before);
}

void Hotkeys1ClientTest::duplicatesAreIgnoredAndGapsRefresh()
{
    Bus bus;
    QVERIFY(bus.valid());
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const int before = bus.fixture.reads();
    bus.fixture.publish(bus.service, 5);
    QTest::qWait(50);
    QCOMPARE(bus.fixture.reads(), before);
    bus.fixture.publish(bus.service, 8);
    QTRY_COMPARE_WITH_TIMEOUT(client.eventSequence(), quint64(8), 3000);
    QVERIFY(bus.fixture.reads() > before);
}

void Hotkeys1ClientTest::relevantEventRefreshesAndGenerationRolloverReconciles()
{
    Bus bus;
    QVERIFY(bus.valid());
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const int before = bus.fixture.reads();
    bus.fixture.publish(bus.service, 6, QStringLiteral("HOTKEY_ACTION"), QStringLiteral("game.capture"), true);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.reads() > before, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const quint64 oldGeneration = client.serviceGeneration();
    bus.fixture.changeGeneration(bus.service);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready() && client.serviceGeneration() != oldGeneration, 3000);
    QCOMPARE(client.eventSequence(), quint64(1));
}

void Hotkeys1ClientTest::eventDuringHeldReadFencesOldSnapshot()
{
    Bus bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNext();
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.pending(), 3000);
    bus.fixture.publish(bus.service, 6, QStringLiteral("HOTKEY_ACTION"), QStringLiteral("game.capture"), true);
    QTest::qWait(50);
    QVERIFY(bus.fixture.release(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.reads() >= 2);
}

void Hotkeys1ClientTest::failedHeldReadRetriesAfterObservedEvent()
{
    Bus bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNext();
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.pending(), 3000);
    bus.fixture.publish(bus.service, 6);
    QTest::qWait(50);
    QVERIFY(bus.fixture.release(bus.service, true));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.reads() >= 2);
}

void Hotkeys1ClientTest::invalidRevisionEventFencesHeldSnapshot()
{
    Bus bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNext();
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.pending(), 3000);
    bus.fixture.publishInvalidRevision(bus.service, 6);
    QTest::qWait(50);
    QVERIFY(bus.fixture.release(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.reads() >= 2);
}

void Hotkeys1ClientTest::ownerRolloverRejectsOldReply()
{
    Bus bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNext();
    Client client(QString::fromLatin1(kName), QString::fromLatin1(kPath), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.pending(), 3000);
    QVERIFY(bus.service.unregisterService(QString::fromLatin1(kName)));
    QTRY_VERIFY_WITH_TIMEOUT(!client.ready(), 3000);
    QVERIFY(bus.service.registerService(QString::fromLatin1(kName)));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QVERIFY(bus.fixture.release(bus.service));
    QTest::qWait(50);
    QVERIFY(client.ready());
    QCOMPARE(client.actions().size(), 1);
}

QTEST_GUILESS_MAIN(Hotkeys1ClientTest)
#include "hotkeys1_client_test.moc"
