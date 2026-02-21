#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "ipc_common.h"
#include "SharedMemory.h"
#include "ServiceHandle.h"
#include "ServicesRegistry.h"



bool connectToService(int socketFD, const char* serviceName) {
    Message message;
    if (!receiveMessage(socketFD, message, "Orchestrator")) {
        std::cerr << "[Orchestrator] Failed to receive CONNECT_REQUEST from " << serviceName << "\n";
        return false;
    }
    if (message.type != CONNECT_REQUEST) {
        std::cerr << "[Orchestrator] Unexpected message type from " << serviceName << ": " << message.type << "\n";
        return false;
    }

    std::cout << "[Orchestrator] Received CONNECT_REQUEST from " << serviceName << ": " << message.payload << "\n";

    Message response;
    response.type = CONNECT_RESPONSE;
    response.payloadSize = snprintf(response.payload, sizeof(response.payload), "Connection established");

    if (!sendMessage(socketFD, response, "Orchestrator")) {
        std::cerr << "[Orchestrator] Failed to send CONNECT_RESPONSE to " << serviceName << "\n";
        return false;
    }
    std::cout << "[Orchestrator] Sent CONNECT_RESPONSE to " << serviceName << "\n";
    return true;

}


int main()
{
    ServicesRegistry services;

    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";

    ServiceHandle *hasherHandler = services.registerService("hasher", "./hasher");

    if (!hasherHandler){
    std::cerr << "[Orchestrator] Failed to create Hasher service\n";
        return 1;
    }

    std::cout << "[Orchestrator] Spawned Hasher (PID: " << hasherHandler->pid() << ")\n";


    ServiceHandle *signerHandler = services.registerService("signer", "./signer");

    if (!signerHandler) {
        std::cerr << "[Orchestrator] Failed to create Signer service\n";
        services.unregisterAllServices();
        return 1;
    }


    std::cout << "[Orchestrator] Spawned Signer (PID: " << signerHandler->pid() << ")\n";

    std::cout << "[Orchestrator] Connecting to child processes...\n";

    if (!connectToService(hasherHandler->orchestratorSocketFD(), "Hasher")) {
        std::cerr << "[Orchestrator] Failed to connect to Hasher service\n";
        services.unregisterAllServices();
        return 1;
    }
    if (!connectToService(signerHandler->orchestratorSocketFD(), "Signer")) {
        std::cerr << "[Orchestrator] Failed to connect to Signer service\n";
        services.unregisterAllServices();
        return 1;
    }

    SharedMemory sharedMemory;
    if (!sharedMemory.create(SHARED_MEMORY_SIZE)) {
        std::cerr << "[Orchestrator] Failed to create shared memory object\n";
        services.unregisterAllServices();
        return 1;
    }
    if (!sendFD(hasherHandler->orchestratorSocketFD(), sharedMemory.fd(), sharedMemory.size(), "Orchestrator")) {
        std::cerr << "[Orchestrator] Failed to send shared memory FD to Hasher\n";
        services.unregisterAllServices();
        return 1;
    }

    std::cout << "[Orchestrator] Sent shared memory FD to Hasher (FD: " << sharedMemory.fd() << ", Size: " << sharedMemory.size() << " bytes)\n";


    std::cout << "[Orchestrator] simulating processing time\n";
    sleep(2);

    std::cout << "[Orchestrator] Setting data in shared memory for Hasher to process...\n";
    const char* dataToBeSent = "Hello from Orchestrator! This is shared memory data.";
    memcpy(sharedMemory.data(), dataToBeSent, strlen(dataToBeSent) + 1);
    sharedMemory.header()->inputReady.store(true, std::memory_order_release);

    while (!sharedMemory.header()->outputReady.load(std::memory_order_acquire)) {
        std::cout << "[Orchestrator] Waiting for data to be sent...\n";
        sleep(1);
    }

    std::vector<char> dataToBeReceived(sharedMemory.size());
    //
    memcpy(dataToBeReceived.data(), sharedMemory.data(), sharedMemory.size());

    std::cout << "[Orchestrator] Received shared memory data: " << dataToBeReceived.data() << "\n";

    // Wait for both children to complete
    int status;
    pid_t finished_pid;
    while ((finished_pid = wait(&status)) > 0) {
        if (finished_pid == hasherHandler->pid()) {
            std::cout << "[Orchestrator] Hasher (PID: " << hasherHandler->pid() << ") finished with status " << WEXITSTATUS(status) << "\n";
        } else if (finished_pid == signerHandler->pid()) {
            std::cout << "[Orchestrator] Signer (PID: " << signerHandler->pid() << ") finished with status " << WEXITSTATUS(status) << "\n";
        } else {
            std::cout << "[Orchestrator] Unknown child process (PID: " << finished_pid << ") finished with status " << WEXITSTATUS(status) << "\n";
        }
    }
    return 0;
}