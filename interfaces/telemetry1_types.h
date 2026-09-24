#pragma once

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusUnixFileDescriptor>
#include <QList>
#include <QString>

namespace adrenalin::contracts::telemetry1 {
struct OpenResult final {
    QString code;
    QString diagnostic;
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 producerGeneration = 0;
    quint64 metricDefinitionGeneration = 0;
    quint64 subjectDefinitionGeneration = 0;
    quint16 abiMajor = 0;
    quint64 mappedSize = 0;
};
struct MetricDefinition final {
    QString metricId;
    QString valueKind;
    QString unit;
    QString stateSemantics;
};
struct SubjectDefinition final {
    QString subjectKind;
    QString subjectId;
    QString displayLabel;
    QString identityScheme;
};
void registerMetaTypes();
QDBusArgument &operator<<(QDBusArgument &, const OpenResult &);
const QDBusArgument &operator>>(const QDBusArgument &, OpenResult &);
QDBusArgument &operator<<(QDBusArgument &, const MetricDefinition &);
const QDBusArgument &operator>>(const QDBusArgument &, MetricDefinition &);
QDBusArgument &operator<<(QDBusArgument &, const SubjectDefinition &);
const QDBusArgument &operator>>(const QDBusArgument &, SubjectDefinition &);
}
Q_DECLARE_METATYPE(adrenalin::contracts::telemetry1::OpenResult)
Q_DECLARE_METATYPE(adrenalin::contracts::telemetry1::MetricDefinition)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::telemetry1::MetricDefinition>)
Q_DECLARE_METATYPE(adrenalin::contracts::telemetry1::SubjectDefinition)
Q_DECLARE_METATYPE(QList<adrenalin::contracts::telemetry1::SubjectDefinition>)
