#pragma once

#include "profiles1_contract.h"

#include <QHash>

namespace adrenalin::contracts::profiles1 {

class Mock final : public Contract {
public:
    Mock();
    ReadReply readProfile(const QString &subjectKind, const QString &subjectId) const override;
    UpdateReply updateProfile(const QString &subjectKind, const QString &subjectId,
                              quint64 expectedRevision, const QString &operationId,
                              const QVariantMap &settingsPatch) override;

private:
    struct Operation final {
        QString subjectKind;
        QString subjectId;
        quint64 expectedRevision = 0;
        QVariantMap patch;
        UpdateReply reply;
    };

    QString keyFor(const QString &subjectKind, const QString &subjectId) const;
    QHash<QString, Profile> profiles_;
    QHash<QString, Operation> operations_;
    QString serviceInstanceUuid_ = QStringLiteral("123e4567-e89b-12d3-a456-426614174001");
    quint64 serviceGeneration_ = 1;
    quint64 eventSequence_ = 20;
};

} // namespace adrenalin::contracts::profiles1
