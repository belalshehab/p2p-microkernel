//
// Created by Belal Shehab on 28/02/2026.
//

#pragma once

#include <capnp/ez-rpc.h>
#include "orchestrator.capnp.h"


class OrchestratorImpl: public Orchestrator::Server {
    public:
    OrchestratorImpl(Validator::Client validator, NetworkListener::Client listener);

    kj::Promise<void> connectToValidator(ConnectToValidatorContext context) override;
    kj::Promise<void> connectToNetworkListener(ConnectToNetworkListenerContext context) override;

    kj::Promise<void> getServices(GetServicesContext context) override;

private:
    Validator::Client m_validator;
    NetworkListener::Client m_listener;

};
