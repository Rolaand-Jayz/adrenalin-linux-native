#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace adrenalin::contracts::hardware1 {

// Implementation-owned v1 identifiers. This vocabulary does not assert that
// a provider exists or that a capability is supported on any machine.
struct CapabilityDefinition {
    QString id;
    QStringList subjectKinds;
};

const QList<CapabilityDefinition> &capabilityRegistryV1();
const CapabilityDefinition *findCapabilityV1(const QString &id);
bool capabilityAppliesToV1(const QString &id, const QString &subjectKind);
QStringList subjectKindsForCapabilityV1(const QString &id);

} // namespace adrenalin::contracts::hardware1
