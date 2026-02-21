cmake_minimum_required(VERSION 3.20)
project(MicroKernel)

set(CMAKE_CXX_STANDARD 20)

add_library(sharedMemory SharedMemory.cpp
        SharedMemory.h)
# Microkernel Architecture Components
add_executable(orchestrator orchestrator.cpp
        ServiceHandle.cpp
        ServiceHandle.h
        ServicesRegistry.cpp
        ServicesRegistry.h)
add_executable(hasher hasher.cpp)
add_executable(signer signer.cpp)

target_link_libraries(orchestrator PRIVATE sharedMemory)
target_link_libraries(hasher PRIVATE sharedMemory)
target_link_libraries(signer PRIVATE sharedMemory)