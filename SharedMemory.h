//
// Created by Belal Shehab on 18/02/2026.
//

#ifndef MICROKERNEL_SHAREDMEMORY_H
#define MICROKERNEL_SHAREDMEMORY_H

#include <sys/mman.h>

constexpr size_t SHARED_MEMORY_SIZE = 4096;

struct SharedMemoryHeader {
    size_t size;
    bool inputReady = false;
    bool outputReady = false;
};

class SharedMemory {

public:
    SharedMemory();
    ~SharedMemory();

    bool create(size_t totalSize);
    bool destroy();
    bool attach(int fd, size_t size = SHARED_MEMORY_SIZE);
    void detach();

    bool isValid() const;
    inline int fd() const {return m_fd;}
    SharedMemoryHeader* header();
    void *data();

    inline size_t size() const {
        return m_size;
    }

private:
    int m_fd = -1;
    void *m_ptr = nullptr;
    size_t m_size = 0;

};


#endif //MICROKERNEL_SHAREDMEMORY_H