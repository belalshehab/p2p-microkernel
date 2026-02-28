//
// Created by Belal Shehab on 28/02/2026.
//

#pragma once

#include <kj/async-io.h>
#include <capnp/rpc-twoparty.h>
#include <iostream>
#include <memory>
#include <unistd.h>

#include "../ServiceHandle.h"
#include "../ServicesRegistry.h"

// Owns the stream and RPC client together so their lifetimes stay coupled.
struct ServiceConnection {
    kj::Own<kj::AsyncIoStream> stream;
    capnp::TwoPartyClient      rpcClient;

    explicit ServiceConnection(kj::Own<kj::AsyncIoStream> s)
        : stream(kj::mv(s)), rpcClient(*stream) {}

    // Non-copyable, non-movable — stream and rpcClient hold internal pointers to each other.
    ServiceConnection(const ServiceConnection&)            = delete;
    ServiceConnection& operator=(const ServiceConnection&) = delete;

    template<typename T>
    [[nodiscard]] auto getClient() -> typename T::Client {
        return rpcClient.bootstrap().castAs<T>();
    }
};

// Spawns a child process, wraps its socket FD into a ServiceConnection,
// and pings it to confirm liveness. Returns nullptr on failure.
inline std::unique_ptr<ServiceConnection> spawnAndConnect(
    ServicesRegistry&   services,
    const std::string&  name,
    const std::string&  binary,
    kj::AsyncIoContext& ioContext)
{
    ServiceHandle* handle = services.registerService(name.c_str(), binary.c_str());
    if (!handle) {
        std::cerr << "[Orchestrator] Failed to spawn " << name << "\n";
        return nullptr;
    }
    std::cout << "[Orchestrator] Spawned " << name << " (PID: " << handle->pid() << ")\n";

    sleep(1);

    auto stream = ioContext.lowLevelProvider->wrapSocketFd(
        handle->orchestratorSocketFD(),
        kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP);

    return std::make_unique<ServiceConnection>(kj::mv(stream));
}


