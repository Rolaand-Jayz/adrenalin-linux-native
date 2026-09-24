#include "session_service.h"
#include "session_identity.h"

#include "settings1_adaptor.h"
#include "service1_adaptor.h"
#include "service1_property_notifications.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Adrenalin Linux"));
    QCoreApplication::setOrganizationName(QStringLiteral("Adrenalin Linux"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("org.adrenalinlinux"));

    const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDirectory.isEmpty()) {
        qCritical("Qt did not provide an XDG-aware application data location");
        return 2;
    }
    const QString databasePath = QDir(dataDirectory).filePath(QStringLiteral("session.sqlite3"));
    SessionService service(databasePath);
    auto *settings = new Settings1Adaptor(&service);
    auto *readiness = new Service1Adaptor(&service);
    installService1PropertyNotifications(&service);
    Q_UNUSED(settings);
    Q_UNUSED(readiness);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCritical("The user session bus is unavailable");
        return 2;
    }
    if (!bus.registerObject(QString::fromLatin1(adrenalin::session1::objectPath), &service,
                            QDBusConnection::ExportAdaptors)) {
        qCritical("Could not export the Session1 Settings1 D-Bus contract");
        return 2;
    }
    if (!bus.registerService(QString::fromLatin1(adrenalin::session1::serviceName))) {
        qCritical("Could not acquire the session service name on the session bus");
        return 2;
    }
    QTimer::singleShot(0, &service, [&service] {
        service.initialize();
    });
    return app.exec();
}
