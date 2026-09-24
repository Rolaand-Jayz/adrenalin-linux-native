#pragma once

#include <service1_adaptor.h>

class SessionServiceRootAdaptor final : public Service1Adaptor
{
public:
    explicit SessionServiceRootAdaptor(QObject *parent);
};

class SessionService;

void installService1PropertyNotifications(SessionService *service);
