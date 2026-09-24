#include <settings1_adaptor.h>
#include <settings1_interface.h>
#include "interfaces/settings1_mock.h"
#include "interfaces/settings1_client.h"
#include "session_identity.h"
#include "sessiond/session_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>
#include <QUuid>

#include <memory>

namespace {
constexpr auto kServiceName = adrenalin::session1::serviceName;
constexpr auto kObjectPath = adrenalin::session1::objectPath;
constexpr auto kSettingsInterface = adrenalin::session1::settingsInterface;
using SettingsWriteReply = QDBusPendingReply<QString, QString, QString, QString, bool, QString,
                                             QString, qulonglong>;

class DelayedSettingsFixture final : public QDBusVirtualObject
{
public:
    DelayedSettingsFixture(bool delayReads, bool consent, qulonglong revision,
                           qulonglong generation, QObject *parent = nullptr)
        : QDBusVirtualObject(parent), delayReads_(delayReads), consent_(consent),
          revision_(revision), generation_(generation),
          instanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node>"
            "<interface name='org.freedesktop.DBus.Properties'>"
            "<method name='Get'><arg direction='in' type='s'/><arg direction='in' type='s'/>"
            "<arg direction='out' type='v'/></method>"
            "<method name='GetAll'><arg direction='in' type='s'/>"
            "<arg direction='out' type='a{sv}'/></method>"
            "<signal name='PropertiesChanged'><arg type='s'/><arg type='a{sv}'/>"
            "<arg type='as'/></signal></interface>"
            "<interface name='org.adrenalinlinux.Session1.Settings1'>"
            "<property name='InitializationState' type='s' access='read'/>"
            "<property name='ServiceInstanceUuid' type='s' access='read'/>"
            "<property name='ServiceGeneration' type='t' access='read'/>"
            "<property name='ApiMajor' type='q' access='read'/>"
            "<property name='ApiMinor' type='q' access='read'/>"
            "<property name='LastInitializationError' type='s' access='read'/>"
            "<method name='GetProductTelemetryConsent'>"
            "<arg direction='out' type='s'/><arg direction='out' type='b'/>"
            "<arg direction='out' type='t'/></method>"
            "<method name='SetProductTelemetryConsent'>"
            "<arg direction='in' type='s'/><arg direction='in' type='b'/>"
            "<arg direction='in' type='t'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='b'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='t'/></method></interface></node>");
    }

    bool updateAndEmitConsentChanged(const QDBusConnection &connection, bool consent,
                                     qulonglong revision)
    {
        consent_ = consent;
        revision_ = revision;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QString::fromLatin1(kSettingsInterface),
            QStringLiteral("ProductTelemetryConsentChanged"));
        signal << consent_ << revision_;
        return connection.send(signal);
    }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")
            && message.member() == QStringLiteral("Get")
            && message.arguments().size() == 2) {
            const QString property = message.arguments().at(1).toString();
            QVariant value;
            if (property == QStringLiteral("InitializationState")) {
                value = QStringLiteral("READY");
            } else if (property == QStringLiteral("ServiceInstanceUuid")) {
                value = instanceUuid_;
            } else if (property == QStringLiteral("ServiceGeneration")) {
                value = QVariant::fromValue(generation_);
            } else if (property == QStringLiteral("ApiMajor")) {
                value = QVariant::fromValue(ushort(1));
            } else if (property == QStringLiteral("ApiMinor")) {
                value = QVariant::fromValue(ushort(0));
            } else if (property == QStringLiteral("LastInitializationError")) {
                value = QString();
            } else {
                return connection.send(message.createErrorReply(
                    QStringLiteral("org.freedesktop.DBus.Error.UnknownProperty"), property));
            }
            return connection.send(message.createReply(
                QVariantList{QVariant::fromValue(QDBusVariant(value))}));
        }
        if (message.interface() == QString::fromLatin1(kSettingsInterface)
            && message.member() == QStringLiteral("GetProductTelemetryConsent")) {
            ++readCount_;
            if (delayReads_) {
                pendingRead_ = message;
                pendingConsent_ = consent_;
                pendingRevision_ = revision_;
                hasPendingRead_ = true;
                return true;
            }
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("OK"), consent_, revision_}));
        }
        if (message.interface() == QString::fromLatin1(kSettingsInterface)
            && message.member() == QStringLiteral("SetProductTelemetryConsent")) {
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("UNSUPPORTED"), message.arguments().value(0),
                             QStringLiteral("settings.operation.unsupported"),
                             QStringLiteral("The fixture does not implement writes"), false,
                             QStringLiteral("test-fixture"),
                             QStringLiteral("product.telemetry_consent"), revision_}));
        }
        return false;
    }

    int readCount() const { return readCount_; }
    bool hasPendingRead() const { return hasPendingRead_; }
    void setDelayReads(bool delay) { delayReads_ = delay; }

    bool releasePendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead_) {
            return false;
        }
        hasPendingRead_ = false;
        return connection.send(pendingRead_.createReply(
            QVariantList{QStringLiteral("OK"), pendingConsent_, pendingRevision_}));
    }

