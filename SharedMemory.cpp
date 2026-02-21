//
// Created by Belal Shehab on 18/02/2026.
//

#include "SharedMemory.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>


SharedMemory::SharedMemory()
{
}

SharedMemory::~SharedMemory() {
    detach();
}

bool SharedMemory::create(size_t size) {
    size_t totalSize = size + sizeof(SharedMemoryHeader);
#ifdef __APPLE__
    char shm_name[1024];
    snprintf(shm_name, sizeof(shm_name), "/shm_%d", getpid());
    m_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0600);
    if (m_fd < 0) {
        std::cerr << "[SharedMemory::create] Error creating shared memory segment"
        << strerror(errno) << "\n";
        return false;
    }
    shm_unlink(shm_name);
#else
    m_fd = memfd_create("shm", MFD_CLOEXEC);
    if (m_fd < 0) {
        std::cerr << "[SharedMemory::create] Error creating shared memory segment: "
        << strerror(errno) << "\n";
        return false;
    }
#endif
    if (ftruncate(m_fd, totalSize) < 0) {
        std::cerr << "[SharedMemory::create] Error setting shared memory size: "
        << strerror(errno) << "\n";
        close(m_fd);
        m_fd = -1;
        return false;
    }
    std::cout << "[SharedMemory::create] Memory segment allocated"
              << " (payload: " << size << " bytes, total: " << totalSize
              << " bytes, fd: " << m_fd << ")\n";

    m_ptr = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (m_ptr == MAP_FAILED) {
        close(m_fd);
        m_fd = -1;
        m_ptr = nullptr;
        std::cerr << "[SharedMemory::create] Error mapping shared memory: "
        << strerror(errno) << "\n";
        return false;
    }

    m_size = totalSize;
    memset(m_ptr, 0, totalSize);
    auto* hdr = header();
    hdr->size = size;
    hdr->inputReady = false;
    hdr->outputReady = false;

    std::cout << "[SharedMemory::create] Memory segment mapped at " << m_ptr << "\n";

    return true;
}

bool SharedMemory::destroy() {
    detach();
    return true;
}

bool SharedMemory::attach(int fd, size_t size) {
    if (m_ptr != nullptr) {
        std::cerr << "[SharedMemory::attach] Error: Already attached to a memory segment\n";
        return false;
    }
    m_size = size;
    m_fd = fd;
    m_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (m_ptr == MAP_FAILED) {
        std::cerr << "[SharedMemory::attach] Error mapping shared memory: " << strerror(errno) << "\n";
        m_ptr = nullptr;
        m_fd = -1;
        m_size = 0;
        return false;
    }
    std::cout << "[SharedMemory::attach] Successfully attached to shared memory at " << m_ptr << "\n";
    return true;
}

void SharedMemory::detach() {
    if (m_ptr != nullptr && m_ptr != MAP_FAILED) {
        munmap(m_ptr, m_size);
        m_ptr = nullptr;
    }
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    m_size = 0;
}

bool SharedMemory::isValid() const {
    return m_fd >= 0 && m_ptr != nullptr && m_ptr != MAP_FAILED;
}

SharedMemoryHeader * SharedMemory::header() {
    if (!isValid()) {
        return nullptr;
    }
    return static_cast<SharedMemoryHeader*>(m_ptr);
}

void * SharedMemory::data() {
    if (!isValid()) {
        return nullptr;
    }
    return static_cast<char*>(m_ptr) + sizeof(SharedMemoryHeader);
}
