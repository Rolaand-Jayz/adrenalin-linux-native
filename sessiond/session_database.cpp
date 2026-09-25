#include "session_database.h"
#include "interfaces/profiles1_contract.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QRegularExpression>
#include <QByteArray>
#include <bit>
#include <cmath>
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

namespace {

// Stored scalar blobs start with format version 1 and a type tag. Integers use
// canonical base-10 bytes, doubles use IEEE-754 bits in big-endian order, and
// strings use UTF-16BE code units. The enclosing map frames sorted keys and
// values, so replay identity does not depend on QVariantMap insertion order.
QByteArray serializeVariant(const QVariant &value);

QByteArray serializeSettings(const QVariantMap &settings)
{
    QByteArray data;
    data.append(char(1));
    const auto appendLength = [&data](quint32 length) {
        data.append(static_cast<char>((length >> 24) & 0xff));
        data.append(static_cast<char>((length >> 16) & 0xff));
        data.append(static_cast<char>((length >> 8) & 0xff));
        data.append(static_cast<char>(length & 0xff));
    };
    if (static_cast<quint64>(settings.size()) > std::numeric_limits<quint32>::max()) {
        return {};
    }
    appendLength(static_cast<quint32>(settings.size()));
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        QByteArray keyBytes;
        keyBytes.reserve(it.key().size() * 2);
        for (QChar character : it.key()) {
            keyBytes.append(static_cast<char>((character.unicode() >> 8) & 0xff));
            keyBytes.append(static_cast<char>(character.unicode() & 0xff));
        }
        const QByteArray valueBytes = serializeVariant(it.value());
        if (static_cast<quint64>(keyBytes.size()) > std::numeric_limits<quint32>::max()
            || static_cast<quint64>(valueBytes.size()) > std::numeric_limits<quint32>::max()
            || valueBytes.isEmpty()) {
            return {};
        }
        appendLength(static_cast<quint32>(keyBytes.size()));
        data.append(keyBytes);
        appendLength(static_cast<quint32>(valueBytes.size()));
        data.append(valueBytes);
    }
    return data;
}

QByteArray serializeVariant(const QVariant &value)
{
    QByteArray data(1, char(1));
    const auto appendInteger = [&data](const QString &number) { data.append(number.toLatin1()); };
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        data.append(char('b'));
        data.append(value.toBool() ? char('1') : char('0'));
        break;
    case QMetaType::Int:
        data.append(char('i'));
        appendInteger(QString::number(value.toInt()));
        break;
    case QMetaType::UInt:
        data.append(char('u'));
        appendInteger(QString::number(value.toUInt()));
        break;
    case QMetaType::LongLong:
        data.append(char('l'));
        appendInteger(QString::number(value.toLongLong()));
        break;
    case QMetaType::ULongLong:
        data.append(char('U'));
        appendInteger(QString::number(value.toULongLong()));
        break;
    case QMetaType::Double: {
        data.append(char('d'));
        const quint64 bits = std::bit_cast<quint64>(value.toDouble());
        for (int shift = 56; shift >= 0; shift -= 8) {
            data.append(static_cast<char>((bits >> shift) & 0xff));
        }
        break;
    }
    case QMetaType::QString: {
        data.append(char('s'));
        const QString string = value.toString();
        for (QChar character : string) {
            data.append(static_cast<char>((character.unicode() >> 8) & 0xff));
            data.append(static_cast<char>(character.unicode() & 0xff));
        }
        break;
    }
    default:
        return {};
    }
    return data;
}

