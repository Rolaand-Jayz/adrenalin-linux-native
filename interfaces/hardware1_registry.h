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
bool isProviderIdV1(const QString &id);
bool isEvidenceCodeV1(const QString &code);
const QStringList &unitRegistryV1();
bool isUnitV1(const QString &unit);
bool unitAppliesToV1(const QString &capabilityId, const QString &unit);
bool enumValueAppliesToV1(const QString &capabilityId, const QString &value);
const QStringList &providerIdRegistryV1();
const QStringList &evidenceCodeRegistryV1();

} // namespace adrenalin::contracts::hardware1
