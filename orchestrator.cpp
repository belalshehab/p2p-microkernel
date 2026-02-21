#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "ipc_common.h"
#include "SharedMemory.h"

pid_t spawn_process(const char* process_name, const char* binary_path, int socketPair[2]) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[Orchestrator] Fork failed to fork: " << process_name << ": " << strerror(errno) << std::endl;
        return -1;
    }
    if (pid == 0) {
        // child process
        fcntl(socketPair[1], F_SETFD, 0);
        close(socketPair[0]);

        char sockFDStr[16];
        snprintf(sockFDStr, sizeof(sockFDStr), "%d", socketPair[1]);

        execl(binary_path, process_name, sockFDStr, nullptr);
        // we shouldn't reach here unless exec fails
        std::cerr << "[Orchestrator] Exec failed for " << process_name << ": " << strerror(errno) << std::endl;
        exit(1);
    }
    // parent process
    close(socketPair[1]);
    std::cout << "[Orchestrator] Spawned Process (PID: " << pid << ")\n";
    return pid;
}

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
    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";

    int hasherSocketPair[2];
    int signerSocketPair[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, hasherSocketPair) < 0) {
        std::cerr << "[Orchestrator] Failed to create hasher socket pair: " << strerror(errno) << "\n";
        return 1;
    }
    std::cout << "[Orchestrator] Created socket pair for Hasher\n";
    fcntl(hasherSocketPair[0], F_SETFD, FD_CLOEXEC);
    fcntl(hasherSocketPair[1], F_SETFD, FD_CLOEXEC);

    std::cout << "[Orchestrator] Created socket pair for Signer\n";
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, signerSocketPair) < 0) {
        std::cerr << "[Orchestrator] Failed to create signer socket pair: " << strerror(errno) << "\n";
        close(hasherSocketPair[0]);
        close(hasherSocketPair[1]);
        return 1;
    }

    fcntl(signerSocketPair[0], F_SETFD, FD_CLOEXEC);
    fcntl(signerSocketPair[1], F_SETFD, FD_CLOEXEC);


    pid_t hasher_pid = spawn_process("hasher", "./hasher", hasherSocketPair);
    if (hasher_pid < 0) {
        return 1;
    }
    std::cout << "[Orchestrator] Spawned Hasher (PID: " << hasher_pid << ")\n";

    pid_t signer_pid = spawn_process("signer", "./signer", signerSocketPair);

    if (signer_pid < 0) {
        // Clean up hasher if signer fails to spawn
        kill(hasher_pid, SIGTERM);
        waitpid(hasher_pid, nullptr, 0);
        return 1;
    }
    std::cout << "[Orchestrator] Spawned Signer (PID: " << signer_pid << ")\n";

    std::cout << "[Orchestrator] Connecting to child processes...\n";

    if (!connectToService(hasherSocketPair[0], "Hasher")) {
        std::cerr << "[Orchestrator] Failed to connect to Hasher service\n";
        return 1;
    }
    if (!connectToService(signerSocketPair[0], "Signer")) {
        std::cerr << "[Orchestrator] Failed to connect to Signer service\n";
        return 1;
    }

    SharedMemory sharedMemory;
    if (!sharedMemory.create(SHARED_MEMORY_SIZE)) {
        std::cerr << "[Orchestrator] Failed to create shared memory object\n";
        return 1;
    }
    if (!sendFD(hasherSocketPair[0], sharedMemory.fd(), sharedMemory.size(), "Orchestrator")) {
        std::cerr << "[Orchestrator] Failed to send shared memory FD to Hasher\n";
        return 1;
    }

    std::cout << "[Orchestrator] Sent shared memory FD to Hasher (FD: " << sharedMemory.fd() << ", Size: " << sharedMemory.size() << " bytes)\n";


    std::cout << "[Orchestrator] simulating processing time\n";
    sleep(2);

    std::cout << "[Orchestrator] Setting data in shared memory for Hasher to process...\n";
    char* dataToBeSent = "Hello from Orchestrator! This is shared memory data.";
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
        if (finished_pid == hasher_pid) {
            std::cout << "[Orchestrator] Hasher (PID: " << hasher_pid << ") finished with status " << WEXITSTATUS(status) << "\n";
        } else if (finished_pid == signer_pid) {
            std::cout << "[Orchestrator] Signer (PID: " << signer_pid << ") finished with status " << WEXITSTATUS(status) << "\n";
        } else {
            std::cout << "[Orchestrator] Unknown child process (PID: " << finished_pid << ") finished with status " << WEXITSTATUS(status) << "\n";
        }
    }
    return 0;
}