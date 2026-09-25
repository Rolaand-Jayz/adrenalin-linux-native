#include <settings1_adaptor.h>
#include <notifications1_adaptor.h>
#include <notifications1_interface.h>
#include <notifications1_interface.h>
#include <settings1_interface.h>
#include <service1_interface.h>
#include "interfaces/settings1_mock.h"
#include "interfaces/settings1_client.h"
#include "interfaces/toast_notifications_client.h"
#include "interfaces/service_readiness_contract.h"
#include "interfaces/hardware1_contract_types.h"
#include "session_identity.h"
#include "sessiond/service1_property_notifications.h"
#include "sessiond/session_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>
#include <QUuid>

#include <algorithm>
#include <memory>
#include <limits>

namespace {
constexpr auto kServiceName = adrenalin::session1::serviceName;
constexpr auto kObjectPath = adrenalin::session1::objectPath;
constexpr auto kSettingsInterface = adrenalin::session1::settingsInterface;
using SettingsWriteReply = QDBusPendingReply<QString, QString, QString, QString, bool, QString,
                                             QString, qulonglong>;
using Notification = adrenalin::contracts::notifications1::Notification;
using NotificationsListReply = QDBusPendingReply<QString, QString, qulonglong, qulonglong,
                                                 qulonglong, QList<Notification>>;
using NotificationsMarkReadReply = QDBusPendingReply<QString, QString, QString, QString, bool,
                                                     QString, QString, qulonglong, bool, QString,
                                                     qulonglong, qulonglong>;

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
            "<interface name='org.adrenalinlinux.Session1.Service1'>"
            "<property name='InitializationState' type='s' access='read'/>"
            "<property name='ServiceInstanceUuid' type='s' access='read'/>"
            "<property name='ServiceGeneration' type='t' access='read'/>"
            "<property name='EventSequence' type='t' access='read'/>"
            "<property name='ApiMajor' type='q' access='read'/>"
            "<property name='ApiMinor' type='q' access='read'/>"
            "<property name='LastInitializationError' type='s' access='read'/>"
            "<signal name='EventPublished'><arg type='s'/><arg type='t'/>"
            "<arg type='t'/><arg type='s'/><arg type='s'/></signal>"
            "</interface>"
            "<interface name='org.adrenalinlinux.Session1.Settings1'>"
            "<method name='GetProductTelemetryConsent'>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='t'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='b'/><arg direction='out' type='t'/></method>"
            "<method name='SetProductTelemetryConsent'>"
            "<arg direction='in' type='s'/><arg direction='in' type='b'/>"
            "<arg direction='in' type='t'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='b'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='s'/>"
            "<arg direction='out' type='t'/></method></interface></node>");
    }

    bool updateAndEmitConsentChanged(const QDBusConnection &connection, bool consent,
                                     qulonglong revision, const QString &eventInstanceUuid = {},
                                     qulonglong eventGeneration = 0,
                                     const QString &subjectId = QStringLiteral("product.telemetry_consent"),
                                     const QString &subjectKind = QStringLiteral("PREFERENCE"))
    {
        consent_ = consent;
        revision_ = revision;
        ++eventSequence_;
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QString::fromLatin1(kSettingsInterface),
            QStringLiteral("ProductTelemetryConsentChanged"));
        signal << (eventInstanceUuid.isEmpty() ? instanceUuid_ : eventInstanceUuid)
               << (eventGeneration == 0 ? generation_ : eventGeneration) << eventSequence_
               << subjectKind << subjectId << consent_ << revision_;
        return connection.send(signal);
    }

    bool emitConsentChanged(const QDBusConnection &connection, bool enabled,
                            qulonglong revision, qulonglong eventSequence,
                            const QString &eventInstanceUuid = {}, qulonglong eventGeneration = 0,
                            const QString &subjectId = QStringLiteral("product.telemetry_consent"),
                            const QString &subjectKind = QStringLiteral("PREFERENCE"))
    {
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QString::fromLatin1(kSettingsInterface),
            QStringLiteral("ProductTelemetryConsentChanged"));
        signal << (eventInstanceUuid.isEmpty() ? instanceUuid_ : eventInstanceUuid)
               << (eventGeneration == 0 ? generation_ : eventGeneration) << eventSequence
               << subjectKind << subjectId << enabled << revision;
        return connection.send(signal);
    }

    void skipEventSequence(qulonglong count) { eventSequence_ += count; }

    bool handleMessage(const QDBusMessage &message,
                       const QDBusConnection &connection) override
    {
        if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")
            && message.member() == QStringLiteral("GetAll")
            && message.arguments().size() == 1
            && message.arguments().at(0).toString()
                == QStringLiteral("org.adrenalinlinux.Session1.Service1")) {
            QVariantMap properties{
                {QStringLiteral("InitializationState"), readinessState_},
                {QStringLiteral("ServiceInstanceUuid"), instanceUuid_},
                {QStringLiteral("ServiceGeneration"), QVariant::fromValue(generation_)},
                {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)},
                {QStringLiteral("ApiMajor"), QVariant::fromValue(apiMajor_)},
                {QStringLiteral("ApiMinor"), QVariant::fromValue(ushort(0))},
                {QStringLiteral("LastInitializationError"), QString()},
            };
            return connection.send(message.createReply(QVariantList{properties}));
        }
        if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")
            && message.member() == QStringLiteral("Get")
            && message.arguments().size() == 2) {
            const QString property = message.arguments().at(1).toString();
            QVariant value;
            if (property == QStringLiteral("InitializationState")) {
                value = readinessState_;
            } else if (property == QStringLiteral("ServiceInstanceUuid")) {
                value = instanceUuid_;
            } else if (property == QStringLiteral("ServiceGeneration")) {
                value = QVariant::fromValue(generation_);
            } else if (property == QStringLiteral("EventSequence")) {
                value = QVariant::fromValue(eventSequence_);
            } else if (property == QStringLiteral("ApiMajor")) {
                value = QVariant::fromValue(apiMajor_);
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
                pendingEventSequence_ = eventSequence_;
                hasPendingRead_ = true;
                return true;
            }
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("OK"), instanceUuid_, generation_, eventSequence_,
                             consent_, revision_}));
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
    bool publishReadiness(const QDBusConnection &connection, QString state, ushort apiMajor = 1)
    {
        const ushort previousApiMajor = apiMajor_;
        readinessState_ = std::move(state);
        apiMajor_ = apiMajor;
        ++eventSequence_;
        QDBusMessage eventSignal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        eventSignal << instanceUuid_ << generation_ << eventSequence_
                    << QStringLiteral("SERVICE") << QStringLiteral("service.readiness");
        if (!connection.send(eventSignal)) {
            return false;
        }
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        QVariantMap changedProperties{
            {QStringLiteral("InitializationState"), readinessState_},
            {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)}};
        if (apiMajor_ != previousApiMajor) {
            changedProperties.insert(QStringLiteral("ApiMajor"), QVariant::fromValue(apiMajor_));
        }
        signal << QStringLiteral("org.adrenalinlinux.Session1.Service1")
               << changedProperties << QStringList{};
        return connection.send(signal);
    }

    bool releasePendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead_) {
            return false;
        }
        hasPendingRead_ = false;
        return connection.send(pendingRead_.createReply(
            QVariantList{QStringLiteral("OK"), instanceUuid_, generation_, pendingEventSequence_,
                         pendingConsent_, pendingRevision_}));
    }

private:
    bool delayReads_ = false;
    QString readinessState_ = QStringLiteral("READY");
    ushort apiMajor_ = 1;
    bool consent_ = false;
    qulonglong revision_ = 0;
    bool pendingConsent_ = false;
    qulonglong pendingRevision_ = 0;
    qulonglong eventSequence_ = 0;
    qulonglong pendingEventSequence_ = 0;
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
    QList<bool> operationValues;
    QList<qulonglong> expectedRevisions;
};

