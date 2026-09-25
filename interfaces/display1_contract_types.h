#pragma once

#include "hardware1_contract_types.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QList>
#include <QString>
#include <QStringList>

namespace adrenalin::contracts::display1 {

using Display = hardware1::Device;
using Capability = hardware1::Capability;

struct ControlChange final {
    QString capabilityId;
    hardware1::Value value;

    bool isValid(QString *error = nullptr) const;
};

struct ListReply final {
    hardware1::Reply snapshot;
    QList<Display> displays;

    bool isValid(QString *error = nullptr) const;
};

struct StateReply final {
    hardware1::Reply snapshot;
    Display display;
    QList<Capability> capabilities;

    bool isValid(QString *error = nullptr) const;
};

struct ValidationReply final {
    QString code;
    QString operationId;
    QString humanMessageKey;
    QString diagnosticMessage;
    bool retryable = false;
    QString provider;
    QString subjectId;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    bool snapshotValid = true;
    quint64 inventoryGeneration = 0;
    quint64 capabilityGeneration = 0;
    bool valid = false;
    QString safetyClass;
    QStringList validationMessageKeys;

    bool isValid(QString *error = nullptr) const;
};

struct ApplyReply final {
    QString code;
    QString operationId;
    QString humanMessageKey;
    QString diagnosticMessage;
    bool retryable = false;
    QString provider;
    QString subjectId;
    quint64 revision = 0;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    bool snapshotValid = true;
    quint64 inventoryGeneration = 0;
    quint64 capabilityGeneration = 0;
    QString safetyRouteIntent;
    bool effectiveStateVerified = false;

    bool isValid(QString *error = nullptr) const;
};

void registerMetaTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const ControlChange &change);
const QDBusArgument &operator>>(const QDBusArgument &argument, ControlChange &change);
QDBusArgument &operator<<(QDBusArgument &argument, const ListReply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, ListReply &reply);
QDBusArgument &operator<<(QDBusArgument &argument, const StateReply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, StateReply &reply);
QDBusArgument &operator<<(QDBusArgument &argument, const ValidationReply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, ValidationReply &reply);
QDBusArgument &operator<<(QDBusArgument &argument, const ApplyReply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, ApplyReply &reply);

} // namespace adrenalin::contracts::display1

Q_DECLARE_METATYPE(adrenalin::contracts::display1::ControlChange)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::display1::ControlChange>)
Q_DECLARE_METATYPE(adrenalin::contracts::display1::ListReply)
Q_DECLARE_METATYPE(adrenalin::contracts::display1::StateReply)
Q_DECLARE_METATYPE(adrenalin::contracts::display1::ValidationReply)
Q_DECLARE_METATYPE(adrenalin::contracts::display1::ApplyReply)
