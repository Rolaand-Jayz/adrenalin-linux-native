#include "interfaces/profiles1_client.h"
#include "session_identity.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QtTest>
#include <QUuid>

#include <utility>

using namespace adrenalin::contracts::profiles1;

namespace {

class ProfilesFixture final : public QDBusVirtualObject
{
public:
    ProfilesFixture()
    {
        instanceUuid_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        profile_.profileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        profile_.subjectKind = QStringLiteral("GLOBAL");
        profile_.subjectId = QStringLiteral("global");
        profile_.referenceState = QStringLiteral("REFERENCE_GATED");
        profile_.revision = 1;
        profile_.settings.insert(QStringLiteral("graphics.sharpening"), 70);
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.freedesktop.DBus.Properties'>"
            "<method name='GetAll'><arg direction='in' type='s'/><arg direction='out' type='a{sv}'/></method>"
            "</interface><interface name='org.adrenalinlinux.Session1.Service1'/>"
            "<interface name='org.adrenalinlinux.Session1.Profiles1'>"
            "<method name='ReadProfile'><arg direction='in' type='s'/><arg direction='in' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='t'/><arg direction='out' type='(sssssta{sv})'/></method>"
            "<signal name='ProfileChanged'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/><arg type='t'/></signal></interface></node>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() == QLatin1String("org.freedesktop.DBus.Properties")
            && message.member() == QLatin1String("GetAll")) {
            const QVariantMap properties{
                {QStringLiteral("InitializationState"), QStringLiteral("READY")},
                {QStringLiteral("ServiceInstanceUuid"), instanceUuid_},
                {QStringLiteral("ServiceGeneration"), QVariant::fromValue<qulonglong>(generation_)},
                {QStringLiteral("EventSequence"), QVariant::fromValue<qulonglong>(sequence_)},
                {QStringLiteral("ApiMajor"), QVariant::fromValue<ushort>(1)},
                {QStringLiteral("ApiMinor"), QVariant::fromValue<ushort>(0)},
                {QStringLiteral("LastInitializationError"), QString{}}
            };
            return connection.send(message.createReply(QVariantList{QVariant::fromValue(properties)}));
        }
        if (message.interface() == QLatin1String("org.adrenalinlinux.Session1.Profiles1")
            && message.member() == QLatin1String("ReadProfile")) {
            ++readCount_;
            const QString kind = message.arguments().at(0).toString();
            const QString id = message.arguments().at(1).toString();
            const Profile response = malformed_ ? Profile{} : profile_;
            const QVariantList reply{QStringLiteral("OK"), instanceUuid_,
                QVariant::fromValue<qulonglong>(generation_),
                QVariant::fromValue<qulonglong>(sequence_), QVariant::fromValue(response)};
            if (holdNextRead_) {
                holdNextRead_ = false;
                pendingRead_ = message;
                pendingReply_ = reply;
                return true;
            }
            Q_UNUSED(kind)
            Q_UNUSED(id)
            return connection.send(message.createReply(reply));
        }
        return false;
    }

