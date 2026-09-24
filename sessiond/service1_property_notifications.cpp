#include "service1_property_notifications.h"

#include "session_identity.h"
#include "session_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>

void installService1PropertyNotifications(SessionService *service)
{
    if (service == nullptr) {
        return;
    }

    QObject::connect(service, &SessionService::initializationStateChanged, service, [service] {
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        QVariantMap changedProperties{
            {QStringLiteral("InitializationState"), service->initializationState()},
            {QStringLiteral("ServiceGeneration"), QVariant::fromValue(service->serviceGeneration())},
            {QStringLiteral("LastInitializationError"), service->lastInitializationError()}
        };
        signal << QStringLiteral("org.adrenalinlinux.Session1.Service1") << changedProperties
               << QStringList{};
        QDBusConnection::sessionBus().send(signal);
    });
}
