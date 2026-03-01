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

    // ── Construct the broker first (empty) ───────────────────────────────────
    // We need the Orchestrator::Client before spawning so each child gets it
    // as a bootstrap capability over their socket. Clients are injected via
    // setters once the connections are established.
    auto  orchestratorOwned = kj::heap<OrchestratorImpl>();
    OrchestratorImpl* orchestratorPtr = orchestratorOwned.get();
    Orchestrator::Client orchestratorCap = kj::mv(orchestratorOwned);

    // ── Spawn both services, exporting the orchestrator cap to each ──────────
    auto validatorConn = spawnAndConnect(services, "validator", "../Validator/validator", ioContext, orchestratorCap);
    if (!validatorConn) return 1;

    auto listenerConn = spawnAndConnect(services, "networkListener", "../Network_Listener/networkListener", ioContext, orchestratorCap);
    if (!listenerConn) return 1;

    // ── Get typed clients from each connection ────────────────────────────────
    auto validatorClient = validatorConn->getClient<Validator>();
    auto listenerClient  = listenerConn->getClient<NetworkListener>();

    // ── Inject live clients into the broker ───────────────────────────────────
    orchestratorPtr->setValidator(validatorConn->getClient<Validator>());
    orchestratorPtr->setListener(listenerConn->getClient<NetworkListener>());

    // ── Ping both to confirm liveness ─────────────────────────────────────────
    validatorClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Validator ping OK\n";

    listenerClient.pingRequest().send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] NetworkListener ping OK\n";

    // ── Smoke-test both services ──────────────────────────────────────────────
    auto validateRequest = validatorClient.validateBlockRequest();
    validateRequest.setData("Hello from Orchestrator!");
    validateRequest.setSignature("signatureToValidateAgainest");
    auto validateResponse = validateRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Validator is SignatureValid: " << validateResponse.getIsValid()
    << ", hash: " << validateResponse.getHash().cStr()<< "\n";

    auto startListeningRequest = listenerClient.startListeningRequest();
    startListeningRequest.setPort(12345);
    startListeningRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] NetworkListener startListening(12345) OK\n";

    std::cout << "[Orchestrator] Broker ready. Both services connected and reachable.\n";

    return 0;
}
