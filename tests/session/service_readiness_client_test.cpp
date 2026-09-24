#include "interfaces/service_readiness_client.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

#include <memory>
#include <utility>

namespace {
constexpr auto kTestServiceName = "org.adrenalinlinux.ReadinessTest1";
constexpr auto kTestObjectPath = "/org/adrenalinlinux/ReadinessTest";
constexpr auto kReadinessInterface = "org.adrenalinlinux.Session1.Service1";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

class PrivateClientConnection final
{
public:
    PrivateClientConnection()
        : name_(QStringLiteral("adrenalin-readiness-client-%1")
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

class ReadinessFixture final : public QDBusVirtualObject
{
public:
    explicit ReadinessFixture(ushort apiMajor = 1, QString state = QStringLiteral("READY"))
        : apiMajor_(apiMajor), state_(std::move(state)),
          instanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.freedesktop.DBus.Properties'>"
            "<method name='GetAll'><arg direction='in' type='s'/>"
            "<arg direction='out' type='a{sv}'/></method>"
            "<signal name='PropertiesChanged'><arg type='s'/><arg type='a{sv}'/>"
            "<arg type='as'/></signal></interface>"
            "<interface name='org.adrenalinlinux.Session1.Service1'>"
            "<property name='InitializationState' type='s' access='read'/>"
            "<property name='ServiceInstanceUuid' type='s' access='read'/>"
            "<property name='ServiceGeneration' type='t' access='read'/>"
            "<property name='EventSequence' type='t' access='read'/>"
            "<property name='ApiMajor' type='q' access='read'/>"
            "<property name='ApiMinor' type='q' access='read'/>"
            "<property name='LastInitializationError' type='s' access='read'/>"
            "<signal name='EventPublished'><arg type='s'/><arg type='t'/>"
            "<arg type='t'/><arg type='s'/></signal>"
            "</interface></node>");
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() != QString::fromLatin1(kPropertiesInterface)
            || message.member() != QStringLiteral("GetAll")
            || message.arguments().size() != 1
            || message.arguments().at(0).toString() != QString::fromLatin1(kReadinessInterface)) {
            return false;
        }
        ++getAllCount_;
        if (holdNextGetAll_) {
            holdNextGetAll_ = false;
            pendingGetAll_ = message;
            pendingSnapshot_ = snapshot();
            return true;
        }
        return connection.send(message.createReply(snapshot()));
    }

    bool publish(const QDBusConnection &connection, QString state, QString error = {},
                 bool incrementGeneration = true)
    {
        const qulonglong previousGeneration = generation_;
        const QString previousError = lastError_;
        state_ = std::move(state);
        lastError_ = std::move(error);
        if (incrementGeneration) {
            ++generation_;
        }
        ++eventSequence_;
        QDBusMessage eventSignal = QDBusMessage::createSignal(
            QString::fromLatin1(kTestObjectPath), QString::fromLatin1(kReadinessInterface),
            QStringLiteral("EventPublished"));
        eventSignal << instanceUuid_ << generation_ << eventSequence_
                    << QStringLiteral("service.readiness");
        const bool eventSent = connection.send(eventSignal);
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kTestObjectPath), QString::fromLatin1(kPropertiesInterface),
            QStringLiteral("PropertiesChanged"));
        QVariantMap changedProperties{
            {QStringLiteral("InitializationState"), state_},
            {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)}};
        if (generation_ != previousGeneration) {
            changedProperties.insert(QStringLiteral("ServiceGeneration"),
                                     QVariant::fromValue(generation_));
        }
        if (lastError_ != previousError) {
            changedProperties.insert(QStringLiteral("LastInitializationError"), lastError_);
        }
        signal << QString::fromLatin1(kReadinessInterface) << changedProperties << QStringList{};
        return eventSent && connection.send(signal);
    }

    bool publishDuplicate(const QDBusConnection &connection)
    {
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kTestObjectPath), QString::fromLatin1(kReadinessInterface),
            QStringLiteral("EventPublished"));
        signal << instanceUuid_ << generation_ << eventSequence_
               << QStringLiteral("service.readiness");
        return connection.send(signal);
    }

    void skipEventSequences(qulonglong count) { eventSequence_ += count; }

    void holdNextGetAll() { holdNextGetAll_ = true; }
    bool hasPendingGetAll() const { return !pendingGetAll_.path().isEmpty(); }
    int getAllCount() const { return getAllCount_; }
    QString instanceUuid() const { return instanceUuid_; }

    bool releasePendingGetAll(const QDBusConnection &connection)
    {
        if (!hasPendingGetAll()) {
            return false;
        }
        const QDBusMessage request = std::exchange(pendingGetAll_, QDBusMessage{});
        const QVariantMap properties = std::exchange(pendingSnapshot_, QVariantMap{});
        return connection.send(request.createReply(properties));
    }

