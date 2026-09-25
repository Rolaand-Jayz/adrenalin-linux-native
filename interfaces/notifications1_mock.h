#pragma once

#include "notifications1_contract.h"

#include <QHash>

namespace adrenalin::contracts::notifications1 {

class Mock final : public Contract {
public:
    Mock();
    ListReply listNotifications() const override;
    MarkReadReply markRead(const QString &notificationId, const QString &operationId,
                           quint64 expectedRevision) override;

private:
    struct Operation final {
        QString notificationId;
        quint64 expectedRevision = 0;
        MarkReadReply reply;
    };

    QList<Notification> notifications_;
    QHash<QString, Operation> operations_;
    QString serviceInstanceUuid_ = QStringLiteral("123e4567-e89b-12d3-a456-426614174000");
    quint64 serviceGeneration_ = 1;
    quint64 eventSequence_ = 10;
    quint64 revision_ = 1;
};

} // namespace adrenalin::contracts::notifications1
