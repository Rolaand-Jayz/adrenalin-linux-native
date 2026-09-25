#include "application_config.h"
#include "interfaces/settings1_client.h"
#include "interfaces/notifications1_client.h"
#include "interfaces/toast_notifications_client.h"
#include "interfaces/hardware1_client.h"
#include "session_identity.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSaveFile>
#include <QTimer>
#include <QtMath>

#include <cmath>

namespace
{
struct CandidateCapture
{
    QString imagePath;
    QString geometryPath;
    QString screenId;
    QString captureId;
    QString fixtureId;
};

constexpr int kMinCaptureWidth = 960;
constexpr int kMinCaptureHeight = 640;
constexpr int kMaxCaptureWidth = 3840;
constexpr int kMaxCaptureHeight = 2160;
constexpr qint64 kMaxCapturePixels = qint64{kMaxCaptureWidth} * kMaxCaptureHeight;

bool writeRuntimeGeometry(QQuickWindow *window, const QImage &image,
                          const CandidateCapture &capture, QString *error)
{
    const QString executablePath = QCoreApplication::applicationFilePath();
    QFile executable(executablePath);
    if (!executable.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read running application build: %1").arg(executable.errorString());
        return false;
    }
    const QByteArray buildIdentity = QCryptographicHash::hash(executable.readAll(), QCryptographicHash::Sha256).toHex();
    if (buildIdentity.size() != 64) {
        *error = QStringLiteral("could not calculate running application build identity");
        return false;
    }

    const QStringList componentIds = {
        QStringLiteral("header"),
        QStringLiteral("brandMark"),
        QStringLiteral("applicationTitle"),
        QStringLiteral("platformLabel"),
        QStringLiteral("screenTitle"),
        QStringLiteral("screenDescription"),
        QStringLiteral("productTelemetryConsent"),
        QStringLiteral("telemetryPreferenceRow"),
        QStringLiteral("telemetryPreferenceLabel"),
        QStringLiteral("productTelemetryConsentSwitch"),
        QStringLiteral("serviceStatus")
    };
    QQuickItem *content = window->contentItem();
    if (content == nullptr || window->width() <= 0 || window->height() <= 0) {
        *error = QStringLiteral("rendered application window has no measurable content area");
        return false;
    }

    const qreal scaleX = static_cast<qreal>(image.width()) / window->width();
    const qreal scaleY = static_cast<qreal>(image.height()) / window->height();
    QJsonArray components;
    for (const QString &identity : componentIds) {
        QQuickItem *item = window->findChild<QQuickItem *>(identity);
        if (item == nullptr || !item->isVisible() || item->width() <= 0 || item->height() <= 0)
            continue;
        const QRectF logical = item->mapRectToItem(content, item->boundingRect());
        const int left = qFloor(logical.left() * scaleX);
        const int top = qFloor(logical.top() * scaleY);
        const int right = qCeil(logical.right() * scaleX);
        const int bottom = qCeil(logical.bottom() * scaleY);
        if (right <= left || bottom <= top || left < 0 || top < 0
            || right > image.width() || bottom > image.height())
            continue;
        QJsonObject component;
        component.insert(QStringLiteral("id"), identity);
        QJsonObject rect;
        rect.insert(QStringLiteral("x"), left);
        rect.insert(QStringLiteral("y"), top);
        rect.insert(QStringLiteral("width"), right - left);
        rect.insert(QStringLiteral("height"), bottom - top);
        component.insert(QStringLiteral("rect"), rect);
        components.append(component);
    }
    if (components.isEmpty()) {
        *error = QStringLiteral("the rendered QML tree exposed no visible named component rectangles");
        return false;
    }

    QJsonObject evidence;
    evidence.insert(QStringLiteral("schema_version"), 1);
    evidence.insert(QStringLiteral("source"), QStringLiteral("qt_qml_application_self_reported_runtime_geometry"));
    evidence.insert(QStringLiteral("build_identity"), QString::fromLatin1(buildIdentity));
    evidence.insert(QStringLiteral("fixture_id"), capture.fixtureId);
    evidence.insert(QStringLiteral("screen_id"), capture.screenId);
    evidence.insert(QStringLiteral("capture_id"), capture.captureId);
    evidence.insert(QStringLiteral("components"), components);

    const QFileInfo outputInfo(capture.geometryPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        *error = QStringLiteral("cannot create runtime-geometry output directory: %1").arg(outputInfo.absolutePath());
        return false;
    }
    QSaveFile output(capture.geometryPath);
    if (!output.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("cannot create runtime-geometry output: %1").arg(output.errorString());
        return false;
    }
    if (output.write(QJsonDocument(evidence).toJson(QJsonDocument::Indented)) < 0
        || !output.commit()) {
        *error = QStringLiteral("cannot write runtime-geometry output: %1").arg(output.errorString());
        return false;
    }
    return true;
}

bool captureRenderedWindow(QQuickWindow *window, const CandidateCapture &capture, QString *error)
{
    const qreal deviceScale = window->devicePixelRatio();
    const qreal expectedPhysicalWidth = std::ceil(window->width() * deviceScale);
    const qreal expectedPhysicalHeight = std::ceil(window->height() * deviceScale);
    if (!std::isfinite(deviceScale) || deviceScale <= 0.0
        || !std::isfinite(expectedPhysicalWidth) || !std::isfinite(expectedPhysicalHeight)
        || expectedPhysicalWidth > kMaxCaptureWidth || expectedPhysicalHeight > kMaxCaptureHeight
        || expectedPhysicalWidth * expectedPhysicalHeight > kMaxCapturePixels) {
        *error = QStringLiteral("The rendered window exceeds the ID-137 physical-pixel capture envelope.");
        return false;
    }

    const QImage image = window->grabWindow();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        *error = QStringLiteral("Qt returned no rendered window image; a working GUI render surface is required");
        return false;
    }
    if (image.width() > kMaxCaptureWidth || image.height() > kMaxCaptureHeight
        || qint64{image.width()} * image.height() > kMaxCapturePixels) {
        *error = QStringLiteral("Qt returned an image beyond the ID-137 physical-pixel capture envelope.");
        return false;
    }
    const QFileInfo outputInfo(capture.imagePath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        *error = QStringLiteral("cannot create candidate-image output directory: %1").arg(outputInfo.absolutePath());
        return false;
    }
    QSaveFile output(capture.imagePath);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit()) {
        *error = QStringLiteral("cannot write candidate PNG: %1").arg(output.errorString());
        return false;
    }
    if (!writeRuntimeGeometry(window, image, capture, error))
        return false;

    qInfo().noquote() << QStringLiteral("Rendered candidate evidence: %1x%2 physical pixels; PNG=%3; geometry=%4")
                             .arg(image.width()).arg(image.height()).arg(capture.imagePath, capture.geometryPath);
    return true;
}

