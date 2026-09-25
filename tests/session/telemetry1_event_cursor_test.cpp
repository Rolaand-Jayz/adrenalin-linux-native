#include "../../interfaces/telemetry1_event_cursor.h"

#include <QTest>

using namespace adrenalin::contracts::telemetry1;

namespace {
OpenResult snapshot() {
    OpenResult value;
    value.code = QStringLiteral("OK");
    value.serviceInstanceUuid = QStringLiteral("12345678-1234-4234-8234-123456789abc");
    value.serviceGeneration = 4;
    value.eventSequenceCursor = 26;
    value.producerGeneration = 7;
    value.metricDefinitionGeneration = 11;
    value.subjectDefinitionGeneration = 13;
    value.abiMajor = 1;
    value.mappedSize = 160;
    return value;
}

DefinitionsChangedEvent makeEvent(quint64 sequence = 27) {
    return {snapshot().serviceInstanceUuid, 4, sequence,
            QStringLiteral("PLATFORM"), QStringLiteral("platform"), 7, 11, 13};
}

CommonEventEnvelope makeCommonEvent(quint64 sequence, const QString &subjectKind) {
    return {snapshot().serviceInstanceUuid, 4, sequence, subjectKind,
            QStringLiteral("opaque-fixture-subject")};
}
}

class TelemetryEventCursorTest final : public QObject {
    Q_OBJECT
private slots:
    void resetRequiresSuccessfulAuthoritativeOpen() {
        EventCursor cursor;
        auto rejected = snapshot();
        rejected.code = QStringLiteral("INCOMPATIBLE_VERSION");
        QVERIFY(!cursor.resetFromOpenResult(rejected));
        QVERIFY(!cursor.isInitialized());
        QCOMPARE(cursor.observe(makeEvent()), EventDecision::ReopenStream);

        auto malformedUuid = snapshot();
        malformedUuid.serviceInstanceUuid = QStringLiteral("fixture-instance");
        QVERIFY(!cursor.resetFromOpenResult(malformedUuid));
        QVERIFY(!cursor.isInitialized());

        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        QCOMPARE(cursor.cursor(), quint64(26));
    }

    void duplicateSequenceDoesNotAdvanceOrReopen() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        auto next = makeEvent();
        QCOMPARE(cursor.observe(next), EventDecision::AdvanceCursor);
        QCOMPARE(cursor.cursor(), quint64(27));
        QCOMPARE(cursor.observe(next), EventDecision::IgnoreDuplicate);
        QCOMPARE(cursor.cursor(), quint64(27));
    }

    void sequenceGapRequiresAuthoritativeOpenStreamRefresh() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        QCOMPARE(cursor.observe(makeEvent(29)), EventDecision::ReopenStream);

        auto refreshed = snapshot();
        refreshed.eventSequenceCursor = 29;
        QVERIFY(cursor.resetFromOpenResult(refreshed));
        QCOMPARE(cursor.observe(makeEvent(30)), EventDecision::AdvanceCursor);
        QCOMPARE(cursor.cursor(), quint64(30));
    }

    void sequentialEventsFromOtherFamiliesAdvanceTheSharedCursor() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        QCOMPARE(cursor.observeCommonEvent(makeCommonEvent(27, QStringLiteral("SETTING"))),
                 EventDecision::AdvanceCursor);
        QCOMPARE(cursor.observe(makeEvent(28)), EventDecision::AdvanceCursor);
        QCOMPARE(cursor.cursor(), quint64(28));
    }

    void changedDefinitionsReopenEvenWhenCommonCursorAlreadySawTheirSequence() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        QCOMPARE(cursor.observeCommonEvent(makeCommonEvent(27, QStringLiteral("HARDWARE_GPU"))),
                 EventDecision::AdvanceCursor);
        auto definitions = makeEvent(27);
        definitions.metricDefinitionGeneration++;
        QCOMPARE(cursor.observe(definitions), EventDecision::ReopenStream);
    }

    void skippedCommonServiceSequenceRequiresRefresh() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        QCOMPARE(cursor.observeCommonEvent(makeCommonEvent(28, QStringLiteral("SERVICE"))),
                 EventDecision::ReopenStream);
    }

    void ownerAndServiceGenerationMismatchRequireRefresh() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        auto changedOwner = makeEvent();
        changedOwner.serviceInstanceUuid = QStringLiteral("87654321-4321-4321-8321-cba987654321");
        QCOMPARE(cursor.observe(changedOwner), EventDecision::ReopenStream);

        auto changedGeneration = makeEvent();
        changedGeneration.serviceGeneration++;
        QCOMPARE(cursor.observe(changedGeneration), EventDecision::ReopenStream);
    }

    void streamDefinitionGenerationChangesRequireReopen() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        auto changedProducer = makeEvent();
        changedProducer.producerGeneration++;
        QCOMPARE(cursor.observe(changedProducer), EventDecision::ReopenStream);

        auto changedMetrics = makeEvent();
        changedMetrics.metricDefinitionGeneration++;
        QCOMPARE(cursor.observe(changedMetrics), EventDecision::ReopenStream);

        auto changedSubjects = makeEvent();
        changedSubjects.subjectDefinitionGeneration++;
        QCOMPARE(cursor.observe(changedSubjects), EventDecision::ReopenStream);
    }

    void malformedOrNonPlatformEventRequiresRefresh() {
        EventCursor cursor;
        QVERIFY(cursor.resetFromOpenResult(snapshot()));
        auto malformed = makeEvent();
        malformed.subjectKind = QStringLiteral("GPU_PCI");
        QCOMPARE(cursor.observe(malformed), EventDecision::ReopenStream);
        malformed = makeEvent();
        malformed.eventSequence = 0;
        QCOMPARE(cursor.observe(malformed), EventDecision::ReopenStream);
    }
};

QTEST_GUILESS_MAIN(TelemetryEventCursorTest)
#include "telemetry1_event_cursor_test.moc"
