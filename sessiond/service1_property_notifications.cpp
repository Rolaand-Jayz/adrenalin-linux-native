#include "service1_property_notifications.h"

#include "session_identity.h"
#include "session_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>

SessionServiceRootAdaptor::SessionServiceRootAdaptor(QObject *parent)
    : Service1Adaptor(parent)
{
    setAutoRelaySignals(false);
}

void installService1PropertyNotifications(SessionService *service)
{
    if (service == nullptr) {
        return;
    }

    QObject::connect(service, &SessionService::eventPublished, service, [service] {
        QDBusMessage eventSignal = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.adrenalinlinux.Session1.Service1"),
            QStringLiteral("EventPublished"));
        eventSignal << service->serviceInstanceUuid() << service->serviceGeneration()
                    << service->eventSequence() << service->eventSubjectKind()
                    << service->eventSubjectId();
        QDBusConnection::sessionBus().send(eventSignal);

        QDBusMessage propertySignal = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        propertySignal << QStringLiteral("org.adrenalinlinux.Session1.Service1")
                       << QVariantMap{{QStringLiteral("EventSequence"),
                                       QVariant::fromValue(service->eventSequence())}}
                       << QStringList{};
        QDBusConnection::sessionBus().send(propertySignal);
    });

    auto lastState = service->initializationState();
    auto lastGeneration = service->serviceGeneration();
    auto lastError = service->lastInitializationError();
    QObject::connect(service, &SessionService::initializationStateChanged, service,
                     [service, lastState, lastGeneration, lastError]() mutable {
        QVariantMap changedProperties;
        const QString state = service->initializationState();
        const qulonglong generation = service->serviceGeneration();
        const QString error = service->lastInitializationError();
        if (state != lastState) {
            changedProperties.insert(QStringLiteral("InitializationState"), state);
            lastState = state;
        }
        if (generation != lastGeneration) {
            changedProperties.insert(QStringLiteral("ServiceGeneration"),
                                     QVariant::fromValue(generation));
            lastGeneration = generation;
        }
        if (error != lastError) {
            changedProperties.insert(QStringLiteral("LastInitializationError"), error);
            lastError = error;
        }
        if (changedProperties.isEmpty()) {
            return;
        }
        QDBusMessage signal = QDBusMessage::createSignal(
            QString::fromLatin1(adrenalin::session1::objectPath),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.adrenalinlinux.Session1.Service1") << changedProperties
               << QStringList{};
        QDBusConnection::sessionBus().send(signal);
    });
}
