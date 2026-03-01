# ── libsodium ─────────────────────────────────────────────────────────────────
find_library(SODIUM_LIB sodium HINTS /opt/homebrew/lib REQUIRED)
find_path(SODIUM_INCLUDE sodium.h HINTS /opt/homebrew/include REQUIRED)

# ── Validator executable ───────────────────────────────────────────────────────
add_executable(validator
    main.cpp
    validator.cpp
    validator.h
)

target_include_directories(validator PRIVATE ${SODIUM_INCLUDE})
target_link_libraries(validator PRIVATE proto ${SODIUM_LIB})

