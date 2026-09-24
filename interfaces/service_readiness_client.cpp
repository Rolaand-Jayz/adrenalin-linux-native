#include "service_readiness_client.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMetaType>
#include <QVariantMap>

#include <utility>

namespace {
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto kPropertiesChangedSignal = "PropertiesChanged";
constexpr auto kServiceReadinessInterface =
    "org.adrenalinlinux.Session1.Service1";

bool hasReadinessState(const QString &state)
{
    return state == QStringLiteral("STARTING") || state == QStringLiteral("RECOVERING")
        || state == QStringLiteral("READY") || state == QStringLiteral("DEGRADED")
        || state == QStringLiteral("FAILED");
}

bool readProperty(const QVariantMap &properties, const QString &name, QMetaType type,
                  QVariant *value)
{
    const auto found = properties.constFind(name);
    if (found == properties.cend() || found->metaType() != type) {
        return false;
    }
    *value = *found;
    return true;
}
} // namespace

ServiceReadinessClient::ServiceReadinessClient(QString serviceName, QString objectPath,
                                               const QDBusConnection &connection,
                                               ushort expectedApiMajor, QObject *parent)
    : QObject(parent),
      proxy_(serviceName, objectPath, connection, this),
      connection_(connection),
      ownerWatcher_(serviceName, connection, QDBusServiceWatcher::WatchForOwnerChange, this),
      serviceName_(std::move(serviceName)),
      objectPath_(std::move(objectPath)),
      expectedApiMajor_(expectedApiMajor)
{
    connect(&ownerWatcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &ServiceReadinessClient::onServiceOwnerChanged);
    connection_.connect(serviceName_, objectPath_, QString::fromLatin1(kPropertiesInterface),
                        QString::fromLatin1(kPropertiesChangedSignal), this,
                        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    refresh();
}

bool ServiceReadinessClient::available() const { return available_; }
bool ServiceReadinessClient::compatible() const { return compatible_; }
bool ServiceReadinessClient::ready() const
{
    return available_ && compatible_ && initializationState_ == QStringLiteral("READY");
}
QString ServiceReadinessClient::status() const { return status_; }
QString ServiceReadinessClient::initializationState() const { return initializationState_; }
QString ServiceReadinessClient::serviceInstanceUuid() const { return serviceInstanceUuid_; }
qulonglong ServiceReadinessClient::serviceGeneration() const { return serviceGeneration_; }
ushort ServiceReadinessClient::apiMajor() const { return apiMajor_; }
ushort ServiceReadinessClient::apiMinor() const { return apiMinor_; }
QString ServiceReadinessClient::lastInitializationError() const
{
    return lastInitializationError_;
}

void ServiceReadinessClient::refresh()
{
    if (!connection_.isConnected()) {
        clearSnapshot(QStringLiteral("DISCONNECTED"));
        return;
    }
    if (requestInFlight_) {
        refreshPending_ = true;
        return;
    }

    requestInFlight_ = true;
    refreshPending_ = false;
    QDBusMessage message = QDBusMessage::createMethodCall(
        proxy_.service(), proxy_.path(), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("GetAll"));
    message << QString::fromLatin1(kServiceReadinessInterface);
    auto *watcher = new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
    const quint64 requestOwnerEpoch = ownerEpoch_;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestOwnerEpoch] { finishRefresh(watcher, requestOwnerEpoch); });
}

void ServiceReadinessClient::onServiceOwnerChanged(const QString &, const QString &,
                                                    const QString &newOwner)
{
    ++ownerEpoch_;
    requestInFlight_ = false;
    refreshPending_ = false;
    if (newOwner.isEmpty()) {
        clearSnapshot(QStringLiteral("DISCONNECTED"));
        return;
    }
    clearSnapshot(QStringLiteral("RECONCILING"));
    refresh();
}

void ServiceReadinessClient::onPropertiesChanged(const QString &interfaceName,
                                                  const QVariantMap &changed,
                                                  const QStringList &invalidated)
{
    Q_UNUSED(changed);
    Q_UNUSED(invalidated);
    if (interfaceName != QString::fromLatin1(kServiceReadinessInterface)) {
        return;
    }
    clearSnapshot(QStringLiteral("RECONCILING"));
    refresh();
}

void ServiceReadinessClient::clearSnapshot(const QString &status)
{
    available_ = false;
    compatible_ = false;
    status_ = status;
    initializationState_.clear();
    serviceInstanceUuid_.clear();
    serviceGeneration_ = 0;
    apiMajor_ = 0;
    apiMinor_ = 0;
    lastInitializationError_.clear();
    emit stateChanged();
}

void ServiceReadinessClient::applySnapshot(const QVariantMap &properties)
{
    QVariant state;
    QVariant instanceUuid;
    QVariant generation;
    QVariant apiMajor;
    QVariant apiMinor;
    QVariant error;
    const bool complete = readProperty(properties, QStringLiteral("InitializationState"),
                                       QMetaType::fromType<QString>(), &state)
        && readProperty(properties, QStringLiteral("ServiceInstanceUuid"),
                        QMetaType::fromType<QString>(), &instanceUuid)
        && readProperty(properties, QStringLiteral("ServiceGeneration"),
                        QMetaType::fromType<qulonglong>(), &generation)
        && readProperty(properties, QStringLiteral("ApiMajor"),
                        QMetaType::fromType<ushort>(), &apiMajor)
        && readProperty(properties, QStringLiteral("ApiMinor"),
                        QMetaType::fromType<ushort>(), &apiMinor)
        && readProperty(properties, QStringLiteral("LastInitializationError"),
                        QMetaType::fromType<QString>(), &error);
    if (!complete || !hasReadinessState(state.toString()) || instanceUuid.toString().isEmpty()
        || apiMajor.value<ushort>() == 0) {
        clearSnapshot(QStringLiteral("INVALID_READINESS"));
        return;
    }

    initializationState_ = state.toString();
    serviceInstanceUuid_ = instanceUuid.toString();
    serviceGeneration_ = generation.toULongLong();
    apiMajor_ = apiMajor.value<ushort>();
    apiMinor_ = apiMinor.value<ushort>();
    lastInitializationError_ = error.toString();
    available_ = true;
    compatible_ = apiMajor_ == expectedApiMajor_;
    status_ = compatible_ ? initializationState_ : QStringLiteral("INCOMPATIBLE_API_MAJOR");
    emit stateChanged();
}

void ServiceReadinessClient::finishRefresh(QDBusPendingCallWatcher *watcher,
                                            quint64 requestOwnerEpoch)
{
    QDBusPendingReply<QVariantMap> reply = *watcher;
    watcher->deleteLater();
    if (requestOwnerEpoch != ownerEpoch_) {
        return;
    }
    requestInFlight_ = false;
    const bool reconcile = std::exchange(refreshPending_, false);
    if (reconcile) {
        clearSnapshot(QStringLiteral("RECONCILING"));
        refresh();
        return;
    }
    if (reply.isError()) {
        clearSnapshot(QStringLiteral("DISCONNECTED"));
    } else {
        applySnapshot(reply.value());
    }
}
