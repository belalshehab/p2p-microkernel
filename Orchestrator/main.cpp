#include <iostream>
#include <unistd.h>

#include "../ServicesRegistry.h"
#include "ServiceConnection.h"
#include "Orchestrator.h"

#include "orchestrator.capnp.h"
#include <capnp/rpc-twoparty.h>

int main() {
    ServicesRegistry services;

    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";

    // Single event loop for the entire process lifetime
    auto ioContext = kj::setupAsyncIo();

    // ── Spawn both services ───────────────────────────────────────────────────
    auto validatorConn = spawnAndConnect(services, "validator", "./validator", ioContext);
    if (!validatorConn) return 1;

    auto listenerConn = spawnAndConnect(services, "networkListener", "./networkListener", ioContext);
    if (!listenerConn) return 1;

    // ── Get typed clients ─────────────────────────────────────────────────────
    auto validatorClient = validatorConn->getClient<Validator>();
    auto listenerClient  = listenerConn->getClient<NetworkListener>();

    // ── Ping both to confirm liveness ─────────────────────────────────────────
    validatorClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Validator ping OK\n";

    listenerClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] NetworkListener ping OK\n";

    // ── Smoke-test both services ──────────────────────────────────────────────
    auto validateRequest = validatorClient.validateBlockRequest();
    validateRequest.setData("Hello from Orchestrator!");
    auto validateResponse = validateRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Validator response: " << validateResponse.getSignature().cStr() << "\n";

    auto startListeningRequest = listenerClient.startListeningRequest();
    startListeningRequest.setPort(12345);
    startListeningRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] NetworkListener startListening(12345) OK\n";

    // ── Construct and hold the Orchestrator broker ────────────────────────────
    // Clients are retrieved again since getClient() re-casts the same bootstrap cap.
    auto orchestratorImpl = kj::heap<OrchestratorImpl>(
        validatorConn->getClient<Validator>(),
        listenerConn->getClient<NetworkListener>()
    );

    std::cout << "[Orchestrator] Broker ready. Holding " << 2 << " live service capabilities.\n";

    // TODO: serve orchestratorImpl over a socketpair so services can call back into it
    // (next step: pass a second FD to each child at spawn time)

    return 0;
}
