#include "profiles1_mock.h"

#include "operation_result.h"

#include <limits>
#include <QRegularExpression>

namespace adrenalin::contracts::profiles1 {
using adrenalin::contracts::OperationResultCode;

Mock::Mock()
{
    Profile global{QStringLiteral("123e4567-e89b-42d3-a456-426614174010"), QStringLiteral("GLOBAL"),
                   QStringLiteral("global"), QString(), QStringLiteral("REFERENCE_GATED"), 1,
                   {{QStringLiteral("fixture_global_setting"), true}, {QStringLiteral("fixture_global_secondary_setting"), 0.5}}};
    Profile game{QStringLiteral("123e4567-e89b-42d3-a456-426614174011"), QStringLiteral("GAME"),
                 QStringLiteral("steam:app/12345"), QString(), QStringLiteral("REFERENCE_GATED"), 1,
                 {{QStringLiteral("fixture_game_setting"), false}}};
    profiles_.insert(keyFor(global.subjectKind, global.subjectId), global);
    profiles_.insert(keyFor(game.subjectKind, game.subjectId), game);
}

QString Mock::keyFor(const QString &subjectKind, const QString &subjectId) const
{
    return subjectKind + QLatin1Char('\n') + subjectId;
}

ReadReply Mock::readProfile(const QString &subjectKind, const QString &subjectId) const
{
    ReadReply reply;
    if (!isValidSubject(subjectKind, subjectId)) {
        reply.code = QStringLiteral("INVALID_ARGUMENT");
        return reply;
    }
    const auto found = profiles_.constFind(keyFor(subjectKind, subjectId));
    if (found == profiles_.cend()) {
        reply.code = QStringLiteral("NOT_FOUND");
        return reply;
    }
    reply.code = QStringLiteral("OK");
    reply.profile = found.value();
    return reply;
}

UpdateReply Mock::updateProfile(const QString &subjectKind, const QString &subjectId,
                                quint64 expectedRevision, const QString &operationId,
                                const QVariantMap &settingsPatch)
{
    UpdateReply reply;
    reply.serviceInstanceUuid = serviceInstanceUuid_;
    reply.serviceGeneration = serviceGeneration_;
    reply.eventSequence = eventSequence_;
    reply.mutation.operationId = operationId;
    reply.mutation.provider = QStringLiteral("profiles-contract-mock");
    reply.mutation.subjectId = isValidSubject(subjectKind, subjectId)
        ? (subjectKind + QLatin1Char(':') + subjectId) : QString();
    const auto setError = [&](OperationResultCode code, const QString &key, const QString &detail) {
        reply.mutation.code = code;
        reply.mutation.humanMessageKey = key;
        reply.mutation.diagnosticMessage = detail;
        return reply;
    };
    static const QRegularExpression opSyntax(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$"));
    if (!opSyntax.match(operationId).hasMatch() || !isValidSubject(subjectKind, subjectId)
        || !isValidSettings(settingsPatch)) {
        return setError(OperationResultCode::InvalidArgument,
                        QStringLiteral("profiles.operation.invalid_argument"),
                        QStringLiteral("Profile subject, operation ID, or settings patch is invalid"));
    }

    const auto prior = operations_.constFind(operationId);
    if (prior != operations_.cend()) {
        if (prior->subjectKind != subjectKind || prior->subjectId != subjectId
            || prior->expectedRevision != expectedRevision || prior->patch != settingsPatch) {
            return setError(OperationResultCode::Conflict,
                            QStringLiteral("profiles.operation.conflict"),
                            QStringLiteral("Operation ID was reused with different profile values"));
        }
        UpdateReply replay = prior->reply;
        replay.changed = false;
        return replay;
    }

    auto profile = profiles_.find(keyFor(subjectKind, subjectId));
    if (profile == profiles_.end()) {
        return setError(OperationResultCode::NotFound, QStringLiteral("profiles.profile.not_found"),
                        QStringLiteral("Profile subject has no contract fixture"));
    }
    reply.mutation.revision = profile->revision;
    if (expectedRevision != profile->revision) {
        return setError(OperationResultCode::StaleRevision,
                        QStringLiteral("profiles.operation.stale_revision"),
                        QStringLiteral("Expected revision does not match current profile revision"));
    }
    bool changed = false;
    for (auto it = settingsPatch.cbegin(); it != settingsPatch.cend(); ++it) {
        if (!profile->settings.contains(it.key()) || profile->settings.value(it.key()) != it.value()) {
            changed = true;
            break;
        }
    }
    if (changed && (profile->revision == std::numeric_limits<quint64>::max()
                    || eventSequence_ == std::numeric_limits<quint64>::max())) {
        return setError(OperationResultCode::InternalError, QStringLiteral("profiles.counter.exhausted"),
                        QStringLiteral("Profile revision or event sequence is exhausted"));
    }

    if (changed) {
        for (auto it = settingsPatch.cbegin(); it != settingsPatch.cend(); ++it) {
            profile->settings.insert(it.key(), it.value());
        }
        ++profile->revision;
        ++eventSequence_;
    }
    reply.eventSequence = eventSequence_;
    reply.changed = changed;
    reply.mutation.code = OperationResultCode::Ok;
    reply.mutation.humanMessageKey = changed ? QStringLiteral("profiles.profile.updated")
                                              : QStringLiteral("profiles.profile.unchanged");
    reply.mutation.revision = profile->revision;
    UpdateReply cached = reply;
    cached.changed = false;
    operations_.insert(operationId, {subjectKind, subjectId, expectedRevision, settingsPatch, cached});
    return reply;
}

} // namespace adrenalin::contracts::profiles1
