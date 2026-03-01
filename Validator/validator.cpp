#include <iostream>
#include "validator.h"


ValidatorImpl::ValidatorImpl(kj::StringPtr name)
    : m_name(name)
    , m_orchestrator(capnp::Capability::Client(nullptr).castAs<Orchestrator>())
{
}

kj::Promise<void> ValidatorImpl::getName(GetNameContext context) {
    context.getResults().setName(m_name);
    return kj::READY_NOW;
}

kj::Promise<void> ValidatorImpl::ping(PingContext context) {
    std::cout << "[Validator] Ping" << std::endl;
    return kj::READY_NOW;
}

kj::Promise<void> ValidatorImpl::validateBlock(ValidateBlockContext context) {
    auto data = context.getParams().getData();
    auto signature = context.getParams().getSignature();
    std::cout << "[Validator] Validating block of data: " << data.cStr() << std::endl;

    // TODO: implement real Ed25519 + SHA-256 validation with libsodium
    context.getResults().setIsValid(true);
    context.getResults().setHash("placeholder-hash");
    return kj::READY_NOW;
}