private:
    QVariantMap snapshot() const
    {
        return QVariantMap{
            {QStringLiteral("InitializationState"), state_},
            {QStringLiteral("ServiceInstanceUuid"), instanceUuid_},
            {QStringLiteral("ServiceGeneration"), QVariant::fromValue(generation_)},
            {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)},
            {QStringLiteral("ApiMajor"), QVariant::fromValue(apiMajor_)},
            {QStringLiteral("ApiMinor"), QVariant::fromValue(apiMinor_)},
            {QStringLiteral("LastInitializationError"), lastError_}
        };
    }

    ushort apiMajor_ = 1;
    ushort apiMinor_ = 0;
    QString state_;
    QString lastError_;
    qulonglong generation_ = 4;
    qulonglong eventSequence_ = 0;
    QString instanceUuid_;
    bool holdNextGetAll_ = false;
    QDBusMessage pendingGetAll_;
    QVariantMap pendingSnapshot_;
    int getAllCount_ = 0;
};

class ServiceReadinessClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void readinessSnapshotAndChangesAreReconciled();
    void readinessEventsReconcileGapsAndIgnoreDuplicates();
    void duplicateDuringReconciliationDoesNotQueueAnotherSnapshot();
    void pendingSnapshotCannotUndoAPropertyChange();
    void unsupportedApiMajorFailsClosed();
    void ownerReplacementReconcilesNewInstance();
};

void ServiceReadinessClientTest::cleanup()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterService(QString::fromLatin1(kTestServiceName));
    bus.unregisterObject(QString::fromLatin1(kTestObjectPath));
}

void ServiceReadinessClientTest::readinessSnapshotAndChangesAreReconciled()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(bus.isConnected());
    QVERIFY(clientBus.connection().isConnected());
    auto fixture = std::make_unique<ReadinessFixture>(1, QStringLiteral("RECOVERING"));
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), fixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
    QVERIFY(client.compatible());
    QVERIFY(!client.ready());
    QCOMPARE(client.status(), QStringLiteral("RECOVERING"));
    QCOMPARE(client.initializationState(), QStringLiteral("RECOVERING"));
    QCOMPARE(client.serviceInstanceUuid(), fixture->instanceUuid());
    QCOMPARE(client.serviceGeneration(), qulonglong(4));
    QCOMPARE(client.apiMajor(), ushort(1));
    QCOMPARE(client.apiMinor(), ushort(0));
    QVERIFY(client.lastInitializationError().isEmpty());

    QSignalSpy changedSpy(&client, &ServiceReadinessClient::stateChanged);
    QVERIFY(changedSpy.isValid());
    QVERIFY(fixture->publish(bus, QStringLiteral("FAILED"),
                             QStringLiteral("recovery could not be verified")));
    QTRY_COMPARE_WITH_TIMEOUT(client.initializationState(), QStringLiteral("FAILED"), 3000);
    QVERIFY(!client.ready());
    QCOMPARE(client.lastInitializationError(),
             QStringLiteral("recovery could not be verified"));
    QVERIFY(changedSpy.count() >= 2);

    QVERIFY(fixture->publish(bus, QStringLiteral("READY")));
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.status(), QStringLiteral("READY"));
    QCOMPARE(client.serviceGeneration(), qulonglong(6));

}

void ServiceReadinessClientTest::pendingSnapshotCannotUndoAPropertyChange()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(clientBus.connection().isConnected());
    auto fixture = std::make_unique<ReadinessFixture>();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), fixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(fixture->getAllCount(), 1);

    fixture->holdNextGetAll();
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(fixture->hasPendingGetAll(), 3000);
    QCOMPARE(fixture->getAllCount(), 2);

    QStringList postChangeStatuses;
    connect(&client, &ServiceReadinessClient::stateChanged, &client,
            [&client, &postChangeStatuses] { postChangeStatuses.append(client.status()); });
    QVERIFY(fixture->publish(bus, QStringLiteral("RECOVERING")));
    QTRY_VERIFY_WITH_TIMEOUT(postChangeStatuses.contains(QStringLiteral("RECONCILING")), 3000);
    QVERIFY(!client.ready());
    QVERIFY(fixture->releasePendingGetAll(bus));
    QTRY_COMPARE_WITH_TIMEOUT(client.status(), QStringLiteral("RECOVERING"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(fixture->getAllCount(), 3, 3000);
    QVERIFY(!postChangeStatuses.contains(QStringLiteral("READY")));
    QVERIFY(!client.ready());
    QCOMPARE(client.serviceGeneration(), qulonglong(5));
}

void ServiceReadinessClientTest::readinessEventsReconcileGapsAndIgnoreDuplicates()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(clientBus.connection().isConnected());
    auto fixture = std::make_unique<ReadinessFixture>();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), fixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(client.eventSequence(), qulonglong(0));
    QCOMPARE(fixture->getAllCount(), 1);

    fixture->skipEventSequences(1);
    QVERIFY(fixture->publish(bus, QStringLiteral("RECOVERING"), {}, false));
    QTRY_COMPARE_WITH_TIMEOUT(client.initializationState(), QStringLiteral("RECOVERING"), 3000);
    QCOMPARE(client.eventSequence(), qulonglong(2));
    QCOMPARE(client.serviceGeneration(), qulonglong(4));
    QTRY_COMPARE_WITH_TIMEOUT(fixture->getAllCount(), 2, 3000);

    QVERIFY(fixture->publishDuplicate(bus));
    QTest::qWait(50);
    QCOMPARE(fixture->getAllCount(), 2);

}

