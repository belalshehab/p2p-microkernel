//
// Created by Belal Shehab on 28/02/2026.
//

#include <iostream>
#include <cstring>
#include <sodium.h>

#include "NetworkListener.h"

constexpr auto LISTENER_PRIVATE_KEY = "b97cd5bf35f02cd8f5f916a4221ca44bc10034342147c16865a54d4807f57c22";
constexpr auto VALIDATOR_PUBLIC_KEY = "cde70defd63dde1212450dd3aa92b2d2842bf317561d6d516c2b4031e342ff9a";


struct MockBlock {
    const uint8_t* data;
    size_t dataSize;
    uint8_t signature[crypto_sign_BYTES];
    const size_t signatureSize = crypto_sign_BYTES;
};

MockBlock buildMockBlock(bool valid) {
    MockBlock block;

    const char* message = "Hello Block";
    block.data = reinterpret_cast<const uint8_t*>(message);
    block.dataSize = std::strlen(message);

    uint8_t seed[crypto_sign_SEEDBYTES];
    sodium_hex2bin(
        seed, sizeof(seed),
        LISTENER_PRIVATE_KEY, 64,
        nullptr,
        nullptr,
        nullptr
    );

    uint8_t listenerPublicKey[crypto_sign_PUBLICKEYBYTES];
    uint8_t listenerExpandedPrivateKey[crypto_sign_SECRETKEYBYTES];
    crypto_sign_seed_keypair(
        listenerPublicKey,
        listenerExpandedPrivateKey,
        seed
    );

    crypto_sign_detached(block.signature, nullptr, block.data, block.dataSize, listenerExpandedPrivateKey);

    if (!valid) {
        block.signature[0] = ~block.signature[0]; // Corrupt the signature to make it invalid
    }
    return block;
}

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

            // --- Valid block ---
            std::cout << "[NetworkListener] Sending VALID block to validator...\n";
            auto validBlock = buildMockBlock(true);
            auto req1 = validator.validateBlockRequest();
            req1.setData(capnp::Data::Reader(validBlock.data, validBlock.dataSize));
            req1.setSignature(capnp::Data::Reader(validBlock.signature, validBlock.signatureSize));

            return req1.send().then([validator](auto validateResponse) mutable -> kj::Promise<void> {
                std::cout << "[NetworkListener] Valid block result: isValid="
                          << (validateResponse.getIsValid() ? "true" : "false") << "\n";

                // --- Invalid block ---
                std::cout << "[NetworkListener] Sending INVALID block to validator...\n";
                auto invalidBlock = buildMockBlock(false);
                auto req2 = validator.validateBlockRequest();
                req2.setData(capnp::Data::Reader(invalidBlock.data, invalidBlock.dataSize));
                req2.setSignature(capnp::Data::Reader(invalidBlock.signature, invalidBlock.signatureSize));

                return req2.send().then([](auto validateResponse2) {
                    std::cout << "[NetworkListener] Invalid block result: isValid="
                              << (validateResponse2.getIsValid() ? "true" : "false") << "\n";
                });
            });
        });
}
