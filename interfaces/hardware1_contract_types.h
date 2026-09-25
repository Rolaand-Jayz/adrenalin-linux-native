#pragma once

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QList>
#include <QString>
#include <QStringList>

namespace adrenalin::contracts::hardware1 {

struct Reply {
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
    quint64 inventoryGeneration = 0;
    quint64 capabilityGeneration = 0;
    quint64 eventSequence = 0;

    bool isValid(QString *error = nullptr) const;
};

struct Device {
    QString subjectKind;
    QString subjectId;
    QString identityEvidence;
    QString displayName;
};

struct DeviceInfo {
    QString subjectKind;
    QString subjectId;
    QString identityEvidence;
    QString displayName;
    QString manufacturer;
    QString model;
    QString driverName;
    QString driverVersion;
    QString pciAddress;
    QString connectorIdentity;
    QString edidIdentityDigest;
};

struct Value {
    QString kind = QStringLiteral("NONE");
    bool booleanValue = false;
    qint64 signedValue = 0;
    quint64 unsignedValue = 0;
    double realValue = 0.0;
    QString enumValue;

    bool isValid(QString *error = nullptr) const;
};

struct Capability {
    QString subjectKind;
    QString subjectId;
    QString capabilityId;
    QString supportState;
    QString providerId;
    QString evidenceCode;
    QString failureCode;
    QString unit;
    Value configuredValue;
    Value effectiveValue;
    Value minimum;
    Value maximum;
    Value step;
    QStringList allowedValues;

    bool isValid(QString *error = nullptr) const;
};

bool isValidSubjectKind(const QString &kind);
void registerMetaTypes();

QDBusArgument &operator<<(QDBusArgument &argument, const Reply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument, Reply &reply);
QDBusArgument &operator<<(QDBusArgument &argument, const Device &device);
const QDBusArgument &operator>>(const QDBusArgument &argument, Device &device);
QDBusArgument &operator<<(QDBusArgument &argument, const DeviceInfo &info);
const QDBusArgument &operator>>(const QDBusArgument &argument, DeviceInfo &info);
QDBusArgument &operator<<(QDBusArgument &argument, const Value &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, Value &value);
QDBusArgument &operator<<(QDBusArgument &argument, const Capability &capability);
const QDBusArgument &operator>>(const QDBusArgument &argument, Capability &capability);

} // namespace adrenalin::contracts::hardware1

Q_DECLARE_METATYPE(adrenalin::contracts::hardware1::Reply)
Q_DECLARE_METATYPE(adrenalin::contracts::hardware1::Device)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::hardware1::Device>)
Q_DECLARE_METATYPE(adrenalin::contracts::hardware1::DeviceInfo)
Q_DECLARE_METATYPE(adrenalin::contracts::hardware1::Value)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::hardware1::Capability>)
