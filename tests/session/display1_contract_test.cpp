#include "interfaces/display1_contract_types.h"
#include "interfaces/display1_mock.h"

#include <display1_interface.h>

#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include <memory>

namespace {
constexpr auto kServiceName = "org.adrenalinlinux.Display1ContractTest";
constexpr auto kObjectPath = "/org/adrenalinlinux/Display1ContractTest";
constexpr auto kInterfaceName = "org.adrenalinlinux.Session1.Display1";

using namespace adrenalin::contracts::display1;
using namespace adrenalin::contracts;

class DisplayFixture final : public QDBusVirtualObject
{
public:
    explicit DisplayFixture(QObject *parent = nullptr) : QDBusVirtualObject(parent) {}

    void setReadinessState(Mock::ReadinessState state)
    {
        mock_.setReadinessStateForTesting(state);
    }

    QString introspect(const QString &) const override
    {
        QFile schema(QStringLiteral(":/display1/org.adrenalinlinux.Session1.Display1.xml"));
        if (!schema.open(QIODevice::ReadOnly)) {
            return QStringLiteral("<node/>");
        }
        return QString::fromUtf8(schema.readAll());
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() != QLatin1String(kInterfaceName)) {
            return false;
        }
        const QList<QVariant> args = message.arguments();
        if (message.member() == QLatin1String("ListDisplays")) {
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(mock_.listDisplays())}));
        }
        if (message.member() == QLatin1String("GetDisplayState")) {
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(mock_.getDisplayState(args.value(0).toString()))}));
        }
        if (message.member() == QLatin1String("ValidateDisplay")) {
            const auto changes = qdbus_cast<QList<ControlChange>>(
                qvariant_cast<QDBusArgument>(args.value(4)));
            return connection.send(message.createReply(QVariantList{
                QVariant::fromValue(mock_.validateDisplay(
                    args.value(0).toString(), args.value(1).toString(),
                    args.value(2).toULongLong(), args.value(3).toULongLong(), changes))}));
        }
        if (message.member() == QLatin1String("ApplyDisplay")) {
            const auto changes = qdbus_cast<QList<ControlChange>>(
                qvariant_cast<QDBusArgument>(args.value(4)));
            const ApplyReply reply = mock_.applyDisplay(
                args.value(0).toString(), args.value(1).toString(),
                args.value(2).toULongLong(), args.value(3).toULongLong(), changes);
            const bool sent = connection.send(message.createReply(QVariantList{
                QVariant::fromValue(reply)}));
            if (sent && reply.code == QLatin1String("OK")
                && reply.eventSequence > lastPublishedEventSequence_) {
                lastPublishedEventSequence_ = reply.eventSequence;
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kInterfaceName),
                    QStringLiteral("DisplayChanged"));
                signal << reply.serviceInstanceUuid
                       << QVariant::fromValue<qulonglong>(reply.serviceGeneration)
                       << QVariant::fromValue<qulonglong>(reply.eventSequence)
                       << QStringLiteral("DISPLAY") << reply.subjectId
                       << QVariant::fromValue<qulonglong>(reply.inventoryGeneration)
                       << QVariant::fromValue<qulonglong>(reply.capabilityGeneration);
                return connection.send(signal);
            }
            return sent;
        }
        return false;
    }

private:
    Mock mock_;
    quint64 lastPublishedEventSequence_ = 30;
};