bool deserializeVariant(const QByteArray &data, QVariant *value)
{
    if (data.size() < 2 || static_cast<unsigned char>(data.at(0)) != 1) {
        return false;
    }
    const QByteArray payload = data.mid(2);
    const auto parseSigned = [&payload](qint64 *parsed) {
        bool ok = false;
        *parsed = QString::fromLatin1(payload).toLongLong(&ok, 10);
        return ok && QString::number(*parsed).toLatin1() == payload;
    };
    const auto parseUnsigned = [&payload](quint64 *parsed) {
        bool ok = false;
        *parsed = QString::fromLatin1(payload).toULongLong(&ok, 10);
        return ok && QString::number(*parsed).toLatin1() == payload;
    };
    switch (data.at(1)) {
    case 'b':
        if (payload == QByteArrayLiteral("1")) { *value = true; return true; }
        if (payload == QByteArrayLiteral("0")) { *value = false; return true; }
        return false;
    case 'i': {
        qint64 parsed = 0;
        if (!parseSigned(&parsed) || parsed < std::numeric_limits<int>::min()
            || parsed > std::numeric_limits<int>::max()) return false;
        *value = static_cast<int>(parsed);
        return true;
    }
    case 'u': {
        quint64 parsed = 0;
        if (!parseUnsigned(&parsed) || parsed > std::numeric_limits<uint>::max()) return false;
        *value = static_cast<uint>(parsed);
        return true;
    }
    case 'l': {
        qint64 parsed = 0;
        if (!parseSigned(&parsed)) return false;
        *value = parsed;
        return true;
    }
    case 'U': {
        quint64 parsed = 0;
        if (!parseUnsigned(&parsed)) return false;
        *value = parsed;
        return true;
    }
    case 'd': {
        if (payload.size() != 8) return false;
        quint64 bits = 0;
        for (unsigned char byte : payload) bits = (bits << 8) | byte;
        const double parsed = std::bit_cast<double>(bits);
        if (!std::isfinite(parsed)) return false;
        *value = parsed;
        return true;
    }
    case 's': {
        if ((payload.size() % 2) != 0) return false;
        QString parsed;
        parsed.reserve(payload.size() / 2);
        for (qsizetype index = 0; index < payload.size(); index += 2) {
            const auto high = static_cast<quint8>(payload.at(index));
            const auto low = static_cast<quint8>(payload.at(index + 1));
            parsed.append(QChar(static_cast<ushort>((high << 8) | low)));
        }
        *value = parsed;
        return true;
    }
    default:
        return false;
    }
}