    void holdNextRead() { holdNextRead_ = true; }
    bool hasPendingRead() const { return !pendingRead_.path().isEmpty(); }
    int readCount() const { return readCount_; }
    bool releasePendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead()) return false;
        const QDBusMessage request = std::exchange(pendingRead_, QDBusMessage{});
        const QVariantList response = std::exchange(pendingReply_, QVariantList{});
        return connection.send(request.createReply(response));
    }
    bool failPendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead()) return false;
        const QDBusMessage request = std::exchange(pendingRead_, QDBusMessage{});
        pendingReply_.clear();
        return connection.send(request.createErrorReply(
            QStringLiteral("org.adrenalinlinux.Session1.Error.Unavailable"),
            QStringLiteral("fixture read failure")));
    }
    void makeMalformed() { malformed_ = true; }
    void repeatCurrentProfileEvent(const QDBusConnection &connection)
    {
        QDBusMessage common = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        common << instanceUuid_ << qulonglong(generation_) << qulonglong(sequence_)
               << QStringLiteral("GLOBAL") << QStringLiteral("global");
        connection.send(common);
        QDBusMessage specific = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Profiles1"),
            QStringLiteral("ProfileChanged"));
        specific << instanceUuid_ << qulonglong(generation_) << qulonglong(sequence_)
                 << QStringLiteral("GLOBAL") << QStringLiteral("global")
                 << qulonglong(profile_.revision);
        connection.send(specific);
    }
    void rotateService(const QDBusConnection &connection)
    {
        rotateIdentity();
        publishUnrelatedEvent(connection, sequence_);
    }
    void rotateIdentity()
    {
        instanceUuid_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ++generation_;
        sequence_ = 1;
    }
    QString instanceUuid() const { return instanceUuid_; }
    void publishInvalidEvent(const QDBusConnection &connection)
    {
        QDBusMessage common = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        common << instanceUuid_ << qulonglong(generation_) << qulonglong(0)
               << QStringLiteral("SETTINGS") << QStringLiteral("settings.global");
        connection.send(common);
    }
    void publishUnrelatedEvent(const QDBusConnection &connection, quint64 sequence)
    {
        sequence_ = sequence;
        QDBusMessage common = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        common << instanceUuid_ << qulonglong(generation_) << qulonglong(sequence_)
               << QStringLiteral("SETTINGS") << QStringLiteral("settings.global");
        connection.send(common);
    }
    void changeProfile(const QDBusConnection &connection, quint64 sequence)
    {
        sequence_ = sequence;
        ++profile_.revision;
        profile_.settings.insert(QStringLiteral("graphics.sharpening"),
                                 static_cast<int>(profile_.revision * 10));
        QDBusMessage common = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        common << instanceUuid_ << qulonglong(generation_) << qulonglong(sequence_)
               << QStringLiteral("GLOBAL") << QStringLiteral("global");
        connection.send(common);
        QDBusMessage specific = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Profiles1"),
            QStringLiteral("ProfileChanged"));
        specific << instanceUuid_ << qulonglong(generation_) << qulonglong(sequence_)
                 << QStringLiteral("GLOBAL") << QStringLiteral("global")
                 << qulonglong(profile_.revision);
        connection.send(specific);
    }

private:
    QString instanceUuid_;
    Profile profile_;
    quint64 generation_ = 2;
    quint64 sequence_ = 5;
    int readCount_ = 0;
    bool malformed_ = false;
    bool holdNextRead_ = false;
    QDBusMessage pendingRead_;
    QVariantList pendingReply_;
};

class Profiles1ClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void authoritativeReadAndProfileEventRefresh();
    void eventGapRefreshesSnapshot();
    void staleInFlightSnapshotIsDiscarded();
    void initialSnapshotMustCoverFirstUnrelatedEvent();
    void eventDuringFailedSnapshotRetries();
    void duplicateEventsAreIgnoredAndGenerationChangeReconciles();
    void ownerRebindRejectsPriorCallReply();
    void invalidEventEnvelopeCannotAdvanceSnapshot();
    void malformedSnapshotFailsClosed();
};

class BusSetup final
{
public:
    BusSetup()
        : service(QDBusConnection::sessionBus()),
          client(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                QStringLiteral("profiles-client-%1").arg(
                                                    QUuid::createUuid().toString(QUuid::WithoutBraces)))),
          path(QString::fromLatin1(adrenalin::session1::objectPath)),
          name(QString::fromLatin1(adrenalin::session1::serviceName))
    {
        if (!service.isConnected() || !client.isConnected()) return;
        if (!service.registerVirtualObject(path, &fixture)) return;
        registeredObject = true;
        registeredService = service.registerService(name);
    }
    ~BusSetup()
    {
        if (registeredService) service.unregisterService(name);
        if (registeredObject) service.unregisterObject(path);
        QDBusConnection::disconnectFromBus(client.name());
    }
    bool valid() const { return registeredObject && registeredService; }
    QDBusConnection service;
    QDBusConnection client;
    QString path;
    QString name;
    ProfilesFixture fixture;
    bool registeredObject = false;
    bool registeredService = false;
};

