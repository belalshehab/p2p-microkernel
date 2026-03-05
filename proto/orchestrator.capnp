@0xd3b2a1c4e5f60718;

struct GossipMessage {
    senderId    @0 :Data;       # sender's node ID (maps to their public key in validator's trusted peer map)
    data        @1 :Data;       # the actual message content
    signature   @2 :Data;       # sender's signature over the data (for authenticity and integrity)
    topicID     @3 :Text;       # eg: microp2p/v1/gossip
}

interface MicroService {
    getName @0 () -> (name :Text);
    # heartbeat for orchestrator to check if the service is still alive
    ping @1 () -> ();
}

interface KeyGuard extends(MicroService) {
    # Read path: validate an incoming gossip message against the trusted peer map
    validateBlock @0 (message :GossipMessage) -> (isValid :Bool, validatorSignature :Data);

    # Write path: sign data with KeyGuard's own private key
    signData @1 (data :Data) -> (signature :Data);

    # Register a known/trusted peer (peerId -> publicKey mapping)
    addTrustedPeer @2 (peerId :Data, publicKey :Data) -> ();
}

interface GossipNode extends(MicroService) {
    startListening @0 (port :UInt16, peerAddrs :List(Text)) -> ();

    # Write path: called by CLI/client to publish a message to the gossip topic
    publishData @1 (data :Data) -> ();
}

interface Orchestrator {
  # Returns the names of all currently registered services.
  getServices @0 () -> (services :List(Text));

  connectToKeyGuard @1 () -> (keyGuard :KeyGuard);
  connectToGossipNode @2 () -> (gossipNode :GossipNode);
}