private:
    bool delayReads_ = false;
    bool consent_ = false;
    qulonglong revision_ = 0;
    bool pendingConsent_ = false;
    qulonglong pendingRevision_ = 0;
    qulonglong generation_ = 0;
    QString instanceUuid_;
    int readCount_ = 0;
    bool hasPendingRead_ = false;
    QDBusMessage pendingRead_;
};

struct ConsentMutationRecord {
    bool enabled = false;
    qulonglong expectedRevision = 0;
    qulonglong resultRevision = 0;
};

struct ConsentMutationState {
    bool enabled = false;
    qulonglong revision = 0;
    QHash<QString, ConsentMutationRecord> operations;
    QStringList operationIds;
};

class RetrySettingsFixture final : public QDBusVirtualObject
{
public:
    RetrySettingsFixture(ConsentMutationState *state, bool dropFirstWriteReply,
                         QObject *parent = nullptr, bool emitChangeSignal = false,
                         bool failNextWrite = false)
        : QDBusVirtualObject(parent), state_(state), dropFirstWriteReply_(dropFirstWriteReply),
          emitChangeSignal_(emitChangeSignal), failNextWrite_(failNextWrite)
    {
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.adrenalinlinux.Session1.Settings1'>"
            "<method name='GetProductTelemetryConsent'><arg direction='out' type='s'/>"
            "<arg direction='out' type='b'/><arg direction='out' type='t'/></method>"
            "<method name='SetProductTelemetryConsent'><arg direction='in' type='s'/>"
            "<arg direction='in' type='b'/><arg direction='in' type='t'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='b'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='t'/></method>"
            "</interface></node>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.interface() != QString::fromLatin1(kSettingsInterface)) {
            return false;
        }
        if (message.member() == QStringLiteral("GetProductTelemetryConsent")) {
            ++readCount_;
            if (holdNextRead_) {
                holdNextRead_ = false;
                pendingRead_ = message;
                hasPendingRead_ = true;
                return true;
            }
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("OK"), state_->enabled, state_->revision}));
        }
        if (message.member() != QStringLiteral("SetProductTelemetryConsent")
            || message.arguments().size() != 3) {
            return false;
        }

        const QString operationId = message.arguments().at(0).toString();
        const bool enabled = message.arguments().at(1).toBool();
        const qulonglong expectedRevision = message.arguments().at(2).toULongLong();
        state_->operationIds.append(operationId);
        if (failNextWrite_) {
            failNextWrite_ = false;
            return connection.send(message.createReply(QVariantList{
                QStringLiteral("IO_ERROR"), operationId,
                QStringLiteral("settings.operation.storage_failed"),
                QStringLiteral("Simulated storage failure"), false,
                QStringLiteral("session-settings"),
                QStringLiteral("product.telemetry_consent"), state_->revision}));
        }
        QString resultCode = QStringLiteral("OK");
        QString humanMessageKey = QStringLiteral("settings.telemetry_consent.updated");
        QString diagnosticMessage;
        bool retryable = false;
        qulonglong resultRevision = state_->revision;
        const auto existing = state_->operations.constFind(operationId);
        if (existing != state_->operations.cend()) {
            if (existing->enabled != enabled || existing->expectedRevision != expectedRevision) {
                resultCode = QStringLiteral("CONFLICT");
                humanMessageKey = QStringLiteral("settings.operation.conflict");
                diagnosticMessage = QStringLiteral("Operation ID was reused with different values");
            } else {
                resultRevision = existing->resultRevision;
            }
        } else if (expectedRevision != state_->revision) {
            resultCode = QStringLiteral("STALE_REVISION");
            humanMessageKey = QStringLiteral("settings.operation.stale_revision");
            diagnosticMessage = QStringLiteral("Expected revision does not match current state");
        } else {
            state_->enabled = enabled;
            ++state_->revision;
            resultRevision = state_->revision;
            state_->operations.insert(operationId,
                ConsentMutationRecord{enabled, expectedRevision, resultRevision});
            if (emitChangeSignal_) {
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kSettingsInterface),
                    QStringLiteral("ProductTelemetryConsentChanged"));
                signal << state_->enabled << state_->revision;
                if (!connection.send(signal)) {
                    return false;
                }
            }
        }

        if (dropFirstWriteReply_) {
            dropFirstWriteReply_ = false;
            return connection.send(message.createErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Simulated accepted write with lost reply")));
        }
        return connection.send(message.createReply(QVariantList{
            resultCode, operationId, humanMessageKey, diagnosticMessage, retryable,
            QStringLiteral("session-settings"), QStringLiteral("product.telemetry_consent"),
            resultRevision}));
    }

    int readCount() const { return readCount_; }
    void holdNextRead() { holdNextRead_ = true; }
    bool hasPendingRead() const { return hasPendingRead_; }
    bool releasePendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead_) {
            return false;
        }
        hasPendingRead_ = false;
        return connection.send(pendingRead_.createReply(
            QVariantList{QStringLiteral("OK"), state_->enabled, state_->revision}));
    }

