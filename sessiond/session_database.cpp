#include "session_database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QRegularExpression>
#include <limits>

SessionDatabase::SessionDatabase(QString databasePath)
    : databasePath_(std::move(databasePath)),
      connectionName_(QStringLiteral("sessiond-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

SessionDatabase::~SessionDatabase()
{
    if (database_.isValid()) {
        database_.close();
    }
    database_ = {};
    QSqlDatabase::removeDatabase(connectionName_);
}

bool SessionDatabase::execute(const QString &sql, QString *error)
{
    QSqlQuery query(database_);
    if (query.exec(sql)) {
        return true;
    }
    if (error != nullptr) {
        *error = query.lastError().text();
    }
    return false;
}

bool SessionDatabase::initialize(QString *error)
{
    const QFileInfo info(databasePath_);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create the XDG data directory");
        }
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databasePath_);
    database_.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!database_.open()) {
        if (error != nullptr) {
            *error = database_.lastError().text();
        }
        return false;
    }

    if (!execute(QStringLiteral("PRAGMA journal_mode=WAL"), error)
        || !execute(QStringLiteral("PRAGMA synchronous=FULL"), error)
        || !execute(QStringLiteral("PRAGMA foreign_keys=ON"), error)) {
        return false;
    }

    QSqlQuery integrity(database_);
    if (!integrity.exec(QStringLiteral("PRAGMA integrity_check")) || !integrity.next()
        || integrity.value(0).toString() != QStringLiteral("ok")) {
        if (error != nullptr) {
            *error = integrity.lastError().isValid()
                ? integrity.lastError().text()
                : QStringLiteral("SQLite integrity check did not report ok");
        }
        return false;
    }

    if (!database_.transaction()) {
        if (error != nullptr) {
            *error = database_.lastError().text();
        }
        return false;
    }

    const bool migrationOk = execute(
            QStringLiteral("CREATE TABLE IF NOT EXISTS service_metadata ("
                           "key TEXT PRIMARY KEY NOT NULL, value TEXT NOT NULL)"), error)
        && execute(QStringLiteral("CREATE TABLE IF NOT EXISTS preferences ("
                                  "key TEXT PRIMARY KEY NOT NULL, value INTEGER NOT NULL, "
                                  "revision INTEGER NOT NULL CHECK(revision >= 0))"), error)
        && execute(QStringLiteral("CREATE TABLE IF NOT EXISTS settings_operations ("
                                  "method TEXT NOT NULL, operation_id TEXT NOT NULL, enabled INTEGER NOT NULL, "
                                  "expected_revision INTEGER NOT NULL, result_revision INTEGER NOT NULL, "
                                  "PRIMARY KEY(method, operation_id))"), error)
        && execute(QStringLiteral("INSERT OR IGNORE INTO service_metadata(key, value) "
                                  "VALUES ('schema_version', '1')"), error)
        && execute(QStringLiteral("INSERT OR IGNORE INTO service_metadata(key, value) "
                                  "VALUES ('service_generation', '0')"), error)
        && execute(QStringLiteral("INSERT OR IGNORE INTO preferences(key, value, revision) "
                                  "VALUES ('product_telemetry_consent', 0, 0)"), error);
    if (!migrationOk) {
        database_.rollback();
        return false;
    }

    QSqlQuery schema(database_);
    if (!schema.prepare(QStringLiteral("SELECT value FROM service_metadata WHERE key='schema_version'"))
        || !schema.exec() || !schema.next()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unsupported or unreadable session database schema version");
        }
        database_.rollback();
        return false;
    }
    const QString schemaVersion = schema.value(0).toString();
    if (schemaVersion == QLatin1String("1")) {
        const bool notificationsMigrationOk = execute(
                QStringLiteral("CREATE TABLE notifications ("
                               "notification_id TEXT PRIMARY KEY NOT NULL, "
                               "category TEXT NOT NULL, created_at_utc TEXT NOT NULL, "
                               "title_message_key TEXT NOT NULL, body_message_key TEXT NOT NULL, "
                               "is_read INTEGER NOT NULL CHECK(is_read IN (0,1)), "
                               "critical INTEGER NOT NULL CHECK(critical IN (0,1)), "
                               "toast_eligible INTEGER NOT NULL CHECK(toast_eligible IN (0,1)))"), error)
            && execute(QStringLiteral("CREATE TABLE notification_metadata ("
                                      "key TEXT PRIMARY KEY NOT NULL, value INTEGER NOT NULL CHECK(value >= 0))"), error)
            && execute(QStringLiteral("INSERT INTO notification_metadata(key, value) VALUES ('revision', 1)"), error)
            && execute(QStringLiteral("CREATE TABLE notification_operations ("
                                      "operation_id TEXT PRIMARY KEY NOT NULL, notification_id TEXT NOT NULL, "
                                      "expected_revision INTEGER NOT NULL, result_revision INTEGER NOT NULL, "
                                      "changed INTEGER NOT NULL CHECK(changed IN (0,1)))"), error)
            && execute(QStringLiteral("UPDATE service_metadata SET value='2' WHERE key='schema_version'"), error);
        if (!notificationsMigrationOk) {
            database_.rollback();
            return false;
        }
    } else if (schemaVersion != QLatin1String("2")) {
        if (error != nullptr) {
            *error = QStringLiteral("Unsupported or unreadable session database schema version");
        }
        database_.rollback();
        return false;
    }

    QSqlQuery notificationRevision(database_);
    if (!notificationRevision.exec(QStringLiteral("SELECT value FROM notification_metadata WHERE key='revision'"))
        || !notificationRevision.next() || notificationRevision.value(0).toLongLong() < 1) {
        if (error != nullptr) {
            *error = QStringLiteral("Notification revision metadata is missing or invalid");
        }
        database_.rollback();
        return false;
    }

    QSqlQuery bump(database_);
    if (!bump.exec(QStringLiteral("UPDATE service_metadata SET value=CAST(value AS INTEGER)+1 "
                                  "WHERE key='service_generation'"))) {
        if (error != nullptr) {
            *error = bump.lastError().text();
        }
        database_.rollback();
        return false;
    }
    QSqlQuery readGeneration(database_);
    if (!readGeneration.exec(QStringLiteral("SELECT value FROM service_metadata "
                                            "WHERE key='service_generation'"))
        || !readGeneration.next()) {
        if (error != nullptr) {
            *error = readGeneration.lastError().text();
        }
        database_.rollback();
        return false;
    }
    bool ok = false;
    generation_ = readGeneration.value(0).toULongLong(&ok);
    if (!ok || generation_ == 0 || !database_.commit()) {
        if (error != nullptr) {
            *error = ok ? database_.lastError().text()
                        : QStringLiteral("Invalid service generation in session database");
        }
        database_.rollback();
        return false;
    }
    return true;
}

