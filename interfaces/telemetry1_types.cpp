#include "telemetry1_types.h"

namespace adrenalin::contracts::telemetry1 {
QDBusArgument &operator<<(QDBusArgument &a, const OpenResult &v) {
    a.beginStructure();
    a << v.code << v.diagnostic << v.serviceInstanceUuid << v.serviceGeneration
      << v.producerGeneration << v.metricDefinitionGeneration
      << v.subjectDefinitionGeneration << v.abiMajor << v.mappedSize;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, OpenResult &v) {
    a.beginStructure();
    a >> v.code >> v.diagnostic >> v.serviceInstanceUuid >> v.serviceGeneration
      >> v.producerGeneration >> v.metricDefinitionGeneration
      >> v.subjectDefinitionGeneration >> v.abiMajor >> v.mappedSize;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const MetricDefinition &v) {
    a.beginStructure();
    a << v.metricId << v.valueKind << v.unit << v.stateSemantics;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, MetricDefinition &v) {
    a.beginStructure();
    a >> v.metricId >> v.valueKind >> v.unit >> v.stateSemantics;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const SubjectDefinition &v) {
    a.beginStructure();
    a << v.subjectKind << v.subjectId << v.displayLabel << v.identityScheme;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, SubjectDefinition &v) {
    a.beginStructure();
    a >> v.subjectKind >> v.subjectId >> v.displayLabel >> v.identityScheme;
    a.endStructure();
    return a;
}
void registerMetaTypes() {
    qDBusRegisterMetaType<OpenResult>();
    qDBusRegisterMetaType<MetricDefinition>();
    qDBusRegisterMetaType<QList<MetricDefinition>>();
    qDBusRegisterMetaType<SubjectDefinition>();
    qDBusRegisterMetaType<QList<SubjectDefinition>>();
}
}
