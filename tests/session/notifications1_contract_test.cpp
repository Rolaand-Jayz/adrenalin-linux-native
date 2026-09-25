#include "interfaces/notifications1_contract_types.h"
#include "interfaces/notifications1_mock.h"

#include <notifications1_interface.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <memory>

namespace {
constexpr auto kServiceName = "org.adrenalinlinux.Notifications1ContractTest";
constexpr auto kObjectPath = "/org/adrenalinlinux/Notifications1ContractTest";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Notifications1";
constexpr auto kNotificationId = "notification-contract-1";
using namespace adrenalin::contracts::notifications1;
using adrenalin::contracts::MutationResult;

class NotificationsFixture final : public QDBusVirtualObject
{
public:
    explicit NotificationsFixture(QObject *parent = nullptr) : QDBusVirtualObject(parent) {}

    QString introspect(const QString &) const override
    {
        QFile schema(QStringLiteral(":/notifications1/org.adrenalinlinux.Session1.Notifications1.xml"));
        if (!schema.open(QIODevice::ReadOnly)) {
            return QStringLiteral("<node/>");
        }
        return QString::fromUtf8(schema.readAll());
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != QLatin1String(kInterfaceName)) {
            return false;
        }
        if (message.member() == QLatin1String("ListNotifications")) {
            const ListReply reply = mock_.listNotifications();
            return connection.send(message.createReply(QVariantList{
                reply.code, reply.serviceInstanceUuid, QVariant::fromValue<qulonglong>(reply.serviceGeneration),
                QVariant::fromValue<qulonglong>(reply.eventSequence),
                QVariant::fromValue<qulonglong>(reply.revision),
                QVariant::fromValue(reply.notifications)}));
        }
        if (message.member() == QLatin1String("MarkRead")) {
            const auto args = message.arguments();
            const MarkReadReply reply = mock_.markRead(args.value(0).toString(),
                                                       args.value(1).toString(),
                                                       args.value(2).toULongLong());
            const MutationResult &mutation = reply.mutation;
            const bool sent = connection.send(message.createReply(QVariantList{
                adrenalin::contracts::operationResultCodeName(mutation.code),
                mutation.operationId, mutation.humanMessageKey, mutation.diagnosticMessage,
                mutation.retryable, mutation.provider, mutation.subjectId,
                QVariant::fromValue<qulonglong>(mutation.revision)}));
            if (sent && reply.changed) {
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
                    QStringLiteral("NotificationsChanged"));
                signal << reply.serviceInstanceUuid
                       << QVariant::fromValue<qulonglong>(reply.serviceGeneration)
                       << QVariant::fromValue<qulonglong>(reply.eventSequence)
                       << QStringLiteral("PLATFORM") << QStringLiteral("platform")
                       << QVariant::fromValue<qulonglong>(mutation.revision);
                return connection.send(signal);
            }
            return sent;
        }
        return false;
    }

private:
    Mock mock_;
};

