//
// Created by Belal Shehab on 28/02/2026.
//

#include <iostream>
#include <cstring>
#include <memory>
#include <mutex>
#include <sodium.h>

#include "GossipNode.h"
#include "nim_gossip_node.h"

#include <thread>

extern "C" void NimMain();

// ── Cross-thread gossip signal ────────────────────────────────────────────────
struct GossipSignal {
    std::mutex mutex;
    std::vector<uint8_t> data;
    std::vector<uint8_t> signature;
    std::vector<uint8_t> senderId;
};

static GossipSignal g_gossipSignal;
static KeyGuard::Client *g_keyGuardCap = nullptr;
static kj::Own<kj::CrossThreadPromiseFulfiller<void> > *g_fulfiller = nullptr;

// Called by Nim thread — must NOT touch KJ event loop directly
extern "C" int nimGossipNodeOnGossip(
    const uint8_t *senderId, size_t senderIdSize,
    const uint8_t *data, size_t dataSize,
    const uint8_t *signature, size_t signatureSize
) {
    std::cout << "[C++] onGossip called — signaling KJ event loop\n";
    if (!g_fulfiller || !g_keyGuardCap) {
        std::cerr << "[C++] onGossip not ready\n";
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(g_gossipSignal.mutex);
        g_gossipSignal.data.assign(data, data + dataSize);
        g_gossipSignal.signature.assign(signature, signature + signatureSize);
        g_gossipSignal.senderId.assign(senderId, senderId + senderIdSize);
    }

    (*g_fulfiller)->fulfill();
    return 0;
}

GossipNodeImpl::GossipNodeImpl(kj::StringPtr name)
    : m_name(name)
      , m_orchestrator(capnp::Capability::Client(nullptr).castAs<Orchestrator>()) {
}

kj::Promise<void> GossipNodeImpl::getName(GetNameContext context) {
    context.getResults().setName(m_name);
    return kj::READY_NOW;
}

kj::Promise<void> GossipNodeImpl::ping(PingContext context) {
    std::cout << "[GossipNode] Ping" << std::endl;
    return kj::READY_NOW;
}

kj::Promise<void> GossipNodeImpl::startListening(StartListeningContext context) {
    auto port = context.getParams().getPort();
    auto peerAddrs = context.getParams().getPeerAddrs();
    std::cout << "[GossipNode] startListening on port " << port << "\n";

    return m_orchestrator.connectToKeyGuardRequest().send().then(
        [this, port, peerAddrs](auto response) -> kj::Promise<void> {
            m_keyGuard = kj::heap<KeyGuard::Client>(response.getKeyGuard());
            g_keyGuardCap = m_keyGuard.get();

            std::thread([port, peerAddrs]() {
                std::vector<std::string> addrs;
                for (auto addr: peerAddrs) {
                    addrs.push_back(addr.cStr());
                }
                std::vector<const char *> cstrs;
                for (auto &s: addrs) {
                    cstrs.push_back(s.c_str());
                }
                NimMain();
                nimGossipNodeInit(
                    static_cast<uint16_t>(port),
                    cstrs.data(),
                    static_cast<int>(cstrs.size())
                );
            }).detach();

            return gossipLoop();
        });
}

kj::Promise<void> GossipNodeImpl::publishData(PublishDataContext context) {
    auto data = context.getParams().getData();

    // Copy data into owned buffer so it survives the async chain
    auto dataVec = kj::heapArray<uint8_t>(data.size());
    memcpy(dataVec.begin(), data.begin(), data.size());

    auto req = m_keyGuard->signDataRequest();
    req.setData(capnp::Data::Reader(dataVec.begin(), data.size()));

    return req.send().then([this, dataVec = kj::mv(dataVec)](auto response) -> kj::Promise<void> {
        auto signature = response.getSignature();

        //TODO: reolace with real node ID once libp2p is integrated
        const char *senderId = "node-1";
        nimGossipNodePublish(
            reinterpret_cast<const uint8_t *>(senderId), strlen(senderId),
            dataVec.begin(), dataVec.size(),
            signature.begin(), signature.size()
        );
        return kj::READY_NOW;
    });
}

kj::Promise<void> GossipNodeImpl::gossipLoop() {
    auto paf = kj::newPromiseAndCrossThreadFulfiller<void>();
    m_fulfiller = kj::mv(paf.fulfiller);
    g_fulfiller = &m_fulfiller;

    return paf.promise.then([this]() -> kj::Promise<void> {
        std::cout << "[C++] gossipLoop — got gossip signal, forwarding to KeyGuard\n";
        std::vector<uint8_t> data, signature, senderId;
        {
            std::lock_guard<std::mutex> lock(g_gossipSignal.mutex);
            data = g_gossipSignal.data;
            signature = g_gossipSignal.signature;
            senderId = g_gossipSignal.senderId;
        }

        auto req = g_keyGuardCap->validateBlockRequest();
        auto msg = req.initMessage();
        msg.setData(capnp::Data::Reader(data.data(), data.size()));
        msg.setSignature(capnp::Data::Reader(signature.data(), signature.size()));
        msg.setSenderId(capnp::Data::Reader(senderId.data(), senderId.size()));

        return req.send().then([this](auto response) -> kj::Promise<void> {
            bool isValid = response.getIsValid();
            auto sigReader = response.getValidatorSignature();
            std::cout << "[C++] validation response: isValid=" << isValid << "\n";
            nimGossipNodeOnValidated(
                isValid,
                const_cast<uint8_t *>(sigReader.begin()),
                sigReader.size()
            );

            return gossipLoop();
        });
    });
}