class RetrySettingsFixture final : public QDBusVirtualObject
{
public:
    RetrySettingsFixture(ConsentMutationState *state, bool dropFirstWriteReply,
                         QObject *parent = nullptr, bool emitChangeSignal = false,
                         bool failNextWrite = false, qulonglong generation = 1,
                         QString readinessState = QStringLiteral("READY"), ushort apiMajor = 1)
        : QDBusVirtualObject(parent), state_(state), dropFirstWriteReply_(dropFirstWriteReply),
          emitChangeSignal_(emitChangeSignal), failNextWrite_(failNextWrite),
          generation_(generation), readinessState_(std::move(readinessState)),
          apiMajor_(apiMajor),
          instanceUuid_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<node><interface name='org.freedesktop.DBus.Properties'>"
            "<method name='GetAll'><arg direction='in' type='s'/><arg direction='out' type='a{sv}'/></method>"
            "<signal name='PropertiesChanged'><arg type='s'/><arg type='a{sv}'/><arg type='as'/></signal>"
            "</interface><interface name='org.adrenalinlinux.Session1.Service1'>"
            "<property name='InitializationState' type='s' access='read'/>"
            "<property name='ServiceInstanceUuid' type='s' access='read'/>"
            "<property name='ServiceGeneration' type='t' access='read'/>"
            "<property name='EventSequence' type='t' access='read'/>"
            "<property name='ApiMajor' type='q' access='read'/>"
            "<property name='ApiMinor' type='q' access='read'/>"
            "<property name='LastInitializationError' type='s' access='read'/>"
            "<signal name='EventPublished'><arg type='s'/><arg type='t'/>"
            "<arg type='t'/><arg type='s'/><arg type='s'/></signal>"
            "</interface><interface name='org.adrenalinlinux.Session1.Settings1'>"
            "<method name='GetProductTelemetryConsent'><arg direction='out' type='s'/>"
            "<arg direction='out' type='s'/><arg direction='out' type='t'/>"
            "<arg direction='out' type='t'/><arg direction='out' type='b'/>"
            "<arg direction='out' type='t'/></method>"
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
        if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")
            && message.member() == QStringLiteral("GetAll")
            && message.arguments().size() == 1
            && message.arguments().at(0).toString()
                == QStringLiteral("org.adrenalinlinux.Session1.Service1")) {
            QVariantMap properties{
                {QStringLiteral("InitializationState"), readinessState_},
                {QStringLiteral("ServiceInstanceUuid"), instanceUuid_},
                {QStringLiteral("ServiceGeneration"), QVariant::fromValue(generation_)},
                {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)},
                {QStringLiteral("ApiMajor"), QVariant::fromValue(apiMajor_)},
                {QStringLiteral("ApiMinor"), QVariant::fromValue(ushort(0))},
                {QStringLiteral("LastInitializationError"), QString()},
            };
            return connection.send(message.createReply(QVariantList{properties}));
        }
        if (message.interface() != QString::fromLatin1(kSettingsInterface)) {
            return false;
        }
        if (message.member() == QStringLiteral("GetProductTelemetryConsent")) {
            ++readCount_;
            if (holdNextRead_) {
                holdNextRead_ = false;
                pendingRead_ = message;
                pendingEnabled_ = state_->enabled;
                pendingRevision_ = state_->revision;
                pendingEventSequence_ = eventSequence_;
                hasPendingRead_ = true;
                return true;
            }
            return connection.send(message.createReply(
                QVariantList{QStringLiteral("OK"), instanceUuid_, generation_, eventSequence_,
                             state_->enabled, state_->revision}));
        }
        if (message.member() != QStringLiteral("SetProductTelemetryConsent")
            || message.arguments().size() != 3) {
            return false;
        }

        const QString operationId = message.arguments().at(0).toString();
        const bool enabled = message.arguments().at(1).toBool();
        const qulonglong expectedRevision = message.arguments().at(2).toULongLong();
        state_->operationIds.append(operationId);
        state_->operationValues.append(enabled);
        state_->expectedRevisions.append(expectedRevision);
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
        bool newOperation = false;
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
            newOperation = true;
            state_->enabled = enabled;
            ++state_->revision;
            resultRevision = state_->revision;
            state_->operations.insert(operationId,
                ConsentMutationRecord{enabled, expectedRevision, resultRevision});
        }

        if (resultCode == QStringLiteral("OK") && newOperation) {
            ++eventSequence_;
            if (emitChangeSignal_) {
                QDBusMessage signal = QDBusMessage::createSignal(
                    QString::fromLatin1(kObjectPath), QString::fromLatin1(kSettingsInterface),
                    QStringLiteral("ProductTelemetryConsentChanged"));
                signal << instanceUuid_ << generation_ << eventSequence_
                       << QStringLiteral("PREFERENCE")
                       << QStringLiteral("product.telemetry_consent")
                       << state_->enabled << state_->revision;
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
    bool publishReadiness(const QDBusConnection &connection, QString state, ushort apiMajor = 1)
    {
        const ushort previousApiMajor = apiMajor_;
        readinessState_ = std::move(state);
        apiMajor_ = apiMajor;
        ++eventSequence_;
        QDBusMessage eventSignal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        eventSignal << instanceUuid_ << generation_ << eventSequence_
                    << QStringLiteral("SERVICE") << QStringLiteral("service.readiness");
        if (!connection.send(eventSignal)) {
            return false;
        }
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(kObjectPath), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        QVariantMap changedProperties{
            {QStringLiteral("InitializationState"), readinessState_},
            {QStringLiteral("EventSequence"), QVariant::fromValue(eventSequence_)}};
        if (apiMajor_ != previousApiMajor) {
            changedProperties.insert(QStringLiteral("ApiMajor"), QVariant::fromValue(apiMajor_));
        }
        signal << QStringLiteral("org.adrenalinlinux.Session1.Service1")
               << changedProperties << QStringList{};
        return connection.send(signal);
    }
    void holdNextRead() { holdNextRead_ = true; }
    bool hasPendingRead() const { return hasPendingRead_; }
    bool releasePendingRead(const QDBusConnection &connection)
    {
        if (!hasPendingRead_) {
            return false;
        }
        hasPendingRead_ = false;
        return connection.send(pendingRead_.createReply(
            QVariantList{QStringLiteral("OK"), instanceUuid_, generation_, pendingEventSequence_,
                         pendingEnabled_, pendingRevision_}));
    }

private:
    ConsentMutationState *state_ = nullptr;
    bool dropFirstWriteReply_ = false;
    bool emitChangeSignal_ = false;
    bool failNextWrite_ = false;
    qulonglong generation_ = 1;
    QString readinessState_ = QStringLiteral("READY");
    ushort apiMajor_ = 1;
    QString instanceUuid_;
    qulonglong eventSequence_ = 0;
    int readCount_ = 0;
    bool pendingEnabled_ = false;
    qulonglong pendingRevision_ = 0;
    qulonglong pendingEventSequence_ = 0;
    bool holdNextRead_ = false;
    bool hasPendingRead_ = false;
    QDBusMessage pendingRead_;
};
}

class SessionContractTest final : public QObject
{
    Q_OBJECT

private slots:
    void servicePropertiesChanged(const QString &interfaceName,
                                  const QVariantMap &changedProperties,
                                  const QStringList &invalidatedProperties);
    void operationResultVocabularyIsStable();
    void mockContractSupportsReadWriteAndOptimisticConcurrency();
    void unsupportedSchemaFailsBeforeReady();
    void schemaUpgradePreservesSettingsOperations();
    void missingNotificationRevisionMetadataFailsBeforeReady();
    void preInitializationGenerationSupportsBusyHardwareReplies();
    void eventSequenceExhaustionFailsClosedBeforeMutation();
    void notificationPersistenceAndMarkReadSurviveRestart();
    void notificationMarkReadFailsClosedWhenEventSequenceExhausted();
    void initializationStopsWhenReadinessSequenceIsExhausted();
    void generatedDbusContractPersistsAndRejectsStaleAndConflictingWrites();
    void toastNotificationsClientTracksSharedEventsAndRestart();
    void clientWaitsForServiceReadinessBeforeSettingsCalls();
    void clientRejectsIncompatibleServiceApiMajor();
    void clientOwnerRecoveryWhileInitialReadIsOutstanding();
    void clientRetriesUncertainMutationWithSameOperationId();
    void clientRecoversAfterTerminalMutationFailure();
    void clientConsumesOwnWriteSignalWithoutReplaying();
    void clientRefreshesAfterRevisionGap();
    void clientRefreshesAfterEventSequenceGap();
    void clientRefreshesAfterForeignOrMismatchedEvents();
    void clientIgnoresDuplicateEventSequence();
    void qmlPreferenceRoundTripsAndSurvivesGuiRestart();
    void qmlToastPreferenceRoundTripsAndSurvivesGuiRestart();

private:
    QVariantMap serviceChangedProperties_;
    QStringList serviceInvalidatedProperties_;
    QString serviceChangedInterface_;
};

