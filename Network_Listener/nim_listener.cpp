#include "nim_listener.h"
#include <iostream>

extern "C" {
  int nimListenerOnGossip(const uint8_t* data, size_t dataSize, const uint8_t* signature, size_t signatureSize) {
    //TODO: forward to validator via RPC

    std::cout << "[C++] Gossip received with data size: " << dataSize << " and signature size: " << signatureSize << "\n";
    return 0; // Return 0 for success, non-zero for failure
  }
}