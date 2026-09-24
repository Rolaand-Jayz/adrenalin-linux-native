#include "settings1_client.h"
#include "operation_result.h"
#include "session_identity.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QTimer>
#include <QUuid>

using adrenalin::contracts::OperationResultCode;
using adrenalin::contracts::operationResultCodeFromName;
using adrenalin::contracts::operationResultCodeName;

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
               QDBusServiceWatcher::WatchForOwnerChange, this),
      readiness_(QString::fromLatin1(adrenalin::session1::serviceName),
                 QString::fromLatin1(adrenalin::session1::objectPath), connection, 1)
{
    connect(&readiness_, &ServiceReadinessClient::stateChanged,
            this, &Settings1Client::updateReadiness);
    connect(&watcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                if (newOwner.isEmpty()) {
                    setDisconnected();
                } else {
                    ready_ = false;
                    serviceInstanceUuid_.clear();
                    serviceGeneration_ = 0;
                    eventSequence_ = 0;
                    status_ = QStringLiteral("RECONCILING");
                    emit stateChanged();
                    readiness_.refresh();
                    refresh();
                }
            });
    connect(&proxy_, &OrgAdrenalinlinuxSession1Settings1Interface::ProductTelemetryConsentChanged,
            this, [this](const QString &serviceInstanceUuid, qulonglong serviceGeneration,
                         qulonglong eventSequence, const QString &subjectKind,
                         const QString &subjectId, bool enabled, qulonglong revision) {
                if (!ready_ || requestInFlight_) {
                    refreshPending_ = true;
                    return;
                }
                if (serviceInstanceUuid != serviceInstanceUuid_
                    || serviceGeneration != serviceGeneration_
                    || subjectKind != QStringLiteral("PREFERENCE")
                    || subjectId != QStringLiteral("product.telemetry_consent")) {
                    ready_ = false;
                    status_ = QStringLiteral("RECONCILING");
                    emit stateChanged();
                    refresh();
                    return;
                }
                if (eventSequence <= eventSequence_) {
                    return;
                }
                if (eventSequence - eventSequence_ != 1 || revision <= revision_
                    || revision - revision_ != 1) {
                    ready_ = false;
                    status_ = QStringLiteral("RECONCILING");
                    emit stateChanged();
                    refresh();
                    return;
                }
                consent_ = enabled;
                revision_ = revision;
                eventSequence_ = eventSequence;
                emit stateChanged();
            });
    updateReadiness();
}

bool Settings1Client::ready() const
{
    return ready_ && canUseSettingsService();
}
bool Settings1Client::productTelemetryConsent() const { return consent_; }
qulonglong Settings1Client::revision() const { return revision_; }
QString Settings1Client::status() const { return status_; }
QString Settings1Client::lastOperationCode() const { return lastOperationCode_; }

void Settings1Client::refresh()
{
    if (!readiness_.ready()) {
        ready_ = false;
        const QString readinessStatus = readiness_.status();
        if (status_ != readinessStatus) {
            status_ = readinessStatus;
            emit stateChanged();
        }
        return;
    }
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }
    requestInFlight_ = true;
    auto *call = new QDBusPendingCallWatcher(proxy_.GetProductTelemetryConsent(), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call] {
        const bool reconcileAgain = finishRequest();
        QDBusPendingReply<QString, QString, qulonglong, qulonglong, bool, qulonglong> reply = *call;
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
        if (!readiness_.ready()) {
            ready_ = false;
            status_ = readiness_.status();
            emit stateChanged();
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
        const QString serviceInstanceUuid = reply.argumentAt<1>();
        const qulonglong serviceGeneration = reply.argumentAt<2>();
        const qulonglong eventSequence = reply.argumentAt<3>();
        if (serviceInstanceUuid.isEmpty() || serviceGeneration == 0) {
            setDisconnected();
            return;
        }
        if (serviceInstanceUuid != readiness_.serviceInstanceUuid()
            || serviceGeneration != readiness_.serviceGeneration()) {
            ready_ = false;
            status_ = QStringLiteral("RECONCILING");
            emit stateChanged();
            readiness_.refresh();
            return;
        }
        serviceInstanceUuid_ = serviceInstanceUuid;
        serviceGeneration_ = serviceGeneration;
        eventSequence_ = eventSequence;
        consent_ = reply.argumentAt<4>();
        revision_ = reply.argumentAt<5>();
        if (pendingOperationId_.isEmpty()) {
            refreshRetryAttempt_ = 0;
        }
        if (!pendingOperationId_.isEmpty()) {
            submitPendingConsent();
            return;
        }
        ready_ = true;
        status_ = QStringLiteral("READY");
        emit stateChanged();
    });
}

void Settings1Client::setProductTelemetryConsent(bool enabled)
{
    if (!ready() || requestInFlight_) {
        return;
    }
    if (!pendingOperationId_.isEmpty()) {
        if (enabled == pendingConsent_) {
            submitPendingConsent();
        }
        return;
    }
    lastOperationCode_.clear();
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
    if (!canUseSettingsService()) {
        ready_ = false;
        status_ = readiness_.status();
        emit stateChanged();
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
        QDBusPendingReply<QString, QString, QString, QString, bool, QString, QString,
                          qulonglong> reply = *call;
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
        const auto resultCode = operationResultCodeFromName(reply.argumentAt<0>());
        const OperationResultCode code = resultCode.value_or(OperationResultCode::InternalError);
        status_ = operationResultCodeName(code);
        if (code == OperationResultCode::Ok) {
            pendingOperationId_.clear();
            pendingOperationUncertain_ = false;
            lastOperationCode_.clear();
            refresh();
            return;
        }
        if (code == OperationResultCode::BackendUnavailable && reply.argumentAt<4>()) {
            pendingOperationUncertain_ = true;
            refresh();
            return;
        }
        pendingOperationId_.clear();
        pendingOperationUncertain_ = false;
        lastOperationCode_ = status_;
        ready_ = false;
        emit stateChanged();
        refresh();
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

void Settings1Client::updateReadiness()
{
    if (!readiness_.ready()) {
        const bool changed = ready_ || status_ != readiness_.status();
        ready_ = false;
        status_ = readiness_.status();
        if (changed) {
            emit stateChanged();
        }
        return;
    }

    if (!ready_ || serviceInstanceUuid_ != readiness_.serviceInstanceUuid()
        || serviceGeneration_ != readiness_.serviceGeneration()) {
        refresh();
    }
}

bool Settings1Client::canUseSettingsService() const
{
    return readiness_.ready() && !serviceInstanceUuid_.isEmpty()
        && serviceInstanceUuid_ == readiness_.serviceInstanceUuid()
        && serviceGeneration_ == readiness_.serviceGeneration();
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
