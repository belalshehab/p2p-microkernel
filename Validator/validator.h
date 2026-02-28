//
// Created by Belal Shehab on 28/02/2026.
//

#pragma once

#include <capnp/ez-rpc.h>
#include "orchestrator.capnp.h"

class ValidatorImpl final: public Validator::Server {
public:
    explicit ValidatorImpl(kj::StringPtr name);

    kj::Promise<void> getName(GetNameContext context) override;
    kj::Promise<void> ping(PingContext context) override;
    kj::Promise<void> validateBlock(ValidateBlockContext context) override;

private:
    std::string m_name;
};