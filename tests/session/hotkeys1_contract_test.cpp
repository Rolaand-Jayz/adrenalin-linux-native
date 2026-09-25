#include "interfaces/hotkeys1_contract_types.h"
#include "interfaces/hotkeys1_mock.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QFile>
#include <QTest>

#include <memory>

namespace {
constexpr auto kServiceName = "org.adrenalinlinux.Hotkeys1ContractTest";
constexpr auto kObjectPath = "/org/adrenalinlinux/Hotkeys1ContractTest";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Hotkeys1";

using namespace adrenalin::contracts;
using namespace adrenalin::contracts::hotkeys1;

class HotkeysFixture final : public QDBusVirtualObject
{
public:
    int publishedChangeCount() const { return publishedChangeCount_; }

    QString introspect(const QString &) const override
    {
        QFile schema(QStringLiteral(":/hotkeys1/org.adrenalinlinux.Session1.Hotkeys1.xml"));
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
        if (message.member() == QLatin1String("ListHotkeys")) {
            QList<Action> actions;
            const Reply reply = mock_.listActions(&actions);
            return connection.send(message.createReply(
                QVariantList{QVariant::fromValue(reply), QVariant::fromValue(actions)}));
        }
        if (message.member() == QLatin1String("SetHotkey")) {
            const auto args = message.arguments();
            const UpdateReply reply = mock_.setBinding(
                args.value(0).toString(), args.value(1).toString(),
                args.value(2).toULongLong(), args.value(3).toString(), args.value(4).toString());
            const MutationResult &mutation = reply.mutation;
            const QDBusMessage response = message.createReply(QVariantList{
                operationResultCodeName(mutation.code), mutation.operationId,
                mutation.humanMessageKey, mutation.diagnosticMessage, mutation.retryable,
                mutation.provider, mutation.subjectId,
                QVariant::fromValue<qulonglong>(mutation.revision),
                QVariant::fromValue(reply.action)});
            bool eventPublished = false;
            if (reply.changed) {
                QDBusMessage changed = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
                    QStringLiteral("HotkeyChanged"));
                changed << reply.serviceInstanceUuid
                        << QVariant::fromValue<qulonglong>(reply.serviceGeneration)
                        << QVariant::fromValue<qulonglong>(reply.eventSequence)
                        << QStringLiteral("HOTKEY_ACTION") << reply.action.actionId
                        << QVariant::fromValue<qulonglong>(mutation.revision);
                eventPublished = connection.send(changed);
                if (eventPublished) {
                    ++publishedChangeCount_;
                }
            }
            const bool replySent = connection.send(response);
            return replySent || eventPublished || reply.changed;
        }
        return false;
    }

private:
    Mock mock_;
    int publishedChangeCount_ = 0;
};