QString profileSubjectId(const QString &kind, const QString &id)
{
    return kind + QLatin1Char(':') + id;
}

} // namespace

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
    QString schemaVersion = schema.value(0).toString();
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
        schemaVersion = QStringLiteral("2");
    }
    if (schemaVersion == QLatin1String("2")) {
        const bool profilesMigrationOk = execute(
                QStringLiteral("CREATE TABLE profiles ("
                               "subject_kind TEXT NOT NULL, subject_id TEXT NOT NULL, "
                               "profile_id TEXT NOT NULL UNIQUE, revision INTEGER NOT NULL CHECK(revision > 0), "
                               "PRIMARY KEY(subject_kind, subject_id))"), error)
            && execute(QStringLiteral("CREATE TABLE profile_settings ("
                                      "subject_kind TEXT NOT NULL, subject_id TEXT NOT NULL, "
                                      "setting_key TEXT NOT NULL, value BLOB NOT NULL, "
                                      "PRIMARY KEY(subject_kind, subject_id, setting_key), "
                                      "FOREIGN KEY(subject_kind, subject_id) REFERENCES profiles(subject_kind, subject_id) "
                                      "ON DELETE CASCADE)"), error)
            && execute(QStringLiteral("CREATE TABLE profile_operations ("
                                      "method TEXT NOT NULL, operation_id TEXT NOT NULL, "
                                      "subject_kind TEXT NOT NULL, subject_id TEXT NOT NULL, "
                                      "expected_revision INTEGER NOT NULL, patch BLOB NOT NULL, "
                                      "result_code TEXT NOT NULL, message_key TEXT NOT NULL, diagnostic TEXT NOT NULL, "
                                      "retryable INTEGER NOT NULL CHECK(retryable IN (0,1)), provider TEXT NOT NULL, "
                                      "result_subject_id TEXT NOT NULL, result_revision INTEGER NOT NULL CHECK(result_revision >= 0), "
                                      "changed INTEGER NOT NULL CHECK(changed IN (0,1)), "
                                      "PRIMARY KEY(method, operation_id))"), error)
            && execute(QStringLiteral("UPDATE service_metadata SET value='3' WHERE key='schema_version'"), error);
        if (!profilesMigrationOk) {
            database_.rollback();
            return false;
        }
        schemaVersion = QStringLiteral("3");
    }
    if (schemaVersion != QLatin1String("3")) {
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

std::optional<adrenalin::contracts::profiles1::Profile> SessionDatabase::readProfile(
    const QString &subjectKind, const QString &subjectId, bool *notFound, QString *error)
{
    using adrenalin::contracts::profiles1::Profile;
    if (notFound != nullptr) *notFound = false;
    if (!adrenalin::contracts::profiles1::isValidSubject(subjectKind, subjectId)) {
        if (error != nullptr) *error = QStringLiteral("Profile subject identity is invalid");
        return std::nullopt;
    }
    if (!database_.transaction()) {
        if (error != nullptr) *error = database_.lastError().text();
        return std::nullopt;
    }
    QSqlQuery row(database_);
    row.prepare(QStringLiteral("SELECT profile_id, revision FROM profiles "
                                "WHERE subject_kind=:kind AND subject_id=:id"));
    row.bindValue(QStringLiteral(":kind"), subjectKind);
    row.bindValue(QStringLiteral(":id"), subjectId);
    if (!row.exec()) {
        database_.rollback();
        if (error != nullptr) *error = row.lastError().text();
        return std::nullopt;
    }
    if (!row.next()) {
        database_.rollback();
        if (notFound != nullptr) *notFound = true;
        return std::nullopt;
    }
    bool revisionOk = false;
    const quint64 revision = row.value(1).toULongLong(&revisionOk);
    if (!revisionOk || revision == 0) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Persisted profile revision is invalid");
        return std::nullopt;
    }
    Profile profile;
    profile.profileId = row.value(0).toString();
    profile.subjectKind = subjectKind;
    profile.subjectId = subjectId;
    profile.referenceState = QStringLiteral("REFERENCE_GATED");
    profile.revision = revision;
    QSqlQuery settings(database_);
    settings.prepare(QStringLiteral("SELECT setting_key, value FROM profile_settings "
                                     "WHERE subject_kind=:kind AND subject_id=:id ORDER BY setting_key"));
    settings.bindValue(QStringLiteral(":kind"), subjectKind);
    settings.bindValue(QStringLiteral(":id"), subjectId);
    if (!settings.exec()) {
        database_.rollback();
        if (error != nullptr) *error = settings.lastError().text();
        return std::nullopt;
    }
    while (settings.next()) {
        QVariant value;
        if (!deserializeVariant(settings.value(1).toByteArray(), &value)) {
            database_.rollback();
            if (error != nullptr) *error = QStringLiteral("Persisted profile setting is corrupt");
            return std::nullopt;
        }
        profile.settings.insert(settings.value(0).toString(), value);
    }
    if (!profile.isValid(error)) {
        database_.rollback();
        return std::nullopt;
    }
    if (!database_.commit()) {
        if (error != nullptr) *error = database_.lastError().text();
        database_.rollback();
        return std::nullopt;
    }
    return profile;
}

