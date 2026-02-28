#include <iostream>
#include <unistd.h>

#include "../ServiceHandle.h"
#include "../ServicesRegistry.h"

#include "orchestrator.capnp.h"
#include <capnp/ez-rpc.h>
#include <capnp/rpc-twoparty.h>


int main() {
    ServicesRegistry services;

    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";


    ServiceHandle *validatorHandler = services.registerService("validator", "./validator");

    if (!validatorHandler) {
        std::cerr << "[Orchestrator] Failed to create Validator service\n";
        return 1;
    }


    std::cout << "[Orchestrator] Spawned Validator (PID: " << validatorHandler->pid() << ")\n";

    sleep(1);

    auto ioContext = kj::setupAsyncIo();
    auto validatorStream = ioContext.lowLevelProvider->wrapSocketFd(
        validatorHandler->orchestratorSocketFD(),
        kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP
        );

    capnp::TwoPartyClient validatorRpcClient(*validatorStream);
    Validator::Client validatorClient = validatorRpcClient.bootstrap().castAs<Validator>();

    auto pingRequest = validatorClient.pingRequest();
    pingRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Sent ping request to Validator and it is OK\n";

    auto validateRequest = validatorClient.validateBlockRequest();
    validateRequest.setData("Hello from Orchestrator!");
    auto validateResponse = validateRequest.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Received validation from Validator: " << validateResponse.getSignature().cStr() << "\n";




    ServiceHandle *networkListenerHandler = services.registerService("networkListener", "./networkListener");

    if (!networkListenerHandler) {
        std::cerr << "[Orchestrator] Failed to create NetworkListener service\n";
        return 1;
    }


    std::cout << "[Orchestrator] Spawned NetworkListener (PID: " << networkListenerHandler->pid() << ")\n";

    sleep(1);

    // auto networkListenerIoContext = kj::setupAsyncIo();
    auto networkListenerStream = ioContext.lowLevelProvider->wrapSocketFd(
        networkListenerHandler->orchestratorSocketFD(),
        kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP
        );

    capnp::TwoPartyClient networkListenerRpcClient(*networkListenerStream);
    NetworkListener::Client networkListenerClient = networkListenerRpcClient.bootstrap().castAs<NetworkListener>();

    auto pingRequest2 = networkListenerClient.pingRequest();
    pingRequest2.send().wait(ioContext.waitScope);
    std::cout << "[Orchestrator] Sent ping request to NetworkListener and it is OK\n";

    auto startListeningRequest = networkListenerClient.startListeningRequest();
    startListeningRequest.setPort(12345);
    auto startListeningResponse = startListeningRequest.send().wait(ioContext.waitScope);



    return 0;
}
