#include <iostream>
#include "validator.h"


ValidatorImpl::ValidatorImpl(kj::StringPtr name)
    : m_name(name)
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
    std::cout << "[Validator] Validating block of data: " << data.cStr() << std::endl;

    //TODO: implement validation logic here
    context.getResults().setSignature("Validated");
    return kj::READY_NOW;
}
