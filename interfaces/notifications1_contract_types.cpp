#include "notifications1_contract_types.h"
#include "operation_result.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

namespace adrenalin::contracts::notifications1 {
namespace {

const QStringList kCategories{
    QStringLiteral("FEATURE_RECOMMENDATION"),
    QStringLiteral("SETTING_APPLIED"),
    QStringLiteral("APPLICATION_UPDATE_AVAILABLE"),
    QStringLiteral("GRAPHICS_STACK_UPDATE_AVAILABLE"),
    QStringLiteral("CAPTURE_STATUS"),
    QStringLiteral("STREAM_STATUS"),
    QStringLiteral("TUNING_WARNING"),
    QStringLiteral("TUNING_RESET"),
    QStringLiteral("HARDWARE_CAPABILITY_CHANGE"),
    QStringLiteral("GAME_DETECTED"),
    QStringLiteral("ERROR"),
};

bool validMessageKey(const QString &key)
{
    static const QRegularExpression syntax(QStringLiteral("^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+$"));
    return key.size() <= 128 && syntax.match(key).hasMatch();
}

bool fail(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

} // namespace

QStringList categories() { return kCategories; }

bool isValidCategory(const QString &category)
{
    return kCategories.contains(category);
}

bool Notification::isValid(QString *error) const
{
    static const QRegularExpression idSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!idSyntax.match(notificationId).hasMatch()) {
        return fail(error, QStringLiteral("Notification ID is outside the stable token syntax"));
    }
    if (!isValidCategory(category)) {
        return fail(error, QStringLiteral("Notification category is outside the audited v1 taxonomy"));
    }
    const QDateTime timestamp = QDateTime::fromString(createdAtUtc, Qt::ISODateWithMs);
    if (!timestamp.isValid() || !createdAtUtc.endsWith(QLatin1Char('Z'))) {
        return fail(error, QStringLiteral("Notification timestamp must be a valid UTC ISO-8601 value"));
    }
    if (!validMessageKey(titleMessageKey) || !validMessageKey(bodyMessageKey)) {
        return fail(error, QStringLiteral("Notification message keys are invalid"));
    }
    return true;
}

bool ListReply::isValid(QString *error) const
{
    if (!adrenalin::contracts::operationResultCodeFromName(code).has_value()) {
        return fail(error, QStringLiteral("Notification result code is outside the canonical result vocabulary"));
    }
    if (code != QLatin1String("OK")) {
        if (!notifications.isEmpty() || !serviceInstanceUuid.isEmpty() || serviceGeneration != 0
            || eventSequence != 0 || revision != 0) {
            return fail(error, QStringLiteral(
                "Failed notification snapshots must not carry records or a success cursor"));
        }
        return true;
    }
    if (serviceInstanceUuid.isEmpty() || serviceGeneration == 0 || eventSequence == 0 || revision == 0) {
        return fail(error, QStringLiteral("Successful notification snapshots require a complete nonzero cursor"));
    }
    QSet<QString> seen;
    for (const Notification &notification : notifications) {
        if (!notification.isValid(error)) {
            return false;
        }
        if (seen.contains(notification.notificationId)) {
            return fail(error, QStringLiteral("Notification snapshot contains a duplicate stable ID"));
        }
        seen.insert(notification.notificationId);
    }
    return true;
}

void registerMetaTypes()
{
    qRegisterMetaType<Notification>();
    qRegisterMetaType<QList<Notification>>();
    qDBusRegisterMetaType<Notification>();
    qDBusRegisterMetaType<QList<Notification>>();
}

QDBusArgument &operator<<(QDBusArgument &argument, const Notification &notification)
{
    argument.beginStructure();
    argument << notification.notificationId << notification.category << notification.createdAtUtc
             << notification.titleMessageKey << notification.bodyMessageKey << notification.isRead
             << notification.critical << notification.toastEligible;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Notification &notification)
{
    argument.beginStructure();
    argument >> notification.notificationId >> notification.category >> notification.createdAtUtc
             >> notification.titleMessageKey >> notification.bodyMessageKey >> notification.isRead
             >> notification.critical >> notification.toastEligible;
    argument.endStructure();
    return argument;
}

} // namespace adrenalin::contracts::notifications1
