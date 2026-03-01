# ── NetworkListener executable ────────────────────────────────────────────────
add_executable(networkListener
    main.cpp
    NetworkListener.cpp
    NetworkListener.h
)

target_link_libraries(networkListener PRIVATE proto)

