# ── libsodium ─────────────────────────────────────────────────────────────────
find_library(SODIUM_LIB sodium HINTS /opt/homebrew/lib REQUIRED)
find_path(SODIUM_INCLUDE sodium.h HINTS /opt/homebrew/include REQUIRED)


find_program(NIM_EXECUTABLE nim REQUIRED)
set(NIM_SRC_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/nim)
set(NIM_SRC_FILE ${NIM_SRC_DIR}/nim_listener.nim)
set(NIM_OUT_DIR  ${CMAKE_BINARY_DIR}/nimcache)
set(NIM_LIB      ${NIM_OUT_DIR}/nim_listener.a)

add_custom_command(
        OUTPUT  ${NIM_LIB}
        COMMAND ${NIM_EXECUTABLE} c
        --noMain
        --app:staticLib
        --outdir:${NIM_OUT_DIR}
        --nimcache:${NIM_OUT_DIR}
        -o:${NIM_LIB}
        ${NIM_SRC_FILE}
        DEPENDS ${NIM_SRC_FILE}
        COMMENT "Compiling Nim listener..."
)

add_custom_target(nim_listener_lib DEPENDS ${NIM_LIB})


# ── NetworkListener executable ────────────────────────────────────────────────
add_executable(networkListener
        main.cpp
        NetworkListener.cpp
        NetworkListener.h
        nim_listener.h
        nim_listener.cpp
)

add_dependencies(networkListener nim_listener_lib)

target_include_directories(networkListener PRIVATE
        ${SODIUM_INCLUDE}
        ${NIM_OUT_DIR}
)

target_link_libraries(networkListener PRIVATE
        proto
        ${SODIUM_LIB}
        ${NIM_LIB}
        pthread
)

