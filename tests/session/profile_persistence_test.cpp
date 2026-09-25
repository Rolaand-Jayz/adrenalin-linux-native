#include "sessiond/session_database.h"

#include <QTemporaryFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

using adrenalin::contracts::MutationResult;
using adrenalin::contracts::OperationResultCode;
using adrenalin::contracts::profiles1::Profile;

class ProfilePersistenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void firstMutationPersistsAndReplays();
    void noOpDoesNotNeedEventSequence();
    void exhaustedEventSequenceDoesNotCreateProfile();
    void migratesExistingSchemaV2();
    void validFailureResultsReplayAfterStateChanges();
    void malformedToastPreferenceFailsClosed();
};

void ProfilePersistenceTest::firstMutationPersistsAndReplays()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    QString error;
    {
        SessionDatabase database(databasePath);
        QVERIFY2(database.initialize(&error), qPrintable(error));
        bool notFound = false;
        QVERIFY(!database.readProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), &notFound, &error));
        QVERIFY(notFound);

        QVariantMap patch;
        patch.insert(QStringLiteral("render.scale"), 1.25);
        patch.insert(QStringLiteral("feature.enabled"), true);
        patch.insert(QStringLiteral("label"), QStringLiteral("balanced"));
        patch.insert(QStringLiteral("integer.signed"), -2147483647);
        patch.insert(QStringLiteral("integer.unsigned"), uint(4294967295U));
        patch.insert(QStringLiteral("integer.long"), qlonglong(-9223372036854775807LL));
        patch.insert(QStringLiteral("integer.ulong"), qulonglong(18446744073709551615ULL));
        MutationResult mutation;
        bool changed = false;
        bool stale = false;
        bool conflict = false;
        bool replayed = false;
        QVERIFY2(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                        QStringLiteral("profile-create-1"), patch, true, &mutation,
                                        &changed, &stale, &conflict, &notFound, &replayed, &error),
                 qPrintable(error));
        QCOMPARE(mutation.code, OperationResultCode::Ok);
        QCOMPARE(mutation.revision, quint64(1));
        QVERIFY(changed);
        QVERIFY(!stale && !conflict && !notFound && !replayed);

        const auto stored = database.readProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"),
                                                 &notFound, &error);
        QVERIFY2(stored.has_value(), qPrintable(error));
        QVERIFY(!notFound);
        QCOMPARE(stored->revision, quint64(1));
        QCOMPARE(stored->settings.value(QStringLiteral("render.scale")).metaType().id(), QMetaType::Double);
        QCOMPARE(stored->settings.value(QStringLiteral("render.scale")).toDouble(), 1.25);
        QCOMPARE(stored->settings.value(QStringLiteral("feature.enabled")).metaType().id(), QMetaType::Bool);
        QCOMPARE(stored->settings.value(QStringLiteral("label")).toString(), QStringLiteral("balanced"));
        QCOMPARE(stored->settings.value(QStringLiteral("integer.signed")).metaType().id(), QMetaType::Int);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.signed")).toInt(), -2147483647);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.unsigned")).metaType().id(), QMetaType::UInt);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.unsigned")).toUInt(), 4294967295U);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.long")).metaType().id(), QMetaType::LongLong);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.long")).toLongLong(), -9223372036854775807LL);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.ulong")).metaType().id(), QMetaType::ULongLong);
        QCOMPARE(stored->settings.value(QStringLiteral("integer.ulong")).toULongLong(), 18446744073709551615ULL);
        QVERIFY(stored->presetId.isEmpty());
        QCOMPARE(stored->referenceState, QStringLiteral("REFERENCE_GATED"));

        QVariantMap reorderedPatch;
        reorderedPatch.insert(QStringLiteral("label"), patch.value(QStringLiteral("label")));
        reorderedPatch.insert(QStringLiteral("integer.ulong"), patch.value(QStringLiteral("integer.ulong")));
        reorderedPatch.insert(QStringLiteral("integer.long"), patch.value(QStringLiteral("integer.long")));
        reorderedPatch.insert(QStringLiteral("integer.unsigned"), patch.value(QStringLiteral("integer.unsigned")));
        reorderedPatch.insert(QStringLiteral("integer.signed"), patch.value(QStringLiteral("integer.signed")));
        reorderedPatch.insert(QStringLiteral("feature.enabled"), patch.value(QStringLiteral("feature.enabled")));
        reorderedPatch.insert(QStringLiteral("render.scale"), patch.value(QStringLiteral("render.scale")));
        replayed = false;
        changed = false;
        QVERIFY2(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                        QStringLiteral("profile-create-1"), reorderedPatch, false, &mutation,
                                        &changed, &stale, &conflict, &notFound, &replayed, &error),
                 qPrintable(error));
        QVERIFY(replayed);
        QVERIFY(changed);
        QCOMPARE(mutation.code, OperationResultCode::Ok);
        QCOMPARE(mutation.revision, quint64(1));

        QVariantMap differentPatch = patch;
        differentPatch.insert(QStringLiteral("label"), QStringLiteral("performance"));
        QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                       QStringLiteral("profile-create-1"), differentPatch, false,
                                       &mutation, &changed, &stale, &conflict, &notFound, &replayed, &error));
        QVERIFY(conflict);
        QCOMPARE(mutation.code, OperationResultCode::Conflict);
    }

    SessionDatabase reopened(databasePath);
    QVERIFY2(reopened.initialize(&error), qPrintable(error));
    bool notFound = false;
    const auto persisted = reopened.readProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"),
                                               &notFound, &error);
    QVERIFY2(persisted.has_value(), qPrintable(error));
    QVERIFY(!notFound);
    QCOMPARE(persisted->revision, quint64(1));
}

