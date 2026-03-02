#include <iostream>
#include <unistd.h>
#include <sodium.h>
#include <capnp/rpc-twoparty.h>
#include "NetworkListener.h"
#include "orchestrator.capnp.h"

int main(int argc, char* argv[]) {
    if (sodium_init() < 0) {
        std::cerr << "[NetworkListener] libsodium init failed\n";
        return 1;
    }
    std::cout << "[NetworkListener] Service initialized, PID: " << getpid() << "\n";

    if (argc < 2) {
        std::cerr << "[NetworkListener] Error: No socket FD provided\n";
        return 1;
    }

    int socketFD = atoi(argv[1]);

    auto ioContext = kj::setupAsyncIo();
    auto stream = ioContext.lowLevelProvider->wrapSocketFd(
        socketFD,
        kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP
    );

    // Construct the impl first, keep a raw pointer so we can inject the cap after.
    auto implOwned = kj::heap<NetworkListenerImpl>("NetworkListener");
    NetworkListenerImpl* implPtr = implOwned.get();

    // Construct the RPC client — exports our impl as bootstrap, imports orchestrator's bootstrap.
    capnp::TwoPartyClient rpc(
        *stream,
        kj::mv(implOwned),
        capnp::rpc::twoparty::Side::SERVER
    );

    // Now we can safely call bootstrap() — rpc is fully constructed.
    // Inject the orchestrator cap into the impl via the setter.
    implPtr->setOrchestrator(rpc.bootstrap().castAs<Orchestrator>());
    std::cout << "[NetworkListener] Got Orchestrator capability — ready to call back.\n";

    // Block until the connection is closed
    rpc.onDisconnect().wait(ioContext.waitScope);

    return 0;
}
