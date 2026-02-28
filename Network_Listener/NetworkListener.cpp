//
// Created by Belal Shehab on 28/02/2026.
//

#include <iostream>
#include "NetworkListener.h"


NetworkListenerImpl::NetworkListenerImpl(kj::StringPtr name)
    : m_name(name)
{
}

kj::Promise<void> NetworkListenerImpl::getName(GetNameContext context) {
    context.getResults().setName(m_name);
    return kj::READY_NOW;
}

kj::Promise<void> NetworkListenerImpl::ping(PingContext context) {
    std::cout << "[NetworkListener] Ping" << std::endl;
    return kj::READY_NOW;
}

kj::Promise<void> NetworkListenerImpl::startListening(StartListeningContext context) {
    std::cout << "NetworkListener: Starting to listen on port " << context.getParams().getPort() << std::endl;
    //TODO: implement actual listening logic here
    return kj::READY_NOW;
}
