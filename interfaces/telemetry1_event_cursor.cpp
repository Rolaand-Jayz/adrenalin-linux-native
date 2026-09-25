#include "telemetry1_event_cursor.h"

#include <QUuid>
#include <limits>

namespace adrenalin::contracts::telemetry1 {

bool EventCursor::resetFromOpenResult(const OpenResult &snapshot) {
    const QUuid instanceUuid(snapshot.serviceInstanceUuid);
    if (snapshot.code != QLatin1String("OK") || instanceUuid.isNull()
        || instanceUuid.toString(QUuid::WithoutBraces) != snapshot.serviceInstanceUuid
        || snapshot.serviceGeneration == 0 || snapshot.producerGeneration == 0
        || snapshot.metricDefinitionGeneration == 0
        || snapshot.subjectDefinitionGeneration == 0 || snapshot.abiMajor == 0
        || snapshot.mappedSize == 0) {
        m_snapshot = {};
        m_cursor = 0;
        m_initialized = false;
        return false;
    }

    m_snapshot = snapshot;
    m_cursor = snapshot.eventSequenceCursor;
    m_initialized = true;
    return true;
}

EventDecision EventCursor::observeSequence(const QString &serviceInstanceUuid,
                                           quint64 serviceGeneration,
                                           quint64 eventSequence,
                                           const QString &subjectKind,
                                           const QString &subjectId) {
    if (!m_initialized || serviceInstanceUuid != m_snapshot.serviceInstanceUuid
        || serviceGeneration != m_snapshot.serviceGeneration || subjectKind.isEmpty()
        || subjectId.isEmpty() || eventSequence == 0) {
        return EventDecision::ReopenStream;
    }

    if (eventSequence <= m_cursor) return EventDecision::IgnoreDuplicate;
    if (m_cursor == std::numeric_limits<quint64>::max()
        || eventSequence != m_cursor + 1) {
        return EventDecision::ReopenStream;
    }

    m_cursor = eventSequence;
    return EventDecision::AdvanceCursor;
}

EventDecision EventCursor::observeCommonEvent(const CommonEventEnvelope &event) {
    return observeSequence(event.serviceInstanceUuid, event.serviceGeneration,
                          event.eventSequence, event.subjectKind, event.subjectId);
}

EventDecision EventCursor::observe(const DefinitionsChangedEvent &event) {
    if (!m_initialized || event.serviceInstanceUuid != m_snapshot.serviceInstanceUuid
        || event.serviceGeneration != m_snapshot.serviceGeneration
        || event.subjectKind != QLatin1String("PLATFORM")
        || event.subjectId != QLatin1String("platform") || event.eventSequence == 0) {
        return EventDecision::ReopenStream;
    }

    if (event.producerGeneration != m_snapshot.producerGeneration
        || event.metricDefinitionGeneration != m_snapshot.metricDefinitionGeneration
        || event.subjectDefinitionGeneration != m_snapshot.subjectDefinitionGeneration) {
        return EventDecision::ReopenStream;
    }
    return observeSequence(event.serviceInstanceUuid, event.serviceGeneration,
                           event.eventSequence, event.subjectKind, event.subjectId);
}

} // namespace adrenalin::contracts::telemetry1
