#pragma once

#include <settings1_interface.h>
#include <service1_interface.h>
#include "service_readiness_client.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>

class QDBusPendingCallWatcher;

class ToastNotificationsClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY stateChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY stateChanged)
    Q_PROPERTY(qulonglong eventSequence READ eventSequence NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString lastOperationCode READ lastOperationCode NOTIFY stateChanged)

public:
    explicit ToastNotificationsClient(QObject *parent = nullptr);
    explicit ToastNotificationsClient(const QDBusConnection &connection,
                                      QObject *parent = nullptr);

    bool ready() const;
    bool configured() const;
    bool enabled() const;
    qulonglong revision() const;
    qulonglong eventSequence() const;
    QString serviceInstanceUuid() const;
    qulonglong serviceGeneration() const;
    QString status() const;
    QString lastOperationCode() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(bool enabled);

signals:
    void stateChanged();

private slots:
    void onOwnerChanged(const QString &, const QString &, const QString &newOwner);
    void onCommonEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                       const QString &kind, const QString &id);
    void onToastEvent(const QString &uuid, qulonglong generation, qulonglong sequence,
                      const QString &kind, const QString &id, bool enabled,
                      qulonglong revision);

private:
    void onReadinessChanged();
    void requestSnapshot();
    void finishSnapshot(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch);
    void finishMutation(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch);
    void observeEvent(const QString &uuid, quint64 generation, quint64 sequence,
                      const QString &kind, const QString &id,
                      bool hasPreferenceValue, bool enabled, quint64 revision);
    void submitPendingMutation();
    void clearSnapshot(const QString &status);
    void resetEventOwner();
    bool identityMatchesReadiness(const QString &uuid, quint64 generation) const;

    OrgAdrenalinlinuxSession1Settings1Interface settingsProxy_;
    OrgAdrenalinlinuxSession1Service1Interface serviceProxy_;
    QDBusConnection connection_;
    QDBusServiceWatcher ownerWatcher_;
    ServiceReadinessClient readiness_;
    bool ready_ = false;
    bool configured_ = false;
    bool enabled_ = false;
    qulonglong revision_ = 0;
    QString status_ = QStringLiteral("CONNECTING");
    QString lastOperationCode_;
    QString serviceInstanceUuid_;
    qulonglong serviceGeneration_ = 0;
    qulonglong eventSequence_ = 0;
    QString eventInstanceUuid_;
    qulonglong eventServiceGeneration_ = 0;
    qulonglong lastObservedSequence_ = 0;
    QString pendingOperationId_;
    bool pendingEnabled_ = false;
    qulonglong pendingExpectedRevision_ = 0;
    bool pendingOperationUncertain_ = false;
    bool requestInFlight_ = false;
    bool refreshPending_ = false;
    quint64 ownerEpoch_ = 0;
};
