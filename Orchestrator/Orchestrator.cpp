//
// Created by Belal Shehab on 28/02/2026.
//

#include "Orchestrator.h"


OrchestratorImpl::OrchestratorImpl(Validator::Client validator, NetworkListener::Client listener) :
    m_validator(kj::mv(validator))
,m_listener(kj::mv(listener))
{
}

kj::Promise<void> OrchestratorImpl::connectToValidator(ConnectToValidatorContext context) {
    context.getResults().setValidator(m_validator);
    return kj::READY_NOW;
}

kj::Promise<void> OrchestratorImpl::connectToNetworkListener(ConnectToNetworkListenerContext context) {
    context.getResults().setListener(m_listener);
    return kj::READY_NOW;
}

kj::Promise<void> OrchestratorImpl::getServices(GetServicesContext context) {
    auto results = context.getResults();
    auto list = results.initServices(2);
    list.set(0, "validator");
    list.set(1, "networkListener");
    return kj::READY_NOW;
}
