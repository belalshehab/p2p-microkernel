//
// Created by Belal Shehab on 28/02/2026.
//

#pragma once

#include <capnp/ez-rpc.h>
#include "orchestrator.capnp.h"
#include <string>

class NetworkListenerImpl final: public NetworkListener::Server {

public:
    explicit NetworkListenerImpl(kj::StringPtr name);

    kj::Promise<void> getName(GetNameContext context) override;
    kj::Promise<void> ping(PingContext context) override;
    kj::Promise<void> startListening(StartListeningContext context) override;

private:
    std::string m_name;
};


