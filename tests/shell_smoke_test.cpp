#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
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
};

QTEST_GUILESS_MAIN(ShellSmokeTest)
#include "shell_smoke_test.moc"
