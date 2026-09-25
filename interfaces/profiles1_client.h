#pragma once

#include "profiles1_contract_types.h"
#include <profiles1_interface.h>
#include <service1_interface.h>

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>

class QDBusPendingCallWatcher;

namespace adrenalin::contracts::profiles1 {

class Client final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
public:
    Client(QString serviceName, QString objectPath, QString subjectKind, QString subjectId,
           const QDBusConnection &connection, QObject *parent = nullptr);

    bool ready() const;
    QString status() const;
    Profile profile() const;
    QString serviceInstanceUuid() const;
    quint64 serviceGeneration() const;
    quint64 eventSequence() const;
    QString subjectKind() const;
    QString subjectId() const;
    Q_INVOKABLE void refresh();

signals:
    void stateChanged();
    void profileChanged();

private slots:
    void onOwnerChanged(const QString &, const QString &, const QString &newOwner);
    void onCommonEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &);
    void onProfileEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &,
                        qulonglong);

private:
    void observeEvent(const QString &uuid, quint64 generation, quint64 sequence,
                      const QString &kind, const QString &id, bool profileSignal);
    void requestSnapshot();
    void finishSnapshot(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch,
                        quint64 eventEpoch, QString requestedKind, QString requestedId);
    void clear(const QString &status);
    bool eventOwnerMatches(const QString &uuid, quint64 generation) const;

    OrgAdrenalinlinuxSession1Profiles1Interface profileProxy_;
    OrgAdrenalinlinuxSession1Service1Interface serviceProxy_;
    QDBusConnection connection_;
    QDBusServiceWatcher ownerWatcher_;
    QString subjectKind_;
    QString subjectId_;
    Profile profile_;
    QString status_ = QStringLiteral("CONNECTING");
    QString serviceInstanceUuid_;
    QString eventInstanceUuid_;
    quint64 serviceGeneration_ = 0;
    quint64 eventServiceGeneration_ = 0;
    quint64 eventSequence_ = 0;
    quint64 lastObservedSequence_ = 0;
    quint64 ownerEpoch_ = 0;
    quint64 eventEpoch_ = 0;
    bool ready_ = false;
    bool inFlight_ = false;
    bool refreshPending_ = false;
};

} // namespace adrenalin::contracts::profiles1
