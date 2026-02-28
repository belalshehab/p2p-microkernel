#include <iostream>
#include <unistd.h>
#include <capnp/rpc-twoparty.h>
#include "NetworkListener.h"

int main(int argc, char* argv[]) {
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
    capnp::TwoPartyServer server(kj::heap<NetworkListenerImpl>("NetworkListener"));
    server.accept(*stream).wait(ioContext.waitScope);

    return 0;
}
