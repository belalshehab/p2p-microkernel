# p2p-microkernel

### A High-Performance P2P Microkernel for Secure Gossip
*Built with C++, Nim, nim-libp2p, Cap'n Proto, and libsodium*

---

A research implementation of a **capability-based P2P node** using microkernel-style process isolation. The goal is to demonstrate how security-critical and network-facing concerns can be separated at the process boundary — without sacrificing performance.

The core idea: security-sensitive logic (private key access, block validation) lives in an isolated **KeyGuard** process that never touches the network. Network-facing gossip logic lives in a separate **GossipNode** process written in Nim using nim-libp2p. Neither can directly access the other. The Orchestrator brokers every connection through typed Cap'n Proto capability references. If the network-facing process is compromised, the KeyGuard and its keys are untouched.

---

## Why this exists

Most examples of Cap'n Proto RPC, Unix process isolation, and Ed25519 signing exist separately. This project puts them together in one working, buildable, multi-node system — a reference for how these primitives compose into a real P2P node runtime.

If you are building a decentralized node, a plugin runtime, or anything that needs process isolation with efficient IPC, the patterns here apply directly.

---

## Why this architecture

### Why microkernel-style isolation?

In a monolithic process, a vulnerability in the network layer can reach the signing keys. By splitting into isolated processes — each with its own address space — we contain the blast radius. The `KeyGuard` holds the private keys and never touches the network. The `GossipNode` touches the network and never sees the keys. The Orchestrator connects them only when needed, through a typed capability interface.

This maps directly to how **L4-style microkernels** work: a minimal core manages process spawning and IPC, everything else runs as isolated user-space services.

### Why Cap'n Proto?

- **Native IPC over Unix socketpairs** — no TCP overhead, direct kernel-buffered communication between local processes
- **Capability-based RPC** — a service reference *is* an access token. You cannot call a service you were never given a capability to
- **Schema versioning and code generation** — no hand-rolled message structs, no silent binary corruption

### Why Nim for the GossipNode?

The `GossipNode` is the process exposed to the network. It uses **nim-libp2p** for gossip protocol support — the most complete libp2p implementation outside of Go, maintained by the Status/Nimbus team. Nim gives us:

- Memory safety without a borrow checker
- Near-C performance
- First-class C FFI — the gossip thread calls into the C++ KJ event loop via a cross-thread fulfiller

The tradeoff: Cap'n Proto RPC stays in C++. The Nim side handles gossip logic and calls into C++ via `extern "C"` FFI.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                  Orchestrator (C++)                  │
│  - spawns and monitors all services via fork/exec    │
│  - holds KeyGuard::Client + GossipNode::Client       │
│  - brokers connectToKeyGuard() requests              │
│  - exposes CLI for publishing messages               │
└────────────┬─────────────────────┬───────────────────┘
             │  socketpair (capnp) │  socketpair (capnp)
     ┌───────▼──────┐     ┌────────▼─────────────────────────┐
     │  KeyGuard    │     │   GossipNode                     │
     │    (C++)     │     │   (Nim libp2p + C++ RPC shim)    │
     │              │     │                                  │
     │  - Ed25519   │     │  - GossipSub mesh networking     │
     │    validate  │     │  - binary framing (sender +      │
     │    and sign  │     │    data + signature)             │
     │  - trusted   │     │  - Nim thread → KJ cross-thread  │
     │    peer map  │     │    fulfiller bridge              │
     └──────────────┘     └──────────────────────────────────┘
```

### Message flow — receive path

```
Nim GossipSub receives message
  → nimGossipNodeOnGossip() [extern C, called from Nim thread]
    → fulfills CrossThreadPromiseFulfiller<void>
      → KJ event loop wakes up in gossipLoop()
        → KeyGuard.validateBlock(data, signature, senderId)
          ← (isValid, validatorSignature)
        → nimGossipNodeOnValidated(isValid, sig)
          → [display / forward]
```

### Message flow — publish path

```
Orchestrator CLI input
  → GossipNode.publishData(data)
    → KeyGuard.signData(data)
      ← signature
    → nimGossipNodePublish(senderId, data, signature)
      → Nim encodeFrame() → GossipSub.publish()
        → floods to all mesh peers
```

---

## Cap'n Proto schema

```capnp
struct GossipMessage {
    senderId  @0 :Data;   # sender node ID
    data      @1 :Data;   # message content
    signature @2 :Data;   # Ed25519 signature over data
    topicID   @3 :Text;   # e.g. microp2p/v1/gossip
}

interface KeyGuard extends(MicroService) {
    validateBlock  @0 (message :GossipMessage) -> (isValid :Bool, validatorSignature :Data);
    signData       @1 (data :Data) -> (signature :Data);
    addTrustedPeer @2 (peerId :Data, publicKey :Data) -> ();
}

interface GossipNode extends(MicroService) {
    startListening @0 (port :UInt16, peerAddrs :List(Text)) -> ();
    publishData    @1 (data :Data) -> ();
}

