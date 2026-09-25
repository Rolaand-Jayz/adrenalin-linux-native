#pragma once

#include "hotkeys1_contract_types.h"
#include "service_readiness_client.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>

class QDBusPendingCallWatcher;

namespace adrenalin::contracts::hotkeys1 {

class Client final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
public:
    Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
           QObject *parent = nullptr);

    bool ready() const;
    QString status() const;
    QList<Action> actions() const;
    QString serviceInstanceUuid() const;
    quint64 serviceGeneration() const;
    quint64 eventSequence() const;
    quint64 revision() const;
    Q_INVOKABLE void refresh();

signals:
    void stateChanged();
    void actionsChanged();

private slots:
    void onOwnerChanged(const QString &, const QString &, const QString &newOwner);
    void onCommonEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &);
    void onHotkeyChanged(const QString &, qulonglong, qulonglong, const QString &, const QString &,
                         qulonglong);
    void onReadinessChanged();

private:
    void observeEvent(const QString &uuid, quint64 generation, quint64 sequence,
                      const QString &kind, const QString &id, bool specific);
    void requestSnapshot();
    void finishSnapshot(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch,
                        quint64 eventEpoch);
    void clear(const QString &status);

    QDBusConnection connection_;
    QString serviceName_;
    QString objectPath_;
    QDBusServiceWatcher ownerWatcher_;
    ServiceReadinessClient readiness_;
    QList<Action> actions_;
    QString status_ = QStringLiteral("CONNECTING");
    QString serviceInstanceUuid_;
    QString eventInstanceUuid_;
    quint64 serviceGeneration_ = 0;
    quint64 eventServiceGeneration_ = 0;
    quint64 eventSequence_ = 0;
    quint64 lastObservedSequence_ = 0;
    quint64 revision_ = 0;
    quint64 ownerEpoch_ = 0;
    quint64 eventEpoch_ = 0;
    bool ready_ = false;
    bool inFlight_ = false;
    bool refreshPending_ = false;
};

} // namespace adrenalin::contracts::hotkeys1