void Profiles1ClientTest::authoritativeReadAndProfileEventRefresh()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.status(), QStringLiteral("OK"));
    QCOMPARE(client.profile().revision, quint64(1));
    QCOMPARE(client.serviceGeneration(), quint64(2));
    QCOMPARE(client.eventSequence(), quint64(5));
    const int before = bus.fixture.readCount();
    bus.fixture.changeProfile(bus.service, 6);
    QTRY_COMPARE_WITH_TIMEOUT(client.profile().revision, quint64(2), 3000);
    QCOMPARE(client.eventSequence(), quint64(6));
    QTRY_COMPARE_WITH_TIMEOUT(bus.fixture.readCount(), before + 1, 3000);
}

void Profiles1ClientTest::eventGapRefreshesSnapshot()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const int before = bus.fixture.readCount();
    bus.fixture.changeProfile(bus.service, 8);
    QTRY_COMPARE_WITH_TIMEOUT(client.profile().revision, quint64(2), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(client.eventSequence(), quint64(8), 3000);
    QVERIFY(bus.fixture.readCount() >= before + 1);
}

void Profiles1ClientTest::staleInFlightSnapshotIsDiscarded()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNextRead();
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.hasPendingRead(), 3000);
    bus.fixture.changeProfile(bus.service, 6);
    QVERIFY(bus.fixture.releasePendingRead(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.profile().revision, quint64(2));
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.readCount() >= 2);
}

void Profiles1ClientTest::initialSnapshotMustCoverFirstUnrelatedEvent()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNextRead();
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.hasPendingRead(), 3000);
    bus.fixture.publishUnrelatedEvent(bus.service, 6);
    QTest::qWait(50);
    QVERIFY(bus.fixture.releasePendingRead(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.readCount() >= 2);
}

void Profiles1ClientTest::eventDuringFailedSnapshotRetries()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNextRead();
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.hasPendingRead(), 3000);
    bus.fixture.changeProfile(bus.service, 6);
    QVERIFY(bus.fixture.failPendingRead(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.profile().revision, quint64(2));
    QCOMPARE(client.eventSequence(), quint64(6));
    QVERIFY(bus.fixture.readCount() >= 2);
}

void Profiles1ClientTest::duplicateEventsAreIgnoredAndGenerationChangeReconciles()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    bus.fixture.changeProfile(bus.service, 6);
    QTRY_COMPARE_WITH_TIMEOUT(client.profile().revision, quint64(2), 3000);
    const int afterChange = bus.fixture.readCount();
    bus.fixture.repeatCurrentProfileEvent(bus.service);
    QTest::qWait(100);
    QCOMPARE(bus.fixture.readCount(), afterChange);

    bus.fixture.rotateService(bus.service);
    QTRY_COMPARE_WITH_TIMEOUT(client.serviceGeneration(), quint64(3), 3000);
    QVERIFY(client.ready());
    QCOMPARE(client.eventSequence(), quint64(1));
}

void Profiles1ClientTest::ownerRebindRejectsPriorCallReply()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    bus.fixture.holdNextRead();
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.hasPendingRead(), 3000);
    QVERIFY(bus.service.unregisterService(bus.name));
    bus.fixture.rotateIdentity();
    QVERIFY(bus.service.registerService(bus.name));
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.readCount() >= 2, 3000);
    QVERIFY(bus.fixture.releasePendingRead(bus.service));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.serviceInstanceUuid(), bus.fixture.instanceUuid());
    QCOMPARE(client.serviceGeneration(), quint64(3));
}

void Profiles1ClientTest::invalidEventEnvelopeCannotAdvanceSnapshot()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const int before = bus.fixture.readCount();
    bus.fixture.publishInvalidEvent(bus.service);
    QTRY_VERIFY_WITH_TIMEOUT(bus.fixture.readCount() > before, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), quint64(5));
}

void Profiles1ClientTest::malformedSnapshotFailsClosed()
{
    BusSetup bus;
    QVERIFY(bus.valid());
    bus.fixture.makeMalformed();
    Client client(bus.name, bus.path, QStringLiteral("GLOBAL"), QStringLiteral("global"), bus.client);
    QTRY_COMPARE_WITH_TIMEOUT(client.status(), QStringLiteral("INVALID_RESPONSE"), 3000);
    QVERIFY(!client.ready());
    QVERIFY(client.profile().profileId.isEmpty());
}

} // namespace

QTEST_GUILESS_MAIN(Profiles1ClientTest)
#include "profiles1_client_test.moc"
