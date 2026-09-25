#pragma once

#include "hotkeys1_contract.h"

#include <QHash>

namespace adrenalin::contracts::hotkeys1 {

class Mock final : public Contract
{
public:
    Mock();

    Reply listActions(QList<Action> *actions) const override;
    UpdateReply setBinding(const QString &actionId, const QString &configuredBinding,
                           quint64 expectedRevision, const QString &operationId,
                           const QString &parentWindowId) override;

private:
    Action action_;
    QString serviceInstanceUuid_;
    quint64 serviceGeneration_ = 1;
    quint64 eventSequence_ = 1;
    quint64 revision_ = 1;
    QHash<QString, UpdateReply> completedOperations_;
    QHash<QString, QString> operationRequests_;
};

} // namespace adrenalin::contracts::hotkeys1
