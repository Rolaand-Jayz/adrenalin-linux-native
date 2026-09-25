#pragma once

#include "operation_result.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QList>
#include <QString>
#include <QStringList>

namespace adrenalin::contracts::hotkeys1 {

struct Action final {
    QString actionId;
    QString labelMessageKey;
    QString descriptionMessageKey;
    QString configuredBinding;
    QString effectiveBinding;
    QString registrationState;
    QString provider;
    QString errorCode;

    bool isValid(QString *error = nullptr) const;
};

struct Reply final {
    QString code;
    QString humanMessageKey;
    QString diagnosticMessage;
    bool retryable = false;
    QString provider;
    QString subjectKind;
    QString subjectId;
    bool snapshotValid = false;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    quint64 revision = 0;

    bool isValid(QString *error = nullptr) const;
};

bool isValidActionId(const QString &actionId);
bool isValidBindingText(const QString &binding);
bool isValidParentWindowId(const QString &parentWindowId);
QStringList registrationStates();
bool isValidSnapshot(const Reply &reply, const QList<Action> &actions,
                     QString *error = nullptr);
void registerMetaTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Action &action);
const QDBusArgument &operator>>(const QDBusArgument &argument, Action &action);
QDBusArgument &operator<<(QDBusArgument &argument, const Reply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, Reply &reply);

} // namespace adrenalin::contracts::hotkeys1

Q_DECLARE_METATYPE(adrenalin::contracts::hotkeys1::Action)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::hotkeys1::Action>)
Q_DECLARE_METATYPE(adrenalin::contracts::hotkeys1::Reply)