void ProfilePersistenceTest::noOpDoesNotNeedEventSequence()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    SessionDatabase database(databasePath);
    QString error;
    QVERIFY2(database.initialize(&error), qPrintable(error));
    QVariantMap initial;
    initial.insert(QStringLiteral("mode"), QStringLiteral("quality"));
    MutationResult mutation;
    bool changed = false;
    bool stale = false;
    bool conflict = false;
    bool notFound = false;
    bool replayed = false;
    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("profile-create-2"), initial, true, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.revision, quint64(1));

    QVERIFY2(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 1,
                                    QStringLiteral("profile-noop-1"), initial, false, &mutation,
                                    &changed, &stale, &conflict, &notFound, &replayed, &error),
             qPrintable(error));
    QCOMPARE(mutation.code, OperationResultCode::Ok);
    QCOMPARE(mutation.revision, quint64(1));
    QVERIFY(!changed && !stale && !conflict && !notFound && !replayed);

    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 1,
                                   QStringLiteral("profile-noop-1"), initial, false, &mutation,
                                   &changed, &stale, &conflict, &notFound, &replayed, &error));
    QVERIFY(replayed);
    QCOMPARE(mutation.revision, quint64(1));
}

void ProfilePersistenceTest::exhaustedEventSequenceDoesNotCreateProfile()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    SessionDatabase database(databasePath);
    QString error;
    QVERIFY2(database.initialize(&error), qPrintable(error));
    QVariantMap patch;
    patch.insert(QStringLiteral("mode"), QStringLiteral("quality"));
    MutationResult mutation;
    bool changed = false;
    bool stale = false;
    bool conflict = false;
    bool notFound = false;
    bool replayed = false;
    QVERIFY(!database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                    QStringLiteral("profile-exhausted-1"), patch, false, &mutation,
                                    &changed, &stale, &conflict, &notFound, &replayed, &error));
    QVERIFY(error.contains(QStringLiteral("Event sequence")));
    QVERIFY(!database.readProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), &notFound, &error));
    QVERIFY(notFound);
}

