proc nimListenerInit(port: uint16) {.exportc, cdecl.} =
    # Initialize the listener on the specified port
    echo "[Nim] Listener initialized on port: ", port


proc nimListenerOnValidated(isValid: bool,  validatorSignature: ptr uint8, validatorSignatureSize: csize_t) {.exportc, cdecl.} =
    echo "[Nim] Validation result: ", isValid, " with validator signature size: ", validatorSignatureSize

proc nimListenerOnGossip(data: ptr uint8, dataSize: csize_t, signature: ptr uint8, signatureSize: csize_t):
    cint {.importc, cdecl.}
   