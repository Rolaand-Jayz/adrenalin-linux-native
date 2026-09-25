#include "display1_adaptor.h"

#include "session_service.h"

Display1Adaptor::Display1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Display1Adaptor::~Display1Adaptor() = default;

adrenalin::contracts::display1::ListReply Display1Adaptor::ListDisplays()
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->listDisplays();
}

adrenalin::contracts::display1::StateReply Display1Adaptor::GetDisplayState(
    const QString &subject_id)
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->getDisplayState(subject_id);
}

adrenalin::contracts::display1::ValidationReply Display1Adaptor::ValidateDisplay(
    const QString &operation_id, const QString &subject_id,
    qulonglong expected_inventory_generation, qulonglong expected_capability_generation,
    const QList<adrenalin::contracts::display1::ControlChange> &changes)
{
    const auto *service = static_cast<const SessionService *>(parent());
    return service->validateDisplay(operation_id, subject_id, expected_inventory_generation,
                                    expected_capability_generation, changes);
}

adrenalin::contracts::display1::ApplyReply Display1Adaptor::ApplyDisplay(
    const QString &operation_id, const QString &subject_id,
    qulonglong expected_inventory_generation, qulonglong expected_capability_generation,
    const QList<adrenalin::contracts::display1::ControlChange> &changes)
{
    auto *service = static_cast<SessionService *>(parent());
    return service->applyDisplay(operation_id, subject_id, expected_inventory_generation,
                                 expected_capability_generation, changes);
}