void SessionContractTest::servicePropertiesChanged(const QString &interfaceName,
                                                   const QVariantMap &changedProperties,
                                                   const QStringList &invalidatedProperties)
{
    if (interfaceName != QStringLiteral("org.adrenalinlinux.Session1.Service1")) {
        return;
    }
    serviceChangedInterface_ = interfaceName;
    for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
        serviceChangedProperties_.insert(it.key(), it.value());
    }
    serviceInvalidatedProperties_.append(invalidatedProperties);
}

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
    const ServiceReadinessContract &readiness = mock;
    QCOMPARE(readiness.initializationState(), QStringLiteral("READY"));
    QVERIFY(!readiness.serviceInstanceUuid().isEmpty());
    QCOMPARE(readiness.serviceGeneration(), quint64(1));
    QCOMPARE(readiness.apiMajor(), ushort(1));
    QCOMPARE(readiness.apiMinor(), ushort(0));
    QVERIFY(readiness.lastInitializationError().isEmpty());
    QCOMPARE(mock.initializationState(), QStringLiteral("READY"));
    QCOMPARE(mock.apiMajor(), ushort(1));
    const auto initial = mock.getProductTelemetryConsent();
    QCOMPARE(initial.resultCode, QStringLiteral("OK"));
    QVERIFY(!initial.serviceInstanceUuid.isEmpty());
    QCOMPARE(initial.serviceGeneration, quint64(1));
    QCOMPARE(initial.eventSequence, quint64(0));
    QVERIFY(!initial.enabled);
    QCOMPARE(initial.revision, quint64(0));

    const auto write = mock.setProductTelemetryConsent(QStringLiteral("operation-1"), true, 0);
    QCOMPARE(write.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(write.result.revision, quint64(1));
    QCOMPARE(mock.getProductTelemetryConsent().eventSequence, quint64(1));
    QCOMPARE(write.result.operationId, QStringLiteral("operation-1"));
    QCOMPARE(write.result.provider, QStringLiteral("session-settings"));
    QCOMPARE(write.result.subjectId, QStringLiteral("product.telemetry_consent"));
    const auto duplicate = mock.setProductTelemetryConsent(QStringLiteral("operation-1"), true, 0);
    QCOMPARE(duplicate.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(duplicate.result.revision, quint64(1));
    QCOMPARE(mock.getProductTelemetryConsent().eventSequence, quint64(1));
    QCOMPARE(mock.setProductTelemetryConsent(QStringLiteral("operation-1"), false, 0).result.code,
             adrenalin::contracts::OperationResultCode::Conflict);
    QCOMPARE(mock.setProductTelemetryConsent(QStringLiteral("operation-2"), false, 0).result.code,
             adrenalin::contracts::OperationResultCode::StaleRevision);

    const auto toastInitial = mock.getToastNotifications();
    QCOMPARE(toastInitial.resultCode, QStringLiteral("OK"));
    QVERIFY(!toastInitial.serviceInstanceUuid.isEmpty());
    QCOMPARE(toastInitial.serviceGeneration, quint64(1));
    QCOMPARE(toastInitial.eventSequence, quint64(1));
    QVERIFY(!toastInitial.configured);
    QVERIFY(!toastInitial.enabled);
    QCOMPARE(toastInitial.revision, quint64(0));
    const auto toastWrite = mock.setToastNotifications(QStringLiteral("toast-operation-1"), false, 0);
    QCOMPARE(toastWrite.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(toastWrite.result.revision, quint64(1));
    QCOMPARE(mock.getToastNotifications().resultCode, QStringLiteral("OK"));
    QVERIFY(mock.getToastNotifications().configured);
    QVERIFY(!mock.getToastNotifications().enabled);
    QCOMPARE(mock.getToastNotifications().eventSequence, quint64(2));
    const auto toastReplay = mock.setToastNotifications(QStringLiteral("toast-operation-1"), false, 0);
    QCOMPARE(toastReplay.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(toastReplay.result.revision, quint64(1));
    QCOMPARE(mock.getToastNotifications().eventSequence, quint64(2));
    QCOMPARE(mock.setToastNotifications(QStringLiteral("toast-operation-1"), true, 0).result.code,
             adrenalin::contracts::OperationResultCode::Conflict);
    QCOMPARE(mock.setToastNotifications(QStringLiteral("toast-operation-2"), true, 0).result.code,
             adrenalin::contracts::OperationResultCode::StaleRevision);
}

void SessionContractTest::preInitializationGenerationSupportsBusyHardwareReplies()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(
        QStringLiteral("session.sqlite3"));
    SessionService service(databasePath);

    QCOMPARE(service.initializationState(), QStringLiteral("STARTING"));
    QCOMPARE(service.serviceGeneration(), quint64(1));

    adrenalin::contracts::hardware1::Reply reply;
    reply.code = QStringLiteral("BUSY");
    reply.humanMessageKey = QStringLiteral("service.recovering");
    reply.retryable = true;
    reply.subjectKind = QStringLiteral("PLATFORM");
    reply.subjectId = QStringLiteral("platform");
    reply.serviceInstanceUuid = service.serviceInstanceUuid();
    reply.serviceGeneration = service.serviceGeneration();
    QVERIFY(reply.isValid());

    QVERIFY(service.initialize());
    QVERIFY(service.serviceGeneration() >= reply.serviceGeneration);
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

void SessionContractTest::schemaUpgradePreservesSettingsOperations()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("schema-v1.sqlite3"));
    const QString connectionName = QStringLiteral("session-schema-v1-fixture");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        {
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE service_metadata (key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)")));
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE preferences (key TEXT PRIMARY KEY NOT NULL, value INTEGER NOT NULL, revision INTEGER NOT NULL CHECK(revision >= 0))")));
            QVERIFY(query.exec(QStringLiteral("CREATE TABLE settings_operations (method TEXT NOT NULL, operation_id TEXT NOT NULL, enabled INTEGER NOT NULL, expected_revision INTEGER NOT NULL, result_revision INTEGER NOT NULL, PRIMARY KEY(method, operation_id))")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO service_metadata VALUES ('schema_version', '1')")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO service_metadata VALUES ('service_generation', '7')")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO preferences VALUES ('product_telemetry_consent', 1, 7)")));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO settings_operations VALUES ('SetProductTelemetryConsent', 'old-operation', 1, 6, 7)")));
        }
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
    }
    SessionService service(databasePath);
    QVERIFY(service.initialize());
    const auto consent = service.getProductTelemetryConsent();
    QCOMPARE(consent.resultCode, QStringLiteral("OK"));
    QVERIFY(consent.enabled);
    QCOMPARE(consent.revision, quint64(7));
    const auto replay = service.setProductTelemetryConsent(QStringLiteral("old-operation"), true, 6);
    QCOMPARE(replay.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QCOMPARE(replay.result.revision, quint64(7));
    const auto notifications = service.listNotifications();
    QCOMPARE(notifications.code, QStringLiteral("OK"));
    QVERIFY(notifications.notifications.isEmpty());
    QCOMPARE(notifications.revision, quint64(1));
    QSqlDatabase verify = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("session-schema-v2-verify"));
    verify.setDatabaseName(databasePath);
    QVERIFY(verify.open());
    QSqlQuery schema(verify);
    QVERIFY(schema.exec(QStringLiteral("SELECT value FROM service_metadata WHERE key='schema_version'")));
    QVERIFY(schema.next());
    QCOMPARE(schema.value(0).toString(), QStringLiteral("3"));
    QSqlQuery profileTable(verify);
    profileTable.prepare(QStringLiteral("SELECT COUNT(*) FROM sqlite_master "
                                         "WHERE type='table' AND name='profiles'"));
    QVERIFY(profileTable.exec());
    QVERIFY(profileTable.next());
    QCOMPARE(profileTable.value(0).toInt(), 1);
    verify.close();
    verify = {};
    QSqlDatabase::removeDatabase(QStringLiteral("session-schema-v2-verify"));
}

void SessionContractTest::missingNotificationRevisionMetadataFailsBeforeReady()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    {
        SessionService service(databasePath);
        QVERIFY(service.initialize());
    }
    const QString connectionName = QStringLiteral("session-notification-metadata-corruption");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        {
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral("DELETE FROM notification_metadata WHERE key='revision'")));
            QCOMPARE(query.numRowsAffected(), 1);
        }
        database.close();
        database = {};
        QSqlDatabase::removeDatabase(connectionName);
    }
    SessionService damaged(databasePath);
    QVERIFY(!damaged.initialize());
    QCOMPARE(damaged.initializationState(), QStringLiteral("FAILED"));
    QVERIFY(damaged.lastInitializationError().contains(QStringLiteral("Notification revision metadata")));
}

