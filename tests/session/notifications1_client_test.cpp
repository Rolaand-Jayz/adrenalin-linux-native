#include "interfaces/notifications1_client.h"
#include "interfaces/notifications1_contract_types.h"
#include "session_identity.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>
#include <QUuid>

namespace {

QQuickItem *findVisualItem(QQuickItem *parent, const QString &objectName)
{
    if (parent == nullptr) return nullptr;
    if (parent->objectName() == objectName) return parent;
    for (QQuickItem *child : parent->childItems()) {
        if (QQuickItem *match = findVisualItem(child, objectName)) return match;
    }
    return nullptr;
}

class NotificationsFixture final : public QDBusVirtualObject
{
public:
    NotificationsFixture()
        : instanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
        item_.notificationId = QStringLiteral("notification-1");
        item_.category = QStringLiteral("SETTING_APPLIED");
        item_.createdAtUtc = QStringLiteral("2026-09-25T12:00:00.000Z");
        item_.titleMessageKey = QStringLiteral("notification.setting_applied.title");
        item_.bodyMessageKey = QStringLiteral("notification.setting_applied.body");
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.freedesktop.DBus.Properties'>"
            "<method name='GetAll'><arg direction='in' type='s'/><arg direction='out' type='a{sv}'/></method>"
            "</interface><interface name='org.adrenalinlinux.Session1.Service1'/>"
            "<interface name='org.adrenalinlinux.Session1.Notifications1'>"
            "<method name='ListNotifications'><arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='t'/><arg direction='out' type='t'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='a(sssssbbb)'/></method>"
            "<method name='MarkRead'><arg direction='in' type='s'/><arg direction='in' type='s'/>"
            "<arg direction='in' type='t'/><arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/><arg direction='out' type='b'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='b'/><arg direction='out' type='s'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='t'/></method>"
            "<signal name='NotificationsChanged'><arg type='s'/><arg type='t'/><arg type='t'/>"
            "<arg type='s'/><arg type='s'/><arg type='t'/></signal>"
            "</interface></node>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() == QLatin1String("org.freedesktop.DBus.Properties")
            && message.member() == QLatin1String("GetAll")) {
            const QVariantMap properties{
                {QStringLiteral("InitializationState"), QStringLiteral("READY")},
                {QStringLiteral("ServiceInstanceUuid"), instanceUuid_},
                {QStringLiteral("ServiceGeneration"), QVariant::fromValue<qulonglong>(3)},
                {QStringLiteral("EventSequence"), QVariant::fromValue<qulonglong>(eventSequence_)},
                {QStringLiteral("ApiMajor"), QVariant::fromValue<ushort>(1)},
                {QStringLiteral("ApiMinor"), QVariant::fromValue<ushort>(0)},
                {QStringLiteral("LastInitializationError"), QString{}}
            };
            return connection.send(message.createReply(QVariantList{QVariant::fromValue(properties)}));
        }
        if (message.interface() == QLatin1String("org.adrenalinlinux.Session1.Notifications1")
            && message.member() == QLatin1String("ListNotifications")) {
            const QVariantList response = listResponse();
            if (holdNextList_) {
                holdNextList_ = false;
                pendingList_ = message;
                pendingListResponse_ = response;
                return true;
            }
            return connection.send(message.createReply(response));
        }
        if (message.interface() == QLatin1String("org.adrenalinlinux.Session1.Notifications1")
            && message.member() == QLatin1String("MarkRead")) {
            const QString operationId = message.arguments().at(1).toString();
            if (!markReadErrorCode_.isEmpty()) {
                const QString code = std::exchange(markReadErrorCode_, QString{});
                operationIds_.append(operationId);
                return connection.send(message.createReply(
                    QVariantList{code, operationId, QString{}, QString{}, false,
                     QStringLiteral("session-notifications"), item_.notificationId,
                     QVariant::fromValue<qulonglong>(revision_), false, instanceUuid_,
                     QVariant::fromValue<qulonglong>(3), QVariant::fromValue(eventSequence_)}));
            }
            operationIds_.append(operationId);
            if (operationIds_.size() == 1) {
                return connection.send(message.createReply(
                    QVariantList{QStringLiteral("BACKEND_UNAVAILABLE"), operationId,
                     QStringLiteral("notifications.unavailable"), QStringLiteral("temporary"), true,
                     QStringLiteral("session-notifications"), item_.notificationId,
                     QVariant::fromValue<qulonglong>(revision_), false, instanceUuid_,
                     QVariant::fromValue<qulonglong>(3), QVariant::fromValue(eventSequence_)}));
            }
            const qulonglong expectedRevision = message.arguments().at(2).toULongLong();
            if (expectedRevision != revision_) {
                return connection.send(message.createReply(
                    QVariantList{QStringLiteral("STALE_REVISION"), operationId,
                     QStringLiteral("notifications.revision.stale"), QStringLiteral("stale"), false,
                     QStringLiteral("session-notifications"), item_.notificationId,
                     QVariant::fromValue<qulonglong>(revision_), false, instanceUuid_,
                     QVariant::fromValue<qulonglong>(3), QVariant::fromValue(eventSequence_)}));
            }
            item_.isRead = true;
            ++revision_;
            ++eventSequence_;
            QDBusMessage signal = QDBusMessage::createSignal(
                QString::fromLatin1(adrenalin::session1::objectPath),
                QStringLiteral("org.adrenalinlinux.Session1.Notifications1"),
                QStringLiteral("NotificationsChanged"));
            signal << instanceUuid_ << qulonglong(3) << eventSequence_
                   << QStringLiteral("NOTIFICATION") << QStringLiteral("platform") << revision_;
            connection.send(signal);
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("OK"), operationId, QString{}, QString{}, false,
                 QStringLiteral("session-notifications"), item_.notificationId,
                 QVariant::fromValue<qulonglong>(revision_), true, instanceUuid_,
                 QVariant::fromValue<qulonglong>(3), QVariant::fromValue(eventSequence_)}));
        }
        return false;
    }

    QString instanceUuid() const { return instanceUuid_; }
    const QStringList &operationIds() const { return operationIds_; }
    void holdNextList() { holdNextList_ = true; }
    void setMarkReadError(QString code) { markReadErrorCode_ = std::move(code); }
    bool hasPendingList() const { return !pendingList_.path().isEmpty(); }
    bool releasePendingList(const QDBusConnection &connection)
    {
        if (!hasPendingList()) return false;
        const QDBusMessage request = std::exchange(pendingList_, QDBusMessage{});
        const QVariantList response = std::exchange(pendingListResponse_, QVariantList{});
        return connection.send(request.createReply(response));
    }
    bool publishReadState(const QDBusConnection &connection)
    {
        item_.isRead = true;
        ++revision_;
        ++eventSequence_;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Notifications1"),
            QStringLiteral("NotificationsChanged"));
        signal << instanceUuid_ << qulonglong(3) << eventSequence_
               << QStringLiteral("NOTIFICATION") << QStringLiteral("platform") << revision_;
        return connection.send(signal);
    }

