//
// Created by Belal Shehab on 21/02/2026.
//

#include "ServiceHandle.h"

ServiceHandle::~ServiceHandle() {
    closeServiceHandle();
}

ServiceHandle::ServiceHandle(ServiceHandle &&other) noexcept
    : m_pid(other.m_pid)
      , m_orchestratorSocketFD(other.m_orchestratorSocketFD)
      , m_serviceSocketFD(other.m_serviceSocketFD)
      , m_name(std::move(other.m_name)) {
    other.m_pid = -1;
    other.m_orchestratorSocketFD = -1;
    other.m_serviceSocketFD = -1;
}

ServiceHandle &ServiceHandle::operator=(ServiceHandle &&other) noexcept {
    if (this != &other) {
        closeServiceHandle();

        m_pid = other.m_pid;
        m_orchestratorSocketFD = other.m_orchestratorSocketFD;
        m_serviceSocketFD = other.m_serviceSocketFD;
        m_name = std::move(other.m_name);

        other.m_pid = -1;
        other.m_orchestratorSocketFD = -1;
        other.m_serviceSocketFD = -1;
    }
    return *this;
}

bool ServiceHandle::init(const char *serviceName, const char *binary_path) {
    m_name = serviceName;
    int socketPair[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketPair) < 0) {
        std::cerr << "[Orchestrator] Failed to create service socket pair: " << strerror(errno) << "\n";
        return false;
    }
    std::cout << "[Orchestrator] Created socket pair for service: " << m_name << "\n";
    fcntl(socketPair[0], F_SETFD, FD_CLOEXEC);
    fcntl(socketPair[1], F_SETFD, FD_CLOEXEC);

    m_orchestratorSocketFD = socketPair[0];
    m_serviceSocketFD = socketPair[1];

    m_pid = spawn_process(serviceName, binary_path);
    if (m_pid < 0) {
        std::cerr << "[Orchestrator] Failed to spawn service: " << serviceName << "\n";
        return false;
    }
    std::cout << "[Orchestrator] Spawned service " << serviceName << ", with pid: " << m_pid << "\n";
    return true;
}

void ServiceHandle::closeServiceHandle() {
    if (m_orchestratorSocketFD >= 0) {
        close(m_orchestratorSocketFD);
        m_orchestratorSocketFD = -1;
    }
    if (m_serviceSocketFD >= 0) {
        close(m_serviceSocketFD);
        m_serviceSocketFD = -1;
    }
    if (m_pid > 0) {
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
        m_pid = -1;
    }
}

pid_t ServiceHandle::spawn_process(const char *process_name, const char *binary_path) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[Orchestrator] Fork failed to fork: " << process_name << ": " << strerror(errno) << std::endl;
        return -1;
    }
    if (pid == 0) {
        // child process
        fcntl(m_serviceSocketFD, F_SETFD, 0);
        close(m_orchestratorSocketFD);
        m_orchestratorSocketFD = -1;
        char sockFDStr[16];
        snprintf(sockFDStr, sizeof(sockFDStr), "%d", m_serviceSocketFD);

        execl(binary_path, process_name, sockFDStr, nullptr);
        // we shouldn't reach here unless exec fails
        std::cerr << "[Orchestrator] Exec failed for " << process_name << ": " << strerror(errno) << std::endl;
        exit(1);
    }
    // parent process
    close(m_serviceSocketFD);
    m_serviceSocketFD = -1;
    std::cout << "[Orchestrator] Spawned Process (PID: " << pid << ")\n";
    return pid;
}