void SessionContractTest::eventSequenceExhaustionFailsClosedBeforeMutation()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    {
        SessionService service(databasePath);
        QVERIFY(service.initialize());
        QSignalSpy exhausted(&service, &SessionService::eventSequenceExhausted);
        QSignalSpy published(&service, &SessionService::eventPublished);
        QSignalSpy consentPublished(&service, &SessionService::ProductTelemetryConsentChanged);
        QVERIFY(exhausted.isValid());
        QVERIFY(published.isValid());
        QVERIFY(consentPublished.isValid());
        QSignalSpy notificationsPublished(&service, &SessionService::NotificationsChanged);
        QVERIFY(notificationsPublished.isValid());
        service.eventSequence_ = std::numeric_limits<quint64>::max() - 2;

        const auto finalValidWrite = service.setProductTelemetryConsent(
            QStringLiteral("last-sequence-write"), true, 0);
        QCOMPARE(finalValidWrite.result.code, adrenalin::contracts::OperationResultCode::Ok);
        QCOMPARE(service.eventSequence(), std::numeric_limits<quint64>::max());
        QCOMPARE(exhausted.count(), 0);
        QCOMPARE(published.count(), 2);
        QCOMPARE(consentPublished.count(), 1);
        QCOMPARE(notificationsPublished.count(), 1);

        const auto overflowWrite = service.setProductTelemetryConsent(
            QStringLiteral("overflow-write"), false, 1);
        QCOMPARE(overflowWrite.result.code, adrenalin::contracts::OperationResultCode::InternalError);
        QCOMPARE(overflowWrite.result.humanMessageKey,
                 QStringLiteral("service.event_sequence_exhausted"));
        QVERIFY(!overflowWrite.result.retryable);
        QCOMPARE(service.initializationState(), QStringLiteral("FAILED"));
        QCOMPARE(service.eventSequence(), std::numeric_limits<quint64>::max());
        QCOMPARE(exhausted.count(), 1);
        QCOMPARE(published.count(), 2);
        QCOMPARE(consentPublished.count(), 1);
        QCOMPARE(notificationsPublished.count(), 1);
    }

    SessionService recovered(databasePath);
    QVERIFY(recovered.initialize());
    const auto persisted = recovered.getProductTelemetryConsent();
    QCOMPARE(persisted.resultCode, QStringLiteral("OK"));
    QVERIFY(persisted.enabled);
    QCOMPARE(persisted.revision, quint64(1));

    recovered.eventSequence_ = std::numeric_limits<quint64>::max();
    QSignalSpy readinessExhausted(&recovered, &SessionService::eventSequenceExhausted);
    QSignalSpy readinessPublished(&recovered, &SessionService::eventPublished);
    QVERIFY(readinessExhausted.isValid());
    QVERIFY(readinessPublished.isValid());
    recovered.setState(SessionService::State::Degraded);
    QCOMPARE(recovered.initializationState(), QStringLiteral("FAILED"));
    QCOMPARE(recovered.eventSequence(), std::numeric_limits<quint64>::max());
    QCOMPARE(readinessExhausted.count(), 1);
    QCOMPARE(readinessPublished.count(), 0);
}

void SessionContractTest::initializationStopsWhenReadinessSequenceIsExhausted()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    SessionService service(databasePath);
    service.eventSequence_ = std::numeric_limits<quint64>::max();
    QSignalSpy exhausted(&service, &SessionService::eventSequenceExhausted);
    QVERIFY(exhausted.isValid());

    QVERIFY(!service.initialize());
    QCOMPARE(service.initializationState(), QStringLiteral("FAILED"));
    QCOMPARE(service.lastInitializationError(), QStringLiteral("Per-service event sequence exhausted"));
    QCOMPARE(service.eventSequence(), std::numeric_limits<quint64>::max());
    QCOMPARE(exhausted.count(), 1);
    QVERIFY(!QFileInfo::exists(databasePath));
}

void SessionContractTest::notificationPersistenceAndMarkReadSurviveRestart()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QString notificationId;
    quint64 notificationRevision = 0;
    quint64 postReadRevision = 0;
    {
        SessionService service(databasePath);
        QVERIFY(service.initialize());
        const auto write = service.setProductTelemetryConsent(QStringLiteral("notification-source-1"), true, 0);
        QCOMPARE(write.result.code, adrenalin::contracts::OperationResultCode::Ok);
        const auto snapshot = service.listNotifications();
        QCOMPARE(snapshot.code, QStringLiteral("OK"));
        QCOMPARE(snapshot.notifications.size(), 1);
        QVERIFY(snapshot.isValid());
        notificationId = snapshot.notifications.constFirst().notificationId;
        notificationRevision = snapshot.revision;
        QVERIFY(!snapshot.notifications.constFirst().isRead);
        QCOMPARE(snapshot.notifications.constFirst().category, QStringLiteral("SETTING_APPLIED"));

        QSignalSpy notificationsPublished(&service, &SessionService::NotificationsChanged);
        QVERIFY(notificationsPublished.isValid());
        const auto marked = service.markRead(notificationId, QStringLiteral("mark-read-1"), notificationRevision);
        QCOMPARE(marked.mutation.code, adrenalin::contracts::OperationResultCode::Ok);
        QVERIFY(marked.changed);
        postReadRevision = marked.mutation.revision;
        QCOMPARE(postReadRevision, notificationRevision + 1);
        QCOMPARE(notificationsPublished.count(), 1);
        const auto replay = service.markRead(notificationId, QStringLiteral("mark-read-1"), notificationRevision);
        QCOMPARE(replay.mutation.code, adrenalin::contracts::OperationResultCode::Ok);
        QVERIFY(!replay.changed);
        QCOMPARE(replay.mutation.revision, postReadRevision);
        QCOMPARE(notificationsPublished.count(), 1);
        const auto conflict = service.markRead(notificationId, QStringLiteral("mark-read-1"), postReadRevision);
        QCOMPARE(conflict.mutation.code, adrenalin::contracts::OperationResultCode::Conflict);
        QCOMPARE(conflict.mutation.revision, postReadRevision);
        const auto missing = service.markRead(QStringLiteral("missing-notification"),
                                              QStringLiteral("mark-read-missing"), postReadRevision);
        QCOMPARE(missing.mutation.code, adrenalin::contracts::OperationResultCode::NotFound);
        QCOMPARE(missing.mutation.revision, postReadRevision);
        const auto stale = service.markRead(notificationId, QStringLiteral("mark-read-stale"),
                                            notificationRevision);
        QCOMPARE(stale.mutation.code, adrenalin::contracts::OperationResultCode::StaleRevision);
        QCOMPARE(stale.mutation.revision, postReadRevision);
    }
    {
        SessionService service(databasePath);
        QVERIFY(service.initialize());
        const auto snapshot = service.listNotifications();
        QCOMPARE(snapshot.code, QStringLiteral("OK"));
        QCOMPARE(snapshot.revision, postReadRevision);
        QCOMPARE(snapshot.notifications.size(), 1);
        QCOMPARE(snapshot.notifications.constFirst().notificationId, notificationId);
        QVERIFY(snapshot.notifications.constFirst().isRead);
        const auto replay = service.markRead(notificationId, QStringLiteral("mark-read-1"), notificationRevision);
        QCOMPARE(replay.mutation.code, adrenalin::contracts::OperationResultCode::Ok);
        QVERIFY(!replay.changed);
        QCOMPARE(replay.mutation.revision, postReadRevision);
    }
}

