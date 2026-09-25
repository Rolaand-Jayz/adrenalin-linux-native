#include "drm_udev_monitor.h"

#include <QSocketNotifier>
#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <libudev.h>

DrmUdevMonitor::DrmUdevMonitor(QObject *parent) : QObject(parent)
{
    debounceTimer_ = new QTimer(this);
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(150);
    connect(debounceTimer_, &QTimer::timeout, this, &DrmUdevMonitor::inventoryDirty);

    retryTimer_ = new QTimer(this);
    retryTimer_->setSingleShot(true);
    connect(retryTimer_, &QTimer::timeout, this, &DrmUdevMonitor::retryMonitor);
}

DrmUdevMonitor::~DrmUdevMonitor()
{
    resetMonitor();
}

void DrmUdevMonitor::resetMonitor()
{
    delete notifier_;
    notifier_ = nullptr;
    if (monitor_ != nullptr) {
        udev_monitor_unref(monitor_);
        monitor_ = nullptr;
    }
    if (udev_ != nullptr) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
}

void DrmUdevMonitor::scheduleRetry()
{
    if (retryTimer_->isActive()) {
        return;
    }
    const int exponent = std::min(retryAttempt_, 5);
    const int delayMs = std::min(1000 * (1 << exponent), 30000);
    ++retryAttempt_;
    retryTimer_->start(delayMs);
}

bool DrmUdevMonitor::start(QString *error)
{
    if (isRunning()) {
        return true;
    }
    resetMonitor();
    const auto fail = [this, error](const QString &message) {
        if (error != nullptr) {
            *error = message;
        }
        resetMonitor();
        scheduleRetry();
        return false;
    };

    udev_ = udev_new();
    if (udev_ == nullptr) {
        return fail(QStringLiteral("libudev context creation failed"));
    }
    monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
    if (monitor_ == nullptr) {
        return fail(QStringLiteral("libudev DRM event monitor creation failed"));
    }
    if (udev_monitor_filter_add_match_subsystem_devtype(monitor_, "drm", nullptr) < 0) {
        return fail(QStringLiteral("libudev DRM subsystem filter setup failed"));
    }
    if (udev_monitor_enable_receiving(monitor_) < 0) {
        return fail(QStringLiteral("libudev DRM event subscription failed"));
    }
    const int descriptor = udev_monitor_get_fd(monitor_);
    if (descriptor < 0) {
        return fail(QStringLiteral("libudev DRM event descriptor is unavailable"));
    }
    notifier_ = new QSocketNotifier(descriptor, QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this, &DrmUdevMonitor::drainEvents);
    retryTimer_->stop();
    retryAttempt_ = 0;
    return true;
}

bool DrmUdevMonitor::isRunning() const
{
    return notifier_ != nullptr && notifier_->isEnabled();
}

void DrmUdevMonitor::retryMonitor()
{
    QString error;
    if (start(&error)) {
        emit monitorRestored();
    } else {
        emit monitorFailed(error);
    }
}

void DrmUdevMonitor::drainEvents()
{
    bool observedDrmEvent = false;
    bool receiveFailed = false;
    for (;;) {
        errno = 0;
        udev_device *device = udev_monitor_receive_device(monitor_);
        if (device == nullptr) {
            receiveFailed = errno != EAGAIN && errno != EWOULDBLOCK;
            break;
        }
        const char *action = udev_device_get_action(device);
        if (action != nullptr
            && (std::strcmp(action, "add") == 0 || std::strcmp(action, "remove") == 0
                || std::strcmp(action, "change") == 0)) {
            observedDrmEvent = true;
        }
        udev_device_unref(device);
    }
    if (observedDrmEvent || receiveFailed) {
        debounceTimer_->start();
    }
    if (receiveFailed) {
        resetMonitor();
        scheduleRetry();
        emit monitorFailed(QStringLiteral("libudev DRM event receive failed; inventory will be reconciled and observation retried"));
    }
}
