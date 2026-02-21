//
// Created by Belal Shehab on 17/02/2026.
//

#ifndef MICROKERNEL_IPC_COMMON_H
#define MICROKERNEL_IPC_COMMON_H
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/uio.h>

enum MessageType {
    CONNECT_REQUEST = 1,
    CONNECT_RESPONSE = 2,
    SHM_FD_TRANSFER = 3
};

struct Message {
    MessageType type;
    int payloadSize;
    char payload[256];
};

inline bool sendMessage(int sock_fd, const Message &message, const char* sender) {
    ssize_t sent = write(sock_fd, &message, sizeof(Message));
    if (sent != sizeof(Message)) {
        std::cerr << "[" << sender << "] Failed to send message: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

inline bool receiveMessage(int sock_fd, Message &message, const char* receiver) {
    ssize_t received = read(sock_fd, &message, sizeof(Message));
    if (received != sizeof(Message)) {
        if (received == 0) {
            std::cout << "[" << receiver << "] Connection closed by peer.\n";
        } else if (received < 0) {
            std::cerr << "[" << receiver << "] Failed to receive message: " << strerror(errno) << std::endl;
        } else {
            std::cerr << "[" << receiver << "] Incomplete message received: " << received << " bytes\n";
        }
        return false;
    }
    return true;
}

inline bool sendFD(int socketFd, int fdToSend, size_t sharedMemorySize, const char *context) {
    Message message;
    message.type = SHM_FD_TRANSFER;
    message.payloadSize = snprintf(message.payload, sizeof(message.payload), "%zu", sharedMemorySize);

    struct iovec ioVector;
    ioVector.iov_base = &message;
    ioVector.iov_len = sizeof(Message);

    char cmsgBuf[CMSG_SPACE(sizeof(int))];

    struct msghdr messageHeader = {};
    messageHeader.msg_iov = &ioVector;
    messageHeader.msg_iovlen = 1;
    messageHeader.msg_control = cmsgBuf;
    messageHeader.msg_controllen = sizeof(cmsgBuf);

    struct cmsghdr *controlMessageHeader = CMSG_FIRSTHDR(&messageHeader);
    controlMessageHeader->cmsg_level = SOL_SOCKET;
    controlMessageHeader->cmsg_type = SCM_RIGHTS;
    controlMessageHeader->cmsg_len = CMSG_LEN(sizeof(int));

    memcpy(CMSG_DATA(controlMessageHeader), &fdToSend, sizeof(int));

    if (sendmsg(socketFd, &messageHeader, 0) < 0) {
        std::cerr << "[" << context << "] Failed to send file descriptor: " << strerror(errno) << std::endl;
        return false;
    }
    std::cout << "[" << context << "] Sent file descriptor: " << fdToSend << std::endl;
    return true;
}

inline bool receiveFD(int socketFd, int &fdToReceive, Message& message, const char *context) {

    struct iovec ioVector;
    ioVector.iov_base = &message;
    ioVector.iov_len = sizeof(Message);

    char cmsgBuf[CMSG_SPACE(sizeof(int))];
    struct msghdr messageHeader = {};
    messageHeader.msg_iov = &ioVector;
    messageHeader.msg_iovlen = 1;
    messageHeader.msg_control = cmsgBuf;
    messageHeader.msg_controllen = sizeof(cmsgBuf);

    if (recvmsg(socketFd, &messageHeader, 0) < 0) {
        std::cerr << "[" << context << "] Failed to receive file descriptor: " << strerror(errno) << std::endl;
        return false;
    }

    struct cmsghdr *controlMessageHeader = CMSG_FIRSTHDR(&messageHeader);
    if (!controlMessageHeader || controlMessageHeader->cmsg_type != SCM_RIGHTS) {
        std::cerr << "[" << context << "] No file descriptor received\n";
        return false;
    }
    memcpy(&fdToReceive, CMSG_DATA(controlMessageHeader), sizeof(int));

    std::cout << "[" << context << "] Received file descriptor: " << fdToReceive << std::endl;
    return true;
}

#endif //MICROKERNEL_IPC_COMMON_H