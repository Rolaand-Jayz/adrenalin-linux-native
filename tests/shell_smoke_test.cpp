#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

namespace {
struct ShellResult
{
    bool started = false;
    bool finished = false;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    int exitCode = -1;
    QString standardError;
    QString processError;
};

ShellResult runShell(const QStringList &arguments)
{
    const QString executable = qEnvironmentVariable("ADRENALIN_SHELL_EXECUTABLE");
    QProcess shell;
    shell.setProgram(executable);
    shell.setArguments(arguments);
    shell.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    shell.start();
    ShellResult result;
    result.started = shell.waitForStarted();
    if (result.started)
        result.finished = shell.waitForFinished(10000);
    result.exitStatus = shell.exitStatus();
    result.exitCode = shell.exitCode();
    result.standardError = QString::fromLocal8Bit(shell.readAllStandardError());
    result.processError = shell.errorString();
    return result;
}
} // namespace

class ShellSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsQmlAndExitsCleanly()
    {
        const QString executable = qEnvironmentVariable("ADRENALIN_SHELL_EXECUTABLE");
        QVERIFY2(!executable.isEmpty(), "ADRENALIN_SHELL_EXECUTABLE is not set");

        const ShellResult result = runShell({QStringLiteral("--smoke")});
        QVERIFY2(result.started, qPrintable(result.processError));
        QVERIFY2(result.finished, "The shell did not exit within 10 seconds");
        QCOMPARE(result.exitStatus, QProcess::NormalExit);
        QCOMPARE(result.exitCode, 0);
        QVERIFY2(result.standardError.isEmpty(), "The shell emitted QML/runtime errors");
    }

    void captureSizeArgumentsFailClosed()
    {
        const QString executable = qEnvironmentVariable("ADRENALIN_SHELL_EXECUTABLE");
        QVERIFY2(!executable.isEmpty(), "ADRENALIN_SHELL_EXECUTABLE is not set");

        const ShellResult sizeWithoutCapture = runShell(
            {QStringLiteral("--capture-size"), QStringLiteral("1920x1080")});
        QVERIFY(sizeWithoutCapture.started);
        QVERIFY(sizeWithoutCapture.finished);
        QCOMPARE(sizeWithoutCapture.exitStatus, QProcess::NormalExit);
        QCOMPARE(sizeWithoutCapture.exitCode, 2);

        const ShellResult validSizeWithSmoke = runShell(
            {QStringLiteral("--smoke"), QStringLiteral("--capture-size"), QStringLiteral("1280x800")});
        QVERIFY(validSizeWithSmoke.started);
        QVERIFY(validSizeWithSmoke.finished);
        QCOMPARE(validSizeWithSmoke.exitStatus, QProcess::NormalExit);
        QCOMPARE(validSizeWithSmoke.exitCode, 2);

        const ShellResult tooSmallWithSmoke = runShell(
            {QStringLiteral("--smoke"), QStringLiteral("--capture-size"), QStringLiteral("959x640")});
        QVERIFY(tooSmallWithSmoke.started);
        QVERIFY(tooSmallWithSmoke.finished);
        QCOMPARE(tooSmallWithSmoke.exitStatus, QProcess::NormalExit);
        QCOMPARE(tooSmallWithSmoke.exitCode, 2);

        const ShellResult tooLargeWithSmoke = runShell(
            {QStringLiteral("--smoke"), QStringLiteral("--capture-size"), QStringLiteral("3841x2160")});
        QVERIFY(tooLargeWithSmoke.started);
        QVERIFY(tooLargeWithSmoke.finished);
        QCOMPARE(tooLargeWithSmoke.exitStatus, QProcess::NormalExit);
        QCOMPARE(tooLargeWithSmoke.exitCode, 2);

        const ShellResult integerOverflow = runShell(
            {QStringLiteral("--smoke"), QStringLiteral("--capture-size"), QStringLiteral("2147483648x1080")});
        QVERIFY(integerOverflow.started);
        QVERIFY(integerOverflow.finished);
        QCOMPARE(integerOverflow.exitStatus, QProcess::NormalExit);
        QCOMPARE(integerOverflow.exitCode, 2);
    }

    void candidateCaptureWritesBoundedPhysicalEvidence()
    {
        const QString executable = qEnvironmentVariable("ADRENALIN_SHELL_EXECUTABLE");
        QVERIFY2(!executable.isEmpty(), "ADRENALIN_SHELL_EXECUTABLE is not set");

        QTemporaryDir outputDirectory;
        QVERIFY2(outputDirectory.isValid(), "Could not create temporary capture output directory");
        const QString pngPath = outputDirectory.filePath(QStringLiteral("candidate.png"));
        const QString geometryPath = outputDirectory.filePath(QStringLiteral("geometry.json"));
        const QString screenId = QStringLiteral("home");
        const QString captureId = QStringLiteral("xvfb-scale-2:default");
        const QString fixtureId = QStringLiteral("candidate-capture-regression");

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
        // This capture-only process does not exercise session IPC. An unserved
        // unique address prevents desktop portal activation on the test bus.
        environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                            QStringLiteral("unix:abstract=adrenalin-capture-test-%1")
                                .arg(QCoreApplication::applicationPid()));
        environment.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("2"));
        QProcess shell;
        shell.setProgram(executable);
        shell.setArguments({
            QStringLiteral("--capture-candidate"), pngPath,
            QStringLiteral("--candidate-geometry"), geometryPath,
            QStringLiteral("--screen-id"), screenId,
            QStringLiteral("--capture-id"), captureId,
            QStringLiteral("--fixture-id"), fixtureId,
            QStringLiteral("--capture-size"), QStringLiteral("1280x800"),
        });
        shell.setProcessEnvironment(environment);
        shell.setStandardOutputFile(QProcess::nullDevice());
        shell.setStandardErrorFile(QProcess::nullDevice());
        shell.start();
        QVERIFY2(shell.waitForStarted(), qPrintable(shell.errorString()));
        QVERIFY2(shell.waitForFinished(20000), "Candidate capture did not finish within 20 seconds");
        QCOMPARE(shell.exitStatus(), QProcess::NormalExit);
        QCOMPARE(shell.exitCode(), 0);

        const QImage candidate(pngPath);
        QVERIFY2(!candidate.isNull(), "Candidate capture did not produce a readable PNG");
        QCOMPARE(candidate.width(), 2560);
        QCOMPARE(candidate.height(), 1600);

        QFile geometryFile(geometryPath);
        QVERIFY2(geometryFile.open(QIODevice::ReadOnly), "Candidate capture did not produce geometry JSON");
        QJsonParseError parseError;
        const QJsonDocument geometryDocument = QJsonDocument::fromJson(geometryFile.readAll(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(parseError.errorString()));
        QVERIFY(geometryDocument.isObject());
        const QJsonObject geometry = geometryDocument.object();
        QStringList geometryKeys = geometry.keys();
        geometryKeys.sort();
        QStringList expectedGeometryKeys = {
            QStringLiteral("build_identity"), QStringLiteral("capture_id"),
            QStringLiteral("components"), QStringLiteral("fixture_id"),
            QStringLiteral("schema_version"), QStringLiteral("screen_id"),
            QStringLiteral("source")
        };
        expectedGeometryKeys.sort();
        QCOMPARE(geometryKeys, expectedGeometryKeys);
        QCOMPARE(geometry.value(QStringLiteral("schema_version")).toInt(), 1);
        QCOMPARE(geometry.value(QStringLiteral("source")).toString(),
                 QStringLiteral("qt_qml_application_self_reported_runtime_geometry"));
        QCOMPARE(geometry.value(QStringLiteral("screen_id")).toString(), screenId);
        QCOMPARE(geometry.value(QStringLiteral("capture_id")).toString(), captureId);
        QCOMPARE(geometry.value(QStringLiteral("fixture_id")).toString(), fixtureId);
        const QString buildIdentity = geometry.value(QStringLiteral("build_identity")).toString();
        QVERIFY2(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$")).match(buildIdentity).hasMatch(),
                 "Geometry build identity must be a lowercase SHA-256 digest");

        const QJsonArray components = geometry.value(QStringLiteral("components")).toArray();
        QVERIFY(!components.isEmpty());
        QSet<QString> identities;
        const QSet<QString> requiredIdentities = {
            QStringLiteral("header"),
            QStringLiteral("applicationTitle"),
            QStringLiteral("platformLabel"),
            QStringLiteral("screenTitle"),
            QStringLiteral("screenDescription"),
            QStringLiteral("productTelemetryConsent"),
            QStringLiteral("telemetryPreferenceRow"),
            QStringLiteral("telemetryPreferenceLabel"),
            QStringLiteral("productTelemetryConsentSwitch"),
        };
        for (const QJsonValue &componentValue : components) {
            QVERIFY(componentValue.isObject());
            const QJsonObject component = componentValue.toObject();
            QStringList componentKeys = component.keys();
            componentKeys.sort();
            QCOMPARE(componentKeys, (QStringList{QStringLiteral("id"), QStringLiteral("rect")}));
            const QString identity = component.value(QStringLiteral("id")).toString();
            QVERIFY(!identity.isEmpty());
            QVERIFY2(!identities.contains(identity), qPrintable(QStringLiteral("Duplicate component id: %1").arg(identity)));
            identities.insert(identity);
            const QJsonObject rect = component.value(QStringLiteral("rect")).toObject();
            QStringList rectKeys = rect.keys();
            rectKeys.sort();
            QCOMPARE(rectKeys, (QStringList{QStringLiteral("height"), QStringLiteral("width"),
                                           QStringLiteral("x"), QStringLiteral("y")}));
            QVERIFY(rect.contains(QStringLiteral("x")));
            QVERIFY(rect.contains(QStringLiteral("y")));
            QVERIFY(rect.contains(QStringLiteral("width")));
            QVERIFY(rect.contains(QStringLiteral("height")));
            const int x = rect.value(QStringLiteral("x")).toInt(-1);
            const int y = rect.value(QStringLiteral("y")).toInt(-1);
            const int width = rect.value(QStringLiteral("width")).toInt(-1);
            const int height = rect.value(QStringLiteral("height")).toInt(-1);
            QVERIFY(x >= 0);
            QVERIFY(y >= 0);
            QVERIFY(width > 0);
            QVERIFY(height > 0);
            QVERIFY(x + width <= candidate.width());
            QVERIFY(y + height <= candidate.height());
        }
        for (const QString &identity : requiredIdentities)
            QVERIFY2(identities.contains(identity), qPrintable(QStringLiteral("Missing component id: %1").arg(identity)));
    }
};

QTEST_GUILESS_MAIN(ShellSmokeTest)
#include "shell_smoke_test.moc"
