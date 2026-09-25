#include "interfaces/profiles1_contract_types.h"
#include "interfaces/profiles1_mock.h"

#include <profiles1_interface.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <limits>
#include <memory>

namespace {
constexpr auto kServiceName = "org.adrenalinlinux.Profiles1ContractTest";
constexpr auto kObjectPath = "/org/adrenalinlinux/Profiles1ContractTest";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Profiles1";
constexpr auto kGameId = "steam:app/12345";
using namespace adrenalin::contracts::profiles1;
using adrenalin::contracts::MutationResult;

class ProfilesFixture final : public QDBusVirtualObject
{
public:
    explicit ProfilesFixture(QObject *parent = nullptr) : QDBusVirtualObject(parent) {}

    QString introspect(const QString &) const override
    {
        QFile schema(QStringLiteral(":/profiles1/org.adrenalinlinux.Session1.Profiles1.xml"));
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
        const auto args = message.arguments();
        if (message.member() == QLatin1String("ReadProfile")) {
            const ReadReply reply = mock_.readProfile(args.value(0).toString(), args.value(1).toString());
            return connection.send(message.createReply(QVariantList{
                reply.code, QVariant::fromValue(reply.profile)}));
        }
        if (message.member() == QLatin1String("UpdateProfile")) {
            const UpdateReply reply = mock_.updateProfile(args.value(0).toString(), args.value(1).toString(),
                args.value(2).toULongLong(), args.value(3).toString(),
                qdbus_cast<QVariantMap>(args.value(4).value<QDBusArgument>()));
            const MutationResult &mutation = reply.mutation;
            const bool sent = connection.send(message.createReply(QVariantList{
                adrenalin::contracts::operationResultCodeName(mutation.code), mutation.operationId,
                mutation.humanMessageKey, mutation.diagnosticMessage, mutation.retryable,
                mutation.provider, mutation.subjectId, QVariant::fromValue<qulonglong>(mutation.revision)}));
            if (sent && reply.changed) {
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
                    QStringLiteral("ProfileChanged"));
                signal << reply.serviceInstanceUuid
                       << QVariant::fromValue<qulonglong>(reply.serviceGeneration)
                       << QVariant::fromValue<qulonglong>(reply.eventSequence)
                       << args.value(0).toString() << args.value(1).toString()
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

class Profiles1ContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
        QVERIFY(bus_.registerService(QString::fromLatin1(kServiceName)));
        fixture_ = std::make_unique<ProfilesFixture>();
        QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kObjectPath), fixture_.get()));
        proxy_ = std::make_unique<OrgAdrenalinlinuxSession1Profiles1Interface>(
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

    void typedWireTypeAndSchemaDeclareReferenceGate()
    {
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Profile>())),
                 QByteArray("(sssssta{sv})"));
        const QDBusMessage request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
        const QDBusMessage response = bus_.call(request);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage, qPrintable(response.errorMessage()));
        const QString schema = response.arguments().value(0).toString();
        QVERIFY(schema.contains(QStringLiteral("org.adrenalinlinux.Session1.Profiles1")));
        QVERIFY(schema.contains(QStringLiteral("type=\"(sssssta{sv})\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"expected_revision\" type=\"t\" direction=\"in\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"operation_id\" type=\"s\" direction=\"in\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"result_subject_id\" type=\"s\" direction=\"out\"")));
        QVERIFY(schema.contains(QStringLiteral("name=\"ProfileChanged\"")));

        QVERIFY(isValidSubject(QStringLiteral("GLOBAL"), QStringLiteral("global")));
        QVERIFY(isValidSubject(QStringLiteral("GAME"), QString::fromLatin1(kGameId)));
        QVERIFY(!isValidSubject(QStringLiteral("GAME"), QStringLiteral("localized game title")));
        QVERIFY(!isValidSubject(QStringLiteral("GLOBAL"), QStringLiteral("other")));
        Profile fixture{QStringLiteral("123e4567-e89b-42d3-a456-426614174011"), QStringLiteral("GAME"),
                        QString::fromLatin1(kGameId), QString(), QStringLiteral("REFERENCE_GATED"), 1,
                        {{QStringLiteral("fixture_setting"), 1}}};
        QVERIFY(fixture.isValid());
        fixture.settings.insert(QStringLiteral("nested"), QVariantMap{});
        QVERIFY(!fixture.isValid());
        fixture.settings.remove(QStringLiteral("nested"));
        fixture.settings.insert(QStringLiteral("not_finite"), std::numeric_limits<double>::quiet_NaN());
        QVERIFY(!fixture.isValid());
        fixture.settings.remove(QStringLiteral("not_finite"));
        fixture.settings.insert(QStringLiteral("UpperCaseKey"), true);
        QVERIFY(!fixture.isValid());
        fixture.settings.remove(QStringLiteral("UpperCaseKey"));
        fixture.presetId = QStringLiteral("CUSTOM");
        QVERIFY(!fixture.isValid());
    }

    void privateBusReadUpdateIdempotencyConflictAndStaleRevision()
    {
        QSignalSpy changed(proxy_.get(), &OrgAdrenalinlinuxSession1Profiles1Interface::ProfileChanged);
        QVERIFY(changed.isValid());

        auto globalRead = proxy_->ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        globalRead.waitForFinished();
        QVERIFY2(!globalRead.isError(), qPrintable(globalRead.error().message()));
        QCOMPARE(globalRead.argumentAt<0>(), QStringLiteral("OK"));
        const Profile global = globalRead.argumentAt<1>();
        QVERIFY(global.isValid());
        QCOMPARE(global.subjectKind, QStringLiteral("GLOBAL"));
        QCOMPARE(global.subjectId, QStringLiteral("global"));
        QCOMPARE(global.referenceState, QStringLiteral("REFERENCE_GATED"));

        auto gameRead = proxy_->ReadProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId));
        gameRead.waitForFinished();
        QVERIFY2(!gameRead.isError(), qPrintable(gameRead.error().message()));
        QCOMPARE(gameRead.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(gameRead.argumentAt<1>().subjectId, QString::fromLatin1(kGameId));

        QVariantMap patch{{QStringLiteral("fixture_setting"), 7}};
        auto update = proxy_->UpdateProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                             qulonglong(1), QStringLiteral("profile-op-1"), patch);
        update.waitForFinished();
        QVERIFY2(!update.isError(), qPrintable(update.error().message()));
        QCOMPARE(update.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(update.argumentAt<1>(), QStringLiteral("profile-op-1"));
        QCOMPARE(update.argumentAt<6>(), QStringLiteral("GAME:steam:app/12345"));
        QCOMPARE(update.argumentAt<7>(), qulonglong(2));
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().at(0).toString(),
                 QStringLiteral("123e4567-e89b-12d3-a456-426614174001"));
        QCOMPARE(changed.constFirst().at(2).toULongLong(), qulonglong(21));
        QCOMPARE(changed.constFirst().at(3).toString(), QStringLiteral("GAME"));

        auto duplicate = proxy_->UpdateProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                                qulonglong(1), QStringLiteral("profile-op-1"), patch);
        duplicate.waitForFinished();
        QVERIFY2(!duplicate.isError(), qPrintable(duplicate.error().message()));
        QCOMPARE(duplicate.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(duplicate.argumentAt<7>(), qulonglong(2));
        QCOMPARE(changed.count(), 1);

        auto conflict = proxy_->UpdateProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                               qulonglong(1), QStringLiteral("profile-op-1"),
                                               {{QStringLiteral("fixture_setting"), 8}});
        conflict.waitForFinished();
        QVERIFY2(!conflict.isError(), qPrintable(conflict.error().message()));
        QCOMPARE(conflict.argumentAt<0>(), QStringLiteral("CONFLICT"));

        auto stale = proxy_->UpdateProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                            qulonglong(1), QStringLiteral("profile-op-2"), patch);
        stale.waitForFinished();
        QVERIFY2(!stale.isError(), qPrintable(stale.error().message()));
        QCOMPARE(stale.argumentAt<0>(), QStringLiteral("STALE_REVISION"));
        QCOMPARE(changed.count(), 1);

        auto badSubject = proxy_->ReadProfile(QStringLiteral("GAME"), QStringLiteral("localized title"));
        badSubject.waitForFinished();
        QVERIFY2(!badSubject.isError(), qPrintable(badSubject.error().message()));
        QCOMPARE(badSubject.argumentAt<0>(), QStringLiteral("INVALID_ARGUMENT"));
        QVERIFY(badSubject.argumentAt<1>().profileId.isEmpty());

        auto finalGameRead = proxy_->ReadProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId));
        finalGameRead.waitForFinished();
        QVERIFY2(!finalGameRead.isError(), qPrintable(finalGameRead.error().message()));
        QCOMPARE(finalGameRead.argumentAt<1>().revision, qulonglong(2));
        QCOMPARE(finalGameRead.argumentAt<1>().settings.value(QStringLiteral("fixture_setting")).toInt(), 7);
        auto finalGlobalRead = proxy_->ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        finalGlobalRead.waitForFinished();
        QVERIFY2(!finalGlobalRead.isError(), qPrintable(finalGlobalRead.error().message()));
        QCOMPARE(finalGlobalRead.argumentAt<1>().revision, qulonglong(1));
        QCOMPARE(finalGlobalRead.argumentAt<1>().settings.value(QStringLiteral("fixture_global_setting")).toBool(), true);
        QCOMPARE(changed.count(), 1);
    }

private:
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<ProfilesFixture> fixture_;
    std::unique_ptr<OrgAdrenalinlinuxSession1Profiles1Interface> proxy_;
};

} // namespace

QTEST_GUILESS_MAIN(Profiles1ContractTest)
#include "profiles1_contract_test.moc"
