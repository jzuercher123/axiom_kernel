import zmq


class ZMQSubscriber:
    def __init__(self, address="tcp://localhost:5555"):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.connect(address)

        # Subscribe to all incoming topics on this port
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

    def receive(self):
        """Blocks until data is received, returning both metadata and binary payload."""
        metadata = self.socket.recv_json()
        payload_bytes = self.socket.recv()
        return metadata, payload_bytes