void ProfilePersistenceTest::migratesExistingSchemaV2()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    const QString connectionName = QStringLiteral("profiles-v2-migration-test");
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        legacy.setDatabaseName(databasePath);
        QVERIFY(legacy.open());
        const QStringList statements{
            QStringLiteral("CREATE TABLE service_metadata(key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)"),
            QStringLiteral("INSERT INTO service_metadata VALUES ('schema_version', '2')"),
            QStringLiteral("INSERT INTO service_metadata VALUES ('service_generation', '8')"),
            QStringLiteral("CREATE TABLE preferences(key TEXT PRIMARY KEY NOT NULL, value INTEGER NOT NULL, revision INTEGER NOT NULL CHECK(revision >= 0))"),
            QStringLiteral("INSERT INTO preferences VALUES ('product_telemetry_consent', 1, 4)"),
            QStringLiteral("CREATE TABLE settings_operations(method TEXT NOT NULL, operation_id TEXT NOT NULL, enabled INTEGER NOT NULL, expected_revision INTEGER NOT NULL, result_revision INTEGER NOT NULL, PRIMARY KEY(method, operation_id))"),
            QStringLiteral("CREATE TABLE notifications(notification_id TEXT PRIMARY KEY NOT NULL, category TEXT NOT NULL, created_at_utc TEXT NOT NULL, title_message_key TEXT NOT NULL, body_message_key TEXT NOT NULL, is_read INTEGER NOT NULL CHECK(is_read IN (0,1)), critical INTEGER NOT NULL CHECK(critical IN (0,1)), toast_eligible INTEGER NOT NULL CHECK(toast_eligible IN (0,1)))"),
            QStringLiteral("CREATE TABLE notification_metadata(key TEXT PRIMARY KEY NOT NULL, value INTEGER NOT NULL CHECK(value >= 0))"),
            QStringLiteral("INSERT INTO notification_metadata VALUES ('revision', 7)"),
            QStringLiteral("CREATE TABLE notification_operations(operation_id TEXT PRIMARY KEY NOT NULL, notification_id TEXT NOT NULL, expected_revision INTEGER NOT NULL, result_revision INTEGER NOT NULL, changed INTEGER NOT NULL CHECK(changed IN (0,1)))")
        };
        for (const QString &statement : statements) {
            QSqlQuery query(legacy);
            QVERIFY2(query.exec(statement), qPrintable(query.lastError().text()));
        }
        legacy.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QString error;
    SessionDatabase migrated(databasePath);
    QVERIFY2(migrated.initialize(&error), qPrintable(error));
    const auto consent = migrated.readProductTelemetryConsent(&error);
    QVERIFY2(consent.has_value(), qPrintable(error));
    QVERIFY(consent->enabled);
    QCOMPARE(consent->revision, quint64(4));
    quint64 notificationRevision = 0;
    const auto notifications = migrated.readNotifications(&notificationRevision, &error);
    QVERIFY2(notifications.has_value(), qPrintable(error));
    QVERIFY(notifications->isEmpty());
    QCOMPARE(notificationRevision, quint64(7));

    QVariantMap patch;
    patch.insert(QStringLiteral("mode"), QStringLiteral("quality"));
    MutationResult mutation;
    bool changed = false;
    bool stale = false;
    bool conflict = false;
    bool notFound = false;
    bool replayed = false;
    QVERIFY2(migrated.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                    QStringLiteral("profile-after-v2-migration"), patch, true,
                                    &mutation, &changed, &stale, &conflict, &notFound, &replayed, &error),
             qPrintable(error));
    QCOMPARE(mutation.code, OperationResultCode::Ok);
    QCOMPARE(mutation.revision, quint64(1));
    QVERIFY(changed);
}

