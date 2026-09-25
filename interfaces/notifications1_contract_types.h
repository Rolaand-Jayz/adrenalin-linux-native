#pragma once

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QList>
#include <QString>
#include <QStringList>

namespace adrenalin::contracts::notifications1 {

struct Notification final {
    QString notificationId;
    QString category;
    QString createdAtUtc;
    QString titleMessageKey;
    QString bodyMessageKey;
    bool isRead = false;
    bool critical = false;
    bool toastEligible = false;

    bool isValid(QString *error = nullptr) const;
};

struct ListReply final {
    QString code;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    quint64 revision = 0;
    QList<Notification> notifications;

    bool isValid(QString *error = nullptr) const;
};

bool isValidCategory(const QString &category);
QStringList categories();
void registerMetaTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Notification &notification);
const QDBusArgument &operator>>(const QDBusArgument &argument, Notification &notification);

} // namespace adrenalin::contracts::notifications1

Q_DECLARE_METATYPE(adrenalin::contracts::notifications1::Notification)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::notifications1::Notification>)
