cmake_minimum_required(VERSION 3.20)
project(MicroKernel)

set(CMAKE_CXX_STANDARD 20)

# Microkernel Architecture Components
add_executable(orchestrator orchestrator.cpp)
