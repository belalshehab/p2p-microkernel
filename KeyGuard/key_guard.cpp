#include <iostream>
#include "key_guard.h"
#include <map>


const auto node1 = "12D3KooWSvR8BKSPus2phMeQyErvomKpv9FhsrVWAiQ15GUtTdwU";
const auto node2 = "12D3KooWJRKrXXqGybdRWnjQBMCiR6757UCEgTZsNSY3fnzWUM7s";

static const std::unordered_map<std::string, const char*> PRIVATE_KEYS = {
    { node1, "8b12a312e6df7a37eed915a7b005ed9d7c534ccff3f1fdcfb3bac9d605b29e44" },
    { node2, "a2333759dfbb47b5a5b2d4ef241e32297199655af27da1090005e96358e98e2a" }
};

static const std::unordered_map<std::string, const char*> KNOWN_NODES = {
    { node1, "c054fcb7717ac01bca5684c91e2e49643eabbb5ad67db2c8410621d3118c68d9" },
    { node2, "07e0e800e708edd9238f552c23073a224a86a51ae03ab190a47958db08c8178a" }
};

KeyGuardImpl::KeyGuardImpl(kj::StringPtr name)
    : m_name(name)
    , m_orchestrator(capnp::Capability::Client(nullptr).castAs<Orchestrator>())
{
    uint8_t seed[crypto_sign_SEEDBYTES];
    sodium_hex2bin(
        seed, sizeof(seed),
        PRIVATE_KEYS.at(node1), 64,
        nullptr, nullptr, nullptr
    );

    crypto_sign_seed_keypair(
        m_keyGuardPublicKey,
        m_keyGuardExpandedPrivateKey,
        seed
    );

    for (auto & [id, hexPubKey] : KNOWN_NODES) {
        std::array<uint8_t, crypto_sign_PUBLICKEYBYTES> pkArr;
        sodium_hex2bin(pkArr.data(), pkArr.size(), hexPubKey, 64, nullptr, nullptr, nullptr);
        m_trustedPeers[id] = pkArr;
        std::cout << "[KeyGuard] Hardcoded known node: " << id << "\n";
    }
}

kj::Promise<void> KeyGuardImpl::getName(GetNameContext context) {
    context.getResults().setName(m_name);
    return kj::READY_NOW;
}

kj::Promise<void> KeyGuardImpl::ping(PingContext context) {
    std::cout << "[KeyGuard] Ping" << std::endl;
    return kj::READY_NOW;
}

kj::Promise<void> KeyGuardImpl::validateBlock(ValidateBlockContext context) {
    auto msg       = context.getParams().getMessage();
    auto data      = msg.getData();
    auto signature = msg.getSignature();
    auto senderId = msg.getSenderId();
    // senderId available as msg.getSenderId() for future per-peer key lookup
    std::cout << "[KeyGuard] Validating block, data size: " << data.size()
              << ", sig size: " << signature.size() << "\n";

    std::string key(reinterpret_cast<const char*>(senderId.begin()), senderId.size());

    // auto it = m_trustedPeers.find(key);
    // if (it == m_trustedPeers.end()) {
    //     std::cout << "[KeyGuard] Unknown sender — rejecting\n";
    //     context.getResults().setIsValid(false);
    //     context.getResults().setValidatorSignature(capnp::Data::Reader(nullptr, 0));
    //     return kj::READY_NOW;
    // }

    int result = crypto_sign_verify_detached(
        signature.begin(),
        data.begin(),
        data.size(),
        m_trustedPeers.at(node1).data()
    );
    bool isValid = (result == 0);
    std::cout << "[KeyGuard] Block signature: " << (isValid ? "VALID" : "INVALID") << "\n";

    std::vector<uint8_t> message(data.begin(), data.end());
    message.push_back(isValid ? 0x01 : 0x00);

    uint8_t keyGuardSignature[crypto_sign_BYTES];
    crypto_sign_detached(
        keyGuardSignature,
        nullptr,
        message.data(),
        message.size(),
        m_keyGuardExpandedPrivateKey
    );
    std::cout << "[KeyGuard] Response signed with KeyGuard private key\n";

    context.getResults().setIsValid(isValid);
    context.getResults().setValidatorSignature(
        capnp::Data::Reader(keyGuardSignature, sizeof(keyGuardSignature))
    );
    return kj::READY_NOW;
}

kj::Promise<void> KeyGuardImpl::signData(SignDataContext context) {
    auto data = context.getParams().getData();

    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(
        signature,
        nullptr,
        data.begin(),
        data.size(),
        m_keyGuardExpandedPrivateKey
    );

    std::cout << "[KeyGuard] Data signed with KeyGuard private key, data size: " << data.size() << "\n";
    context.getResults().setSignature(capnp::Data::Reader(signature, sizeof(signature)));
    return kj::READY_NOW;
}

kj::Promise<void> KeyGuardImpl::addTrustedPeer(AddTrustedPeerContext context) {
    auto peerId = context.getParams().getPeerId();
    auto publicKeyData = context.getParams().getPublicKey();

    if (publicKeyData.size() != crypto_sign_PUBLICKEYBYTES) {
        std::cerr << "[KeyGuard] Invalid public key size: " << publicKeyData.size() << "\n";
        return kj::READY_NOW;
    }

    std::string key(reinterpret_cast<const char*>(peerId.begin()), peerId.size());
    std::array<uint8_t, crypto_sign_PUBLICKEYBYTES> publicKeyArr;
    std::memcpy(publicKeyArr.data(), publicKeyData.begin(), crypto_sign_PUBLICKEYBYTES);
    m_trustedPeers[key] = publicKeyArr;
    std::cout << "[KeyGuard] Added trusted peer, id size: " << peerId.size() << "\n";
    return kj::READY_NOW;
}