void SessionContractTest::notificationMarkReadFailsClosedWhenEventSequenceExhausted()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QString notificationId;
    quint64 notificationRevision = 0;
    {
        SessionService service(databasePath);
        QVERIFY(service.initialize());
        const auto write = service.setProductTelemetryConsent(QStringLiteral("notification-source-2"), true, 0);
        QCOMPARE(write.result.code, adrenalin::contracts::OperationResultCode::Ok);
        const auto snapshot = service.listNotifications();
        QCOMPARE(snapshot.notifications.size(), 1);
        notificationId = snapshot.notifications.constFirst().notificationId;
        notificationRevision = snapshot.revision;
        service.eventSequence_ = std::numeric_limits<quint64>::max();
        QSignalSpy exhausted(&service, &SessionService::eventSequenceExhausted);
        QVERIFY(exhausted.isValid());
        const auto marked = service.markRead(notificationId, QStringLiteral("mark-read-at-exhaustion"),
                                             notificationRevision);
        QCOMPARE(marked.mutation.code, adrenalin::contracts::OperationResultCode::InternalError);
        QCOMPARE(marked.mutation.humanMessageKey, QStringLiteral("service.event_sequence_exhausted"));
        QCOMPARE(service.initializationState(), QStringLiteral("FAILED"));
        QCOMPARE(exhausted.count(), 1);
    }
    SessionService recovered(databasePath);
    QVERIFY(recovered.initialize());
    const auto snapshot = recovered.listNotifications();
    QCOMPARE(snapshot.code, QStringLiteral("OK"));
    QCOMPARE(snapshot.revision, notificationRevision);
    QCOMPARE(snapshot.notifications.size(), 1);
    QCOMPARE(snapshot.notifications.constFirst().notificationId, notificationId);
    QVERIFY(!snapshot.notifications.constFirst().isRead);
}

