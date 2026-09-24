#pragma once

#include <settings1_interface.h>

#include <QDBusConnection>
#include <QObject>
#include <QDBusServiceWatcher>

class Settings1Client final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool productTelemetryConsent READ productTelemetryConsent NOTIFY stateChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)

public:
    explicit Settings1Client(QObject *parent = nullptr);
    explicit Settings1Client(const QDBusConnection &connection, QObject *parent = nullptr);
    bool ready() const;
    bool productTelemetryConsent() const;
    qulonglong revision() const;
    QString status() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setProductTelemetryConsent(bool enabled);

signals:
    void stateChanged();

private:
    void setDisconnected();
    bool finishRequest();
    void submitPendingConsent();
    void scheduleRefreshRetry();

    OrgAdrenalinlinuxSession1Settings1Interface proxy_;
    QDBusServiceWatcher watcher_;
    bool ready_ = false;
    bool consent_ = false;
    qulonglong revision_ = 0;
    QString status_ = QStringLiteral("CONNECTING");
    QString pendingOperationId_;
    bool pendingConsent_ = false;
    qulonglong pendingExpectedRevision_ = 0;
    bool pendingOperationUncertain_ = false;
    bool requestInFlight_ = false;
    bool refreshPending_ = false;
    bool refreshRetryScheduled_ = false;
    int refreshRetryAttempt_ = 0;
};
