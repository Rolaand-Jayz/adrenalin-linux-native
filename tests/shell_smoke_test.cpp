#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>

class ShellSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsQmlAndExitsCleanly()
    {
        const QString executable = qEnvironmentVariable("ADRENALIN_SHELL_EXECUTABLE");
        QVERIFY2(!executable.isEmpty(), "ADRENALIN_SHELL_EXECUTABLE is not set");

        QProcess shell;
        shell.setProgram(executable);
        shell.setArguments({QStringLiteral("--smoke")});
        shell.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        shell.start();

        QVERIFY2(shell.waitForStarted(), qPrintable(shell.errorString()));
        QVERIFY2(shell.waitForFinished(10000), "The shell did not exit within 10 seconds");
        QCOMPARE(shell.exitStatus(), QProcess::NormalExit);
        QCOMPARE(shell.exitCode(), 0);
        QVERIFY2(shell.readAllStandardError().isEmpty(), "The shell emitted QML/runtime errors");
    }
};

QTEST_GUILESS_MAIN(ShellSmokeTest)
#include "shell_smoke_test.moc"