private:
    ConsentMutationState *state_ = nullptr;
    bool dropFirstWriteReply_ = false;
    bool emitChangeSignal_ = false;
    bool failNextWrite_ = false;
    int readCount_ = 0;
    bool holdNextRead_ = false;
    bool hasPendingRead_ = false;
    QDBusMessage pendingRead_;
};
}

class SessionContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void operationResultVocabularyIsStable();
    void mockContractSupportsReadWriteAndOptimisticConcurrency();
    void unsupportedSchemaFailsBeforeReady();
    void generatedDbusContractPersistsAndRejectsStaleAndConflictingWrites();
    void clientOwnerRecoveryWhileInitialReadIsOutstanding();
    void clientRetriesUncertainMutationWithSameOperationId();
    void clientRecoversAfterTerminalMutationFailure();
    void clientConsumesOwnWriteSignalWithoutReplaying();
    void clientRefreshesAfterRevisionGap();
    void qmlPreferenceRoundTripsAndSurvivesGuiRestart();
};

void SessionContractTest::operationResultVocabularyIsStable()
{
    const QStringList expected{
        QStringLiteral("OK"),
        QStringLiteral("INVALID_ARGUMENT"),
        QStringLiteral("UNSUPPORTED"),
        QStringLiteral("NOT_FOUND"),
        QStringLiteral("PERMISSION_DENIED"),
        QStringLiteral("AUTHORIZATION_CANCELLED"),
        QStringLiteral("CANCELLED"),
        QStringLiteral("INTERACTION_REQUIRED"),
        QStringLiteral("BUSY"),
        QStringLiteral("CONFLICT"),
        QStringLiteral("STALE_REVISION"),
        QStringLiteral("INCOMPATIBLE_VERSION"),
        QStringLiteral("BACKEND_UNAVAILABLE"),
        QStringLiteral("BACKEND_FAILURE"),
        QStringLiteral("TIMEOUT"),
        QStringLiteral("DEVICE_DISCONNECTED"),
        QStringLiteral("STALE_CAPABILITY"),
        QStringLiteral("VALIDATION_FAILED"),
        QStringLiteral("APPLY_FAILED"),
        QStringLiteral("VERIFY_FAILED"),
        QStringLiteral("ROLLBACK_FAILED"),
        QStringLiteral("IO_ERROR"),
        QStringLiteral("ENCODER_UNAVAILABLE"),
        QStringLiteral("PORTAL_DENIED"),
        QStringLiteral("AUTH_REQUIRED"),
        QStringLiteral("NETWORK_ERROR"),
        QStringLiteral("INTERNAL_ERROR"),
    };
    QSet<QString> uniqueCodes;
    for (const QString &name : expected) {
        const auto code = adrenalin::contracts::operationResultCodeFromName(name);
        QVERIFY(code.has_value());
        QCOMPARE(adrenalin::contracts::operationResultCodeName(*code), name);
        QVERIFY(!uniqueCodes.contains(name));
        uniqueCodes.insert(name);
    }
    QCOMPARE(uniqueCodes.size(), expected.size());
    QVERIFY(!adrenalin::contracts::operationResultCodeFromName(QStringLiteral("NOT_READY")));
}

