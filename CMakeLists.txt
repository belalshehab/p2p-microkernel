cmake_minimum_required(VERSION 3.20)
project(MicroKernel)

set(CMAKE_CXX_STANDARD 20)

# ── Cap'n Proto ────────────────────────────────────────────────────────────────
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

# ── Shared memory (global — used by orchestrator, available for future services)
add_library(sharedMemory SharedMemory.cpp SharedMemory.h)

# ── Services ───────────────────────────────────────────────────────────────────
add_subdirectory(Orchestrator)
add_subdirectory(Validator)
add_subdirectory(Network_Listener)
