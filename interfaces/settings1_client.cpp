#include "settings1_client.h"
#include "session_identity.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QTimer>
#include <QUuid>

Settings1Client::Settings1Client(QObject *parent)
    : Settings1Client(QDBusConnection::sessionBus(), parent)
{
}

Settings1Client::Settings1Client(const QDBusConnection &connection, QObject *parent)
    : QObject(parent),
      proxy_(QString::fromLatin1(adrenalin::session1::serviceName),
             QString::fromLatin1(adrenalin::session1::objectPath),
             connection, this),
      watcher_(QString::fromLatin1(adrenalin::session1::serviceName), connection,
               QDBusServiceWatcher::WatchForOwnerChange, this)
{
    connect(&watcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                if (newOwner.isEmpty()) {
                    setDisconnected();
                } else {
                    ready_ = false;
                    status_ = QStringLiteral("RECONCILING");
                    emit stateChanged();
                    refresh();
                }
            });
    connect(&proxy_, &OrgAdrenalinlinuxSession1Settings1Interface::ProductTelemetryConsentChanged,
            this, [this](bool enabled, qulonglong revision) {
                if (!ready_ || requestInFlight_) {
                    refreshPending_ = true;
                    return;
                }
                if (revision <= revision_) {
                    return;
                }
                if (revision - revision_ != 1) {
                    ready_ = false;
                    status_ = QStringLiteral("RECONCILING");
                    emit stateChanged();
                    refresh();
                    return;
                }
                consent_ = enabled;
                revision_ = revision;
                emit stateChanged();
            });
    refresh();
}

bool Settings1Client::ready() const { return ready_; }
bool Settings1Client::productTelemetryConsent() const { return consent_; }
qulonglong Settings1Client::revision() const { return revision_; }
QString Settings1Client::status() const { return status_; }

void Settings1Client::refresh()
{
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }
    requestInFlight_ = true;
    auto *call = new QDBusPendingCallWatcher(proxy_.GetProductTelemetryConsent(), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        const bool reconcileAgain = finishRequest();
        QDBusPendingReply<QString, bool, qulonglong> reply = *call;
        call->deleteLater();
        if (reconcileAgain) {
            ready_ = false;
            status_ = QStringLiteral("RECONCILING");
            emit stateChanged();
            refresh();
            return;
        }
        if (reply.isError()) {
            setDisconnected();
            return;
        }
        const QString resultCode = reply.argumentAt<0>();
        if (resultCode != QStringLiteral("OK")) {
            ready_ = false;
            status_ = resultCode;
            emit stateChanged();
            if (resultCode == QStringLiteral("NOT_READY")) {
                scheduleRefreshRetry();
            }
            return;
        }
        consent_ = reply.argumentAt<1>();
        revision_ = reply.argumentAt<2>();
        if (pendingOperationId_.isEmpty()) {
            refreshRetryAttempt_ = 0;
        }
        if (!pendingOperationId_.isEmpty()) {
            submitPendingConsent();
            return;
        }
        // GetProductTelemetryConsent returns OK only after the service has reached READY.
        ready_ = true;
        status_ = QStringLiteral("READY");
        emit stateChanged();
    });
}

void Settings1Client::setProductTelemetryConsent(bool enabled)
{
    if (!ready_ || requestInFlight_) {
        return;
    }
    if (!pendingOperationId_.isEmpty()) {
        if (enabled == pendingConsent_) {
            submitPendingConsent();
        }
        return;
    }
    pendingOperationId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pendingConsent_ = enabled;
    pendingExpectedRevision_ = revision_;
    pendingOperationUncertain_ = false;
    submitPendingConsent();
}

void Settings1Client::submitPendingConsent()
{
    if (pendingOperationId_.isEmpty() || requestInFlight_) {
        return;
    }
    requestInFlight_ = true;
    ready_ = false;
    status_ = pendingOperationUncertain_ ? QStringLiteral("RECONCILING")
                                         : QStringLiteral("SAVING");
    emit stateChanged();
    auto *call = new QDBusPendingCallWatcher(
        proxy_.SetProductTelemetryConsent(pendingOperationId_, pendingConsent_,
                                          pendingExpectedRevision_), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        const bool reconcileAgain = finishRequest();
        QDBusPendingReply<QString, qulonglong> reply = *call;
        call->deleteLater();
        if (reply.isError()) {
            pendingOperationUncertain_ = true;
            if (reconcileAgain) {
                ready_ = false;
                status_ = QStringLiteral("RECONCILING");
                emit stateChanged();
                refresh();
            } else {
                setDisconnected();
            }
            return;
        }
        status_ = reply.argumentAt<0>();
        if (status_ == QStringLiteral("OK")) {
            pendingOperationId_.clear();
            pendingOperationUncertain_ = false;
            refresh();
            return;
        }
        if (status_ == QStringLiteral("NOT_READY")) {
            pendingOperationUncertain_ = true;
            refresh();
            return;
        }
        pendingOperationId_.clear();
        pendingOperationUncertain_ = false;
        ready_ = false;
        if (reconcileAgain || status_ == QStringLiteral("STALE_REVISION")) {
            refresh();
        }
        emit stateChanged();
    });
}

void Settings1Client::scheduleRefreshRetry()
{
    if (refreshRetryScheduled_) {
        return;
    }
    constexpr int initialDelayMs = 100;
    constexpr int maximumDelayMs = 2000;
    const int exponent = qMin(refreshRetryAttempt_, 5);
    const int delayMs = qMin(initialDelayMs * (1 << exponent), maximumDelayMs);
    ++refreshRetryAttempt_;
    refreshRetryScheduled_ = true;
    QTimer::singleShot(delayMs, this, [this] {
        refreshRetryScheduled_ = false;
        if (requestInFlight_ || (ready_ && pendingOperationId_.isEmpty())) {
            return;
        }
        refresh();
    });
}

bool Settings1Client::finishRequest()
{
    requestInFlight_ = false;
    const bool reconcileAgain = refreshPending_;
    refreshPending_ = false;
    return reconcileAgain;
}

void Settings1Client::setDisconnected()
{
    ready_ = false;
    status_ = QStringLiteral("DISCONNECTED");
    emit stateChanged();
    scheduleRefreshRetry();
}