class Notifications1ContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
        QVERIFY(bus_.registerService(QString::fromLatin1(kServiceName)));
        fixture_ = std::make_unique<NotificationsFixture>();
        QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kObjectPath), fixture_.get()));
        proxy_ = std::make_unique<OrgAdrenalinlinuxSession1Notifications1Interface>(
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

    void closedTaxonomyAndWireRecordsMatchTheSchema()
    {
        QCOMPARE(categories(), QStringList({
            QStringLiteral("FEATURE_RECOMMENDATION"),
            QStringLiteral("SETTING_APPLIED"),
            QStringLiteral("APPLICATION_UPDATE_AVAILABLE"),
            QStringLiteral("GRAPHICS_STACK_UPDATE_AVAILABLE"),
            QStringLiteral("CAPTURE_STATUS"),
            QStringLiteral("STREAM_STATUS"),
            QStringLiteral("TUNING_WARNING"),
            QStringLiteral("TUNING_RESET"),
            QStringLiteral("HARDWARE_CAPABILITY_CHANGE"),
            QStringLiteral("GAME_DETECTED"),
            QStringLiteral("ERROR")}));
        for (const QString &category : categories()) {
            QVERIFY(isValidCategory(category));
        }
        QVERIFY(!isValidCategory(QStringLiteral("UNKNOWN")));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Notification>())),
                 QByteArray("(sssssbbb)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<QList<Notification>>())), QByteArray("a(sssssbbb)"));

        const QDBusMessage request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
        const QDBusMessage response = bus_.call(request);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage, qPrintable(response.errorMessage()));
        const QString schema = response.arguments().value(0).toString();
        QVERIFY(schema.contains(QStringLiteral(
            "<arg name=\"notifications\" type=\"a(sssssbbb)\" direction=\"out\"/>")));
        QVERIFY(schema.contains(QStringLiteral("<signal name=\"NotificationsChanged\">")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"event_sequence\" type=\"t\"/>")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"operation_id\" type=\"s\" direction=\"in\"/>")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"expected_revision\" type=\"t\" direction=\"in\"/>")));
    }

    void privateBusListMarkReadAndConflictRoundTrip()
    {
        QSignalSpy changed(proxy_.get(),
            &OrgAdrenalinlinuxSession1Notifications1Interface::NotificationsChanged);
        QVERIFY(changed.isValid());

        auto listPending = proxy_->ListNotifications();
        listPending.waitForFinished();
        QVERIFY2(!listPending.isError(), qPrintable(listPending.error().message()));
        QCOMPARE(listPending.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(listPending.argumentAt<1>(), QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
        QCOMPARE(listPending.argumentAt<2>(), qulonglong(1));
        QCOMPARE(listPending.argumentAt<3>(), qulonglong(10));
        QCOMPARE(listPending.argumentAt<4>(), qulonglong(1));
        const auto notifications = listPending.argumentAt<5>();
        QCOMPARE(notifications.size(), 2);
        QVERIFY(notifications.constFirst().isValid());
        QCOMPARE(notifications.constFirst().notificationId, QString::fromLatin1(kNotificationId));
        QVERIFY(notifications.constFirst().critical);
        QVERIFY(notifications.constFirst().toastEligible);

        auto markPending = proxy_->MarkRead(QString::fromLatin1(kNotificationId),
                                            QStringLiteral("mark-read-op-1"), qulonglong(1));
        markPending.waitForFinished();
        QVERIFY2(!markPending.isError(), qPrintable(markPending.error().message()));
        QCOMPARE(markPending.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(markPending.argumentAt<1>(), QStringLiteral("mark-read-op-1"));
        QCOMPARE(markPending.argumentAt<6>(), QStringLiteral("platform"));
        QCOMPARE(markPending.argumentAt<7>(), qulonglong(2));
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().at(0).toString(),
                 QStringLiteral("123e4567-e89b-12d3-a456-426614174000"));
        QCOMPARE(changed.constFirst().at(1).toULongLong(), qulonglong(1));
        QCOMPARE(changed.constFirst().at(2).toULongLong(), qulonglong(11));
        QCOMPARE(changed.constFirst().at(3).toString(), QStringLiteral("PLATFORM"));
        QCOMPARE(changed.constFirst().at(4).toString(), QStringLiteral("platform"));
        QCOMPARE(changed.constFirst().at(5).toULongLong(), qulonglong(2));

        auto duplicatePending = proxy_->MarkRead(QString::fromLatin1(kNotificationId),
                                                 QStringLiteral("mark-read-op-1"), qulonglong(1));
        duplicatePending.waitForFinished();
        QVERIFY2(!duplicatePending.isError(), qPrintable(duplicatePending.error().message()));
        QCOMPARE(duplicatePending.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(duplicatePending.argumentAt<7>(), qulonglong(2));
        QCOMPARE(changed.count(), 1);

        auto conflictPending = proxy_->MarkRead(QStringLiteral("notification-contract-2"),
                                                QStringLiteral("mark-read-op-1"), qulonglong(1));
        conflictPending.waitForFinished();
        QVERIFY2(!conflictPending.isError(), qPrintable(conflictPending.error().message()));
        QCOMPARE(conflictPending.argumentAt<0>(), QStringLiteral("CONFLICT"));
        QCOMPARE(conflictPending.argumentAt<7>(), qulonglong(2));

        auto stalePending = proxy_->MarkRead(QStringLiteral("notification-contract-2"),
                                             QStringLiteral("mark-read-op-2"), qulonglong(1));
        stalePending.waitForFinished();
        QVERIFY2(!stalePending.isError(), qPrintable(stalePending.error().message()));
        QCOMPARE(stalePending.argumentAt<0>(), QStringLiteral("STALE_REVISION"));
        QCOMPARE(stalePending.argumentAt<7>(), qulonglong(2));
        QCOMPARE(changed.count(), 1);

        auto missingPending = proxy_->MarkRead(QStringLiteral("missing-notification"),
                                               QStringLiteral("mark-read-op-3"), qulonglong(2));
        missingPending.waitForFinished();
        QVERIFY2(!missingPending.isError(), qPrintable(missingPending.error().message()));
        QCOMPARE(missingPending.argumentAt<0>(), QStringLiteral("NOT_FOUND"));
        QCOMPARE(changed.count(), 1);

        auto finalListPending = proxy_->ListNotifications();
        finalListPending.waitForFinished();
        QVERIFY2(!finalListPending.isError(), qPrintable(finalListPending.error().message()));
        QCOMPARE(finalListPending.argumentAt<4>(), qulonglong(2));
        QVERIFY(finalListPending.argumentAt<5>().constFirst().isRead);
    }

    void typedSnapshotRejectsInvalidRecordsAndDuplicates()
    {
        Mock mock;
        ListReply snapshot = mock.listNotifications();
        QVERIFY(snapshot.isValid());
        snapshot.notifications[0].category = QStringLiteral("UNREGISTERED");
        QVERIFY(!snapshot.isValid());
        snapshot = mock.listNotifications();
        snapshot.notifications.append(snapshot.notifications.constFirst());
        QVERIFY(!snapshot.isValid());
        snapshot = mock.listNotifications();
        snapshot.serviceInstanceUuid.clear();
        snapshot.serviceGeneration = 0;
        snapshot.eventSequence = 0;
        snapshot.revision = 0;
        snapshot.notifications.clear();
        snapshot.code = QStringLiteral("BACKEND_UNAVAILABLE");
        QVERIFY(snapshot.isValid());
        snapshot.notifications = mock.listNotifications().notifications;
        QVERIFY(!snapshot.isValid());
        snapshot = mock.listNotifications();
        snapshot.code = QStringLiteral("BACKEND_UNAVAILABLE");
        snapshot.notifications.clear();
        QVERIFY(!snapshot.isValid());
    }

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<NotificationsFixture> fixture_;
    std::unique_ptr<OrgAdrenalinlinuxSession1Notifications1Interface> proxy_;
};

} // namespace

QTEST_GUILESS_MAIN(Notifications1ContractTest)
#include "notifications1_contract_test.moc"
