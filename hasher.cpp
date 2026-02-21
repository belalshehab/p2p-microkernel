#include <iostream>
#include <unistd.h>

#include "ipc_common.h"
#include "SharedMemory.h"

int main(int argc, char* argv[]) {
    std::cout << "[Hasher] Service initialized, PID: " << getpid() << "\n";

    if (argc < 2) {
        std::cerr << "[Hasher] Error: No socket FD provided\n";
        return 1;
    }

    int socketFD = atoi(argv[1]);
    std::cout << "[Hasher] Received socket FD: " << socketFD << "\n";

    std::cout << "[Hasher] Service running, PID: " << getpid() << "\n";

    Message connectMessage;
    connectMessage.type = CONNECT_REQUEST;
    connectMessage.payloadSize = snprintf(connectMessage.payload, sizeof(connectMessage.payload), "Hasher service ready");

    std::cout << "[Hasher] Sending CONNECT_REQUEST to orchestrator...\n";
    if (!sendMessage(socketFD, connectMessage, "Hasher")) {
        std::cerr << "[Hasher] Failed to send CONNECT_REQUEST\n";
        return 1;
    }
    Message responseMessage;
    if (!receiveMessage(socketFD, responseMessage, "Hasher")) {
        std::cerr << "[Hasher] Failed to receive CONNECT_RESPONSE\n";
        return 1;
    }

    if (responseMessage.type == CONNECT_RESPONSE) {
        std::cout << "[Hasher] Received CONNECT_RESPONSE: " << responseMessage.payload << "\n";
        std::cout << "[Hasher] Successfully connected to orchestrator!\n";
    } else {
        std::cerr << "[Hasher] Unexpected message type: " << responseMessage.type << "\n";
        return 1;
    }

    Message sharedMemoryMessage;
    int sharedMemoryFD = -1;
    std::cout << "[Hasher] Waiting for shared memory FD...\n";
    receiveFD(socketFD, sharedMemoryFD, sharedMemoryMessage, "Hasher");
    if (sharedMemoryMessage.type != SHM_FD_TRANSFER) {
        std::cerr << "[Hasher] Expected SHM_FD_TRANSFER message, got type " << sharedMemoryMessage.type << "\n";
        return 1;
    }

    size_t sharedMemorySize = std::stoul(sharedMemoryMessage.payload);

    SharedMemory sharedMemorySegment;

    if (!sharedMemorySegment.attach(sharedMemoryFD, sharedMemorySize)) {
        std::cerr << "[Hasher] Failed to attach to shared memory segment with FD " << sharedMemoryFD << "\n";
        return 1;
    }
    std::cout << "[Hasher] Successfully attached to shared memory segment (FD: " << sharedMemoryFD << ", Size: " << sharedMemorySize << " bytes)\n";

    std::cout << "[Hasher] Waiting for data to be set in shared memeory\n";

    while (!sharedMemorySegment.header()->inputReady.load(std::memory_order_acquire)) {
        std::cout << "[Hasher] Waiting for data to be sent...\n";
        sleep(1);
    }

    std::vector<char> inputData(sharedMemorySize);
    memcpy(inputData.data(), sharedMemorySegment.data(), sharedMemorySize);

    std::cout << "[Hasher] Received shared data: " << inputData.data() << "\n";

    std::cout << "[Hasher] Processing data...\n";
    sleep(3); // Simulate some processing time
    char *outputData = "Hash result from Hasher";
    memcpy(sharedMemorySegment.data(), outputData, strlen(outputData) + 1);
    sharedMemorySegment.header()->inputReady.store(false, std::memory_order_release);
    sharedMemorySegment.header()->outputReady.store(true, std::memory_order_release);
    std::cout << "[Hasher] Successfully finished processing data. Output written to shared memory.\n";

    std::cout << "[Hasher] Service shutting down.\n";

    return 0;
}
