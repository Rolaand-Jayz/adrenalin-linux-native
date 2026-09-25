#pragma once

#include <QObject>

struct udev;
struct udev_monitor;
class QSocketNotifier;
class QTimer;

class DrmUdevMonitor final : public QObject
{
    Q_OBJECT

public:
    explicit DrmUdevMonitor(QObject *parent = nullptr);
    ~DrmUdevMonitor() override;

    bool start(QString *error = nullptr);
    bool isRunning() const;

signals:
    void inventoryDirty();
    void monitorFailed(const QString &message);
    void monitorRestored();

private slots:
    void drainEvents();
    void retryMonitor();

private:
    void resetMonitor();
    void scheduleRetry();

    udev *udev_ = nullptr;
    udev_monitor *monitor_ = nullptr;
    QSocketNotifier *notifier_ = nullptr;
    QTimer *debounceTimer_ = nullptr;
    QTimer *retryTimer_ = nullptr;
    int retryAttempt_ = 0;
};
