#include "hardware1_mock.h"

#include <QSet>

namespace adrenalin::contracts::hardware1 {
namespace {

constexpr auto kServiceInstanceUuid = "123e4567-e89b-12d3-a456-426614174000";
constexpr auto kCpuSubjectId = "cpu-package-test-0";
constexpr auto kGpuSubjectId = "gpu-pci-test-0";
constexpr auto kDisplaySubjectId = "display-test-0";

Reply makeReply(const QString &subjectKind, const QString &subjectId)
{
    Reply reply;
    reply.code = QStringLiteral("OK");
    reply.humanMessageKey = QStringLiteral("hardware.read.ok");
    reply.subjectKind = subjectKind;
    reply.subjectId = subjectId;
    reply.snapshotValid = true;
    reply.serviceInstanceUuid = QString::fromLatin1(kServiceInstanceUuid);
    reply.serviceGeneration = 2;
    reply.inventoryGeneration = 5;
    reply.capabilityGeneration = 9;
    reply.eventSequence = 17;
    return reply;
}

bool isKnownSubject(const QString &kind, const QString &id)
{
    return (kind == QLatin1String("CPU_PACKAGE") && id == QLatin1String(kCpuSubjectId))
        || (kind == QLatin1String("GPU_PCI") && id == QLatin1String(kGpuSubjectId))
        || (kind == QLatin1String("DISPLAY") && id == QLatin1String(kDisplaySubjectId))
        || (kind == QLatin1String("PLATFORM") && id == QLatin1String("platform"));
}

Reply missingSubjectReply(const QString &kind, const QString &id)
{
    Reply reply = makeReply(kind, id);
    reply.code = QStringLiteral("NOT_FOUND");
    reply.humanMessageKey = QStringLiteral("hardware.subject.notFound");
    reply.diagnosticMessage = QStringLiteral("The requested subject is absent from the mock snapshot");
    return reply;
}

Reply invalidArgumentReply(const QString &kind, const QString &id)
{
    Reply reply = makeReply(kind, id);
    reply.code = QStringLiteral("INVALID_ARGUMENT");
    reply.humanMessageKey = QStringLiteral("operation.invalidArgument");
    reply.diagnosticMessage = QStringLiteral("A required output pointer is null");
    return reply;
}

Value unsignedValue(quint64 value)
{
    Value result;
    result.kind = QStringLiteral("UNSIGNED_INTEGER");
    result.unsignedValue = value;
    return result;
}

} // namespace

QString Mock::testGpuSubjectId()
{
    return QString::fromLatin1(kGpuSubjectId);
}

Reply Mock::listDevices(QList<Device> *devices) const
{
    if (devices) {
        *devices = {};
    }
    if (!devices) {
        Reply reply = makeReply(QStringLiteral("PLATFORM"), QStringLiteral("platform"));
        reply.code = QStringLiteral("INVALID_ARGUMENT");
        reply.humanMessageKey = QStringLiteral("operation.invalidArgument");
        reply.diagnosticMessage = QStringLiteral("The device output pointer is null");
        return reply;
    }
    *devices = {
        {QStringLiteral("CPU_PACKAGE"), QString::fromLatin1(kCpuSubjectId),
         QStringLiteral("test.cpu.package"), QStringLiteral("Test CPU Package")},
        {QStringLiteral("GPU_PCI"), QString::fromLatin1(kGpuSubjectId),
         QStringLiteral("test.gpu.pci"), QStringLiteral("Test Radeon GPU")},
        {QStringLiteral("DISPLAY"), QString::fromLatin1(kDisplaySubjectId),
         QStringLiteral("test.display.edid"), QStringLiteral("Test Display")},
    };
    return makeReply(QStringLiteral("PLATFORM"), QStringLiteral("platform"));
}

Reply Mock::getDeviceInfo(const QString &subjectKind, const QString &subjectId,
                          DeviceInfo *info) const
{
    if (!info) {
        return invalidArgumentReply(subjectKind, subjectId);
    }
    *info = {};
    if (!isKnownSubject(subjectKind, subjectId)) {
        return missingSubjectReply(subjectKind, subjectId);
    }
    info->subjectKind = subjectKind;
    info->subjectId = subjectId;
    if (subjectKind == QLatin1String("GPU_PCI")) {
        info->identityEvidence = QStringLiteral("test.gpu.pci");
        info->displayName = QStringLiteral("Test Radeon GPU");
        info->manufacturer = QStringLiteral("Test Vendor");
        info->model = QStringLiteral("Test Graphics Device");
        info->driverName = QStringLiteral("test.amdgpu");
        info->driverVersion = QStringLiteral("test-version");
        info->pciAddress = QStringLiteral("0000:03:00.0");
    } else if (subjectKind == QLatin1String("DISPLAY")) {
        info->identityEvidence = QStringLiteral("test.display.edid");
        info->displayName = QStringLiteral("Test Display");
        info->manufacturer = QStringLiteral("Test Vendor");
        info->model = QStringLiteral("Test Monitor");
        info->connectorIdentity = QStringLiteral("test-connector");
        info->edidIdentityDigest = QStringLiteral("test-edid-digest");
    } else if (subjectKind == QLatin1String("CPU_PACKAGE")) {
        info->identityEvidence = QStringLiteral("test.cpu.package");
        info->displayName = QStringLiteral("Test CPU Package");
        info->manufacturer = QStringLiteral("Test Vendor");
        info->model = QStringLiteral("Test Processor");
    } else {
        info->subjectKind = QStringLiteral("PLATFORM");
        info->identityEvidence = QStringLiteral("test.platform");
        info->displayName = QStringLiteral("Test Platform");
    }
    return makeReply(subjectKind, subjectId);
}

Reply Mock::getCapabilityGraph(const QString &subjectKind, const QString &subjectId,
                               QList<Capability> *capabilities) const
{
    if (!capabilities) {
        return invalidArgumentReply(subjectKind, subjectId);
    }
    *capabilities = {};
    if (!isKnownSubject(subjectKind, subjectId)) {
        return missingSubjectReply(subjectKind, subjectId);
    }
    if (subjectKind == QLatin1String("GPU_PCI") && subjectId == QLatin1String(kGpuSubjectId)) {
        Capability frequency;
        frequency.subjectKind = subjectKind;
        frequency.subjectId = subjectId;
        frequency.capabilityId = QStringLiteral("gpu.clock.maximum");
        frequency.supportState = QStringLiteral("SUPPORTED");
        frequency.providerId = QStringLiteral("test.provider.gpu");
        frequency.evidenceCode = QStringLiteral("test.observed.range");
        frequency.unit = QStringLiteral("megahertz");
        frequency.configuredValue = unsignedValue(2400);
        frequency.effectiveValue = unsignedValue(2250);
        frequency.minimum = unsignedValue(500);
        frequency.maximum = unsignedValue(3000);
        frequency.step = unsignedValue(1);
        Q_ASSERT(frequency.isValid());

        Capability fanControl;
        fanControl.subjectKind = subjectKind;
        fanControl.subjectId = subjectId;
        fanControl.capabilityId = QStringLiteral("gpu.fan.control");
        fanControl.supportState = QStringLiteral("UNKNOWN");
        Q_ASSERT(fanControl.isValid());

        Capability manualVoltage;
        manualVoltage.subjectKind = subjectKind;
        manualVoltage.subjectId = subjectId;
        manualVoltage.capabilityId = QStringLiteral("gpu.voltage.manual");
        manualVoltage.supportState = QStringLiteral("UNSUPPORTED");
        manualVoltage.evidenceCode = QStringLiteral("test.provider.no-control");
        Q_ASSERT(manualVoltage.isValid());

        Capability temperature;
        temperature.subjectKind = subjectKind;
        temperature.subjectId = subjectId;
        temperature.capabilityId = QStringLiteral("gpu.metric.edge_temperature");
        temperature.supportState = QStringLiteral("PROVIDER_UNAVAILABLE");
        temperature.providerId = QStringLiteral("test.provider.telemetry");
        temperature.failureCode = QStringLiteral("BACKEND_UNAVAILABLE");
        Q_ASSERT(temperature.isValid());
        *capabilities = {frequency, fanControl, manualVoltage, temperature};
    }
    return makeReply(subjectKind, subjectId);
}

} // namespace adrenalin::contracts::hardware1
