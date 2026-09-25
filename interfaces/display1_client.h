#pragma once

#include "display1_contract_types.h"
#include <display1_interface.h>
#include <hardware1_interface.h>
#include <service1_interface.h>

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QMap>
#include <QObject>

class QDBusPendingCallWatcher;

namespace adrenalin::contracts::display1 {

struct DisplayState final {
    Display display;
    QList<Capability> capabilities;
};

class Client final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY snapshotChanged)
    Q_PROPERTY(QString status READ status NOTIFY snapshotChanged)
public:
    Client(QString serviceName, QString objectPath, const QDBusConnection &connection,
           QObject *parent = nullptr);

    bool available() const;
    QString status() const;
    QList<DisplayState> displays() const;
    QString serviceInstanceUuid() const;
    quint64 serviceGeneration() const;
    quint64 eventSequence() const;
    quint64 inventoryGeneration() const;
    quint64 capabilityGeneration() const;
    Q_INVOKABLE void refresh();

signals:
    void snapshotChanged();

private slots:
    void onOwnerChanged(const QString &, const QString &, const QString &newOwner);
    void onCommonEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &);
    void onInventoryEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &,
                         qulonglong, qulonglong);
    void onCapabilityEvent(const QString &, qulonglong, qulonglong, const QString &,
                           const QString &, qulonglong, qulonglong);
    void onDisplayEvent(const QString &, qulonglong, qulonglong, const QString &, const QString &,
                       qulonglong, qulonglong);

private:
    struct EventRecord { QString uuid; quint64 generation = 0; QString kind; QString id;
                         quint64 inventory = 0; quint64 capability = 0; bool common = false;
                         bool refreshIssued = false; };
    void acceptEvent(const EventRecord &event, quint64 sequence, bool relevant);
    void clear(const QString &status);
    void invalidate(const QString &status);
    void requestList();
    void finishList(QDBusPendingCallWatcher *watcher, quint64 ownerEpoch, quint64 eventEpoch);
    void requestState(const ListReply &list, int index, QList<DisplayState> states,
                      quint64 ownerEpoch, quint64 eventEpoch);
    void finishState(QDBusPendingCallWatcher *watcher, ListReply list, int index,
                     QList<DisplayState> states, quint64 ownerEpoch, quint64 eventEpoch);
    bool sameSnapshot(const hardware1::Reply &a, const hardware1::Reply &b) const;

    OrgAdrenalinlinuxSession1Display1Interface displayProxy_;
    OrgAdrenalinlinuxSession1Hardware1Interface hardwareProxy_;
    OrgAdrenalinlinuxSession1Service1Interface serviceProxy_;
    QDBusConnection connection_;
    QDBusServiceWatcher ownerWatcher_;
    QString serviceName_;
    QString objectPath_;
    bool available_ = false;
    QString status_ = QStringLiteral("CONNECTING");
    QList<DisplayState> displays_;
    QString serviceInstanceUuid_;
    quint64 serviceGeneration_ = 0;
    quint64 eventSequence_ = 0;
    quint64 lastObservedSequence_ = 0;
    quint64 inventoryGeneration_ = 0;
    quint64 capabilityGeneration_ = 0;
    QString eventInstanceUuid_;
    quint64 eventServiceGeneration_ = 0;
    quint64 ownerEpoch_ = 0;
    quint64 eventEpoch_ = 0;
    bool inFlight_ = false;
    bool refreshPending_ = false;
    QMap<quint64, EventRecord> recentEvents_;
};

} // namespace adrenalin::contracts::display1
