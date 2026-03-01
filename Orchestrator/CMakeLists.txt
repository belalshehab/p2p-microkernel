# ── Orchestrator executable ───────────────────────────────────────────────────
add_executable(orchestrator
    main.cpp
    Orchestrator.cpp
    Orchestrator.h
    ServiceConnection.h
    ../ServiceHandle.cpp ../ServiceHandle.h
    ../ServicesRegistry.cpp ../ServicesRegistry.h
)

target_link_libraries(orchestrator PRIVATE sharedMemory proto)

