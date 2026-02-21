# cpp-microKernel

A microkernel runtime built from scratch in C++ — mainly as a learning project, but one I'm taking seriously enough to get the architecture right. The goal is to eventually reach something close to what L4-style microkernels do: a minimal core that manages process spawning, IPC, and memory, while everything else runs as isolated user-space services.

---

## What's been built so far

### Process management
The orchestrator forks and launches child services (`hasher`, `signer`) using `execl`. Each service gets its own Unix socket file descriptor passed as a command-line argument — no shared global state.

Socket pairs use `FD_CLOEXEC` so that when a child process calls `exec`, all file descriptors it wasn't explicitly given are automatically closed. No leaking FDs across process boundaries.

### Service lifecycle — `ServiceHandle` and `ServicesRegistry`

Each running service is represented by a `ServiceHandle`, which owns the process PID and the orchestrator-side socket FD. It is move-only (copy constructor is deleted) to make ownership explicit — there is exactly one place that owns a service at any time.

`ServiceHandle` is RAII: the destructor calls `closeServiceHandle()`, which closes the socket and sends `SIGTERM` to the process, then waits for it. All FDs are reset to `-1` after closing to prevent double-close bugs.

Services are managed through `ServicesRegistry`, which is the single source of truth for all running services. The API is designed to prevent a common mistake — using a handle after transferring ownership:

```cpp
// registerService constructs, initializes, stores, and returns a non-owning pointer
// in one step. There is no local ServiceHandle to accidentally use after the move.
ServiceHandle* hasher = services.registerService("hasher", "./hasher");
if (!hasher) { return 1; }
```

`unregisterAllServices()` simply clears the map — destructors handle the cleanup automatically.

### IPC via Unix domain sockets
Services communicate with the orchestrator through a simple message protocol defined in `ipc_common.h`:

```cpp
struct Message {
    MessageType type;
    int payloadSize;
    char payload[256];
};
```

Supported message types so far:
- `CONNECT_REQUEST` / `CONNECT_RESPONSE` — service registration handshake on startup
- `SHM_FD_TRANSFER` — passing a shared memory file descriptor to a service at runtime

File descriptors are passed using `sendmsg` / `recvmsg` with `SCM_RIGHTS` — the standard POSIX way to transfer FDs between processes over a Unix socket without going through the filesystem.

### Shared memory
The `SharedMemory` class handles creating and attaching to memory segments:

- On **macOS**: uses `shm_open` + `shm_unlink` (anonymous, unlinked immediately after creation)
- On **Linux**: uses `memfd_create` (cleaner, no name needed)

The memory layout puts a `SharedMemoryHeader` at the start of the segment, followed by the data region:

```
[ SharedMemoryHeader | ... data ... ]
```

The header contains two `std::atomic<bool>` flags — `inputReady` and `outputReady` — for coordination between processes without a mutex. Reads use `memory_order_acquire` and writes use `memory_order_release` to ensure the CPU doesn't reorder memory operations around the flag flips. A plain `bool` is not sufficient here — without `atomic`, the compiler is allowed to cache the value in a register and never re-read from memory, causing an infinite polling loop under optimization.

The orchestrator creates the shared memory segment, sends the FD to the hasher via the socket, then simulates a delay before writing data and setting `inputReady`. The hasher polls `inputReady`, processes the data, writes the result back, and sets `outputReady`. The orchestrator polls `outputReady` and reads the result.

No copying between processes. The same physical memory pages are mapped into both address spaces.

---

## Architecture

```
┌─────────────────────────────────────┐
│             Orchestrator            │
│  - spawns and monitors services     │
│  - owns ServicesRegistry            │
│  - creates shared memory segments   │
│  - sends FDs to services at runtime │
└──────────┬──────────────┬───────────┘
           │ socket pair  │ socket pair
    ┌──────▼──────┐  ┌────▼────────┐
    │   Hasher    │  │   Signer    │
    │  (hashing)  │  │  (signing)  │
    └─────────────┘  └─────────────┘
```

Services don't know about each other. All coordination goes through the orchestrator.

---

## What's next

- **Cap'n Proto** — replace the hand-rolled message structs with a proper serialization schema, which also sets the foundation for cross-language services.
- **Rust service** — rewrite one of the services in Rust, communicating with the C++ orchestrator over the same socket + shared memory interface.
- **Fault tolerance and isolation** — orchestrator restarts crashed services automatically, with Linux namespaces/cgroups for proper process isolation.

---

## Building

```bash
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
./orchestrator
```

Requires CMake 3.x and a C++17 compiler. Tested on macOS (Apple Clang) and should work on Linux.

---

## Why

I'm building this to deeply understand the primitives that real microkernel and plugin runtimes are built on — `fork`/`exec`, `socketpair`, `mmap`, `sendmsg` with `SCM_RIGHTS`, atomic memory ordering across processes, and ownership semantics in systems code. The kind of stuff that is easy to use incorrectly and hard to debug when you do.

