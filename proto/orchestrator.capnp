@0xd3b2a1c4e5f60718;

interface MicroService {
    getName @0 () -> (name :Text);
    # heartbeat for orchestrator to check if the service is still alive
    ping @1 () -> ();
}

interface Validator extends(MicroService) {
    validateBlock @0 (data :Text, signature :Text) -> (isValid :Bool, hash :Text);
}

interface NetworkListener extends(MicroService) {
    startListening @0 (port :UInt16) -> ();

}

interface Orchestrator {
  # Returns the names of all currently registered services.
  getServices @0 () -> (services :List(Text));

  connectToValidator @1 () -> (validator :Validator);
  connectToNetworkListener @2 () -> (listener :NetworkListener);
}