std::optional<ProductTelemetryConsent> SessionDatabase::readProductTelemetryConsent(QString *error)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT value, revision FROM preferences "
                                 "WHERE key='product_telemetry_consent'"));
    if (!query.exec() || !query.next()) {
        if (error != nullptr) {
            *error = query.lastError().isValid() ? query.lastError().text()
                                                  : QStringLiteral("Consent preference is missing");
        }
        return std::nullopt;
    }
    const int enabledValue = query.value(0).toInt();
    const qlonglong revisionValue = query.value(1).toLongLong();
    if ((enabledValue != 0 && enabledValue != 1) || revisionValue < 0) {
        if (error != nullptr) {
            *error = QStringLiteral("Consent preference contains invalid persisted data");
        }
        return std::nullopt;
    }
    return ProductTelemetryConsent{enabledValue == 1, static_cast<quint64>(revisionValue)};
}

bool SessionDatabase::updateProductTelemetryConsent(
    const QString &operationId, bool enabled, quint64 expectedRevision,
    const adrenalin::contracts::notifications1::Notification &notification,
    quint64 *newRevision, quint64 *notificationRevision, bool *stale, bool *conflict,
    bool *replayed, bool *notificationInserted, QString *error)
{
    if (stale != nullptr) *stale = false;
    if (conflict != nullptr) *conflict = false;
    if (replayed != nullptr) *replayed = false;
    if (notificationInserted != nullptr) *notificationInserted = false;
    static const QRegularExpression operationSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!operationSyntax.match(operationId).hasMatch()
        || expectedRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
        if (error != nullptr) *error = QStringLiteral("Operation ID or revision is outside its supported range");
        return false;
    }
    QString notificationError;
    if (!notification.isValid(&notificationError)) {
        if (error != nullptr) *error = notificationError;
        return false;
    }
    if (!database_.transaction()) {
        if (error != nullptr) *error = database_.lastError().text();
        return false;
    }
    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral("SELECT enabled, expected_revision, result_revision FROM settings_operations "
                                     "WHERE method=:method AND operation_id=:operation_id"));
    existing.bindValue(QStringLiteral(":method"), QStringLiteral("SetProductTelemetryConsent"));
    existing.bindValue(QStringLiteral(":operation_id"), operationId);
    if (!existing.exec()) {
        database_.rollback();
        if (error != nullptr) *error = existing.lastError().text();
        return false;
    }
    if (existing.next()) {
        if (existing.value(0).toBool() != enabled || existing.value(1).toULongLong() != expectedRevision) {
            database_.rollback();
            if (conflict != nullptr) *conflict = true;
            return false;
        }
        QSqlQuery readNotificationRevision(database_);
        if (!readNotificationRevision.exec(QStringLiteral("SELECT value FROM notification_metadata WHERE key='revision'"))
            || !readNotificationRevision.next()) {
            database_.rollback();
            if (error != nullptr) *error = readNotificationRevision.lastError().text();
            return false;
        }
        const quint64 priorResult = existing.value(2).toULongLong();
        const quint64 currentNotificationRevision = readNotificationRevision.value(0).toULongLong();
        database_.rollback();
        if (newRevision != nullptr) *newRevision = priorResult;
        if (notificationRevision != nullptr) *notificationRevision = currentNotificationRevision;
        if (replayed != nullptr) *replayed = true;
        return true;
    }
    QSqlQuery currentNotificationRevisionQuery(database_);
    if (!currentNotificationRevisionQuery.exec(QStringLiteral("SELECT value FROM notification_metadata WHERE key='revision'"))
        || !currentNotificationRevisionQuery.next()) {
        database_.rollback();
        if (error != nullptr) *error = currentNotificationRevisionQuery.lastError().text();
        return false;
    }
    const quint64 currentNotificationRevision = currentNotificationRevisionQuery.value(0).toULongLong();
    if (currentNotificationRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Notification revision is exhausted");
        return false;
    }
    QSqlQuery update(database_);
    update.prepare(QStringLiteral("UPDATE preferences SET value=:value, revision=revision+1 "
                                  "WHERE key='product_telemetry_consent' AND revision=:revision"));
    update.bindValue(QStringLiteral(":value"), enabled ? 1 : 0);
    update.bindValue(QStringLiteral(":revision"), static_cast<qlonglong>(expectedRevision));
    if (!update.exec()) {
        database_.rollback();
        if (error != nullptr) *error = update.lastError().text();
        return false;
    }
    if (update.numRowsAffected() != 1) {
        database_.rollback();
        if (stale != nullptr) *stale = true;
        return false;
    }
    QSqlQuery record(database_);
    record.prepare(QStringLiteral("INSERT INTO settings_operations "
                                  "(method, operation_id, enabled, expected_revision, result_revision) "
                                  "VALUES (:method, :operation_id, :enabled, :expected_revision, :result_revision)"));
    record.bindValue(QStringLiteral(":method"), QStringLiteral("SetProductTelemetryConsent"));
    record.bindValue(QStringLiteral(":operation_id"), operationId);
    record.bindValue(QStringLiteral(":enabled"), enabled ? 1 : 0);
    record.bindValue(QStringLiteral(":expected_revision"), static_cast<qlonglong>(expectedRevision));
    record.bindValue(QStringLiteral(":result_revision"), static_cast<qlonglong>(expectedRevision + 1));
    if (!record.exec()) {
        database_.rollback();
        if (error != nullptr) *error = record.lastError().text();
        return false;
    }
    QSqlQuery insertNotification(database_);
    insertNotification.prepare(QStringLiteral("INSERT INTO notifications "
        "(notification_id, category, created_at_utc, title_message_key, body_message_key, is_read, critical, toast_eligible) "
        "VALUES (:id, :category, :created, :title, :body, :read, :critical, :toast)"));
    insertNotification.bindValue(QStringLiteral(":id"), notification.notificationId);
    insertNotification.bindValue(QStringLiteral(":category"), notification.category);
    insertNotification.bindValue(QStringLiteral(":created"), notification.createdAtUtc);
    insertNotification.bindValue(QStringLiteral(":title"), notification.titleMessageKey);
    insertNotification.bindValue(QStringLiteral(":body"), notification.bodyMessageKey);
    insertNotification.bindValue(QStringLiteral(":read"), notification.isRead ? 1 : 0);
    insertNotification.bindValue(QStringLiteral(":critical"), notification.critical ? 1 : 0);
    insertNotification.bindValue(QStringLiteral(":toast"), notification.toastEligible ? 1 : 0);
    if (!insertNotification.exec()) {
        database_.rollback();
        if (error != nullptr) *error = insertNotification.lastError().text();
        return false;
    }
    QSqlQuery bumpNotificationRevision(database_);
    if (!bumpNotificationRevision.exec(QStringLiteral("UPDATE notification_metadata SET value=value+1 WHERE key='revision'"))
        || bumpNotificationRevision.numRowsAffected() != 1) {
        database_.rollback();
        if (error != nullptr) {
            *error = bumpNotificationRevision.lastError().isValid()
                ? bumpNotificationRevision.lastError().text()
                : QStringLiteral("Notification revision metadata row is missing");
        }
        return false;
    }
    if (!database_.commit()) {
        if (error != nullptr) *error = database_.lastError().text();
        database_.rollback();
        return false;
    }
    if (newRevision != nullptr) *newRevision = expectedRevision + 1;
    if (notificationRevision != nullptr) *notificationRevision = currentNotificationRevision + 1;
    if (notificationInserted != nullptr) *notificationInserted = true;
    return true;
}

