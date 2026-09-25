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
                reply.code, reply.serviceInstanceUuid,
                QVariant::fromValue<qulonglong>(reply.serviceGeneration),
                QVariant::fromValue<qulonglong>(reply.eventSequence),
                QVariant::fromValue(reply.profile)}));
        }
        if (message.member() == QLatin1String("UpdateProfile")) {
            const UpdateOutcome outcome = mock_.updateProfile(args.value(0).toString(), args.value(1).toString(),
                args.value(2).toULongLong(), args.value(3).toString(),
                qdbus_cast<QVariantMap>(args.value(4).value<QDBusArgument>()));
            const MutationResult &mutation = outcome.mutation;
            const bool sent = connection.send(message.createReply(QVariantList{
                adrenalin::contracts::operationResultCodeName(mutation.code), mutation.operationId,
                mutation.humanMessageKey, mutation.diagnosticMessage, mutation.retryable,
                mutation.provider, mutation.subjectId, QVariant::fromValue<qulonglong>(mutation.revision)}));
            if (sent && outcome.event.has_value()) {
                const ProfileChangedEvent &event = *outcome.event;
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
                    QStringLiteral("ProfileChanged"));
                signal << event.serviceInstanceUuid
                       << QVariant::fromValue<qulonglong>(event.serviceGeneration)
                       << QVariant::fromValue<qulonglong>(event.eventSequence)
                       << event.subjectKind << event.subjectId
                       << QVariant::fromValue<qulonglong>(event.revision);
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
        const QStringList readOutputs{
            QStringLiteral("name=\"result_code\" type=\"s\" direction=\"out\""),
            QStringLiteral("name=\"service_instance_uuid\" type=\"s\" direction=\"out\""),
            QStringLiteral("name=\"service_generation\" type=\"t\" direction=\"out\""),
            QStringLiteral("name=\"event_sequence\" type=\"t\" direction=\"out\""),
            QStringLiteral("name=\"profile\" type=\"(sssssta{sv})\" direction=\"out\"")};
        int readPosition = schema.indexOf(QStringLiteral("<method name=\"ReadProfile\">"));
        QVERIFY(readPosition >= 0);
        for (const QString &output : readOutputs) {
            const int next = schema.indexOf(output, readPosition);
            QVERIFY2(next >= readPosition, qPrintable(output));
            readPosition = next + output.size();
        }

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

        ProfileChangedEvent validEvent{QStringLiteral("123e4567-e89b-12d3-a456-426614174001"),
                                       1, 1, QStringLiteral("GAME"),
                                       QString::fromLatin1(kGameId), 2};
        QVERIFY(validEvent.isValid());
        validEvent.eventSequence = 0;
        QVERIFY(!validEvent.isValid());

        ReadReply unavailable;
        unavailable.code = QStringLiteral("BACKEND_UNAVAILABLE");
        QVERIFY(unavailable.isValid());
        ReadReply unboundSuccess;
        unboundSuccess.code = QStringLiteral("OK");
        unboundSuccess.profile = Profile{QStringLiteral("123e4567-e89b-42d3-a456-426614174011"),
                                         QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                         QString(), QStringLiteral("REFERENCE_GATED"), 1,
                                         {{QStringLiteral("fixture_setting"), 1}}};
        QVERIFY(!unboundSuccess.isValid());
    }

    void privateBusReadUpdateIdempotencyConflictAndStaleRevision()
    {
        QSignalSpy changed(proxy_.get(), &OrgAdrenalinlinuxSession1Profiles1Interface::ProfileChanged);
        QVERIFY(changed.isValid());

        auto globalRead = proxy_->ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        globalRead.waitForFinished();
        QVERIFY2(!globalRead.isError(), qPrintable(globalRead.error().message()));
        QCOMPARE(globalRead.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(globalRead.argumentAt<1>(), QStringLiteral("123e4567-e89b-12d3-a456-426614174001"));
        QCOMPARE(globalRead.argumentAt<2>(), qulonglong(1));
        QCOMPARE(globalRead.argumentAt<3>(), qulonglong(0));
        const ReadReply initialGlobalReply{globalRead.argumentAt<0>(), globalRead.argumentAt<1>(),
                                            globalRead.argumentAt<2>(), globalRead.argumentAt<3>(),
                                            globalRead.argumentAt<4>()};
        QVERIFY(initialGlobalReply.isValidFor(QStringLiteral("GLOBAL"), QStringLiteral("global")));
        const Profile global = globalRead.argumentAt<4>();
        QVERIFY(global.isValid());
        QCOMPARE(global.subjectKind, QStringLiteral("GLOBAL"));
        QCOMPARE(global.subjectId, QStringLiteral("global"));
        QCOMPARE(global.referenceState, QStringLiteral("REFERENCE_GATED"));

        auto gameRead = proxy_->ReadProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId));
        gameRead.waitForFinished();
        QVERIFY2(!gameRead.isError(), qPrintable(gameRead.error().message()));
        QCOMPARE(gameRead.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(gameRead.argumentAt<4>().subjectId, QString::fromLatin1(kGameId));
        const ReadReply gameReply{gameRead.argumentAt<0>(), gameRead.argumentAt<1>(),
                                  gameRead.argumentAt<2>(), gameRead.argumentAt<3>(),
                                  gameRead.argumentAt<4>()};
        QVERIFY(gameReply.isValidFor(QStringLiteral("GAME"), QString::fromLatin1(kGameId)));
        QVERIFY(!gameReply.isValidFor(QStringLiteral("GAME"), QStringLiteral("steam:app/98765")));

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
        QCOMPARE(changed.constFirst().at(2).toULongLong(), qulonglong(1));
        QCOMPARE(changed.constFirst().at(3).toString(), QStringLiteral("GAME"));
        QCOMPARE(changed.constFirst().at(4).toString(), QString::fromLatin1(kGameId));
        QCOMPARE(changed.constFirst().at(5).toULongLong(), update.argumentAt<7>());

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

        auto noOp = proxy_->UpdateProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId),
                                           qulonglong(2), QStringLiteral("profile-op-noop"), patch);
        noOp.waitForFinished();
        QVERIFY2(!noOp.isError(), qPrintable(noOp.error().message()));
        QCOMPARE(noOp.argumentAt<0>(), QStringLiteral("OK"));
        QCOMPARE(noOp.argumentAt<7>(), qulonglong(2));
        QCOMPARE(changed.count(), 1);

        auto missing = proxy_->ReadProfile(QStringLiteral("GAME"), QStringLiteral("steam:app/98765"));
        missing.waitForFinished();
        QVERIFY2(!missing.isError(), qPrintable(missing.error().message()));
        QCOMPARE(missing.argumentAt<0>(), QStringLiteral("NOT_FOUND"));
        QCOMPARE(missing.argumentAt<1>(), QStringLiteral("123e4567-e89b-12d3-a456-426614174001"));
        QCOMPARE(missing.argumentAt<2>(), qulonglong(1));
        QCOMPARE(missing.argumentAt<3>(), qulonglong(1));
        QVERIFY(missing.argumentAt<4>().profileId.isEmpty());
        const ReadReply missingReply{missing.argumentAt<0>(), missing.argumentAt<1>(),
                                     missing.argumentAt<2>(), missing.argumentAt<3>(),
                                     missing.argumentAt<4>()};
        QVERIFY(missingReply.isValidFor(QStringLiteral("GAME"), QStringLiteral("steam:app/98765")));

        auto badSubject = proxy_->ReadProfile(QStringLiteral("GAME"), QStringLiteral("localized title"));
        badSubject.waitForFinished();
        QVERIFY2(!badSubject.isError(), qPrintable(badSubject.error().message()));
        QCOMPARE(badSubject.argumentAt<0>(), QStringLiteral("INVALID_ARGUMENT"));
        QCOMPARE(badSubject.argumentAt<1>(), QString());
        QCOMPARE(badSubject.argumentAt<2>(), qulonglong(0));
        QCOMPARE(badSubject.argumentAt<3>(), qulonglong(0));
        QVERIFY(badSubject.argumentAt<4>().profileId.isEmpty());
        const ReadReply invalidReply{badSubject.argumentAt<0>(), badSubject.argumentAt<1>(),
                                     badSubject.argumentAt<2>(), badSubject.argumentAt<3>(),
                                     badSubject.argumentAt<4>()};
        QVERIFY(invalidReply.isValidFor(QStringLiteral("GAME"), QStringLiteral("localized title")));
        QVERIFY(!invalidReply.isValidFor(QStringLiteral("GAME"), QString::fromLatin1(kGameId)));

        auto finalGameRead = proxy_->ReadProfile(QStringLiteral("GAME"), QString::fromLatin1(kGameId));
        finalGameRead.waitForFinished();
        QVERIFY2(!finalGameRead.isError(), qPrintable(finalGameRead.error().message()));
        QCOMPARE(finalGameRead.argumentAt<1>(), QStringLiteral("123e4567-e89b-12d3-a456-426614174001"));
        QCOMPARE(finalGameRead.argumentAt<2>(), qulonglong(1));
        QCOMPARE(finalGameRead.argumentAt<3>(), qulonglong(1));
        QCOMPARE(finalGameRead.argumentAt<4>().revision, qulonglong(2));
        QCOMPARE(finalGameRead.argumentAt<4>().settings.value(QStringLiteral("fixture_setting")).toInt(), 7);
        auto finalGlobalRead = proxy_->ReadProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"));
        finalGlobalRead.waitForFinished();
        QVERIFY2(!finalGlobalRead.isError(), qPrintable(finalGlobalRead.error().message()));
        QCOMPARE(finalGlobalRead.argumentAt<4>().revision, qulonglong(1));
        QCOMPARE(finalGlobalRead.argumentAt<4>().settings.value(QStringLiteral("fixture_global_setting")).toBool(), true);
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