interface Orchestrator {
    getServices       @0 () -> (services :List(Text));
    connectToKeyGuard @1 () -> (keyGuard :KeyGuard);
    connectToGossipNode @2 () -> (gossipNode :GossipNode);
}
```

---

## What's implemented

### Process management
The Orchestrator forks and launches child services using `execl`. Each service gets its own Unix socket FD passed as `argv[1]`. `FD_CLOEXEC` ensures no file descriptors leak across `exec` boundaries.

Services are managed through `ServiceHandle` (RAII, move-only) and `ServicesRegistry` (single source of truth for all running processes).

### Cap'n Proto RPC over socketpairs
Each connection uses a single bidirectional `socketpair`. Both sides use `TwoPartyClient`. The Orchestrator exports itself as a bootstrap capability to each service, so services can call back into the Orchestrator (e.g. `connectToKeyGuard`).

### Nim ↔ C++ FFI bridge
The GossipNode runs Nim's libp2p event loop in a background `std::thread`. When Nim receives a gossip message it calls `nimGossipNodeOnGossip` (an `extern "C"` function in C++), which signals the KJ event loop via a `CrossThreadPromiseFulfiller<void>`. This is the correct pattern for bridging Nim's Chronos async runtime with KJ's async runtime — no blocking, no races.

### GossipSub mesh networking (nim-libp2p)
- Subscribes to topic `microp2p/v1/gossip`
- Connects to bootstrap peers on startup via multiaddr (`/ip4/.../tcp/.../p2p/<PeerID>`)
- GossipSub automatically floods messages to all mesh peers — a message published by node 3 (connected only to node 2) is received by node 1 without a direct connection
- Binary framing: each message encodes `[senderIdLen][senderId][dataLen][data][sigLen][signature]`

### Ed25519 signing and validation (libsodium)
- KeyGuard expands a 32-byte seed into a 64-byte signing key at startup via `crypto_sign_seed_keypair`
- `signData`: signs outgoing data before publish
- `validateBlock`: verifies incoming signature, signs the validation result with KeyGuard's own key, returns `(isValid, validatorSignature)`

### Multi-node tested
Three nodes running simultaneously, each on a different port, connected in a chain (node3 → node2 → node1). Messages published by any node are received and validated by all others via GossipSub mesh flooding.

---

## Building

```bash
# Install dependencies (macOS)
brew install capnp libsodium nim
nimble install libp2p  # from GossipNode/nim/

mkdir -p cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

## Running

```bash
# Terminal 1 — first node (no bootstrap peer needed)
./Orchestrator/orchestrator --port 12345

# Terminal 2 — connect to node 1 (copy PeerID from node 1 logs)
./Orchestrator/orchestrator --port 12346 \
  --peer /ip4/127.0.0.1/tcp/12345/p2p/<NODE1_PEERID>

# Terminal 3 — connect to node 2
./Orchestrator/orchestrator --port 12347 \
  --peer /ip4/127.0.0.1/tcp/12346/p2p/<NODE2_PEERID>
```

Node 2 will connect to node 1 and publish a test message after 5 seconds. The message will appear on all nodes, including node 1, validated and signed by each node's KeyGuard.

---

## Project structure

```
├── Orchestrator/
│   ├── Orchestrator.h/.cpp     # OrchestratorImpl — capability broker
│   ├── ServiceConnection.h     # spawnAndConnect() helper
│   └── main.cpp                # CLI args, spawn, ping, test publish
├── KeyGuard/
│   ├── key_guard.h/.cpp        # Ed25519 validate + sign + trusted peer map
│   └── main.cpp
├── GossipNode/
│   ├── GossipNode.h/.cpp       # Cap'n Proto server + cross-thread KJ bridge
│   ├── nim_gossip_node.h       # extern "C" interface to Nim
│   ├── main.cpp
│   └── nim/
│       └── gossip_node.nim     # nim-libp2p GossipSub node
├── proto/
│   └── orchestrator.capnp      # Cap'n Proto schema
├── ServiceHandle.h/.cpp        # RAII process + socket ownership
└── ServicesRegistry.h/.cpp     # Registry of all running services
```

---

## What's next

### mDNS peer discovery
Add mDNS to the Nim GossipNode so nodes on the same local network discover each other automatically — no `--peer` argument needed. This is how libp2p bootstraps local connections natively.

### CLI chat interface
Replace the test `publishData` call in `Orchestrator/main.cpp` with an interactive stdin read loop. Each node becomes a chat terminal — type a message, it appears on all other nodes.

### Per-node identity
Currently all nodes use the same hardcoded Ed25519 key pair. Each node needs its own generated key pair, with the public key exchanged via `addTrustedPeer` during peer discovery. The Nim PeerID should be derived from the node's Ed25519 key for consistency.

### Shared memory fast path
Reintroduce `SharedMemory` for bulk data transfer between services. Cap'n Proto handles signalling and capability passing; shared memory carries the data itself — true zero-copy between isolated processes.

### Fault tolerance
The Orchestrator should detect crashed services via `SIGCHLD` and restart them automatically.