private:
    QVariantList listResponse() const
    {
        return {QStringLiteral("OK"), instanceUuid_, QVariant::fromValue<qulonglong>(3),
                QVariant::fromValue<qulonglong>(eventSequence_),
                QVariant::fromValue<qulonglong>(revision_),
                QVariant::fromValue(QList<adrenalin::contracts::notifications1::Notification>{item_})};
    }
    QString instanceUuid_;
    adrenalin::contracts::notifications1::Notification item_;
    qulonglong revision_ = 1;
    qulonglong eventSequence_ = 1;
    bool holdNextList_ = false;
    QDBusMessage pendingList_;
    QVariantList pendingListResponse_;
    QStringList operationIds_;
    QString markReadErrorCode_;
};

class Notifications1ClientTest final : public QObject
{
    Q_OBJECT
private slots:
    void listAndMarkReadUseAsyncServiceContract();
    void markReadFailureShowsHumanReadableStatus();
    void signalDuringSnapshotDiscardsTheOlderSnapshot();
};

void Notifications1ClientTest::listAndMarkReadUseAsyncServiceContract()
{
    QDBusConnection serviceBus = QDBusConnection::sessionBus();
    const QString clientName = QStringLiteral("adrenalin-notifications-client-%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientName);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    NotificationsFixture fixture;
    const QString path = QString::fromLatin1(adrenalin::session1::objectPath);
    const QString name = QString::fromLatin1(adrenalin::session1::serviceName);
    QVERIFY(serviceBus.registerVirtualObject(path, &fixture));
    QVERIFY(serviceBus.registerService(name));
    {
        Notifications1Client client(clientBus);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        QCOMPARE(client.rowCount(), 1);
        QCOMPARE(client.unreadCount(), 1);
        QCOMPARE(client.revision(), qulonglong(1));
        QCOMPARE(client.data(client.index(0), Notifications1Client::NotificationIdRole).toString(),
                 QStringLiteral("notification-1"));
        const auto roles = client.roleNames();
        QCOMPARE(roles.value(Notifications1Client::TitleMessageKeyRole), QByteArray("titleMessageKey"));
        QVERIFY(!roles.contains(Qt::DisplayRole));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("sessionNotificationsClient"),
                                                 &client);
        engine.loadFromModule(QStringLiteral("Adrenalin.NotificationsClientTests"),
                              QStringLiteral("NotificationsPanelHarness"));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *window = engine.rootObjects().constFirst();
        QObject *notificationsButton = window->findChild<QObject *>(
            QStringLiteral("notificationsButton"));
        QObject *historyPopup = window->findChild<QObject *>(
            QStringLiteral("notificationsHistoryPopup"));
        QObject *unreadBadge = window->findChild<QObject *>(
            QStringLiteral("notificationsUnreadBadge"));
        QVERIFY(notificationsButton != nullptr);
        QVERIFY(historyPopup != nullptr);
        QVERIFY(unreadBadge != nullptr);
        QVERIFY(unreadBadge->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(notificationsButton, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(historyPopup->property("visible").toBool(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        auto *popupContent = qvariant_cast<QQuickItem *>(
            historyPopup->property("contentItem"));
        QVERIFY(popupContent != nullptr);
        QQuickItem *historyList = findVisualItem(
            popupContent, QStringLiteral("notificationHistoryList"));
        QVERIFY(historyList != nullptr);
        QCOMPARE(historyList->property("count").toInt(), 1);
        QQuickItem *markReadButton = findVisualItem(
            popupContent, QStringLiteral("notificationsMarkReadButton"));
        QVERIFY(markReadButton != nullptr);
        QVERIFY(markReadButton->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(markReadButton, "click"));

        QTRY_COMPARE_WITH_TIMEOUT(fixture.operationIds().size(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.unreadCount(), 0, 3000);
        QVERIFY(!unreadBadge->property("visible").toBool());
        QCOMPARE(client.revision(), qulonglong(2));
        QCOMPARE(fixture.operationIds().size(), 2);
        QCOMPARE(fixture.operationIds().at(0), fixture.operationIds().at(1));
        QCOMPARE(client.lastOperationCode(), QString{});
    }
    serviceBus.unregisterService(name);
    serviceBus.unregisterObject(path);
    QDBusConnection::disconnectFromBus(clientName);
}



void Notifications1ClientTest::markReadFailureShowsHumanReadableStatus()
{
    QDBusConnection serviceBus = QDBusConnection::sessionBus();
    const QString clientName = QStringLiteral("adrenalin-notifications-error-%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientName);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    NotificationsFixture fixture;
    fixture.setMarkReadError(QStringLiteral("BACKEND_FAILURE"));
    const QString path = QString::fromLatin1(adrenalin::session1::objectPath);
    const QString name = QString::fromLatin1(adrenalin::session1::serviceName);
    QVERIFY(serviceBus.registerVirtualObject(path, &fixture));
    QVERIFY(serviceBus.registerService(name));
    {
        Notifications1Client client(clientBus);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("sessionNotificationsClient"),
                                                 &client);
        engine.loadFromModule(QStringLiteral("Adrenalin.NotificationsClientTests"),
                              QStringLiteral("NotificationsPanelHarness"));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *window = engine.rootObjects().constFirst();
        QObject *notificationsButton = window->findChild<QObject *>(
            QStringLiteral("notificationsButton"));
        QObject *historyPopup = window->findChild<QObject *>(
            QStringLiteral("notificationsHistoryPopup"));
        QVERIFY(notificationsButton != nullptr);
        QVERIFY(historyPopup != nullptr);
        QVERIFY(QMetaObject::invokeMethod(notificationsButton, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(historyPopup->property("visible").toBool(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        auto *popupContent = qvariant_cast<QQuickItem *>(
            historyPopup->property("contentItem"));
        QVERIFY(popupContent != nullptr);
        QQuickItem *markReadButton = findVisualItem(
            popupContent, QStringLiteral("notificationsMarkReadButton"));
        QVERIFY(markReadButton != nullptr);
        QVERIFY(markReadButton->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(markReadButton, "click"));
        QTRY_VERIFY_WITH_TIMEOUT(client.lastOperationCode() == QLatin1String("BACKEND_FAILURE"),
                                 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        QObject *errorMessage = window->findChild<QObject *>(
            QStringLiteral("notificationsOperationErrorMessage"));
        QVERIFY(errorMessage != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(errorMessage->property("visible").toBool(), 3000);
        const QString displayedMessage = errorMessage->property("text").toString();
        QVERIFY(displayedMessage.contains(QStringLiteral("could not be marked as read")));
        QVERIFY(!displayedMessage.contains(QStringLiteral("BACKEND_FAILURE")));
        QCOMPARE(client.unreadCount(), 1);
        QCOMPARE(fixture.operationIds().size(), 1);
    }
    serviceBus.unregisterService(name);
    serviceBus.unregisterObject(path);
    QDBusConnection::disconnectFromBus(clientName);
}

void Notifications1ClientTest::signalDuringSnapshotDiscardsTheOlderSnapshot()
{
    QDBusConnection serviceBus = QDBusConnection::sessionBus();
    const QString clientName = QStringLiteral("adrenalin-notifications-race-%1")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientName);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(clientBus.isConnected());
    NotificationsFixture fixture;
    fixture.holdNextList();
    const QString path = QString::fromLatin1(adrenalin::session1::objectPath);
    const QString name = QString::fromLatin1(adrenalin::session1::serviceName);
    QVERIFY(serviceBus.registerVirtualObject(path, &fixture));
    QVERIFY(serviceBus.registerService(name));
    {
        Notifications1Client client(clientBus);
        QTRY_VERIFY_WITH_TIMEOUT(fixture.hasPendingList(), 3000);
        QVERIFY(fixture.publishReadState(serviceBus));
        QTest::qWait(50);
        QVERIFY(fixture.releasePendingList(serviceBus));
        QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 3000);
        QCOMPARE(client.rowCount(), 1);
        QVERIFY(client.data(client.index(0), Notifications1Client::IsReadRole).toBool());
        QCOMPARE(client.revision(), qulonglong(2));
    }
    serviceBus.unregisterService(name);
    serviceBus.unregisterObject(path);
    QDBusConnection::disconnectFromBus(clientName);
}

} // namespace

QTEST_MAIN(Notifications1ClientTest)
#include "notifications1_client_test.moc"
