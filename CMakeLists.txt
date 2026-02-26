cmake_minimum_required(VERSION 3.20)
project(MicroKernel)

set(CMAKE_CXX_STANDARD 20)

# ── Cap'n Proto (auto-download if not on system) ───────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CapnProto)

capnp_generate(
    SCHEMA  ${CMAKE_SOURCE_DIR}/proto/orchestrator.capnp
    OUT_DIR ${CMAKE_BINARY_DIR}/proto
    OUT_H   CAPNP_GENERATED_H
    OUT_CPP CAPNP_GENERATED_CPP
)

add_library(proto STATIC ${CAPNP_GENERATED_CPP} ${CAPNP_GENERATED_H})
target_include_directories(proto PUBLIC ${CMAKE_BINARY_DIR}/proto ${CMAKE_SOURCE_DIR})
target_link_libraries(proto PUBLIC CapnProto::capnp-rpc CapnProto::kj-async CapnProto::capnp CapnProto::kj)

# ── Shared memory ──────────────────────────────────────────────────────────────
add_library(sharedMemory SharedMemory.cpp SharedMemory.h)

# ── Services ───────────────────────────────────────────────────────────────────
add_executable(orchestrator
    orchestrator.cpp
    ServiceHandle.cpp ServiceHandle.h
    ServicesRegistry.cpp ServicesRegistry.h)

add_executable(hasher hasher.cpp)
add_executable(signer signer.cpp)

target_link_libraries(orchestrator PRIVATE sharedMemory proto)
target_link_libraries(hasher       PRIVATE sharedMemory proto)
target_link_libraries(signer       PRIVATE sharedMemory proto)
