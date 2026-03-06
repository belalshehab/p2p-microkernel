#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstring>

#include "../ServicesRegistry.h"
#include "ServiceConnection.h"
#include "Orchestrator.h"

#include "orchestrator.capnp.h"
#include <capnp/rpc-twoparty.h>

// Usage:
//   orchestrator                              # node 1, port 12345, no bootstrap peers
//   orchestrator --port 12346 --peer /ip4/127.0.0.1/tcp/12345/p2p/<PeerID>
static void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--port <port>] [--peer <multiaddr>] ...\n"
              << "  --port  TCP port for the gossip node (default: 12345)\n"
              << "  --peer  Bootstrap peer multiaddr (can be repeated)\n";
}

int main(int argc, char* argv[]) {
    uint16_t port = 12345;
    std::vector<std::string> peerAddrs;

    // ── Parse CLI args ─────────────────────────────────────────────────────────
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--peer") == 0 && i + 1 < argc) {
            peerAddrs.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "[Orchestrator] Unknown argument: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    ServicesRegistry services;

    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";
    std::cout << "[Orchestrator] GossipNode port: " << port << "\n";
    if (!peerAddrs.empty()) {
        std::cout << "[Orchestrator] Bootstrap peers:\n";
        for (auto& p : peerAddrs) std::cout << "  " << p << "\n";
    }

    auto ioContext = kj::setupAsyncIo();

    auto orchestratorOwned = kj::heap<OrchestratorImpl>();
    OrchestratorImpl* orchestratorPtr = orchestratorOwned.get();
    Orchestrator::Client orchestratorCap = kj::mv(orchestratorOwned);

    auto keyGuardConn = spawnAndConnect(services, "keyGuard", "../KeyGuard/keyGuard", ioContext, orchestratorCap);
    if (!keyGuardConn) return 1;

    auto gossipNodeConn = spawnAndConnect(services, "gossipNode", "../GossipNode/gossipNode", ioContext, orchestratorCap);
    if (!gossipNodeConn) return 1;

    auto keyGuardClient   = keyGuardConn->getClient<KeyGuard>();
    auto gossipNodeClient = gossipNodeConn->getClient<GossipNode>();

    orchestratorPtr->setKeyGuard(keyGuardConn->getClient<KeyGuard>());
    orchestratorPtr->setGossipNode(gossipNodeConn->getClient<GossipNode>());

    // ── Ping both to confirm liveness ─────────────────────────────────────────
    keyGuardClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] KeyGuard ping OK\n";

    gossipNodeClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] GossipNode ping OK\n";

    // ── Tell the GossipNode to start ──────────────────────────────────────────
    auto req = gossipNodeClient.startListeningRequest();
    req.setPort(port);
    auto addrs = req.initPeerAddrs(peerAddrs.size());
    for (size_t i = 0; i < peerAddrs.size(); ++i)
        addrs.set(i, peerAddrs[i]);
    // startListening never returns (runs gossipLoop forever), so detach it
    req.send().detach([](kj::Exception&& e) {
        std::cerr << "[Orchestrator] startListening error: " << e.getDescription().cStr() << "\n";
    });
    std::cout << "[Orchestrator] GossipNode startListening(" << port << ") sent\n";

    // ── If we have peers (node 2), publish a test message after Nim starts ────
    if (!peerAddrs.empty()) {
        // Use evalAfterDelay to yield the event loop (non-blocking) for 5 seconds
        // This allows RPC messages to flow while we wait
        ioContext.provider->getTimer()
            .afterDelay(5 * kj::SECONDS)
            .wait(ioContext.waitScope);

        std::cout << "[Orchestrator] Publishing test message...\n";
        auto pubReq = gossipNodeClient.publishDataRequest();
        const char* msg = "Hello from node 2!";
        pubReq.setData(capnp::Data::Reader(
            reinterpret_cast<const uint8_t*>(msg), strlen(msg)));
        pubReq.send().wait(ioContext.waitScope);
        std::cout << "[Orchestrator] Published.\n";
    }

    kj::NEVER_DONE.wait(ioContext.waitScope);
    return 0;
}
