import os, chronos, stew/byteutils
import libp2p
import libp2p/crypto/crypto as crypto
import libp2p/protocols/pubsub/gossipsub
import libp2p/protocols/pubsub/rpc/messages
import libp2p/protocols/pubsub/pubsub
import libp2p/transports/tcptransport
import libp2p/multiaddress




proc nimGossipNodeOnGossip(
    senderId: ptr uint8, senderIdSize: csize_t,
    data: ptr uint8, dataSize: csize_t,
    signature: ptr uint8, signatureSize: csize_t
): cint {.importc, cdecl.}

var gGossipSub: GossipSub
var gSwitch: Switch
var gTopic = "microp2p/v1/gossip"
var gPeerId: string   # set after switch starts, read by C++ via nimGossipNodeGetPeerId

# we will use cap'n proto serilization for the message format, but for this example, we'll just use a simple byte array
# ── binary framing helpers ────────────────────────────────────────────────────
# Frame layout:
#   [4 bytes LE: senderIdSize][senderId]
#   [4 bytes LE: dataSize][data]
#   [4 bytes LE: signatureSize][signature]

proc encodeFrame(
    senderId:  openArray[byte],
    data:      openArray[byte],
    signature: openArray[byte]
): seq[byte] =
    var frame: seq[byte]

    proc appendU32(s: var seq[byte], v: uint32) =
        s.add byte(v and 0xff)
        s.add byte((v shr 8) and 0xff)
        s.add byte((v shr 16) and 0xff)
        s.add byte((v shr 24) and 0xff)

    frame.appendU32(uint32(senderId.len))
    frame.add senderId
    frame.appendU32(uint32(data.len))
    frame.add data
    frame.appendU32(uint32(signature.len))
    frame.add signature
    frame

proc decodeFrame(frame: openArray[byte]): tuple[
    senderId: seq[byte], data: seq[byte], signature: seq[byte]
] =
    proc readU32(s: openArray[byte], offset: int): uint32 =
        uint32(s[offset]) or
        (uint32(s[offset+1]) shl 8) or
        (uint32(s[offset+2]) shl 16) or
        (uint32(s[offset+3]) shl 24)

    var pos = 0

    let senderIdSize = int(readU32(frame, pos)); pos += 4
    let senderId = @(frame[pos ..< pos + senderIdSize]); pos += senderIdSize

    let dataSize = int(readU32(frame, pos)); pos += 4
    let data = @(frame[pos ..< pos + dataSize]); pos += dataSize

    let sigSize = int(readU32(frame, pos)); pos += 4
    let signature = @(frame[pos ..< pos + sigSize])

    (senderId, data, signature)



proc gossipNodeMain(port: uint16, peerAddrs: seq[string]) {.async.} =
    {.gcsafe.}:
        let listenAddr = MultiAddress.init("/ip4/0.0.0.0/tcp/" & $port).tryGet()
        gSwitch = newStandardSwitch(addrs = @[listenAddr])

        gGossipSub = GossipSub.init(switch = gSwitch)
        gSwitch.mount(gGossipSub)

        proc onMessage(topic: string, msg: Message): Future[ValidationResult] {.async.} =
            {.gcsafe.}:
                echo "[Nim] Received message on topic: ", topic

                let (senderId, data, signature) = decodeFrame(msg.data)
                let ret = nimGossipNodeOnGossip(
                    unsafeAddr senderId[0], senderId.len.csize_t,
                    unsafeAddr data[0], data.len.csize_t,
                    unsafeAddr signature[0], signature.len.csize_t
                )

                if ret == 0:
                    return ValidationResult.Accept
                else:
                    return ValidationResult.Reject

        gGossipSub.addValidator(gTopic, onMessage)
        gGossipSub.subscribe(gTopic, nil)
                
        await gSwitch.start()

        gPeerId = $gSwitch.peerInfo.peerId   # store for C++ to read

        echo "[Nim] Switch started with GossipSub!"
        echo "[Nim] Subscribed to topic: ", gTopic
        echo "[Nim] PeerID: ", gSwitch.peerInfo.peerId
        echo "[Nim] Listening on:"
        for a in gSwitch.peerInfo.addrs:
            echo "  ", a

        for addrStr in peerAddrs:
            proc doConnect(ma: MultiAddress) {.async.} =
                {.gcsafe.}:
                    let remotePeerId = await gSwitch.connect(ma)
                    echo "[Nim] Connected to peer: ", remotePeerId, " at ", ma
            let ma = MultiAddress.init(addrStr).tryGet()
            asyncSpawn doConnect(ma)

        # Block forever — the switch runs in the background
        await sleepAsync(hours(24))

proc nimGossipNodeInit(port: uint16, peerAddrsPtr: ptr cstring, peerAddrsCount: cint) {.exportc, cdecl.} =
    echo "[Nim] GossipNode starting on port: ", port
    var peerAddrs: seq[string]
    for i in 0 ..< int(peerAddrsCount):
        peerAddrs.add($cast[ptr UncheckedArray[cstring]](peerAddrsPtr)[i])
    waitFor gossipNodeMain(port, peerAddrs)

proc nimGossipNodeGetPeerId(): cstring {.exportc, cdecl.} =
    # Returns the PeerID string after init. Returns empty string if called too early.
    return cstring(gPeerId)



proc nimGossipNodeOnValidated(isValid: bool, keyGuardSignature: ptr uint8, keyGuardSignatureSize: csize_t) {.exportc, cdecl.} =
    echo "[Nim] Validation result: ", isValid, " with KeyGuard signature size: ", keyGuardSignatureSize


proc nimGossipNodeConnectPeer(multiaddr: cstring) {.exportc, cdecl.} =
    proc doConnect() {.async.} =
        {.gcsafe.}:
            let ma = MultiAddress.init($multiaddr).tryGet()
            let remotePeerId = await gSwitch.connect(ma)
            echo "[Nim] Connected to peer: ", remotePeerId, " at ", ma
    asyncSpawn doConnect()

proc nimGossipNodePublish(
    senderId: ptr uint8, senderIdSize: csize_t,
    data: ptr uint8, dataSize: csize_t,
    signature: ptr uint8, signatureSize: csize_t
) {.exportc, cdecl.} = 
    let senderIdBytes = @(cast[ptr UncheckedArray[byte]](senderId).toOpenArray(0, int(senderIdSize) - 1))
    let dataBytes     = @(cast[ptr UncheckedArray[byte]](data).toOpenArray(0, int(dataSize) - 1))
    let sigBytes      = @(cast[ptr UncheckedArray[byte]](signature).toOpenArray(0, int(signatureSize) - 1))

    let frame = encodeFrame(senderIdBytes, dataBytes, sigBytes)

    if gGossipSub == nil or gSwitch == nil:
        echo "[Nim] Error: GossipSub or Switch not initialized!"
        return

    proc doPublish() {.async.} =
        {.gcsafe.}:
            discard await gGossipSub.publish(gTopic, frame)

    asyncSpawn doPublish()
    echo "[Nim] Published ", frame.len, " bytes to topic: ", gTopic
    

