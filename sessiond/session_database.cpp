#include "session_database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

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
        || !schema.exec() || !schema.next() || schema.value(0).toString() != QStringLiteral("1")) {
        if (error != nullptr) {
            *error = QStringLiteral("Unsupported or unreadable session database schema version");
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

bool SessionDatabase::updateProductTelemetryConsent(const QString &operationId, bool enabled,
                                                    quint64 expectedRevision,
                                                    quint64 *newRevision, bool *stale,
                                                    bool *conflict,
                                                    bool *replayed,
                                                    QString *error)
{
    if (stale != nullptr) {
        *stale = false;
    }
    if (conflict != nullptr) {
        *conflict = false;
    }
    if (replayed != nullptr) {
        *replayed = false;
    }
    if (operationId.isEmpty() || operationId.size() > 128) {
        if (error != nullptr) {
            *error = QStringLiteral("Operation ID must contain 1 to 128 characters");
        }
        return false;
    }
    if (!database_.transaction()) {
        if (error != nullptr) {
            *error = database_.lastError().text();
        }
        return false;
    }
    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral("SELECT enabled, expected_revision, result_revision "
                                     "FROM settings_operations WHERE method=:method "
                                     "AND operation_id=:operation_id"));
    existing.bindValue(QStringLiteral(":method"), QStringLiteral("SetProductTelemetryConsent"));
    existing.bindValue(QStringLiteral(":operation_id"), operationId);
    if (!existing.exec()) {
        database_.rollback();
        if (error != nullptr) {
            *error = existing.lastError().text();
        }
        return false;
    }
    if (existing.next()) {
        const bool samePayload = existing.value(0).toBool() == enabled
            && existing.value(1).toULongLong() == expectedRevision;
        if (!samePayload) {
            database_.rollback();
            if (conflict != nullptr) {
                *conflict = true;
            }
            return false;
        }
        const quint64 priorResult = existing.value(2).toULongLong();
        database_.rollback();
        if (newRevision != nullptr) {
            *newRevision = priorResult;
        }
        if (replayed != nullptr) {
            *replayed = true;
        }
        return true;
    }
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("UPDATE preferences SET value=:value, revision=revision+1 "
                                 "WHERE key='product_telemetry_consent' AND revision=:revision"));
    query.bindValue(QStringLiteral(":value"), enabled ? 1 : 0);
    query.bindValue(QStringLiteral(":revision"), static_cast<qlonglong>(expectedRevision));
    if (!query.exec()) {
        database_.rollback();
        if (error != nullptr) {
            *error = query.lastError().text();
        }
        return false;
    }
    if (query.numRowsAffected() != 1) {
        database_.rollback();
        if (stale != nullptr) {
            *stale = true;
        }
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
        if (error != nullptr) {
            *error = record.lastError().text();
        }
        return false;
    }
    if (!database_.commit()) {
        if (error != nullptr) {
            *error = database_.lastError().text();
        }
        database_.rollback();
        return false;
    }
    if (newRevision != nullptr) {
        *newRevision = expectedRevision + 1;
    }
    return true;
}

quint64 SessionDatabase::generation() const
{
    return generation_;
}