void SessionContractTest::generatedDbusContractPersistsAndRejectsStaleAndConflictingWrites()
{
    serviceChangedProperties_.clear();
    serviceInvalidatedProperties_.clear();
    serviceChangedInterface_.clear();
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    auto startService = [&](bool initializeNow = true) {
        auto *service = new SessionService(databasePath, this);
        auto *adaptor = new Settings1Adaptor(service);
        auto *notificationsAdaptor = new Notifications1Adaptor(service);
        auto *readinessAdaptor = new SessionServiceRootAdaptor(service);
        installService1PropertyNotifications(service);
        Q_UNUSED(adaptor);
        Q_UNUSED(notificationsAdaptor);
        Q_UNUSED(readinessAdaptor);
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
    OrgAdrenalinlinuxSession1Service1Interface serviceProxy(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), bus);
    QSignalSpy serviceEvents(
        &serviceProxy, &OrgAdrenalinlinuxSession1Service1Interface::EventPublished);
    QVERIFY(serviceEvents.isValid());
    QVERIFY(proxy.isValid());
    QVERIFY(serviceProxy.isValid());
    QVERIFY(bus.connect(QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
                        QStringLiteral("org.freedesktop.DBus.Properties"),
                        QStringLiteral("PropertiesChanged"), this,
                        SLOT(servicePropertiesChanged(QString,QVariantMap,QStringList))));
    QCOMPARE(serviceProxy.apiMajor(), ushort(1));
    QCOMPARE(serviceProxy.apiMinor(), ushort(0));
    QCOMPARE(serviceProxy.initializationState(), QStringLiteral("STARTING"));
    QCOMPARE(serviceProxy.lastInitializationError(), QString());
    auto notReadyPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(notReadyPending.isFinished(), 2000);
    const QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, qulonglong> notReady =
        notReadyPending;
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
    const qulonglong startupEventSequence = service->eventSequence();
    QCOMPARE(service->initializationState(), QStringLiteral("READY"));

    QCOMPARE(serviceProxy.initializationState(), QStringLiteral("READY"));
    QCOMPARE(serviceProxy.lastInitializationError(), QString());
    QCOMPARE(serviceProxy.serviceInstanceUuid(), firstUuid);
    QCOMPARE(serviceProxy.serviceGeneration(), firstGeneration);
    QTRY_COMPARE_WITH_TIMEOUT(
        serviceChangedProperties_.value(QStringLiteral("InitializationState")).toString(),
        QStringLiteral("READY"), 2000);
    QCOMPARE(serviceChangedInterface_, QStringLiteral("org.adrenalinlinux.Session1.Service1"));
    if (firstGeneration == 1) {
        // The STARTING envelope already has generation 1, so PropertiesChanged
        // must not announce that unchanged value again.
        QVERIFY(!serviceChangedProperties_.contains(QStringLiteral("ServiceGeneration")));
    } else {
        QCOMPARE(serviceChangedProperties_.value(QStringLiteral("ServiceGeneration")).toULongLong(),
                 firstGeneration);
    }
    QCOMPARE(serviceChangedProperties_.value(QStringLiteral("EventSequence")).toULongLong(),
             service->eventSequence());
    QVERIFY(!serviceChangedProperties_.contains(QStringLiteral("ServiceInstanceUuid")));
    QVERIFY(!serviceChangedProperties_.contains(QStringLiteral("EventSubjectId")));
    const int expectedServiceEvents = static_cast<int>(startupEventSequence);
    QTRY_COMPARE_WITH_TIMEOUT(serviceEvents.count(), expectedServiceEvents, 2000);
    QCOMPARE(serviceEvents.at(1).at(0).toString(), firstUuid);
    QCOMPARE(serviceEvents.at(1).at(1).toULongLong(), firstGeneration);
    QCOMPARE(serviceEvents.at(1).at(2).toULongLong(), qulonglong(2));
    QCOMPARE(serviceEvents.at(1).at(3).toString(), QStringLiteral("SERVICE"));
    QCOMPARE(serviceEvents.at(1).at(4).toString(), QStringLiteral("service.readiness"));
    for (int index = 0; index < serviceEvents.count(); ++index) {
        QCOMPARE(serviceEvents.at(index).at(2).toULongLong(), qulonglong(index + 1));
        QCOMPARE(serviceEvents.at(index).at(0).toString(), firstUuid);
        QCOMPARE(serviceEvents.at(index).at(1).toULongLong(), firstGeneration);
    }
    QVERIFY(serviceChangedProperties_.contains(QStringLiteral("InitializationState")));

    auto readPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(readPending.isFinished(), 2000);
    const QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, qulonglong> initial =
        readPending;
    QVERIFY(!initial.isError());
    QCOMPARE(initial.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(initial.argumentAt<1>(), firstUuid);
    QCOMPARE(initial.argumentAt<2>(), firstGeneration);
    QCOMPARE(initial.argumentAt<3>(), startupEventSequence);
    QVERIFY(!initial.argumentAt<4>());
    QCOMPARE(initial.argumentAt<5>(), qulonglong(0));

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
    QTRY_COMPARE_WITH_TIMEOUT(serviceEvents.count(), startupEventSequence + 2, 2000);
    QCOMPARE(serviceEvents.last().at(0).toString(), firstUuid);
    QCOMPARE(serviceEvents.last().at(1).toULongLong(), firstGeneration);
    QCOMPARE(serviceEvents.at(serviceEvents.count() - 2).at(2).toULongLong(), startupEventSequence + 1);
    QCOMPARE(serviceEvents.at(serviceEvents.count() - 2).at(3).toString(), QStringLiteral("PREFERENCE"));
    QCOMPARE(serviceEvents.at(serviceEvents.count() - 2).at(4).toString(), QStringLiteral("product.telemetry_consent"));
    QCOMPARE(serviceEvents.last().at(2).toULongLong(), startupEventSequence + 2);
    QCOMPARE(serviceEvents.last().at(3).toString(), QStringLiteral("NOTIFICATION"));
    QCOMPARE(serviceEvents.last().at(4).toString(), QStringLiteral("platform"));
    QCOMPARE(changed.at(0).at(0).toString(), firstUuid);
    QCOMPARE(changed.at(0).at(1).toULongLong(), firstGeneration);
    QCOMPARE(changed.at(0).at(2).toULongLong(), startupEventSequence + 1);
    QCOMPARE(changed.at(0).at(3).toString(), QStringLiteral("PREFERENCE"));
    QCOMPARE(changed.at(0).at(4).toString(), QStringLiteral("product.telemetry_consent"));
    QVERIFY(changed.at(0).at(5).toBool());
    QCOMPARE(changed.at(0).at(6).toULongLong(), qulonglong(1));

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
    QTRY_COMPARE_WITH_TIMEOUT(serviceEvents.count(), startupEventSequence + 4, 2000);
    QCOMPARE(serviceEvents.at(serviceEvents.count() - 2).at(2).toULongLong(), startupEventSequence + 3);
    QCOMPARE(serviceEvents.last().at(2).toULongLong(), startupEventSequence + 4);
    QCOMPARE(serviceEvents.last().at(3).toString(), QStringLiteral("NOTIFICATION"));
    QCOMPARE(changed.at(1).at(0).toString(), firstUuid);
    QCOMPARE(changed.at(1).at(1).toULongLong(), firstGeneration);
    QCOMPARE(changed.at(1).at(2).toULongLong(), startupEventSequence + 3);
    QCOMPARE(changed.at(1).at(3).toString(), QStringLiteral("PREFERENCE"));
    QCOMPARE(changed.at(1).at(4).toString(), QStringLiteral("product.telemetry_consent"));
    QCOMPARE(changed.at(1).at(5).toBool(), false);
    QCOMPARE(changed.at(1).at(6).toULongLong(), qulonglong(2));

    adrenalin::contracts::notifications1::registerMetaTypes();
    OrgAdrenalinlinuxSession1Notifications1Interface notificationsProxy(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), bus);
    QVERIFY(notificationsProxy.isValid());
    QSignalSpy notificationChanged(
        &notificationsProxy, &OrgAdrenalinlinuxSession1Notifications1Interface::NotificationsChanged);
    QVERIFY(notificationChanged.isValid());
    auto notificationListPending = notificationsProxy.ListNotifications();
    QTRY_VERIFY_WITH_TIMEOUT(notificationListPending.isFinished(), 2000);
    const NotificationsListReply notificationList = notificationListPending;
    QVERIFY(!notificationList.isError());
    QCOMPARE(notificationList.argumentAt<0>(), QStringLiteral("OK"));
    const QList<Notification> currentNotifications = notificationList.argumentAt<5>();
    QCOMPARE(currentNotifications.size(), 2);
    const QString markedNotificationId = currentNotifications.constLast().notificationId;
    const qulonglong beforeMarkRevision = notificationList.argumentAt<4>();
    auto markReadPending = notificationsProxy.MarkRead(markedNotificationId,
                                                       QStringLiteral("dbus-mark-read-1"),
                                                       beforeMarkRevision);
    QTRY_VERIFY_WITH_TIMEOUT(markReadPending.isFinished(), 2000);
    const NotificationsMarkReadReply markRead = markReadPending;
    QVERIFY(!markRead.isError());
    QCOMPARE(markRead.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(markRead.argumentAt<1>(), QStringLiteral("dbus-mark-read-1"));
    QCOMPARE(markRead.argumentAt<7>(), beforeMarkRevision + 1);
    QVERIFY(markRead.argumentAt<8>());
    QCOMPARE(markRead.argumentAt<9>(), firstUuid);
    QCOMPARE(markRead.argumentAt<10>(), firstGeneration);
    QCOMPARE(markRead.argumentAt<11>(), startupEventSequence + 5);
    QTRY_COMPARE_WITH_TIMEOUT(notificationChanged.count(), 1, 2000);
    QCOMPARE(notificationChanged.constFirst().at(3).toString(), QStringLiteral("NOTIFICATION"));
    QCOMPARE(notificationChanged.constFirst().at(4).toString(), QStringLiteral("platform"));

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
    const QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, qulonglong> current =
        currentPending;
    QCOMPARE(current.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(!current.argumentAt<4>());
    QCOMPARE(current.argumentAt<3>(), startupEventSequence + 5);
    QCOMPARE(current.argumentAt<5>(), qulonglong(2));

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

    auto notificationsAfterRestartPending = notificationsProxy.ListNotifications();
    QTRY_VERIFY_WITH_TIMEOUT(notificationsAfterRestartPending.isFinished(), 2000);
    const NotificationsListReply notificationsAfterRestart = notificationsAfterRestartPending;
    QCOMPARE(notificationsAfterRestart.argumentAt<0>(), QStringLiteral("OK"));
    const QList<Notification> persistedNotifications = notificationsAfterRestart.argumentAt<5>();
    QCOMPARE(persistedNotifications.size(), 2);
    const auto markedPersisted = std::find_if(persistedNotifications.cbegin(), persistedNotifications.cend(),
        [&](const Notification &item) { return item.notificationId == markedNotificationId; });
    QVERIFY(markedPersisted != persistedNotifications.cend());
    QVERIFY(markedPersisted->isRead);
    auto markReplayPending = notificationsProxy.MarkRead(markedNotificationId,
                                                         QStringLiteral("dbus-mark-read-1"),
                                                         beforeMarkRevision);
    QTRY_VERIFY_WITH_TIMEOUT(markReplayPending.isFinished(), 2000);
    const NotificationsMarkReadReply markReplay = markReplayPending;
    QCOMPARE(markReplay.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(markReplay.argumentAt<7>(), beforeMarkRevision + 1);
    QVERIFY(!markReplay.argumentAt<8>());
    QCOMPARE(markReplay.argumentAt<9>(), service->serviceInstanceUuid());
    QCOMPARE(markReplay.argumentAt<10>(), service->serviceGeneration());
    QCOMPARE(markReplay.argumentAt<11>(), service->eventSequence());
    QTest::qWait(50);
    QCOMPARE(notificationChanged.count(), 1);

    auto afterRestartPending = proxy.GetProductTelemetryConsent();
    QTRY_VERIFY_WITH_TIMEOUT(afterRestartPending.isFinished(), 2000);
    const QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, qulonglong> afterRestart =
        afterRestartPending;
    QVERIFY(!afterRestart.isError());
    QCOMPARE(afterRestart.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(!afterRestart.argumentAt<4>());
    QCOMPARE(afterRestart.argumentAt<3>(), service->eventSequence());
    QCOMPARE(afterRestart.argumentAt<5>(), qulonglong(2));

    QSignalSpy toastChanged(&proxy, &OrgAdrenalinlinuxSession1Settings1Interface::ToastNotificationsChanged);
    QVERIFY(toastChanged.isValid());
    auto toastInitialPending = proxy.GetToastNotifications();
    QTRY_VERIFY_WITH_TIMEOUT(toastInitialPending.isFinished(), 2000);
    const QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, bool,
                            qulonglong> toastInitial = toastInitialPending;
    QVERIFY(!toastInitial.isError());
    QCOMPARE(toastInitial.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(toastInitial.argumentAt<1>(), service->serviceInstanceUuid());
    QCOMPARE(toastInitial.argumentAt<2>(), service->serviceGeneration());
    QCOMPARE(toastInitial.argumentAt<3>(), service->eventSequence());
    QVERIFY(!toastInitial.argumentAt<4>());
    QVERIFY(!toastInitial.argumentAt<5>());
    QCOMPARE(toastInitial.argumentAt<6>(), qulonglong(0));
    const qulonglong toastEventBefore = toastInitial.argumentAt<3>();
    const auto toastWritePending = proxy.SetToastNotifications(QStringLiteral("toast-write-1"), false, 0);
    QTRY_VERIFY_WITH_TIMEOUT(toastWritePending.isFinished(), 2000);
    const QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString, qulonglong> toastWrite =
        toastWritePending;
    QVERIFY(!toastWrite.isError());
    QCOMPARE(toastWrite.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(toastWrite.argumentAt<1>(), QStringLiteral("toast-write-1"));
    QCOMPARE(toastWrite.argumentAt<5>(), QStringLiteral("session-settings"));
    QCOMPARE(toastWrite.argumentAt<6>(), QStringLiteral("toast.notifications"));
    QCOMPARE(toastWrite.argumentAt<7>(), qulonglong(1));
    QTRY_COMPARE_WITH_TIMEOUT(toastChanged.count(), 1, 2000);
    QCOMPARE(toastChanged.constFirst().at(2).toULongLong(), toastEventBefore + 1);
    QCOMPARE(toastChanged.constFirst().at(3).toString(), QStringLiteral("PREFERENCE"));
    QCOMPARE(toastChanged.constFirst().at(4).toString(), QStringLiteral("toast.notifications"));
    QVERIFY(!toastChanged.constFirst().at(5).toBool());
    QCOMPARE(toastChanged.constFirst().at(6).toULongLong(), qulonglong(1));
    auto toastReplayPending = proxy.SetToastNotifications(QStringLiteral("toast-write-1"), false, 0);
    QTRY_VERIFY_WITH_TIMEOUT(toastReplayPending.isFinished(), 2000);
    QCOMPARE(toastReplayPending.argumentAt<0>(), QStringLiteral("OK"));
    QCOMPARE(toastReplayPending.argumentAt<7>(), qulonglong(1));
    auto toastConflictPending = proxy.SetToastNotifications(QStringLiteral("toast-write-1"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(toastConflictPending.isFinished(), 2000);
    QCOMPARE(toastConflictPending.argumentAt<0>(), QStringLiteral("CONFLICT"));
    auto toastStalePending = proxy.SetToastNotifications(QStringLiteral("toast-write-2"), true, 0);
    QTRY_VERIFY_WITH_TIMEOUT(toastStalePending.isFinished(), 2000);
    QCOMPARE(toastStalePending.argumentAt<0>(), QStringLiteral("STALE_REVISION"));
    QCOMPARE(toastStalePending.argumentAt<7>(), qulonglong(1));
    const auto toastStoredPending = proxy.GetToastNotifications();
    QTRY_VERIFY_WITH_TIMEOUT(toastStoredPending.isFinished(), 2000);
    QCOMPARE(toastStoredPending.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(toastStoredPending.argumentAt<4>());
    QVERIFY(!toastStoredPending.argumentAt<5>());
    QCOMPARE(toastStoredPending.argumentAt<6>(), qulonglong(1));
    stopService(service);
    service = startService();
    QVERIFY(service != nullptr);
    auto toastAfterRestartPending = proxy.GetToastNotifications();
    QTRY_VERIFY_WITH_TIMEOUT(toastAfterRestartPending.isFinished(), 2000);
    QCOMPARE(toastAfterRestartPending.argumentAt<0>(), QStringLiteral("OK"));
    QVERIFY(toastAfterRestartPending.argumentAt<4>());
    QVERIFY(!toastAfterRestartPending.argumentAt<5>());
    QCOMPARE(toastAfterRestartPending.argumentAt<6>(), qulonglong(1));
    stopService(service);

}

void SessionContractTest::toastNotificationsClientTracksSharedEventsAndRestart()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    auto startService = [&]() -> SessionService * {
        auto *service = new SessionService(databasePath, this);
        auto *settings = new Settings1Adaptor(service);
        auto *notifications = new Notifications1Adaptor(service);
        auto *readiness = new SessionServiceRootAdaptor(service);
        Q_UNUSED(settings);
        Q_UNUSED(notifications);
        Q_UNUSED(readiness);
        installService1PropertyNotifications(service);
        if (!bus.registerObject(QString::fromLatin1(kObjectPath), service,
                                QDBusConnection::ExportAdaptors)
            || !bus.registerService(QString::fromLatin1(kServiceName))
            || !service->initialize()) {
            bus.unregisterService(QString::fromLatin1(kServiceName));
            bus.unregisterObject(QString::fromLatin1(kObjectPath));
            delete service;
            return nullptr;
        }
        return service;
    };
    auto stopService = [&](SessionService *service) {
        if (service == nullptr) return;
        bus.unregisterService(QString::fromLatin1(kServiceName));
        bus.unregisterObject(QString::fromLatin1(kObjectPath));
        delete service;
    };

    SessionService *service = startService();
    QVERIFY(service != nullptr);
    ToastNotificationsClient client(bus);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready(), 2000);
    QVERIFY(!client.configured());
    QCOMPARE(client.status(), QStringLiteral("UNCONFIGURED"));
    QCOMPARE(client.revision(), qulonglong(0));

    client.setEnabled(false);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready() && client.configured(), 2000);
    QVERIFY(!client.enabled());
    QCOMPARE(client.revision(), qulonglong(1));
    QCOMPARE(client.status(), QStringLiteral("READY"));

    const qulonglong beforeForeignSettingsEvent = client.eventSequence();
    const auto consentWrite = service->setProductTelemetryConsent(
        QStringLiteral("interleaved-consent"), true, 0);
    QCOMPARE(consentWrite.result.code, adrenalin::contracts::OperationResultCode::Ok);
    QTRY_COMPARE_WITH_TIMEOUT(client.eventSequence(), beforeForeignSettingsEvent + 2, 2000);
    client.setEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready() && client.enabled(), 2000);
    QCOMPARE(client.revision(), qulonglong(2));

    const QString firstInstance = client.serviceInstanceUuid();
    const qulonglong firstGeneration = client.serviceGeneration();
    stopService(service);
    service = startService();
    QVERIFY(service != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready()
                             && client.serviceGeneration() == firstGeneration + 1
                             && client.serviceInstanceUuid() != firstInstance, 2000);
    QVERIFY(client.configured());
    QVERIFY(client.enabled());
    QCOMPARE(client.revision(), qulonglong(2));

    const qulonglong eventGapGeneration = client.serviceGeneration();
    QDBusMessage gap = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QStringLiteral("org.adrenalinlinux.Session1.Service1"),
        QStringLiteral("EventPublished"));
    gap << client.serviceInstanceUuid() << eventGapGeneration << client.eventSequence() + 2
        << QStringLiteral("PREFERENCE") << QStringLiteral("test.unobserved");
    QVERIFY(bus.send(gap));
    QTRY_VERIFY_WITH_TIMEOUT(!client.ready(), 2000);
    QVERIFY(client.status() == QStringLiteral("EVENT_GAP")
            || client.status() == QStringLiteral("RECONCILING"));

    stopService(service);
    service = startService();
    QVERIFY(service != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(client.ready()
                             && client.serviceGeneration() == eventGapGeneration + 1, 2000);
    QVERIFY(client.configured());
    QVERIFY(client.enabled());
    QCOMPARE(client.revision(), qulonglong(2));
    stopService(service);
}

void SessionContractTest::clientWaitsForServiceReadinessBeforeSettingsCalls()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ConsentMutationState mutationState;
    auto service = std::make_unique<RetrySettingsFixture>(
        &mutationState, false, this, false, false, 1, QStringLiteral("RECOVERING"));
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-not-ready-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_COMPARE_WITH_TIMEOUT(client->status(), QStringLiteral("RECOVERING"), 2000);
    QVERIFY(!client->ready());
    QCOMPARE(service->readCount(), 0);
    client->setProductTelemetryConsent(true);
    QCOMPARE(mutationState.operationIds.size(), 0);

    QVERIFY(service->publishReadiness(bus, QStringLiteral("READY")));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QCOMPARE(service->readCount(), 1);
    client->setProductTelemetryConsent(true);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready() && client->productTelemetryConsent(), 2000);
    QCOMPARE(mutationState.operationIds.size(), 1);

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientRejectsIncompatibleServiceApiMajor()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    ConsentMutationState mutationState;
    auto service = std::make_unique<RetrySettingsFixture>(
        &mutationState, false, this, false, false, 1, QStringLiteral("READY"), 2);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-incompatible-api-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_COMPARE_WITH_TIMEOUT(client->status(), QStringLiteral("INCOMPATIBLE_API_MAJOR"), 2000);
    QVERIFY(!client->ready());
    QCOMPARE(service->readCount(), 0);
    client->setProductTelemetryConsent(true);
    QCOMPARE(mutationState.operationIds.size(), 0);

    QVERIFY(service->publishReadiness(bus, QStringLiteral("READY"), 1));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QCOMPARE(service->readCount(), 1);
    client->setProductTelemetryConsent(true);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready() && client->productTelemetryConsent(), 2000);
    QCOMPARE(mutationState.operationIds.size(), 1);

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
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
    auto replacement = std::make_unique<RetrySettingsFixture>(&mutationState, false, this,
                                                              false, false, 2,
                                                              QStringLiteral("RECOVERING"));
    replacement->holdNextRead();
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), replacement.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    QTRY_COMPARE_WITH_TIMEOUT(client->status(), QStringLiteral("RECOVERING"), 2000);
    QVERIFY(!client->ready());
    QCOMPARE(replacement->readCount(), 0);
    QCOMPARE(mutationState.operationIds.size(), 1);
    QVERIFY(replacement->publishReadiness(bus, QStringLiteral("READY")));
    QTRY_VERIFY_WITH_TIMEOUT(replacement->hasPendingRead(), 2000);
    QCOMPARE(replacement->readCount(), 1);
    QCOMPARE(mutationState.operationIds.size(), 1);
    QVERIFY(replacement->releasePendingRead(bus));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 3000);
    QCOMPARE(client->status(), QStringLiteral("READY"));
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(1));
    QCOMPARE(mutationState.revision, qulonglong(1));
    QCOMPARE(mutationState.operationIds.size(), 2);
    QCOMPARE(mutationState.operationIds.at(0), mutationState.operationIds.at(1));
    QCOMPARE(mutationState.operationValues, QList<bool>({true, true}));
    QCOMPARE(mutationState.expectedRevisions, QList<qulonglong>({0, 0}));
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

