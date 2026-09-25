#pragma once

#include "telemetry1_types.h"

namespace adrenalin::contracts::telemetry1 {

struct CommonEventEnvelope final {
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    QString subjectKind;
    QString subjectId;
};

struct DefinitionsChangedEvent final {
    QString serviceInstanceUuid;
    quint64 serviceGeneration = 0;
    quint64 eventSequence = 0;
    QString subjectKind;
    QString subjectId;
    quint64 producerGeneration = 0;
    quint64 metricDefinitionGeneration = 0;
    quint64 subjectDefinitionGeneration = 0;
};

enum class EventDecision {
    AdvanceCursor,
    IgnoreDuplicate,
    ReopenStream
};

// Test/contract model only. Production event allocation and client integration
// remain owned by the shared Session1 implementation and its ID-093 work.
class EventCursor final {
public:
    bool resetFromOpenResult(const OpenResult &snapshot);
    EventDecision observeCommonEvent(const CommonEventEnvelope &event);
    EventDecision observe(const DefinitionsChangedEvent &event);
    quint64 cursor() const { return m_cursor; }
    bool isInitialized() const { return m_initialized; }

private:
    EventDecision observeSequence(const QString &serviceInstanceUuid,
                                  quint64 serviceGeneration,
                                  quint64 eventSequence,
                                  const QString &subjectKind,
                                  const QString &subjectId);
    OpenResult m_snapshot;
    quint64 m_cursor = 0;
    bool m_initialized = false;
};

} // namespace adrenalin::contracts::telemetry1