void SessionContractTest::mockContractSupportsReadWriteAndOptimisticConcurrency()
{
    Settings1Mock mock;
    QCOMPARE(mock.initializationState(), QStringLiteral("READY"));
    QCOMPARE(mock.apiMajor(), ushort(1));
    const auto initial = mock.getProductTelemetryConsent();
    QCOMPARE(initial.resultCode, QStringLiteral("OK"));
    QVERIFY(!initial.enabled);
    QCOMPARE(initial.revision, quint64(0));

    const auto write = mock.setProductTelemetryConsent(QStringLiteral("operation-1"), true, 0);
    QCOMPARE(write.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(write.result.revision, quint64(1));
    QCOMPARE(write.result.operationId, QStringLiteral("operation-1"));
    QCOMPARE(write.result.provider, QStringLiteral("session-settings"));
    QCOMPARE(write.result.subjectId, QStringLiteral("product.telemetry_consent"));
    const auto duplicate = mock.setProductTelemetryConsent(QStringLiteral("operation-1"), true, 0);
    QCOMPARE(duplicate.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(duplicate.result.revision, quint64(1));
    QCOMPARE(mock.setProductTelemetryConsent(QStringLiteral("operation-1"), false, 0).result.code,
             adrenalin::contracts::OperationResultCode::Conflict);
    QCOMPARE(mock.setProductTelemetryConsent(QStringLiteral("operation-2"), false, 0).result.code,
             adrenalin::contracts::OperationResultCode::StaleRevision);
}

void SessionContractTest::unsupportedSchemaFailsBeforeReady()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("unsupported.sqlite3"));
    {
        const QString connectionName = QStringLiteral("unsupported-schema-fixture");
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        {
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE service_metadata (key TEXT PRIMARY KEY, value TEXT)")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO service_metadata VALUES ('schema_version', '99')")));
        }
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
    }

    SessionService service(databasePath);
    QVERIFY(!service.initialize());
    QCOMPARE(service.initializationState(), QStringLiteral("FAILED"));
    QVERIFY(!service.lastInitializationError().isEmpty());
}

