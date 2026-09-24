#pragma once

#include <QSqlDatabase>
#include <QString>

#include <optional>

struct ProductTelemetryConsent {
    bool enabled = false;
    quint64 revision = 0;
};

class SessionDatabase final
{
public:
    explicit SessionDatabase(QString databasePath);
    ~SessionDatabase();

    SessionDatabase(const SessionDatabase &) = delete;
    SessionDatabase &operator=(const SessionDatabase &) = delete;

    bool initialize(QString *error);
    std::optional<ProductTelemetryConsent> readProductTelemetryConsent(QString *error);
    bool updateProductTelemetryConsent(const QString &operationId, bool enabled, quint64 expectedRevision,
                                       quint64 *newRevision, bool *stale,
                                       bool *conflict,
                                       bool *replayed,
                                       QString *error);
    quint64 generation() const;

private:
    bool execute(const QString &sql, QString *error);

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
    quint64 generation_ = 0;
};
