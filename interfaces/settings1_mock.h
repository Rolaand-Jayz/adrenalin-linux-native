#pragma once

#include "settings1_contract.h"

#include <QHash>
#include <QUuid>

class Settings1Mock final : public Settings1Contract
{
public:
    QString initializationState() const override { return QStringLiteral("READY"); }
    QString serviceInstanceUuid() const override { return instanceUuid_; }
    quint64 serviceGeneration() const override { return generation_; }
    ushort apiMajor() const override { return 1; }
    ushort apiMinor() const override { return 0; }
    Settings1ReadResult getProductTelemetryConsent() override;
    Settings1WriteResult setProductTelemetryConsent(const QString &operationId,
                                                    bool enabled,
                                                    quint64 expectedRevision) override;

private:
    QString instanceUuid_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    quint64 generation_ = 1;
    bool enabled_ = false;
    quint64 revision_ = 0;
    struct Operation { bool enabled; quint64 expected; quint64 result; };
    QHash<QString, Operation> operations_;
};
