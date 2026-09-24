#include <settings1_adaptor.h>

#include "session_service.h"

Settings1Adaptor::Settings1Adaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

Settings1Adaptor::~Settings1Adaptor() = default;

ushort Settings1Adaptor::apiMajor() const
{
    return qobject_cast<SessionService *>(parent())->apiMajor();
}

ushort Settings1Adaptor::apiMinor() const
{
    return qobject_cast<SessionService *>(parent())->apiMinor();
}

QString Settings1Adaptor::initializationState() const
{
    return qobject_cast<SessionService *>(parent())->initializationState();
}

QString Settings1Adaptor::lastInitializationError() const
{
    return qobject_cast<SessionService *>(parent())->lastInitializationError();
}

qulonglong Settings1Adaptor::serviceGeneration() const
{
    return qobject_cast<SessionService *>(parent())->serviceGeneration();
}

QString Settings1Adaptor::serviceInstanceUuid() const
{
    return qobject_cast<SessionService *>(parent())->serviceInstanceUuid();
}

QString Settings1Adaptor::GetProductTelemetryConsent(bool &enabled, qulonglong &revision)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        enabled = false;
        revision = 0;
        return QStringLiteral("SERVICE_UNAVAILABLE");
    }
    const Settings1ReadResult result = service->getProductTelemetryConsent();
    enabled = result.enabled;
    revision = result.revision;
    return result.resultCode;
}

QString Settings1Adaptor::SetProductTelemetryConsent(const QString &operationId, bool enabled,
                                                      qulonglong expectedRevision,
                                                      qulonglong &newRevision)
{
    auto *service = qobject_cast<SessionService *>(parent());
    if (service == nullptr) {
        newRevision = expectedRevision;
        return QStringLiteral("SERVICE_UNAVAILABLE");
    }
    const Settings1WriteResult result = service->setProductTelemetryConsent(
        operationId, enabled, expectedRevision);
    newRevision = result.revision;
    return result.resultCode;
}
