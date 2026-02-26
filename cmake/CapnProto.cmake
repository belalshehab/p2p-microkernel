# cmake/CapnProto.cmake
# Downloads and builds Cap'n Proto via FetchContent if it is not already
# available on the host.  Defines:
#
#   CAPNP_EXECUTABLE   – path to the capnpc compiler (used for code-gen)
#   CAPNP_CXX_PLUGIN   – path to the capnpc-c++ plugin
#   CAPNP_GEN_DIR      – where generated .capnp.h / .capnp.c++ land
#
# After including this file call capnp_generate() with your schema files.

include(FetchContent)

# ── Try the system install first (brew / apt / etc.) ──────────────────────────
find_program(CAPNP_EXECUTABLE  capnpc    PATHS /opt/homebrew/bin /usr/local/bin)
find_program(CAPNP_CXX_PLUGIN  capnpc-c++ PATHS /opt/homebrew/bin /usr/local/bin)
find_library(CAPNP_LIB  capnp  PATHS /opt/homebrew/lib /usr/local/lib)
find_library(KJ_LIB     kj     PATHS /opt/homebrew/lib /usr/local/lib)
find_library(CAPNP_RPC_LIB  capnp-rpc  PATHS /opt/homebrew/lib /usr/local/lib)
find_library(KJ_ASYNC_LIB   kj-async   PATHS /opt/homebrew/lib /usr/local/lib)


if(CAPNP_EXECUTABLE AND CAPNP_LIB AND KJ_LIB)
    message(STATUS "[CapnProto] Using system install: ${CAPNP_EXECUTABLE}")

    # Expose imported targets so the rest of the build just does
    # target_link_libraries(... CapnProto::capnp CapnProto::kj)
    if(NOT TARGET CapnProto::kj)
        add_library(CapnProto::kj UNKNOWN IMPORTED GLOBAL)
        set_target_properties(CapnProto::kj PROPERTIES
            IMPORTED_LOCATION "${KJ_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}")
    endif()

    if(NOT TARGET CapnProto::capnp)
        add_library(CapnProto::capnp UNKNOWN IMPORTED GLOBAL)
        set_target_properties(CapnProto::capnp PROPERTIES
            IMPORTED_LOCATION "${CAPNP_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "CapnProto::kj")
    endif()

    if(NOT TARGET CapnProto::kj-async)
        add_library(CapnProto::kj-async UNKNOWN IMPORTED GLOBAL)
        set_target_properties(CapnProto::kj-async PROPERTIES
            IMPORTED_LOCATION "${KJ_ASYNC_LIB}"
            INTERFACE_LINK_LIBRARIES "CapnProto::kj")
    endif()

    if(NOT TARGET CapnProto::capnp-rpc)
        add_library(CapnProto::capnp-rpc UNKNOWN IMPORTED GLOBAL)
        set_target_properties(CapnProto::capnp-rpc PROPERTIES
            IMPORTED_LOCATION "${CAPNP_RPC_LIB}"
            INTERFACE_LINK_LIBRARIES "CapnProto::capnp;CapnProto::kj-async")
    endif()

    # Detect include dir next to the library
    if(NOT CAPNP_INCLUDE_DIR)
        foreach(_hint /opt/homebrew/include /usr/local/include /usr/include)
            if(EXISTS "${_hint}/capnp/message.h")
                set(CAPNP_INCLUDE_DIR "${_hint}" CACHE PATH "Cap'n Proto include dir")
                break()
            endif()
        endforeach()
    endif()
    set_target_properties(CapnProto::kj       PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}")
    set_target_properties(CapnProto::capnp    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}")
    set_target_properties(CapnProto::kj-async PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}")
    set_target_properties(CapnProto::capnp-rpc PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CAPNP_INCLUDE_DIR}")

else()
    # ── Fall back: download and build from source ──────────────────────────────
    message(STATUS "[CapnProto] Not found on system – fetching v1.1.0 from GitHub...")

    # Disable things we don't need to keep the build fast.
    set(BUILD_TESTING        OFF CACHE BOOL "" FORCE)
    set(CAPNP_LITE           OFF CACHE BOOL "" FORCE)
    set(WITH_OPENSSL         OFF CACHE BOOL "" FORCE)
    set(WITH_ZLIB            OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        capnproto
        GIT_REPOSITORY https://github.com/capnproto/capnproto.git
        GIT_TAG        v1.1.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(capnproto)

    # After FetchContent the targets CapnProto::capnp and CapnProto::kj exist.
    # The compiler tools are in the build tree.
    set(CAPNP_EXECUTABLE  $<TARGET_FILE:capnp_tool>  CACHE FILEPATH "" FORCE)
    set(CAPNP_CXX_PLUGIN  $<TARGET_FILE:capnpc_cpp>  CACHE FILEPATH "" FORCE)
    set(CAPNP_INCLUDE_DIR "${capnproto_SOURCE_DIR}/src" CACHE PATH "" FORCE)
endif()

# ── Helper: generate C++ from a .capnp schema ─────────────────────────────────
#
# Usage:
#   capnp_generate(
#       SCHEMA   path/to/foo.capnp
#       OUT_DIR  ${CMAKE_BINARY_DIR}/proto      # where to write the files
#       OUT_H    generated_header_variable
#       OUT_CPP  generated_source_variable
#   )
#
# Sets OUT_H and OUT_CPP to the absolute paths of the generated files.
function(capnp_generate)
    cmake_parse_arguments(ARG "" "SCHEMA;OUT_DIR;OUT_H;OUT_CPP" "" ${ARGN})

    get_filename_component(_schema_name "${ARG_SCHEMA}" NAME)   # foo.capnp
    get_filename_component(_schema_dir  "${ARG_SCHEMA}" DIRECTORY)

    set(_out_h   "${ARG_OUT_DIR}/${_schema_name}.h")
    set(_out_cpp "${ARG_OUT_DIR}/${_schema_name}.c++")

    file(MAKE_DIRECTORY "${ARG_OUT_DIR}")

    add_custom_command(
        OUTPUT  "${_out_h}" "${_out_cpp}"
        COMMAND "${CAPNP_EXECUTABLE}"
                --src-prefix=${_schema_dir}
                -o${CAPNP_CXX_PLUGIN}:${ARG_OUT_DIR}
                ${ARG_SCHEMA}
        DEPENDS "${ARG_SCHEMA}"
        COMMENT "capnp → ${_schema_name}.h / ${_schema_name}.c++"
    )

    set(${ARG_OUT_H}   "${_out_h}"   PARENT_SCOPE)
    set(${ARG_OUT_CPP} "${_out_cpp}" PARENT_SCOPE)
endfunction()