void SessionContractTest::clientRefreshesAfterEventSequenceGap()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    auto service = std::make_unique<DelayedSettingsFixture>(false, false, 0, 1, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-event-sequence-gap-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QCOMPARE(client->revision(), qulonglong(0));
    QVERIFY(!client->productTelemetryConsent());

    service->setDelayReads(true);
    service->skipEventSequence(1);
    QVERIFY(service->updateAndEmitConsentChanged(bus, true, 1));
    QTRY_COMPARE_WITH_TIMEOUT(service->readCount(), 2, 2000);
    QVERIFY(service->hasPendingRead());
    QVERIFY(!client->ready());
    QCOMPARE(client->revision(), qulonglong(0));
    QVERIFY(!client->productTelemetryConsent());

    QVERIFY(service->releasePendingRead(bus));
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(1));

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientRefreshesAfterForeignOrMismatchedEvents()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    auto service = std::make_unique<DelayedSettingsFixture>(false, false, 0, 1, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-event-identity-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);

    auto waitUntil = [](const auto &predicate) {
        QElapsedTimer elapsed;
        elapsed.start();
        while (!predicate() && elapsed.elapsed() < 2000) {
            QCoreApplication::processEvents();
            QTest::qWait(1);
        }
        return predicate();
    };
    auto verifyRefresh = [&](bool enabled, qulonglong revision, const QString &eventUuid,
                             qulonglong eventGeneration, const QString &subjectId,
                             const QString &subjectKind = QStringLiteral("PREFERENCE")) {
        service->setDelayReads(true);
        if (!service->updateAndEmitConsentChanged(bus, enabled, revision, eventUuid,
                                                  eventGeneration, subjectId, subjectKind)) {
            return false;
        }
        if (!waitUntil([&] {
                return static_cast<qulonglong>(service->readCount()) == revision + 1;
            })
            || !service->hasPendingRead() || client->ready()) {
            return false;
        }
        if (!service->releasePendingRead(bus)) {
            return false;
        }
        if (!waitUntil([&] { return client->ready(); })) {
            return false;
        }
        return client->productTelemetryConsent() == enabled && client->revision() == revision;
    };

    QVERIFY(verifyRefresh(true, 1, QStringLiteral("different-service-instance"), 1,
                          QStringLiteral("product.telemetry_consent")));
    QVERIFY(verifyRefresh(false, 2, {}, 99, QStringLiteral("product.telemetry_consent")));
    QVERIFY(verifyRefresh(true, 3, {}, 0, QStringLiteral("product.telemetry_consent"),
                          QStringLiteral("SERVICE")));
    QVERIFY(verifyRefresh(false, 4, {}, 0, QStringLiteral("different.subject")));

    client.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
    QDBusConnection::disconnectFromBus(clientConnectionName);
}

