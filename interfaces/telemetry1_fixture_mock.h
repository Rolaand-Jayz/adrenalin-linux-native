#pragma once

#include "telemetry1_abi_v1.h"
#include "telemetry1_types.h"

#include <QDBusUnixFileDescriptor>
#include <QString>

namespace adrenalin::contracts::telemetry1 {
class FixtureMock final {
public:
    FixtureMock();
    ~FixtureMock();
    FixtureMock(const FixtureMock &) = delete;
    FixtureMock &operator=(const FixtureMock &) = delete;
    bool isValid() const { return m_producerFd >= 0 && m_readerFd >= 0; }
    int readerDescriptor() const { return m_readerFd; }
    OpenResult openResult() const;
    QDBusUnixFileDescriptor readOnlyHandle() const;
    QList<MetricDefinition> metrics() const;
    QList<SubjectDefinition> subjects() const;
private:
    int m_producerFd = -1;
    int m_readerFd = -1;
    QString m_instance;
};
}
