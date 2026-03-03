//
// Created by Belal Shehab on 03/03/2026.
//

#pragma once
#include <cstdint>
#include <cstddef>


extern "C" {
  void nimListenerInit(uint16_t port);

  int nimListenerOnGossip(const uint8_t* data, size_t dataSize, const uint8_t* signature, size_t signatureSize);

  void nimListenerOnValidated(bool isValid, const uint8_t* validatorSignature, size_t validatorSignatureSize);
}