void SessionContractTest::clientIgnoresDuplicateEventSequence()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    auto service = std::make_unique<DelayedSettingsFixture>(false, false, 0, 1, this);
    QVERIFY(bus.registerVirtualObject(QString::fromLatin1(kObjectPath), service.get()));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));

    const QString clientConnectionName = QStringLiteral("settings1-client-event-duplicate-test");
    QDBusConnection clientBus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, clientConnectionName);
    QVERIFY(clientBus.isConnected());
    OrgAdrenalinlinuxSession1Settings1Interface eventMonitor(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath), clientBus);
    QSignalSpy observedEvents(
        &eventMonitor, &OrgAdrenalinlinuxSession1Settings1Interface::ProductTelemetryConsentChanged);
    QVERIFY(observedEvents.isValid());
    auto client = std::make_unique<Settings1Client>(clientBus);
    QTRY_VERIFY_WITH_TIMEOUT(client->ready(), 2000);

    QVERIFY(service->updateAndEmitConsentChanged(bus, true, 1));
    QTRY_COMPARE_WITH_TIMEOUT(observedEvents.count(), 1, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(client->productTelemetryConsent()
                                 && client->revision() == qulonglong(1),
                             2000);
    QCOMPARE(service->readCount(), 1);
    QVERIFY(service->emitConsentChanged(bus, false, 0, 1));
    QTRY_COMPARE_WITH_TIMEOUT(observedEvents.count(), 2, 2000);
    QVERIFY(client->ready());
    QVERIFY(client->productTelemetryConsent());
    QCOMPARE(client->revision(), qulonglong(1));
    QCOMPARE(service->readCount(), 1);

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
    SessionServiceRootAdaptor readinessAdaptor(&service);
    installService1PropertyNotifications(&service);
    Q_UNUSED(readinessAdaptor);
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

void SessionContractTest::qmlToastPreferenceRoundTripsAndSurvivesGuiRestart()
{
    QTemporaryDir dataDirectory;
    QVERIFY(dataDirectory.isValid());
    const QString databasePath = QDir(dataDirectory.path()).filePath(QStringLiteral("session.sqlite3"));
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());

    SessionService service(databasePath);
    Settings1Adaptor settingsAdaptor(&service);
    SessionServiceRootAdaptor readinessAdaptor(&service);
    installService1PropertyNotifications(&service);
    Q_UNUSED(settingsAdaptor);
    Q_UNUSED(readinessAdaptor);
    QVERIFY(bus.registerObject(QString::fromLatin1(kObjectPath), &service,
                               QDBusConnection::ExportAdaptors));
    QVERIFY(bus.registerService(QString::fromLatin1(kServiceName)));
    QVERIFY(service.initialize());

    auto launchUi = [](std::unique_ptr<ToastNotificationsClient> &client,
                       std::unique_ptr<QQmlApplicationEngine> &engine) -> QObject * {
        client = std::make_unique<ToastNotificationsClient>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty(
            QStringLiteral("sessionToastNotificationsClient"), client.get());
        engine->loadFromModule(QStringLiteral("Adrenalin.SessionTests"),
                               QStringLiteral("ToastPreferenceHarness"));
        if (engine->rootObjects().isEmpty()) return nullptr;
        return engine->rootObjects().constFirst();
    };

    std::unique_ptr<ToastNotificationsClient> firstClient;
    std::unique_ptr<QQmlApplicationEngine> firstEngine;
    QObject *firstRoot = launchUi(firstClient, firstEngine);
    QVERIFY(firstRoot != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(firstClient->ready(), 2000);
    QVERIFY(!firstClient->configured());
    QObject *firstSwitch = firstRoot->findChild<QObject *>(
        QStringLiteral("toastNotificationsSwitch"));
    QObject *saveOff = firstRoot->findChild<QObject *>(
        QStringLiteral("saveToastNotificationsOffButton"));
    QObject *unconfiguredStatus = firstRoot->findChild<QObject *>(
        QStringLiteral("toastNotificationsUnconfiguredStatus"));
    QVERIFY(firstSwitch != nullptr);
    QVERIFY(saveOff != nullptr);
    QVERIFY(unconfiguredStatus != nullptr);
    QVERIFY(!firstSwitch->property("checked").toBool());
    QVERIFY(firstSwitch->property("enabled").toBool());
    QVERIFY(unconfiguredStatus->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(saveOff, "click"));
    QTRY_VERIFY_WITH_TIMEOUT(firstClient->ready() && firstClient->configured(), 2000);
    QVERIFY(!firstClient->enabled());
    QCOMPARE(firstClient->revision(), qulonglong(1));

    firstEngine.reset();
    firstClient.reset();

    std::unique_ptr<ToastNotificationsClient> restartedClient;
    std::unique_ptr<QQmlApplicationEngine> restartedEngine;
    QObject *restartedRoot = launchUi(restartedClient, restartedEngine);
    QVERIFY(restartedRoot != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(restartedClient->ready(), 2000);
    QVERIFY(restartedClient->configured());
    QVERIFY(!restartedClient->enabled());
    QCOMPARE(restartedClient->revision(), qulonglong(1));
    QObject *restartedSwitch = restartedRoot->findChild<QObject *>(
        QStringLiteral("toastNotificationsSwitch"));
    QVERIFY(restartedSwitch != nullptr);
    QVERIFY(!restartedSwitch->property("checked").toBool());
    QVERIFY(restartedSwitch->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(restartedSwitch, "click"));
    QTRY_VERIFY_WITH_TIMEOUT(restartedClient->ready() && restartedClient->enabled(), 2000);
    QCOMPARE(restartedClient->revision(), qulonglong(2));

    restartedEngine.reset();
    restartedClient.reset();
    QVERIFY(bus.unregisterService(QString::fromLatin1(kServiceName)));
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
}

QTEST_MAIN(SessionContractTest)
#include "session_contract_test.moc"
