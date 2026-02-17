#include <iostream>
#include <unistd.h>
#include <signal.h>

int main()
{
    std::cout << "[Orchestrator] Starting microkernel...\n";
    std::cout << "[Orchestrator] PID: " << getpid() << "\n";

    pid_t hasher_pid = fork();
    if (hasher_pid < 0) {
        std::cerr << "[Orchestrator] Fork failed to fork hasher: " << strerror(errno) << std::endl;
        return 1;
    }

    if (hasher_pid == 0) {
        // Child process - will become Hasher
        std::cout << "[Hasher] Process started, PID: " << getpid() << "\n";
        std::cout << "[Hasher] Waiting for work...\n";
        sleep(5);
        std::cout << "[Hasher] Exiting.\n";
        return 0;
    }

    pid_t signer_pid = fork();
    if (signer_pid < 0) {
        std::cerr << "[Orchestrator] Fork failed to fork signer: " << strerror(errno) << std::endl;
        // Clean up hasher
        kill(hasher_pid, SIGTERM);
        waitpid(hasher_pid, nullptr, 0);
        return 1;
    }

    if (signer_pid == 0) {
        // Child process - will become Signer
        std::cout << "[Signer] Process started, PID: " << getpid() << "\n";
        std::cout << "[Signer] Waiting for work...\n";
        sleep(5);
        std::cout << "[Signer] Exiting.\n";
        return 0;
    }
    // Parent - orchestrator
    std::cout << "[Orchestrator] Spawned Hasher (PID: " << hasher_pid << ")\n";
    std::cout << "[Orchestrator] Spawned Signer (PID: " << signer_pid << ")\n";
    std::cout << "[Orchestrator] Monitoring child processes...\n";

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