//
// Created by Belal Shehab on 21/02/2026.
//

#ifndef MICROKERNEL_SERVICEHANDLE_H
#define MICROKERNEL_SERVICEHANDLE_H

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "ipc_common.h"
#include "SharedMemory.h"

class ServiceHandle {
public:
    ServiceHandle() = default;
    ~ServiceHandle();
    ServiceHandle(const ServiceHandle& other) = delete;
    ServiceHandle& operator=(const ServiceHandle& other) = delete;

    ServiceHandle(ServiceHandle&& other) noexcept;
    ServiceHandle& operator=(ServiceHandle&& other) noexcept;

    bool init(const char *serviceName, const char *binary_path);

    void closeServiceHandle();

    inline pid_t pid() const { return m_pid; }
    inline int orchestratorSocketFD() const { return m_orchestratorSocketFD; }
    inline int serviceSocketFD() const { return m_serviceSocketFD; }
    inline const std::string &name() const { return m_name; }

private:
    pid_t spawn_process(const char *process_name, const char *binary_path);

private:
    pid_t m_pid = -1;
    int m_orchestratorSocketFD = -1;
    int m_serviceSocketFD = -1;
    std::string m_name;
};


#endif //MICROKERNEL_SERVICEHANDLE_H