class Hotkeys1ContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
        QVERIFY(bus_.registerService(QString::fromLatin1(kServiceName)));
        fixture_ = std::make_unique<HotkeysFixture>();
        QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kObjectPath), fixture_.get()));
    }

    void cleanupTestCase()
    {
        if (fixture_) {
            bus_.unregisterObject(QString::fromLatin1(kObjectPath));
        }
        fixture_.reset();
        bus_.unregisterService(QString::fromLatin1(kServiceName));
    }

    void schemaPinsRequiredFamiliesAndEventEnvelope()
    {
        const QDBusMessage request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
        const QDBusMessage response = bus_.call(request);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage, qPrintable(response.errorMessage()));
        const QString schema = response.arguments().value(0).toString();
        QVERIFY(schema.contains(QStringLiteral("org.adrenalinlinux.Session1.Hotkeys1")));
        QVERIFY(schema.contains(QStringLiteral("name=\"ListHotkeys\"")));
        QVERIFY2(schema.contains(QStringLiteral("name=\"SetHotkey\"")), qPrintable(schema));
        QVERIFY(schema.contains(QStringLiteral("name=\"expected_revision\" type=\"t\" direction=\"in\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"operation_id\" type=\"s\" direction=\"in\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"parent_window_id\" type=\"s\" direction=\"in\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"HotkeyChanged\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"event_sequence\" type=\"t\"")));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Action>())),
                 QByteArray("(ssssssss)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Reply>())),
                 QByteArray("(sssbsssbsttt)"));
    }

    void snapshotAndBindingUpdateRoundTrip()
    {
        const QDBusMessage listRequest = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QString::fromLatin1(kInterfaceName), QStringLiteral("ListHotkeys"));
        const QDBusMessage listResponse = bus_.call(listRequest);
        QVERIFY2(listResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(listResponse.errorMessage()));
        const Reply snapshot = qdbus_cast<Reply>(
            listResponse.arguments().value(0).value<QDBusArgument>());
        const QList<Action> actions = qdbus_cast<QList<Action>>(
            listResponse.arguments().value(1).value<QDBusArgument>());
        QVERIFY2(isValidSnapshot(snapshot, actions), "mock snapshot must satisfy typed contract");
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.constFirst().actionId, QStringLiteral("overlay.toggle"));
        QCOMPARE(actions.constFirst().registrationState, QStringLiteral("UNBOUND"));

        const QString operationId = QStringLiteral("123e4567-e89b-42d3-a456-426614174012");
        QDBusMessage update = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QString::fromLatin1(kInterfaceName), QStringLiteral("SetHotkey"));
        update << actions.constFirst().actionId << QStringLiteral("Ctrl+Shift+O")
               << QVariant::fromValue<qulonglong>(snapshot.revision) << operationId
               << QStringLiteral("wayland:parent-window-token");
        const QDBusMessage updateResponse = bus_.call(update);
        QVERIFY2(updateResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(updateResponse.errorMessage()));
        const auto values = updateResponse.arguments();
        QCOMPARE(values.value(0).toString(), QStringLiteral("OK"));
        QCOMPARE(values.value(1).toString(), operationId);
        QCOMPARE(values.value(6).toString(), QStringLiteral("overlay.toggle"));
        QCOMPARE(values.value(7).toULongLong(), snapshot.revision + 1);
        const Action updated = qdbus_cast<Action>(values.value(8).value<QDBusArgument>());
        QVERIFY(updated.isValid());
        QCOMPARE(updated.configuredBinding, QStringLiteral("Ctrl+Shift+O"));
        QCOMPARE(updated.effectiveBinding, QStringLiteral("Ctrl+Shift+O"));
        QCOMPARE(updated.registrationState, QStringLiteral("ACTIVE"));
        QCOMPARE(updated.provider, QStringLiteral("mock.hotkeys"));
        QCOMPARE(fixture_->publishedChangeCount(), 1);

        const QDBusMessage replayResponse = bus_.call(update);
        QVERIFY2(replayResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(replayResponse.errorMessage()));
        QCOMPARE(replayResponse.arguments().value(0).toString(), QStringLiteral("OK"));
        QCOMPARE(replayResponse.arguments().value(7).toULongLong(), snapshot.revision + 1);
        QCOMPARE(fixture_->publishedChangeCount(), 1);

        QDBusMessage conflictingReplay = update;
        auto conflictingArguments = conflictingReplay.arguments();
        conflictingArguments[1] = QStringLiteral("Ctrl+Alt+O");
        conflictingReplay.setArguments(conflictingArguments);
        const QDBusMessage conflictResponse = bus_.call(conflictingReplay);
        QVERIFY2(conflictResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(conflictResponse.errorMessage()));
        QCOMPARE(conflictResponse.arguments().value(0).toString(), QStringLiteral("CONFLICT"));
        QCOMPARE(fixture_->publishedChangeCount(), 1);

        const QDBusMessage readBack = bus_.call(listRequest);
        const Reply updatedSnapshot = qdbus_cast<Reply>(
            readBack.arguments().value(0).value<QDBusArgument>());
        const QList<Action> updatedActions = qdbus_cast<QList<Action>>(
            readBack.arguments().value(1).value<QDBusArgument>());
        QVERIFY(isValidSnapshot(updatedSnapshot, updatedActions));
        QCOMPARE(updatedSnapshot.revision, snapshot.revision + 1);
        QCOMPARE(updatedActions.constFirst().effectiveBinding, QStringLiteral("Ctrl+Shift+O"));

        QDBusMessage staleUpdate = update;
        auto staleArguments = staleUpdate.arguments();
        staleArguments[1] = QStringLiteral("Ctrl+Shift+P");
        staleArguments[3] = QStringLiteral("123e4567-e89b-42d3-a456-426614174014");
        staleUpdate.setArguments(staleArguments);
        const QDBusMessage staleResponse = bus_.call(staleUpdate);
        QVERIFY2(staleResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(staleResponse.errorMessage()));
        QCOMPARE(staleResponse.arguments().value(0).toString(), QStringLiteral("STALE_REVISION"));
        QCOMPARE(fixture_->publishedChangeCount(), 1);

        const QDBusMessage staleReplayResponse = bus_.call(staleUpdate);
        QVERIFY2(staleReplayResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(staleReplayResponse.errorMessage()));
        QCOMPARE(staleReplayResponse.arguments().value(0).toString(), QStringLiteral("STALE_REVISION"));
        QCOMPARE(fixture_->publishedChangeCount(), 1);

        auto conflictingStaleArguments = staleUpdate.arguments();
        conflictingStaleArguments[1] = QStringLiteral("Ctrl+Alt+P");
        staleUpdate.setArguments(conflictingStaleArguments);
        const QDBusMessage staleConflictResponse = bus_.call(staleUpdate);
        QVERIFY2(staleConflictResponse.type() == QDBusMessage::ReplyMessage,
                 qPrintable(staleConflictResponse.errorMessage()));
        QCOMPARE(staleConflictResponse.arguments().value(0).toString(), QStringLiteral("CONFLICT"));
        QCOMPARE(fixture_->publishedChangeCount(), 1);
    }

    void rejectsInvalidAndDuplicateSnapshotActions()
    {
        QVERIFY(!isValidActionId(QStringLiteral("Overlay.Toggle")));
        QVERIFY(!isValidActionId(QStringLiteral("../toggle")));
        QVERIFY(!isValidActionId(QString(129, QLatin1Char('a'))));
        QVERIFY(isValidBindingText(QString{}));
        QVERIFY(!isValidBindingText(QStringLiteral("Ctrl+\tO")));
        QVERIFY(!isValidBindingText(QString(257, QLatin1Char('a'))));
        QVERIFY(isValidParentWindowId(QString{}));
        QVERIFY(!isValidParentWindowId(QStringLiteral("portal\nparent")));

        Reply reply;
        reply.code = QStringLiteral("OK");
        reply.snapshotValid = true;
        reply.serviceInstanceUuid = QStringLiteral("123e4567-e89b-42d3-a456-426614174013");
        reply.serviceGeneration = 1;
        reply.eventSequence = 1;
        reply.revision = 1;
        Action action;
        action.actionId = QStringLiteral("capture.toggle");
        action.labelMessageKey = QStringLiteral("hotkeys.capture.toggle");
        action.descriptionMessageKey = QStringLiteral("hotkeys.capture.toggle.description");
        action.registrationState = QStringLiteral("UNBOUND");
        QVERIFY(action.isValid());
        QVERIFY(!isValidSnapshot(reply, {action, action}));
        action.registrationState = QStringLiteral("ACTIVE");
        QVERIFY(!action.isValid());
    }

private:
    QDBusConnection bus_{QDBusConnection::sessionBus()};
    std::unique_ptr<HotkeysFixture> fixture_;
};

} // namespace

QTEST_GUILESS_MAIN(Hotkeys1ContractTest)

#include "hotkeys1_contract_test.moc"