void ServiceReadinessClientTest::duplicateDuringReconciliationDoesNotQueueAnotherSnapshot()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(clientBus.connection().isConnected());
    auto fixture = std::make_unique<ReadinessFixture>();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), fixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    QCOMPARE(fixture->getAllCount(), 1);

    fixture->holdNextGetAll();
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(fixture->hasPendingGetAll(), 3000);
    QCOMPARE(fixture->getAllCount(), 2);
    QVERIFY(fixture->publish(bus, QStringLiteral("RECOVERING")));

    QVERIFY(fixture->publishDuplicate(bus));
    QTest::qWait(50);
    QCOMPARE(fixture->getAllCount(), 2);
    QVERIFY(fixture->releasePendingGetAll(bus));
    QTRY_COMPARE_WITH_TIMEOUT(client.initializationState(), QStringLiteral("RECOVERING"), 3000);
    QTest::qWait(50);
    QCOMPARE(fixture->getAllCount(), 3);
}

void ServiceReadinessClientTest::unsupportedApiMajorFailsClosed()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(clientBus.connection().isConnected());
    auto fixture = std::make_unique<ReadinessFixture>(2, QStringLiteral("READY"));
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), fixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
    QCOMPARE(client.apiMajor(), ushort(2));
    QVERIFY(!client.compatible());
    QVERIFY(!client.ready());
    QCOMPARE(client.status(), QStringLiteral("INCOMPATIBLE_API_MAJOR"));

}

void ServiceReadinessClientTest::ownerReplacementReconcilesNewInstance()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    PrivateClientConnection clientBus;
    QVERIFY(clientBus.connection().isConnected());
    auto firstFixture = std::make_unique<ReadinessFixture>();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), firstFixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    ServiceReadinessClient client(QString::fromLatin1(kTestServiceName),
                                  QString::fromLatin1(kTestObjectPath),
                                  clientBus.connection(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
    const QString firstInstance = client.serviceInstanceUuid();
    QVERIFY(!firstInstance.isEmpty());

    firstFixture->holdNextGetAll();
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(firstFixture->hasPendingGetAll(), 3000);

    QVERIFY(bus.unregisterService(QString::fromLatin1(kTestServiceName)));
    QTRY_COMPARE_WITH_TIMEOUT(client.status(), QStringLiteral("DISCONNECTED"), 3000);
    bus.unregisterObject(QString::fromLatin1(kTestObjectPath));

    QVERIFY(firstFixture->releasePendingGetAll(bus));
    const auto makeBarrierCall = [] {
        return QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetId"));
    };
    QDBusPendingCallWatcher serviceBarrier(bus.asyncCall(makeBarrierCall()));
    QSignalSpy serviceBarrierFinished(&serviceBarrier, &QDBusPendingCallWatcher::finished);
    QVERIFY(serviceBarrierFinished.isValid());
    QTRY_COMPARE_WITH_TIMEOUT(serviceBarrierFinished.count(), 1, 3000);
    QDBusPendingCallWatcher clientBarrier(clientBus.connection().asyncCall(makeBarrierCall()));
    QSignalSpy clientBarrierFinished(&clientBarrier, &QDBusPendingCallWatcher::finished);
    QVERIFY(clientBarrierFinished.isValid());
    QTRY_COMPARE_WITH_TIMEOUT(clientBarrierFinished.count(), 1, 3000);
    QCOMPARE(client.status(), QStringLiteral("DISCONNECTED"));
    QVERIFY(!client.available());
    QCOMPARE(client.serviceInstanceUuid(), QString());

    auto secondFixture = std::make_unique<ReadinessFixture>(1,
                                                            QStringLiteral("STARTING"));
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kTestObjectPath), secondFixture.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kTestServiceName)));

    QTRY_COMPARE_WITH_TIMEOUT(client.serviceInstanceUuid(), secondFixture->instanceUuid(), 3000);
    QVERIFY(client.available());
    QVERIFY(client.serviceInstanceUuid() != firstInstance);
    QCOMPARE(client.initializationState(), QStringLiteral("STARTING"));
    QVERIFY(!client.ready());

}

} // namespace

QTEST_GUILESS_MAIN(ServiceReadinessClientTest)
#include "service_readiness_client_test.moc"
