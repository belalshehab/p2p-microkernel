//
// Created by Belal Shehab on 28/02/2026.
//

#include <iostream>
#include "NetworkListener.h"


NetworkListenerImpl::NetworkListenerImpl(kj::StringPtr name)
    : m_name(name)
    , m_orchestrator(capnp::Capability::Client(nullptr).castAs<Orchestrator>())
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
    auto port = context.getParams().getPort();
    std::cout << "[NetworkListener] Starting to listen on port " << port << "\n";

    // Ask the Orchestrator for the Validator cap, then forward a gossip message to it.
    // Both steps are chained inside one .then() to avoid RemotePromise flattening issues.
    return m_orchestrator.connectToValidatorRequest().send()
        .then([](auto response) -> kj::Promise<void> {
            auto validator = response.getValidator();
            auto req = validator.validateBlockRequest();
            req.setData("gossip packet from NetworkListener");
            return req.send().then([](auto validateResponse) {
                std::cout << "[NetworkListener] Validator says: isValid="
                          << validateResponse.getIsValid() << "\n";
            });
        });
}
