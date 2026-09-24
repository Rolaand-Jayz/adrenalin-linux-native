#include "application_config.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromUtf8(ADRENALIN_APP_NAME));
    QCoreApplication::setOrganizationName(QString::fromUtf8(ADRENALIN_APP_ORGANIZATION));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(ADRENALIN_APP_VERSION));
    QGuiApplication::setDesktopFileName(QString::fromUtf8(ADRENALIN_APP_ID));

    const bool smokeMode = app.arguments().contains(QStringLiteral("--smoke"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("smokeMode"), smokeMode);
    engine.rootContext()->setContextProperty(
        QStringLiteral("appDisplayName"), QString::fromUtf8(ADRENALIN_APP_NAME));
    engine.loadFromModule(QStringLiteral("Adrenalin.Shell"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (smokeMode) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    return app.exec();
}