bool parseCaptureSize(const QString &value, QSize *size)
{
    const QStringList parts = value.split(QLatin1Char('x'));
    if (parts.size() != 2)
        return false;
    bool widthOk = false;
    bool heightOk = false;
    const int width = parts.at(0).toInt(&widthOk);
    const int height = parts.at(1).toInt(&heightOk);
    if (!widthOk || !heightOk || width < kMinCaptureWidth || height < kMinCaptureHeight
        || width > kMaxCaptureWidth || height > kMaxCaptureHeight
        || qint64{width} * qint64{height} > kMaxCapturePixels)
        return false;
    *size = QSize(width, height);
    return true;
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromUtf8(ADRENALIN_APP_NAME));
    QCoreApplication::setOrganizationName(QString::fromUtf8(ADRENALIN_APP_ORGANIZATION));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(ADRENALIN_APP_VERSION));
    QGuiApplication::setDesktopFileName(QString::fromUtf8(ADRENALIN_APP_ID));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native Adrenalin Linux desktop shell"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringLiteral("smoke"), QStringLiteral("Load QML and exit after the first event-loop turn.")));
    parser.addOption(QCommandLineOption(QStringLiteral("capture-candidate"), QStringLiteral("Write a PNG of the rendered Qt/QML window."), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("candidate-geometry"), QStringLiteral("Write measured runtime geometry JSON."), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("screen-id"), QStringLiteral("Reference manifest screen identity for this capture."), QStringLiteral("id")));
    parser.addOption(QCommandLineOption(QStringLiteral("capture-id"), QStringLiteral("Reference manifest capture/state identity for this capture."), QStringLiteral("id")));
    parser.addOption(QCommandLineOption(QStringLiteral("fixture-id"), QStringLiteral("Caller-supplied identity of the displayed data/state fixture."), QStringLiteral("id")));
    parser.addOption(QCommandLineOption(QStringLiteral("capture-size"), QStringLiteral("Requested logical client size, WIDTHxHEIGHT; output reports physical PNG pixels."), QStringLiteral("size")));
    parser.process(app);

    const bool smokeMode = parser.isSet(QStringLiteral("smoke"));
    const bool captureSizeSet = parser.isSet(QStringLiteral("capture-size"));
    QSize requestedCaptureSize;
    if (captureSizeSet && !parseCaptureSize(parser.value(QStringLiteral("capture-size")), &requestedCaptureSize)) {
        qCritical("--capture-size must be between 960x640 and 3840x2160 logical pixels and within the ID-137 capture pixel envelope.");
        return 2;
    }
    if (smokeMode && captureSizeSet) {
        qCritical("--capture-size cannot be combined with --smoke.");
        return 2;
    }

    const bool anyCaptureArgument = parser.isSet(QStringLiteral("capture-candidate"))
        || parser.isSet(QStringLiteral("candidate-geometry")) || parser.isSet(QStringLiteral("screen-id"))
        || parser.isSet(QStringLiteral("capture-id")) || parser.isSet(QStringLiteral("fixture-id"))
        || captureSizeSet;
    CandidateCapture capture;
    const bool captureMode = parser.isSet(QStringLiteral("capture-candidate"))
        && parser.isSet(QStringLiteral("candidate-geometry")) && parser.isSet(QStringLiteral("screen-id"))
        && parser.isSet(QStringLiteral("capture-id")) && parser.isSet(QStringLiteral("fixture-id"));
    if (anyCaptureArgument && !captureMode) {
        qCritical("Candidate capture requires --capture-candidate, --candidate-geometry, --screen-id, --capture-id, and --fixture-id.");
        return 2;
    }
    if (smokeMode && captureMode) {
        qCritical("--smoke cannot be combined with candidate capture options.");
        return 2;
    }
    if (captureMode) {
        capture.imagePath = parser.value(QStringLiteral("capture-candidate"));
        capture.geometryPath = parser.value(QStringLiteral("candidate-geometry"));
        capture.screenId = parser.value(QStringLiteral("screen-id"));
        capture.captureId = parser.value(QStringLiteral("capture-id"));
        capture.fixtureId = parser.value(QStringLiteral("fixture-id"));
        if (capture.screenId.trimmed().isEmpty() || capture.captureId.trimmed().isEmpty() || capture.fixtureId.trimmed().isEmpty()
            || QFileInfo(capture.imagePath).absoluteFilePath() == QFileInfo(capture.geometryPath).absoluteFilePath()) {
            qCritical("Capture identities must be nonempty, and PNG/geometry output paths must differ.");
            return 2;
        }
    }

    Settings1Client settingsClient;
    Notifications1Client notificationsClient;
    ToastNotificationsClient toastNotificationsClient;
    adrenalin::contracts::hardware1::Client hardwareClient(
        QString::fromLatin1(adrenalin::session1::serviceName),
        QString::fromLatin1(adrenalin::session1::objectPath), QDBusConnection::sessionBus());
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("appDisplayName"), QString::fromUtf8(ADRENALIN_APP_NAME));
    engine.rootContext()->setContextProperty(QStringLiteral("sessionSettingsClient"),
                                             &settingsClient);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionNotificationsClient"),
                                             &notificationsClient);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionToastNotificationsClient"),
                                             &toastNotificationsClient);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionHardware1Client"),
                                             &hardwareClient);
    engine.loadFromModule(QStringLiteral("Adrenalin.Shell"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty())
        return 1;

    if (smokeMode) {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    } else if (captureMode) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (window == nullptr) {
            qCritical("The loaded QML root is not a Qt Quick window.");
            return 2;
        }
        if (captureSizeSet) {
            const qreal deviceScale = window->devicePixelRatio();
            const qreal physicalWidth = std::ceil(requestedCaptureSize.width() * deviceScale);
            const qreal physicalHeight = std::ceil(requestedCaptureSize.height() * deviceScale);
            if (!std::isfinite(deviceScale) || deviceScale <= 0.0
                || !std::isfinite(physicalWidth) || !std::isfinite(physicalHeight)
                || physicalWidth > kMaxCaptureWidth || physicalHeight > kMaxCaptureHeight
                || physicalWidth * physicalHeight > kMaxCapturePixels) {
                qCritical("--capture-size exceeds the ID-137 physical-pixel envelope at this display scale.");
                return 2;
            }
            window->resize(requestedCaptureSize);
        }
        QTimer::singleShot(750, &app, [&app, window, capture] {
            QString error;
            if (!captureRenderedWindow(window, capture, &error)) {
                qCritical().noquote() << error;
                app.exit(2);
                return;
            }
            app.quit();
        });
    }

    return app.exec();
}