void SessionContractTest::generatedDbusContractPersistsAndRejectsStaleAndConflictingWrites()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    auto startService = [&](bool initializeNow = true) {
        auto *service = new SessionService(databasePath, this);
        auto *adaptor = new Settings1Adaptor(service);
        Q_UNUSED(adaptor);
        if (!bus.registerObject(QString::fromLatin1(kObjectPath), service,
                                QDBusConnection::ExportAdaptors)) {
            delete service;
            return static_cast<SessionService *>(nullptr);
        }
        if (!bus.registerService(QString::fromLatin1(kServiceName))) {
            bus.unregisterObject(QString::fromLatin1(kObjectPath));
            delete service;
            return static_cast<SessionService *>(nullptr);
        }
        if (initializeNow && !service->initialize()) {
            bus.unregisterService(QString::fromLatin1(kServiceName));
            bus.unregisterObject(QString::fromLatin1(kObjectPath));
            delete service;
            return static_cast<SessionService *>(nullptr);
        }
        return service;
    };
    auto stopService = [&bus](SessionService *service) {
        bus.unregisterService(QString::fromLatin1(kServiceName));
        bus.unregisterObject(QString::fromLatin1(kObjectPath));
        delete service;
    };

    SessionService *service = startService(false);
    QVERIFY(service != nullptr);
    QCOMPARE(service->initializationState(), QStringLiteral("STARTING"));
    QCOMPARE(service->apiMajor(), ushort(1));
    QCOMPARE(service->apiMinor(), ushort(0));

    OrgAdrenalinlinuxSession1Settings1Interface proxy(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), bus);
    QVERIFY(proxy.isValid());
    QCOMPARE(proxy.apiMajor(), ushort(1));
    QCOMPARE(proxy.apiMinor(), ushort(0));
    QCOMPARE(proxy.initializationState(), QStringLiteral("STARTING"));
    QCOMPARE(proxy.lastInitializationError(), QString());
    auto notReadyPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(notReadyPending.isFinished(), 2000);
    const QDBusPendingReply<QString, bool, qulonglong> notReady = notReadyPending;
    QVERIFY(!notReady.isError());
    QCOMPARE(notReady.argumentAt<0>(), QStringLiteral("NOT_READY"));
    auto notReadyWritePending = proxy.SetProductTelemetryConsent(
        QStringLiteral("pre-recovery-write"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(notReadyWritePending.isFinished(), 2000);
    const SettingsWriteReply notReadyWrite = notReadyWritePending;
    QVERIFY(!notReadyWrite.isError());
    QCOMPARE(notReadyWrite.argumentAt<0>(), QStringLiteral("BACKEND_UNAVAILABLE"));
    QCOMPARE(notReadyWrite.argumentAt<1>(), QStringLiteral("pre-recovery-write"));
    QVERIFY(notReadyWrite.argumentAt<4>());
    QVERIFY(service->initialize());

    const QString firstUuid = service->serviceInstanceUuid();
    const quint64 firstGeneration = service->serviceGeneration();
    QCOMPARE(service->initializationState(), QStringLiteral("READY"));

    QCOMPARE(proxy.initializationState(), QStringLiteral("READY"));
    QCOMPARE(proxy.lastInitializationError(), QString());
    QCOMPARE(proxy.serviceInstanceUuid(), firstUuid);
    QCOMPARE(proxy.serviceGeneration(), firstGeneration);

    auto readPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(readPending.isFinished(), 2000);
    const QDBusPendingReply<QString, bool, qulonglong> initial = readPending;
    QVERIFY(!initial.isError());
    QCOMPARE(initial.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(!initial.argumentAt<1>());
    QCOMPARE(initial.argumentAt<2>(), qulonglong(0));

    QSignalSpy changed(&proxy, &OrgAdrenalinlinuxSession1Settings1Interface::ProductTelemetryConsentChanged);
    QVERIFY(changed.isValid());

    auto writePending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-1"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(writePending.isFinished(), 2000);
    const SettingsWriteReply write = writePending;
    QVERIFY(!write.isError());
    QCOMPARE(write.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(write.argumentAt<1>(), QStringLiteral("settings-write-1"));
    QCOMPARE(write.argumentAt<2>(), QStringLiteral("settings.telemetry_consent.updated"));
    QCOMPARE(write.argumentAt<3>(), QString());
    QVERIFY(!write.argumentAt<4>());
    QCOMPARE(write.argumentAt<5>(), QStringLiteral("session-settings"));
    QCOMPARE(write.argumentAt<6>(), QStringLiteral("product.telemetry_consent"));
    QCOMPARE(write.argumentAt<7>(), qulonglong(1));
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 2000);

    auto duplicatePending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-1"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(duplicatePending.isFinished(), 2000);
    const SettingsWriteReply duplicate = duplicatePending;
    QCOMPARE(duplicate.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(duplicate.argumentAt<7>(), qulonglong(1));
    QTest::qWait(50);
    QCOMPARE(changed.count(), 1);

    auto conflictPending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-1"), false, 0);
    QTRY_VERIFY_WITH_TIMEOUT(conflictPending.isFinished(), 2000);
    const SettingsWriteReply conflict = conflictPending;
    QCOMPARE(conflict.argumentAt<0>(), QStringLiteral("CONFLICT"));
    QCOMPARE(conflict.argumentAt<1>(), QStringLiteral("settings-write-1"));
    QCOMPARE(conflict.argumentAt<2>(), QStringLiteral("settings.operation.conflict"));
    QVERIFY(!conflict.argumentAt<4>());

    auto nextWritePending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-2"), false, 1);
    QTRY_VERIFY_WITH_TIMEOUT(nextWritePending.isFinished(), 2000);
    const SettingsWriteReply nextWrite = nextWritePending;
    QCOMPARE(nextWrite.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(nextWrite.argumentAt<7>(), qulonglong(2));
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 2000);
    QCOMPARE(changed.at(1).at(0).toBool(), false);
    QCOMPARE(changed.at(1).at(1).toULongLong(), qulonglong(2));

    auto stalePending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-3"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(stalePending.isFinished(), 2000);
    const SettingsWriteReply stale = stalePending;
    QCOMPARE(stale.argumentAt<0>(), QStringLiteral("STALE_REVISION"));
    QCOMPARE(stale.argumentAt<1>(), QStringLiteral("settings-write-3"));
    QCOMPARE(stale.argumentAt<2>(), QStringLiteral("settings.operation.stale_revision"));

    auto replayPending = proxy.SetProductTelemetryConsent(QStringLiteral("settings-write-1"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(replayPending.isFinished(), 2000);
    const SettingsWriteReply replay = replayPending;
    QCOMPARE(replay.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(replay.argumentAt<7>(), qulonglong(1));
    QTest::qWait(50);
    QCOMPARE(changed.count(), 2);

    auto currentPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(currentPending.isFinished(), 2000);
    const QDBusPendingReply<QString, bool, qulonglong> current = currentPending;
    QCOMPARE(current.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(!current.argumentAt<1>());
    QCOMPARE(current.argumentAt<2>(), qulonglong(2));

    stopService(service);
    service = startService();
    QVERIFY(service != nullptr);
    QCOMPARE(service->initializationState(), QStringLiteral("READY"));
    QCOMPARE(service->serviceGeneration(), firstGeneration + 1);
    QVERIFY(service->serviceInstanceUuid() != firstUuid);

    auto retriedAfterRestartPending = proxy.SetProductTelemetryConsent(
        QStringLiteral("settings-write-1"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(retriedAfterRestartPending.isFinished(), 2000);
    const SettingsWriteReply retriedAfterRestart = retriedAfterRestartPending;
    QCOMPARE(retriedAfterRestart.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(retriedAfterRestart.argumentAt<7>(), qulonglong(1));
    QTest::qWait(50);
    QCOMPARE(changed.count(), 2);

    auto afterRestartPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(afterRestartPending.isFinished(), 2000);
    const QDBusPendingReply<QString, bool, qulonglong> afterRestart = afterRestartPending;
    QVERIFY(!afterRestart.isError());
    QCOMPARE(afterRestart.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(!afterRestart.argumentAt<1>());
    QCOMPARE(afterRestart.argumentAt<2>(), qulonglong(2));
    stopService(service);

}

void SessionContractTest::clientOwnerRecoveryWhileInitialReadIsOutstanding()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    auto oldService = std::make_unique<DelayedSettingsFixture>(true, false, 7, 1, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), oldService.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-reconnect-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_COMPARE_WITH_TIMEOUT(oldService->readCount(), 1, 2000);
    QVERIFY(oldService->hasPendingRead());
    QVERIFY(!client->ready());

    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    auto newService = std::make_unique<DelayedSettingsFixture>(true, true, 8, 2, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), newService.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    QTRY_COMPARE_WITH_TIMEOUT(client->status(), QStringLiteral("RECONCILING"), 2000);
    QVERIFY(oldService->hasPendingRead());
    QCOMPARE(oldService->readCount(), 1);
    QCOMPARE(newService->readCount(), 0);

    QVERIFY(oldService->releasePendingRead(bus));
    QTRY_COMPARE_WITH_TIMEOUT(newService->readCount(), 1, 2000);
    QVERIFY(newService->hasPendingRead());
    QVERIFY(!client->ready());
    QCOMPARE(client->status(), QStringLiteral("RECONCILING"));
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(0));

    QVERIFY(newService->updateAndEmitConsentChanged(bus, false, 9));
    QTest::qWait(50);
    QVERIFY(!client->ready());
    QCOMPARE(client->status(), QStringLiteral("RECONCILING"));
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(0));

    QVERIFY(newService->releasePendingRead(bus));
    QTRY_COMPARE_WITH_TIMEOUT(newService->readCount(), 2, 2000);
    QVERIFY(newService->hasPendingRead());
    QVERIFY(!client->ready());
    QVERIFY(newService->releasePendingRead(bus));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QCOMPARE(client->status(), QStringLiteral("READY"));
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(9));

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientRetriesUncertainMutationWithSameOperationId()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ConsentMutationState mutationState;
    auto firstService = std::make_unique<RetrySettingsFixture>(&mutationState, true, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), firstService.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-retry-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    client->setProductTelemetryConsent(true);
    QTRY_COMPARE_WITH_TIMEOUT(client->status(), QStringLiteral("DISCONNECTED"), 2000);
    QCOMPARE(mutationState.operationIds.size(), 1);
    QCOMPARE(mutationState.revision, qulonglong(1));
    QVERIFY(mutationState.enabled);

    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    auto replacement = std::make_unique<RetrySettingsFixture>(&mutationState, false, this);
    replacement->holdNextRead();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), replacement.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    QTRY_VERIFY_WITH_TIMEOUT(replacement->hasPendingRead(), 2000);
    QTest::qWait(150);
    QCOMPARE(replacement->readCount(), 1);
    QVERIFY(replacement->releasePendingRead(bus));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 3000);
    QCOMPARE(client->status(), QStringLiteral("READY"));
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(1));
    QCOMPARE(mutationState.revision, qulonglong(1));
    QCOMPARE(mutationState.operationIds.size(), 2);
    QCOMPARE(mutationState.operationIds.at(0), mutationState.operationIds.at(1));
    QCOMPARE(replacement->readCount(), 2);

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientRecoversAfterTerminalMutationFailure()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ConsentMutationState mutationState;
    auto service = std::make_unique<RetrySettingsFixture>(&mutationState, false, this, false, true);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-terminal-failure-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    client->setProductTelemetryConsent(true);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready()
                                 && client->lastOperationCode() == QStringLiteral("IO_ERROR"),
                             2000);
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(0));

    client->setProductTelemetryConsent(true);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready() && client->productTelemetryConsent(), 2000);
    QCOMPARE(client->lastOperationCode(), QString());
    QCOMPARE(client->revision(), qulonglong(1));
    QCOMPARE(mutationState.revision, qulonglong(1));
    QCOMPARE(mutationState.operationIds.size(), 2);
    QVERIFY(mutationState.operationIds.at(0) != mutationState.operationIds.at(1));

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientConsumesOwnWriteSignalWithoutReplaying()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ConsentMutationState mutationState;
    auto service = std::make_unique<RetrySettingsFixture>(&mutationState, false, this, true);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-own-signal-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    client->setProductTelemetryConsent(true);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(1));
    QCOMPARE(mutationState.revision, qulonglong(1));
    QCOMPARE(mutationState.operationIds.size(), 1);

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientRefreshesAfterRevisionGap()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    auto service = std::make_unique<DelayedSettingsFixture>(false, false, 3, 1, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-gap-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(3));

    service->setDelayReads(true);
    QVERIFY(service->updateAndEmitConsentChanged(bus, true, 5));
    QTRY_COMPARE_WITH_TIMEOUT(service->readCount(), 2, 2000);
    QVERIFY(service->hasPendingRead());
    QVERIFY(!client->ready());
    QCOMPARE(client->status(), QStringLiteral("RECONCILING"));
    QVERIFY(!client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(3));

    QVERIFY(service->releasePendingRead(bus));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(5));

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::qmlPreferenceRoundTripsAndSurvivesGuiRestart()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    SessionService service(databasePath);
    Settings1Adaptor adaptor(&service);
    QVERIFY(bus.registerObject(QString::fromLatin1(kObjectPath), &service,
                               QDBusConnection::ExportAdaptors));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));
    QVERIFY(service.initialize());

    auto launchUi = [](std::unique_ptr<Settings1Client> &client,
                       std::unique_ptr<QQmlApplicationEngine> &engine) -> QObject * {
        client = std::make_unique<Settings1Client>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty(QStringLiteral("sessionSettingsClient"),
                                                  client.get());
        engine->loadFromModule(QStringLiteral("Adrenalin.SessionTests"),
                               QStringLiteral("PreferenceHarness"));
        if (engine->rootObjects().isEmpty()) {
            return nullptr;
        }
        return engine->rootObjects().constFirst();
    };

    std::unique_ptr<Settings1Client> firstClient;
    std::unique_ptr<QQmlApplicationEngine> firstEngine;
    QObject *firstRoot = launchUi(firstClient, firstEngine);
    QVERIFY(firstRoot != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(firstClient->ready(), 2000);
    QObject *firstSwitch = firstRoot->findChild<QObject *>(
        QStringLiteral("productTelemetryConsentSwitch"));
    QVERIFY(firstSwitch != nullptr);
    QVERIFY(!firstSwitch->property("checked").toBool());
    QVERIFY(QMetaObject::invokeMethod(firstSwitch, "click"));
    QTRY_VERIFY_WITH_TIMEOUT(firstClient->ready(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(firstClient->productTelemetryConsent(), 2000);
    QCOMPARE(firstClient->revision(), qulonglong(1));
    QTRY_VERIFY_WITH_TIMEOUT(firstSwitch->property("checked").toBool(), 2000);

    firstEngine.reset();
    firstClient.reset();

    std::unique_ptr<Settings1Client> restartedClient;
    std::unique_ptr<QQmlApplicationEngine> restartedEngine;
    QObject *restartedRoot = launchUi(restartedClient, restartedEngine);
    QVERIFY(restartedRoot != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(restartedClient->ready(), 2000);
    QObject *restartedSwitch = restartedRoot->findChild<QObject *>(
        QStringLiteral("productTelemetryConsentSwitch"));
    QVERIFY(restartedSwitch != nullptr);
    QVERIFY(restartedSwitch->property("checked").toBool());
    QCOMPARE(restartedClient->revision(), qulonglong(1));

    restartedEngine.reset();
    restartedClient.reset();
    bus.unregisterService(QString::fromLatin1(kServiceName));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
}

QTEST_MAIN(SessionContractTest)
#include "session_contract_test.moc"
