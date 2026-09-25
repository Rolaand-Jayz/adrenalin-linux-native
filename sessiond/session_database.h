#pragma once

#include "interfaces/notifications1_contract_types.h"
#include "interfaces/profiles1_contract_types.h"
#include "interfaces/operation_result.h"

#include <QSqlDatabase>
#include <QString>
#include <QList>

#include <optional>

struct ProductTelemetryConsent {
    bool enabled = false;
    quint64 revision = 0;
};

struct BooleanPreferenceSnapshot {
    bool configured = false;
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
    std::optional<BooleanPreferenceSnapshot> readToastNotifications(QString *error);
    bool updateToastNotifications(const QString &operationId, bool enabled,
                                 quint64 expectedRevision, bool eventSequenceAvailable,
                                 quint64 *newRevision, bool *stale, bool *conflict,
                                 bool *replayed, bool *changed, QString *error);
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
    std::optional<adrenalin::contracts::profiles1::Profile> readProfile(
        const QString &subjectKind, const QString &subjectId, bool *notFound, QString *error);
    bool updateProfile(const QString &subjectKind, const QString &subjectId, quint64 expectedRevision,
                       const QString &operationId, const QVariantMap &settingsPatch,
                       bool eventSequenceAvailable,
                       adrenalin::contracts::MutationResult *mutation, bool *changed,
                       bool *stale, bool *conflict, bool *notFound, bool *replayed, QString *error);
    quint64 generation() const;

private:
    bool execute(const QString &sql, QString *error);

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
    quint64 generation_ = 0;
};