std::optional<QList<adrenalin::contracts::notifications1::Notification>> SessionDatabase::readNotifications(
    quint64 *revision, QString *error)
{
    using adrenalin::contracts::notifications1::Notification;
    if (!database_.transaction()) {
        if (error != nullptr) *error = database_.lastError().text();
        return std::nullopt;
    }
    QSqlQuery revisionQuery(database_);
    if (!revisionQuery.exec(QStringLiteral("SELECT value FROM notification_metadata WHERE key='revision'"))
        || !revisionQuery.next()) {
        database_.rollback();
        if (error != nullptr) *error = revisionQuery.lastError().text();
        return std::nullopt;
    }
    const qlonglong revisionValue = revisionQuery.value(0).toLongLong();
    if (revisionValue < 1) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Persisted notification revision is invalid");
        return std::nullopt;
    }
    QList<Notification> notifications;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT notification_id, category, created_at_utc, title_message_key, "
                                   "body_message_key, is_read, critical, toast_eligible FROM notifications "
                                   "ORDER BY created_at_utc DESC, notification_id ASC"))) {
        database_.rollback();
        if (error != nullptr) *error = query.lastError().text();
        return std::nullopt;
    }
    while (query.next()) {
        Notification notification;
        notification.notificationId = query.value(0).toString();
        notification.category = query.value(1).toString();
        notification.createdAtUtc = query.value(2).toString();
        notification.titleMessageKey = query.value(3).toString();
        notification.bodyMessageKey = query.value(4).toString();
        notification.isRead = query.value(5).toBool();
        notification.critical = query.value(6).toBool();
        notification.toastEligible = query.value(7).toBool();
        if (!notification.isValid(error)) {
            database_.rollback();
            return std::nullopt;
        }
        notifications.append(std::move(notification));
    }
    if (!database_.commit()) {
        if (error != nullptr) *error = database_.lastError().text();
        database_.rollback();
        return std::nullopt;
    }
    if (revision != nullptr) *revision = static_cast<quint64>(revisionValue);
    return notifications;
}