bool SessionDatabase::updateProfile(const QString &subjectKind, const QString &subjectId,
                                    quint64 expectedRevision, const QString &operationId,
                                    const QVariantMap &settingsPatch, bool eventSequenceAvailable,
                                    adrenalin::contracts::MutationResult *mutation, bool *changed,
                                    bool *stale, bool *conflict, bool *notFound, bool *replayed,
                                    QString *error)
{
    using namespace adrenalin::contracts;
    if (changed != nullptr) *changed = false;
    if (stale != nullptr) *stale = false;
    if (conflict != nullptr) *conflict = false;
    if (notFound != nullptr) *notFound = false;
    if (replayed != nullptr) *replayed = false;
    static const QRegularExpression operationSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    const QByteArray patchBytes = serializeSettings(settingsPatch);
    if (!profiles1::isValidSubject(subjectKind, subjectId) || !operationSyntax.match(operationId).hasMatch()
        || !profiles1::isValidSettings(settingsPatch) || patchBytes.isEmpty()
        || expectedRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
        if (error != nullptr) *error = QStringLiteral("Profile subject, operation ID, revision, or settings patch is invalid");
        return false;
    }
    MutationResult result;
    result.operationId = operationId;
    result.provider = QStringLiteral("profiles1");
    result.subjectId = profileSubjectId(subjectKind, subjectId);
    const auto finish = [&](OperationResultCode code, const QString &key, const QString &detail,
                            quint64 revision) {
        result.code = code;
        result.humanMessageKey = key;
        result.diagnosticMessage = detail;
        result.retryable = false;
        result.revision = revision;
        if (mutation != nullptr) *mutation = result;
    };
    if (!database_.transaction()) {
        if (error != nullptr) *error = database_.lastError().text();
        return false;
    }
    QSqlQuery previous(database_);
    previous.prepare(QStringLiteral("SELECT subject_kind, subject_id, expected_revision, patch, result_code, "
                                    "message_key, diagnostic, retryable, provider, result_subject_id, "
                                    "result_revision, changed FROM profile_operations "
                                    "WHERE method=:method AND operation_id=:operation_id"));
    previous.bindValue(QStringLiteral(":method"), QStringLiteral("UpdateProfile"));
    previous.bindValue(QStringLiteral(":operation_id"), operationId);
    if (!previous.exec()) {
        database_.rollback();
        if (error != nullptr) *error = previous.lastError().text();
        return false;
    }
    const auto persistOperationResult = [&](bool didChange) {
        QSqlQuery record(database_);
        record.prepare(QStringLiteral("INSERT INTO profile_operations "
            "(method, operation_id, subject_kind, subject_id, expected_revision, patch, result_code, message_key, "
            "diagnostic, retryable, provider, result_subject_id, result_revision, changed) "
            "VALUES ('UpdateProfile', :operation_id, :kind, :id, :expected, :patch, :code, :key, :diagnostic, "
            ":retryable, :provider, :subject, :revision, :changed)"));
        record.bindValue(QStringLiteral(":operation_id"), operationId);
        record.bindValue(QStringLiteral(":kind"), subjectKind);
        record.bindValue(QStringLiteral(":id"), subjectId);
        record.bindValue(QStringLiteral(":expected"), static_cast<qlonglong>(expectedRevision));
        record.bindValue(QStringLiteral(":patch"), patchBytes);
        record.bindValue(QStringLiteral(":code"), operationResultCodeName(result.code));
        record.bindValue(QStringLiteral(":key"), result.humanMessageKey);
        record.bindValue(QStringLiteral(":diagnostic"), result.diagnosticMessage);
        record.bindValue(QStringLiteral(":retryable"), result.retryable ? 1 : 0);
        record.bindValue(QStringLiteral(":provider"), result.provider);
        record.bindValue(QStringLiteral(":subject"), result.subjectId);
        record.bindValue(QStringLiteral(":revision"), static_cast<qlonglong>(result.revision));
        record.bindValue(QStringLiteral(":changed"), didChange ? 1 : 0);
        if (!record.exec()) {
            database_.rollback();
            if (error != nullptr) *error = record.lastError().text();
            return false;
        }
        if (!database_.commit()) {
            if (error != nullptr) *error = database_.lastError().text();
            database_.rollback();
            return false;
        }
        if (mutation != nullptr) *mutation = result;
        if (changed != nullptr) *changed = didChange;
        return true;
    };
    if (previous.next()) {
        if (previous.value(0).toString() != subjectKind || previous.value(1).toString() != subjectId
            || previous.value(2).toULongLong() != expectedRevision
            || previous.value(3).toByteArray() != patchBytes) {
            database_.rollback();
            if (conflict != nullptr) *conflict = true;
            finish(OperationResultCode::Conflict, QStringLiteral("profiles.operation.conflict"),
                   QStringLiteral("Operation ID was reused with different profile values"), 0);
            return true;
        }
        const auto priorCode = operationResultCodeFromName(previous.value(4).toString());
        if (!priorCode.has_value()) {
            database_.rollback();
            if (error != nullptr) *error = QStringLiteral("Persisted profile operation code is invalid");
            return false;
        }
        result.code = *priorCode;
        result.humanMessageKey = previous.value(5).toString();
        result.diagnosticMessage = previous.value(6).toString();
        result.retryable = previous.value(7).toBool();
        result.provider = previous.value(8).toString();
        result.subjectId = previous.value(9).toString();
        result.revision = previous.value(10).toULongLong();
        const bool priorChanged = previous.value(11).toBool();
        database_.rollback();
        if (mutation != nullptr) *mutation = result;
        if (changed != nullptr) *changed = priorChanged;
        if (stale != nullptr) *stale = result.code == OperationResultCode::StaleRevision;
        if (notFound != nullptr) *notFound = result.code == OperationResultCode::NotFound;
        if (replayed != nullptr) *replayed = true;
        return true;
    }

    QSqlQuery current(database_);
    current.prepare(QStringLiteral("SELECT profile_id, revision FROM profiles "
                                   "WHERE subject_kind=:kind AND subject_id=:id"));
    current.bindValue(QStringLiteral(":kind"), subjectKind);
    current.bindValue(QStringLiteral(":id"), subjectId);
    if (!current.exec()) {
        database_.rollback();
        if (error != nullptr) *error = current.lastError().text();
        return false;
    }
    const bool exists = current.next();
    if (!exists && (settingsPatch.isEmpty() || expectedRevision != 0)) {
        if (notFound != nullptr) *notFound = true;
        finish(OperationResultCode::NotFound, QStringLiteral("profiles.profile.not_found"),
               QStringLiteral("Profile subject has no persisted profile"), 0);
        return persistOperationResult(false);
    }
    bool currentRevisionOk = true;
    const quint64 currentRevision = exists ? current.value(1).toULongLong(&currentRevisionOk) : 0;
    if (!currentRevisionOk || (exists && currentRevision == 0)) {
        database_.rollback();
        if (error != nullptr) *error = QStringLiteral("Persisted profile revision is invalid");
        return false;
    }
    if (expectedRevision != currentRevision) {
        if (stale != nullptr) *stale = true;
        finish(OperationResultCode::StaleRevision, QStringLiteral("profiles.operation.stale_revision"),
               QStringLiteral("Expected revision does not match current profile revision"), currentRevision);
        return persistOperationResult(false);
    }

    QVariantMap currentSettings;
    if (exists) {
        QSqlQuery settings(database_);
        settings.prepare(QStringLiteral("SELECT setting_key, value FROM profile_settings "
                                         "WHERE subject_kind=:kind AND subject_id=:id"));
        settings.bindValue(QStringLiteral(":kind"), subjectKind);
        settings.bindValue(QStringLiteral(":id"), subjectId);
        if (!settings.exec()) {
            database_.rollback();
            if (error != nullptr) *error = settings.lastError().text();
            return false;
        }
        while (settings.next()) {
            QVariant value;
            if (!deserializeVariant(settings.value(1).toByteArray(), &value)) {
                database_.rollback();
                if (error != nullptr) *error = QStringLiteral("Persisted profile setting is corrupt");
                return false;
            }
            currentSettings.insert(settings.value(0).toString(), value);
        }
    }
    bool didChange = !exists;
    for (auto it = settingsPatch.cbegin(); it != settingsPatch.cend(); ++it) {
        if (!currentSettings.contains(it.key()) || currentSettings.value(it.key()) != it.value()) {
            didChange = true;
            break;
        }
    }
    if (!didChange) {
        finish(OperationResultCode::Ok, QStringLiteral("profiles.profile.unchanged"),
               QStringLiteral("Profile settings already have the requested values"), currentRevision);
    } else {
        if (!eventSequenceAvailable) {
            database_.rollback();
            if (error != nullptr) *error = QStringLiteral("Event sequence is exhausted");
            return false;
        }
        if (currentRevision >= static_cast<quint64>(std::numeric_limits<qlonglong>::max())) {
            database_.rollback();
            if (error != nullptr) *error = QStringLiteral("Profile revision is exhausted");
            return false;
        }
        const quint64 resultRevision = currentRevision + 1;
        if (exists) {
            QSqlQuery update(database_);
            update.prepare(QStringLiteral("UPDATE profiles SET revision=:revision "
                                          "WHERE subject_kind=:kind AND subject_id=:id AND revision=:expected"));
            update.bindValue(QStringLiteral(":revision"), static_cast<qlonglong>(resultRevision));
            update.bindValue(QStringLiteral(":kind"), subjectKind);
            update.bindValue(QStringLiteral(":id"), subjectId);
            update.bindValue(QStringLiteral(":expected"), static_cast<qlonglong>(expectedRevision));
            if (!update.exec() || update.numRowsAffected() != 1) {
                database_.rollback();
                if (error != nullptr) *error = update.lastError().text();
                return false;
            }
        } else {
            QSqlQuery insert(database_);
            insert.prepare(QStringLiteral("INSERT INTO profiles(subject_kind, subject_id, profile_id, revision) "
                                          "VALUES (:kind, :id, :profile_id, :revision)"));
            insert.bindValue(QStringLiteral(":kind"), subjectKind);
            insert.bindValue(QStringLiteral(":id"), subjectId);
            insert.bindValue(QStringLiteral(":profile_id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
            insert.bindValue(QStringLiteral(":revision"), static_cast<qlonglong>(resultRevision));
            if (!insert.exec()) {
                database_.rollback();
                if (error != nullptr) *error = insert.lastError().text();
                return false;
            }
        }
        QSqlQuery upsert(database_);
        upsert.prepare(QStringLiteral("INSERT INTO profile_settings(subject_kind, subject_id, setting_key, value) "
                                      "VALUES (:kind, :id, :key, :value) "
                                      "ON CONFLICT(subject_kind, subject_id, setting_key) DO UPDATE SET value=excluded.value"));
        for (auto it = settingsPatch.cbegin(); it != settingsPatch.cend(); ++it) {
            const QByteArray valueBytes = serializeVariant(it.value());
            if (valueBytes.isEmpty()) {
                database_.rollback();
                if (error != nullptr) *error = QStringLiteral("Could not serialize profile setting");
                return false;
            }
            upsert.bindValue(QStringLiteral(":kind"), subjectKind);
            upsert.bindValue(QStringLiteral(":id"), subjectId);
            upsert.bindValue(QStringLiteral(":key"), it.key());
            upsert.bindValue(QStringLiteral(":value"), valueBytes);
            if (!upsert.exec()) {
                database_.rollback();
                if (error != nullptr) *error = upsert.lastError().text();
                return false;
            }
        }
        finish(OperationResultCode::Ok, QStringLiteral("profiles.profile.updated"),
               QStringLiteral("Profile settings were updated"), resultRevision);
    }

    return persistOperationResult(didChange);
}

quint64 SessionDatabase::generation() const
{
    return generation_;
}
