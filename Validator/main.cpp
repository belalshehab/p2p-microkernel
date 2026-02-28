//
// Created by Belal Shehab on 28/02/2026.
//

#include <iostream>
#include <unistd.h>
#include <capnp/rpc-twoparty.h>
#include "validator.h"

int main(int argc, char* argv[]) {
    std::cout << "[Validator] Service initialized, PID: " << getpid() << "\n";

    if (argc < 2) {
        std::cerr << "[Validator] Error: No socket FD provided\n";
        return 1;
    }

    int socketFD = atoi(argv[1]);

    auto ioContext = kj::setupAsyncIo();
    auto stream = ioContext.lowLevelProvider->wrapSocketFd(
        socketFD,
        kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP
        );
    capnp::TwoPartyServer server(kj::heap<ValidatorImpl>("Validator"));
    server.accept(*stream).wait(ioContext.waitScope);

    return 0;
}
