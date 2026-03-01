# cpp-microKernel

A microkernel runtime built from scratch in C++ — mainly as a learning project, but one I'm taking seriously enough to get the architecture right. The goal is to eventually reach something close to what L4-style microkernels do: a minimal core that manages process spawning, IPC, and capability-based service discovery, while everything else runs as isolated user-space services.

---

## Architecture

The system uses a **capability broker pattern**. The Orchestrator is the central process — it spawns all services, holds live Cap'n Proto capability references to each one, and brokers connections between them. Services never connect to each other directly.

```
┌──────────────────────────────────────────────┐
│                  Orchestrator                │
│  - spawns and monitors all services          │
│  - holds Validator::Client                   │
│  - holds NetworkListener::Client             │
│  - brokers connectToValidator() requests     │
└────────────┬─────────────────┬───────────────┘
             │  socketpair     │  socketpair
     ┌───────▼──────┐   ┌──────▼──────────────┐
     │  Validator   │   │   NetworkListener    │
     │              │   │                      │
     │  - signs /   │   │  - ingests P2P       │
     │    validates │   │    gossip traffic    │
     │    messages  │   │  - forwards to       │
     │              │   │    Validator via     │
     └──────────────┘   │    Orchestrator      │
                        └──────────────────────┘
```

Communication flow when the NetworkListener receives a gossip packet:

```
NetworkListener → orchestrator.connectToValidator()
               → validator.validateBlock("gossip data")
               → "Validated"
```

All traffic is brokered through the Orchestrator. Two sockets total — one per service.

---

## What's been built

### Process management
The Orchestrator forks and launches child services using `execl`. Each service gets its own Unix socket file descriptor passed as `argv[1]` — no shared global state, no leaking FDs. `FD_CLOEXEC` ensures file descriptors not explicitly given to a child are automatically closed on `exec`.

Services are managed through `ServiceHandle` (RAII, move-only) and `ServicesRegistry` (single source of truth). The destructor closes the socket and sends `SIGTERM`, then waits. No double-close bugs.

### IPC — Cap'n Proto RPC over Unix domain sockets

All inter-process communication uses **Cap'n Proto RPC**. The schema is defined in `proto/orchestrator.capnp`:

```capnp
interface MicroService {
    getName @0 () -> (name :Text);
    ping    @1 () -> ();
}

interface Validator extends(MicroService) {
    validateBlock @0 (data :Text) -> (signature :Text);
}

interface NetworkListener extends(MicroService) {
    startListening @0 (port :UInt16) -> ();
}

interface Orchestrator {
    getServices            @0 () -> (services :List(Text));
    connectToValidator     @1 () -> (validator :Validator);
    connectToNetworkListener @2 () -> (listener :NetworkListener);
}
```

Each connection uses a single bidirectional `socketpair`. Both sides use `TwoPartyClient` — the Orchestrator with `Side::CLIENT` (initiates handshake), services with `Side::SERVER` (wait for handshake). After the handshake both sides are symmetric: either can call the other.

### Capability broker — bidirectional RPC

The Orchestrator exports itself as a bootstrap capability to each service over the existing socket. This means:

- Orchestrator can call `validator.validateBlock()` — it holds a `Validator::Client`
- Validator can call `orchestrator.connectToValidator()` — it holds an `Orchestrator::Client`
- NetworkListener can ask for the Validator cap and call it directly through the broker

The `OrchestratorImpl` is constructed empty first (before services are spawned), then service clients are injected via setters after connections are established — breaking the circular dependency.

### Shared memory (dormant, planned)
`SharedMemory.h/.cpp` and `ipc_common.h` are kept for future use. The plan is to reintroduce shared memory as a fast path for transferring large data chunks between services, while Cap'n Proto RPC handles control flow and capability passing.

---

## Project structure

```
├── Orchestrator/
│   ├── Orchestrator.h/.cpp     # OrchestratorImpl — capability broker
│   ├── ServiceConnection.h     # spawnAndConnect() helper + ServiceConnection struct
│   └── main.cpp
├── Validator/
│   ├── validator.h/.cpp        # ValidatorImpl — signing/validation logic
│   └── main.cpp
├── Network_Listener/
│   ├── NetworkListener.h/.cpp  # NetworkListenerImpl — P2P traffic ingestion
│   └── main.cpp
├── proto/
│   └── orchestrator.capnp      # Cap'n Proto schema for all interfaces
├── ServiceHandle.h/.cpp        # RAII process + socket ownership
├── ServicesRegistry.h/.cpp     # Registry of all running services
├── SharedMemory.h/.cpp         # Shared memory (dormant, planned for future)
├── ipc_common.h                # Legacy message structs (kept for reference)
└── ARCHITECTURE.md             # Deep-dive into the capability broker pattern
```

---

## Building

```bash
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
./orchestrator
```

Requires CMake 3.20+, a C++20 compiler, and Cap'n Proto. On macOS with Homebrew:

```bash
brew install capnp
```

---

## What's next

- **Real gossip parsing** — NetworkListener binds an actual UDP/TCP socket and parses P2P gossip messages instead of the current stub.
- **Nim NetworkListener** — rewrite the listener in Nim to align with the `nim-libp2p` ecosystem, communicating with the C++ Orchestrator over the same Cap'n Proto socket interface.
- **Shared memory fast path** — reintroduce `SharedMemory` for bulk data transfer between services, using Cap'n Proto only for signalling and capability passing.
- **Fault tolerance** — Orchestrator detects crashed services via `SIGCHLD` and restarts them automatically.

---

## Why

I'm building this to deeply understand the primitives that real microkernel and P2P node runtimes are built on — `fork`/`exec`, `socketpair`, Cap'n Proto capability-based RPC, atomic memory ordering across processes, and ownership semantics in systems code. The kind of stuff that is easy to use incorrectly and hard to debug when you do.
