#pragma once

#include "interfaces/notifications1_contract_types.h"

#include <QSqlDatabase>
#include <QString>
#include <QList>

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
                                       const adrenalin::contracts::notifications1::Notification &notification,
                                       quint64 *newRevision, quint64 *notificationRevision,
                                       bool *stale, bool *conflict, bool *replayed,
                                       bool *notificationInserted, QString *error);
    std::optional<QList<adrenalin::contracts::notifications1::Notification>> readNotifications(
        quint64 *revision, QString *error);
    bool markNotificationRead(const QString &notificationId, const QString &operationId,
                              quint64 expectedRevision, bool eventSequenceAvailable, quint64 *newRevision,
                              bool *changed, bool *stale, bool *conflict, bool *notFound,
                              bool *replayed, QString *error);
    quint64 generation() const;

private:
    bool execute(const QString &sql, QString *error);

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
    quint64 generation_ = 0;
};
