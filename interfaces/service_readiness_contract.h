#pragma once

#include <QString>

class ServiceReadinessContract
{
public:
    virtual ~ServiceReadinessContract() = default;
    virtual QString initializationState() const = 0;
    virtual QString serviceInstanceUuid() const = 0;
    virtual quint64 serviceGeneration() const = 0;
    virtual ushort apiMajor() const = 0;
    virtual ushort apiMinor() const = 0;
    virtual QString lastInitializationError() const = 0;
};