class Display1ContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        registerMetaTypes();
        bus_ = QDBusConnection::sessionBus();
        QVERIFY(bus_.isConnected());
        QVERIFY(bus_.registerService(QString::fromLatin1(kServiceName)));
        proxy_ = std::make_unique<OrgAdrenalinlinuxSession1Display1Interface>(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), bus_);
        QVERIFY(proxy_->isValid());
    }

    void init()
    {
        if (fixture_) {
            bus_.unregisterObject(QString::fromLatin1(kObjectPath));
            fixture_.reset();
        }
        fixture_ = std::make_unique<DisplayFixture>();
        QVERIFY(bus_.registerVirtualObject(QString::fromLatin1(kObjectPath), fixture_.get()));
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

    void wireTypesAndIntrospectionMatchTheVersionedSchema()
    {
        using hardware1::Capability;
        using hardware1::Device;
        using hardware1::Reply;
        using hardware1::Value;
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Device>())),
                 QByteArray("(ssss)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Value>())),
                 QByteArray("(sbxtds)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Capability>())),
                 QByteArray("(ssssssss(sbxtds)(sbxtds)(sbxtds)(sbxtds)(sbxtds)as)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Reply>())),
                 QByteArray("(sssbsssbstttt)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<ControlChange>())),
                 QByteArray("(s(sbxtds))"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<QList<ControlChange>>())),
                 QByteArray("a(s(sbxtds))"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<ListReply>())),
                 QByteArray("((sssbsssbstttt)a(ssss))"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<StateReply>())),
                 QByteArray("((sssbsssbstttt)(ssss)a(ssssssss(sbxtds)(sbxtds)(sbxtds)(sbxtds)(sbxtds)as))"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(
                     QMetaType::fromType<ValidationReply>())),
                 QByteArray("(ssssbssstbttbsas)"));
        QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<ApplyReply>())),
                 QByteArray("(ssssbsststtbttsb)"));

        const QDBusMessage request = QDBusMessage::createMethodCall(
            QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
        const QDBusMessage response = bus_.call(request);
        QVERIFY2(response.type() == QDBusMessage::ReplyMessage,
                 qPrintable(response.errorMessage()));
        const QString schema = response.arguments().value(0).toString();
        QVERIFY(schema.contains(QStringLiteral("<method name=\"ListDisplays\">")));
        QVERIFY(schema.contains(QStringLiteral("<method name=\"GetDisplayState\">")));
        QVERIFY(schema.contains(QStringLiteral("<method name=\"ValidateDisplay\">")));
        QVERIFY(schema.contains(QStringLiteral("<method name=\"ApplyDisplay\">")));
        QVERIFY(schema.contains(QStringLiteral("<signal name=\"DisplayChanged\">")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"expected_capability_generation\" type=\"t\" direction=\"in\"/>")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"changes\" type=\"a(s(sbxtds))\" direction=\"in\"/>")));
        QVERIFY(schema.contains(QStringLiteral("<arg name=\"reply\" type=\"(ssssbsststtbttsb)\" direction=\"out\"/>")));
        QVERIFY(!schema.contains(QStringLiteral("name=\"token\"")));
        QVERIFY(!schema.contains(QStringLiteral("name=\"capability_token\"")));
    }

    void privateBusReadsCarryHardwareIdentityAndOneGenerationEnvelope()
    {
        auto listPending = proxy_->ListDisplays();
        listPending.waitForFinished();
        QVERIFY2(!listPending.isError(), qPrintable(listPending.error().message()));
        const ListReply list = listPending.argumentAt<0>();
        QVERIFY2(list.isValid(), "list reply must satisfy the shared Hardware1 envelope");
        QCOMPARE(list.snapshot.subjectKind, QStringLiteral("PLATFORM"));
        QCOMPARE(list.snapshot.inventoryGeneration, quint64(5));
        QCOMPARE(list.snapshot.capabilityGeneration, quint64(9));
        QCOMPARE(list.snapshot.eventSequence, quint64(30));
        QCOMPARE(list.displays.size(), 1);
        QCOMPARE(list.displays.constFirst().subjectKind, QStringLiteral("DISPLAY"));
        QCOMPARE(list.displays.constFirst().subjectId, Mock::testDisplaySubjectId());

        auto statePending = proxy_->GetDisplayState(Mock::testDisplaySubjectId());
        statePending.waitForFinished();
        QVERIFY2(!statePending.isError(), qPrintable(statePending.error().message()));
        const StateReply state = statePending.argumentAt<0>();
        QVERIFY2(state.isValid(), "state reply must carry matching display identity and capabilities");
        QCOMPARE(state.snapshot.subjectId, Mock::testDisplaySubjectId());
        QCOMPARE(state.snapshot.inventoryGeneration, list.snapshot.inventoryGeneration);
        QCOMPARE(state.snapshot.capabilityGeneration, list.snapshot.capabilityGeneration);
        QVERIFY(!state.capabilities.isEmpty());

        auto missingPending = proxy_->GetDisplayState(QStringLiteral("unknown-display"));
        missingPending.waitForFinished();
        QVERIFY2(!missingPending.isError(), qPrintable(missingPending.error().message()));
        const StateReply missing = missingPending.argumentAt<0>();
        QCOMPARE(missing.snapshot.code, QStringLiteral("NOT_FOUND"));
        QVERIFY(missing.display.subjectId.isEmpty());
        QVERIFY(missing.capabilities.isEmpty());
    }

    void preSnapshotReadinessFailuresUseInvalidHardwareEnvelope()
    {
        fixture_->setReadinessState(Mock::ReadinessState::Recovering);
        auto recoveringListPending = proxy_->ListDisplays();
        recoveringListPending.waitForFinished();
        QVERIFY2(!recoveringListPending.isError(),
                 qPrintable(recoveringListPending.error().message()));
        const ListReply recoveringList = recoveringListPending.argumentAt<0>();
        QVERIFY(recoveringList.isValid());
        QCOMPARE(recoveringList.snapshot.code, QStringLiteral("BUSY"));
        QVERIFY(!recoveringList.snapshot.snapshotValid);
        QCOMPARE(recoveringList.snapshot.inventoryGeneration, quint64(0));
        QCOMPARE(recoveringList.snapshot.capabilityGeneration, quint64(0));
        QCOMPARE(recoveringList.snapshot.serviceGeneration, quint64(2));
        QVERIFY(recoveringList.displays.isEmpty());

        fixture_->setReadinessState(Mock::ReadinessState::Failed);
        auto unavailableStatePending = proxy_->GetDisplayState(Mock::testDisplaySubjectId());
        unavailableStatePending.waitForFinished();
        QVERIFY2(!unavailableStatePending.isError(),
                 qPrintable(unavailableStatePending.error().message()));
        const StateReply unavailableState = unavailableStatePending.argumentAt<0>();
        QVERIFY(unavailableState.isValid());
        QCOMPARE(unavailableState.snapshot.code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(!unavailableState.snapshot.snapshotValid);
        QCOMPARE(unavailableState.snapshot.inventoryGeneration, quint64(0));
        QCOMPARE(unavailableState.snapshot.capabilityGeneration, quint64(0));
        QVERIFY(unavailableState.display.subjectId.isEmpty());
        QVERIFY(unavailableState.capabilities.isEmpty());

        const QList<ControlChange> changes{brightnessChange(45.0)};
        const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto validationPending = proxy_->ValidateDisplay(operationId,
            Mock::testDisplaySubjectId(), qulonglong(5), qulonglong(9), changes);
        validationPending.waitForFinished();
        QVERIFY2(!validationPending.isError(), qPrintable(validationPending.error().message()));
        const ValidationReply validation = validationPending.argumentAt<0>();
        QVERIFY(validation.isValid());
        QCOMPARE(validation.code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(!validation.snapshotValid);
        QCOMPARE(validation.inventoryGeneration, quint64(0));
        QCOMPARE(validation.capabilityGeneration, quint64(0));

        auto applyPending = proxy_->ApplyDisplay(operationId, Mock::testDisplaySubjectId(),
            qulonglong(5), qulonglong(9), changes);
        applyPending.waitForFinished();
        QVERIFY2(!applyPending.isError(), qPrintable(applyPending.error().message()));
        const ApplyReply apply = applyPending.argumentAt<0>();
        QVERIFY(apply.isValid());
        QCOMPARE(apply.code, QStringLiteral("BACKEND_UNAVAILABLE"));
        QVERIFY(!apply.snapshotValid);
        QCOMPARE(apply.inventoryGeneration, quint64(0));
        QCOMPARE(apply.capabilityGeneration, quint64(0));
        QVERIFY(!apply.effectiveStateVerified);
    }

    void staleCapabilitiesAreRejectedBeforeMutation()
    {
        const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QList<ControlChange> changes{brightnessChange(55.0)};
        auto pending = proxy_->ValidateDisplay(operationId, Mock::testDisplaySubjectId(),
                                               qulonglong(5), qulonglong(8), changes);
        pending.waitForFinished();
        QVERIFY2(!pending.isError(), qPrintable(pending.error().message()));
        const ValidationReply reply = pending.argumentAt<0>();
        QVERIFY(reply.isValid());
        QCOMPARE(reply.code, QStringLiteral("STALE_CAPABILITY"));
        QVERIFY(!reply.valid);
        QCOMPARE(reply.capabilityGeneration, quint64(9));
        QCOMPARE(reply.safetyClass, QStringLiteral("UNKNOWN"));

        ValidationReply invalidSuccess = reply;
        invalidSuccess.code = QStringLiteral("OK");
        invalidSuccess.valid = true;
        invalidSuccess.safetyClass = QStringLiteral("UNKNOWN");
        QVERIFY(!invalidSuccess.isValid());
    }

    void malformedMutationInputsReturnTypedInvalidArgument()
    {
        const QString goodOperationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QList<ControlChange> changes{brightnessChange(45.0)};
        for (const auto &request : {qMakePair(QStringLiteral("malformed"), Mock::testDisplaySubjectId()),
                                    qMakePair(goodOperationId, QString())}) {
            auto validationPending = proxy_->ValidateDisplay(request.first, request.second,
                qulonglong(5), qulonglong(9), changes);
            validationPending.waitForFinished();
            QVERIFY2(!validationPending.isError(), qPrintable(validationPending.error().message()));
            const ValidationReply validation = validationPending.argumentAt<0>();
            QCOMPARE(validation.code, QStringLiteral("INVALID_ARGUMENT"));
            QVERIFY(validation.isValid());

            auto applyPending = proxy_->ApplyDisplay(request.first, request.second,
                qulonglong(5), qulonglong(9), changes);
            applyPending.waitForFinished();
            QVERIFY2(!applyPending.isError(), qPrintable(applyPending.error().message()));
            const ApplyReply apply = applyPending.argumentAt<0>();
            QCOMPARE(apply.code, QStringLiteral("INVALID_ARGUMENT"));
            QVERIFY(apply.isValid());
            QVERIFY(!apply.effectiveStateVerified);
        }
    }

    void safeApplyVerifiesEffectiveStateAndDeduplicatesOperation()
    {
        QSignalSpy changed(proxy_.get(),
            &OrgAdrenalinlinuxSession1Display1Interface::DisplayChanged);
        QVERIFY(changed.isValid());
        const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QList<ControlChange> changes{brightnessChange(60.0)};
        auto validationPending = proxy_->ValidateDisplay(operationId,
            Mock::testDisplaySubjectId(), qulonglong(5), qulonglong(9), changes);
        validationPending.waitForFinished();
        QVERIFY2(!validationPending.isError(), qPrintable(validationPending.error().message()));
        const ValidationReply validation = validationPending.argumentAt<0>();
        QVERIFY(validation.isValid());
        QVERIFY(validation.valid);
        QCOMPARE(validation.safetyClass, QStringLiteral("SAFE"));

        auto firstPending = proxy_->ApplyDisplay(operationId, Mock::testDisplaySubjectId(),
                                                  qulonglong(5), qulonglong(9), changes);
        firstPending.waitForFinished();
        QVERIFY2(!firstPending.isError(), qPrintable(firstPending.error().message()));
        const ApplyReply first = firstPending.argumentAt<0>();
        QVERIFY(first.isValid());
        QCOMPARE(first.code, QStringLiteral("OK"));
        QVERIFY(first.effectiveStateVerified);
        QCOMPARE(first.safetyRouteIntent, QStringLiteral("SAFE_DIRECT_INTENT"));
        QVERIFY(first.eventSequence > 0);
        QCOMPARE(first.capabilityGeneration, quint64(10));

        QTRY_COMPARE(changed.size(), 1);
        const QList<QVariant> event = changed.takeFirst();
        QCOMPARE(event.size(), 7);
        QCOMPARE(event.at(0).toString(), first.serviceInstanceUuid);
        QCOMPARE(event.at(1).toULongLong(), first.serviceGeneration);
        QCOMPARE(event.at(2).toULongLong(), first.eventSequence);
        QCOMPARE(event.at(3).toString(), QStringLiteral("DISPLAY"));
        QCOMPARE(event.at(4).toString(), Mock::testDisplaySubjectId());
        QCOMPARE(event.at(5).toULongLong(), first.inventoryGeneration);
        QCOMPARE(event.at(6).toULongLong(), first.capabilityGeneration);

        auto refreshedPending = proxy_->GetDisplayState(Mock::testDisplaySubjectId());
        refreshedPending.waitForFinished();
        QVERIFY2(!refreshedPending.isError(), qPrintable(refreshedPending.error().message()));
        const StateReply refreshed = refreshedPending.argumentAt<0>();
        QVERIFY(refreshed.isValid());
        QCOMPARE(refreshed.snapshot.capabilityGeneration, first.capabilityGeneration);
        QCOMPARE(refreshed.snapshot.eventSequence, first.eventSequence);
        QCOMPARE(refreshed.capabilities.constFirst().effectiveValue.realValue, 60.0);

        auto retryPending = proxy_->ApplyDisplay(operationId, Mock::testDisplaySubjectId(),
                                                  qulonglong(5), qulonglong(9), changes);
        retryPending.waitForFinished();
        QVERIFY2(!retryPending.isError(), qPrintable(retryPending.error().message()));
        const ApplyReply retry = retryPending.argumentAt<0>();
        QCOMPARE(retry.code, QStringLiteral("OK"));
        QCOMPARE(retry.revision, first.revision);
        QCoreApplication::processEvents();
        QCOMPARE(changed.size(), 0);

        auto reusePending = proxy_->ApplyDisplay(operationId, Mock::testDisplaySubjectId(),
            qulonglong(5), qulonglong(9), {brightnessChange(61.0)});
        reusePending.waitForFinished();
        QVERIFY2(!reusePending.isError(), qPrintable(reusePending.error().message()));
        const ApplyReply reuse = reusePending.argumentAt<0>();
        QCOMPARE(reuse.code, QStringLiteral("CONFLICT"));
        QVERIFY(!reuse.effectiveStateVerified);

        ApplyReply unverifiableSuccess = first;
        unverifiableSuccess.effectiveStateVerified = false;
        QVERIFY(!unverifiableSuccess.isValid());
        unverifiableSuccess = first;
        unverifiableSuccess.safetyRouteIntent = QStringLiteral("NONE");
        QVERIFY(!unverifiableSuccess.isValid());
        unverifiableSuccess = first;
        unverifiableSuccess.safetyRouteIntent = QStringLiteral("UNKNOWN");
        QVERIFY(!unverifiableSuccess.isValid());
    }

    void outputRiskingChangesCarrySyntheticGuardRouteIntentOnly()
    {
        const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ControlChange customResolution;
        customResolution.capabilityId = QStringLiteral("display.custom_resolution");
        customResolution.value.kind = QStringLiteral("ENUM");
        customResolution.value.enumValue = QStringLiteral("mode_1920x1080_60");
        const QList<ControlChange> changes{customResolution};

        auto validationPending = proxy_->ValidateDisplay(operationId,
            Mock::testDisplaySubjectId(), qulonglong(5), qulonglong(9), changes);
        validationPending.waitForFinished();
        QVERIFY2(!validationPending.isError(), qPrintable(validationPending.error().message()));
        const ValidationReply validation = validationPending.argumentAt<0>();
        QCOMPARE(validation.safetyClass, QStringLiteral("OUTPUT_RISKING"));
        QCOMPARE(validation.code, QStringLiteral("UNSUPPORTED"));
        QVERIFY(!validation.valid);

        auto applyPending = proxy_->ApplyDisplay(operationId, Mock::testDisplaySubjectId(),
                                                  qulonglong(5), qulonglong(9), changes);
        applyPending.waitForFinished();
        QVERIFY2(!applyPending.isError(), qPrintable(applyPending.error().message()));
        const ApplyReply apply = applyPending.argumentAt<0>();
        QCOMPARE(apply.code, QStringLiteral("UNSUPPORTED"));
        QCOMPARE(apply.safetyRouteIntent, QStringLiteral("DISPLAY_GUARD_INTENT"));
        QVERIFY(!apply.effectiveStateVerified);
        QVERIFY(apply.isValid());
    }

private:
    static ControlChange brightnessChange(double value)
    {
        ControlChange change;
        change.capabilityId = QStringLiteral("display.brightness");
        change.value.kind = QStringLiteral("REAL");
        change.value.realValue = value;
        return change;
    }

    QDBusConnection bus_ = QDBusConnection::sessionBus();
    std::unique_ptr<DisplayFixture> fixture_;
    std::unique_ptr<OrgAdrenalinlinuxSession1Display1Interface> proxy_;
};

} // namespace

QTEST_GUILESS_MAIN(Display1ContractTest)

#include "display1_contract_test.moc"
