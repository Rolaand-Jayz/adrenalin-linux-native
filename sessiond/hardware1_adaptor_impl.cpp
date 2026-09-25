#include "hardware1_adaptor.h"

#include "sessiond/session_service.h"

Hardware1Adaptor::Hardware1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Hardware1Adaptor::~Hardware1Adaptor() = default;

adrenalin::contracts::hardware1::Reply Hardware1Adaptor::ListDevices(
    QList<adrenalin::contracts::hardware1::Device> &devices)
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->listHardwareDevices(&devices);
}

adrenalin::contracts::hardware1::Reply Hardware1Adaptor::GetDeviceInfo(
    const QString &subject_kind, const QString &subject_id,
    adrenalin::contracts::hardware1::DeviceInfo &device_info)
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->getHardwareDeviceInfo(subject_kind, subject_id, &device_info);
}

adrenalin::contracts::hardware1::Reply Hardware1Adaptor::GetCapabilityGraph(
    const QString &subject_kind, const QString &subject_id,
    QList<adrenalin::contracts::hardware1::Capability> &capabilities)
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->getHardwareCapabilityGraph(subject_kind, subject_id, &capabilities);
}
