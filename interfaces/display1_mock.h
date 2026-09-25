#pragma once

#include "display1_contract.h"

#include <QHash>

namespace adrenalin::contracts::display1 {

class Mock final : public Contract
{
public:
    enum class ReadinessState { Ready, Recovering, Failed };

    Mock();

    void setReadinessStateForTesting(ReadinessState state) { readinessState_ = state; }

    ListReply listDisplays() const override;
    StateReply getDisplayState(const QString &subjectId) const override;
    ValidationReply validateDisplay(const QString &operationId, const QString &subjectId,
                                    quint64 expectedInventoryGeneration,
                                    quint64 expectedCapabilityGeneration,
                                    const QList<ControlChange> &changes) const override;
    ApplyReply applyDisplay(const QString &operationId, const QString &subjectId,
                            quint64 expectedInventoryGeneration,
                            quint64 expectedCapabilityGeneration,
                            const QList<ControlChange> &changes) override;

    static QString testDisplaySubjectId();

private:
    struct Operation final {
        QString subjectId;
        quint64 inventoryGeneration = 0;
        quint64 capabilityGeneration = 0;
        QList<ControlChange> changes;
        ApplyReply reply;
    };

    QString serviceInstanceUuid_ = QStringLiteral("123e4567-e89b-12d3-a456-426614174000");
    quint64 serviceGeneration_ = 2;
    quint64 inventoryGeneration_ = 5;
    quint64 capabilityGeneration_ = 9;
    quint64 eventSequence_ = 30;
    quint64 revision_ = 1;
    ReadinessState readinessState_ = ReadinessState::Ready;
    QList<Capability> capabilities_;
    QHash<QString, Operation> operations_;
};

} // namespace adrenalin::contracts::display1