void ProfilePersistenceTest::validFailureResultsReplayAfterStateChanges()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    SessionDatabase database(databasePath);
    QString error;
    QVERIFY2(database.initialize(&error), qPrintable(error));

    MutationResult mutation;
    bool changed = false;
    bool stale = false;
    bool conflict = false;
    bool notFound = false;
    bool replayed = false;
    QVariantMap emptyPatch;
    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("fixed-not-found"), emptyPatch, false, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::NotFound);
    QVERIFY(notFound && !changed && !stale && !conflict && !replayed);

    QVariantMap firstPatch;
    firstPatch.insert(QStringLiteral("mode"), QStringLiteral("quality"));
    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("profile-for-fixed-results"), firstPatch, true,
                                  &mutation, &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::Ok);
    QCOMPARE(mutation.revision, quint64(1));

    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("fixed-not-found"), emptyPatch, false, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::NotFound);
    QVERIFY(notFound && replayed && !changed);

    QVariantMap differentPatch;
    differentPatch.insert(QStringLiteral("mode"), QStringLiteral("performance"));
    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("fixed-not-found"), differentPatch, true, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::Conflict);
    QVERIFY(conflict && !replayed);

    QVariantMap stalePatch;
    stalePatch.insert(QStringLiteral("mode"), QStringLiteral("balanced"));
    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("fixed-stale"), stalePatch, true, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::StaleRevision);
    QCOMPARE(mutation.revision, quint64(1));
    QVERIFY(stale && !changed && !replayed);

    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 1,
                                  QStringLiteral("advance-profile"), differentPatch, true, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::Ok);
    QCOMPARE(mutation.revision, quint64(2));
    QVERIFY(changed);

    QVERIFY(database.updateProfile(QStringLiteral("GLOBAL"), QStringLiteral("global"), 0,
                                  QStringLiteral("fixed-stale"), stalePatch, true, &mutation,
                                  &changed, &stale, &conflict, &notFound, &replayed, &error));
    QCOMPARE(mutation.code, OperationResultCode::StaleRevision);
    QCOMPARE(mutation.revision, quint64(1));
    QVERIFY(stale && replayed && !changed);
}

void ProfilePersistenceTest::malformedToastPreferenceFailsClosed()
{
    QTemporaryFile databaseFile;
    QVERIFY(databaseFile.open());
    const QString databasePath = databaseFile.fileName();
    databaseFile.close();
    QString error;
    SessionDatabase database(databasePath);
    QVERIFY2(database.initialize(&error), qPrintable(error));
    const auto initial = database.readToastNotifications(&error);
    QVERIFY2(initial.has_value(), qPrintable(error));
    QVERIFY(!initial->configured);
    QVERIFY(!initial->enabled);
    QCOMPARE(initial->revision, quint64(0));

    const QString connectionName = QStringLiteral("malformed-toast-fixture");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery corruption(raw);
        QVERIFY(corruption.exec(QStringLiteral("UPDATE preferences SET value=1 "
                                               "WHERE key='toast_notifications'")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    error.clear();
    QVERIFY(!database.readToastNotifications(&error).has_value());
    QVERIFY(error.contains(QStringLiteral("invalid persisted data")));

    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery corruption(raw);
        QVERIFY(corruption.exec(QStringLiteral("UPDATE preferences SET value=2, revision=1 "
                                               "WHERE key='toast_notifications'")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    error.clear();
    QVERIFY(!database.readToastNotifications(&error).has_value());
    QVERIFY(error.contains(QStringLiteral("invalid persisted data")));

    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery corruption(raw);
        QVERIFY(corruption.exec(QStringLiteral("UPDATE preferences SET value='garbage' "
                                               "WHERE key='toast_notifications'")));
        raw.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    error.clear();
    QVERIFY(!database.readToastNotifications(&error).has_value());
    QVERIFY(error.contains(QStringLiteral("invalid persisted data")));
    bool stale = false;
    bool conflict = false;
    bool replayed = false;
    bool changed = false;
    quint64 revision = 0;
    error.clear();
    QVERIFY(!database.updateToastNotifications(QStringLiteral("malformed-write"), true, 0, true,
                                                &revision, &stale, &conflict, &replayed,
                                                &changed, &error));
    QVERIFY(error.contains(QStringLiteral("invalid persisted data")));
    QVERIFY(!stale && !conflict && !replayed && !changed);

    const QString verifyConnectionName = QStringLiteral("verify-malformed-toast-fixture");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnectionName);
        raw.setDatabaseName(databasePath);
        QVERIFY(raw.open());
        QSqlQuery verify(raw);
        QVERIFY(verify.exec(QStringLiteral("SELECT value, typeof(value) FROM preferences "
                                           "WHERE key='toast_notifications'")));
        QVERIFY(verify.next());
        QCOMPARE(verify.value(0).toString(), QStringLiteral("garbage"));
        QCOMPARE(verify.value(1).toString(), QStringLiteral("text"));
        raw.close();
    }
    QSqlDatabase::removeDatabase(verifyConnectionName);
}

QTEST_GUILESS_MAIN(ProfilePersistenceTest)
#include "profile_persistence_test.moc"