bool SessionDatabase::markNotificationRead(const QString &notificationId, const QString &operationId,
                                           quint64 expectedRevision, bool eventSequenceAvailable, quint64 *newRevision,
                                           bool *changed, bool *stale, bool *conflict,
                                           bool *notFound, bool *replayed, QString *error)
{
    if (changed != nullptr) *changed = false;
    if (stale != nullptr) *stale = false;
    if (conflict != nullptr) *conflict = false;
    if (notFound != nullptr) *notFound = false;
    if (replayed != nullptr) *replayed = false;
    static const QRegularExpression tokenSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!tokenSyntax.match(notificationId).hasMatch() || !tokenSyntax.match(operationId).hasMatch()
        || expectedRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
        if (error != nullptr) *error = QStringLiteral("Notification ID, operation ID, or revision is invalid");
        return false;
    }
    if (!database_.transaction()) {
        if (error != nullptr) *error = database_.lastError().text();
        return false;
    }
    QSqlQuery currentRevisionQuery(database_);
    if (!currentRevisionQuery.exec(QStringLiteral("SELECT value FROM notification_metadata WHERE key='revision'"))
        || !currentRevisionQuery.next()) {
        database_.rollback();
        if (error != nullptr) *error = currentRevisionQuery.lastError().text();
        return false;
    }
    const quint64 currentRevision = currentRevisionQuery.value(0).toULongLong();
    if (newRevision != nullptr) *newRevision = currentRevision;
    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral("SELECT notification_id, expected_revision, result_revision, changed "
                                     "FROM notification_operations WHERE operation_id=:operation_id"));
    existing.bindValue(QStringLiteral(":operation_id"), operationId);
    if (!existing.exec()) {
        database_.rollback();
        if (error != nullptr) *error = existing.lastError().text();
        return false;
    }
    if (existing.next()) {
        if (existing.value(0).toString() != notificationId || existing.value(1).toULongLong() != expectedRevision) {
            database_.rollback();
            if (conflict != nullptr) *conflict = true;
            return false;
        }
        const quint64 priorRevision = existing.value(2).toULongLong();
        const bool priorChanged = existing.value(3).toBool();
        database_.rollback();
        if (newRevision != nullptr) *newRevision = priorRevision;
        if (changed != nullptr) *changed = priorChanged;
        if (replayed != nullptr) *replayed = true;
        return true;
    }
    QSqlQuery row(database_);
    row.prepare(QStringLiteral("SELECT is_read FROM notifications WHERE notification_id=:id"));
    row.bindValue(QStringLiteral(":id"), notificationId);
    if (!row.exec()) {
        database_.rollback();
        if (error != nullptr) *error = row.lastError().text();
        return false;
    }
    if (!row.next()) {
        database_.rollback();
        if (notFound != nullptr) *notFound = true;
        return false;
    }
    if (expectedRevision != currentRevision) {
        database_.rollback();
        if (stale != nullptr) *stale = true;
        return false;
    }
    const bool didChange = !row.value(0).toBool();
    if (didChange && !eventSequenceAvailable) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Event sequence is exhausted");
        return false;
    }
    if (didChange && currentRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Notification revision is exhausted");
        return false;
    }
    const quint64 resultRevision = currentRevision + (didChange ? 1 : 0);
    if (didChange) {
        QSqlQuery update(database_);
        update.prepare(QStringLiteral("UPDATE notifications SET is_read=1 WHERE notification_id=:id"));
        update.bindValue(QStringLiteral(":id"), notificationId);
        if (!update.exec() || update.numRowsAffected() != 1) {
            database_.rollback();
            if (error != nullptr) *error = update.lastError().text();
            return false;
        }
    }
    if (didChange) {
        QSqlQuery bumpRevision(database_);
        if (!bumpRevision.exec(QStringLiteral("UPDATE notification_metadata SET value=value+1 WHERE key='revision'"))
            || bumpRevision.numRowsAffected() != 1) {
            database_.rollback();
            if (error != nullptr) {
                *error = bumpRevision.lastError().isValid()
                    ? bumpRevision.lastError().text()
                    : QStringLiteral("Notification revision metadata row is missing");
            }
            return false;
        }
    }
    QSqlQuery record(database_);
    record.prepare(QStringLiteral("INSERT INTO notification_operations "
                                  "(operation_id, notification_id, expected_revision, result_revision, changed) "
                                  "VALUES (:operation_id, :notification_id, :expected_revision, :result_revision, :changed)"));
    record.bindValue(QStringLiteral(":operation_id"), operationId);
    record.bindValue(QStringLiteral(":notification_id"), notificationId);
    record.bindValue(QStringLiteral(":expected_revision"), static_cast<qlonglong>(expectedRevision));
    record.bindValue(QStringLiteral(":result_revision"), static_cast<qlonglong>(resultRevision));
    record.bindValue(QStringLiteral(":changed"), didChange ? 1 : 0);
    if (!record.exec() || !database_.commit()) {
        if (error != nullptr) *error = record.lastError().isValid() ? record.lastError().text() : database_.lastError().text();
        database_.rollback();
        return false;
    }
    if (newRevision != nullptr) *newRevision = resultRevision;
    if (changed != nullptr) *changed = didChange;
    return true;
}

quint64 SessionDatabase::generation() const
{
    return generation_;
}
