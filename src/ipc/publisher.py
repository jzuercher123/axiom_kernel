import zmq

class ZMQPublisher:
    def __init__(self, address="tcp://*:5555"):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.bind(address)

    def publish(self, metadata: dict, payload_bytes: bytes):
        """Transmits the JSON metadata followed immediately by the binary frame."""
        self.socket.send_json(metadata, flags=zmq.SNDMORE)
        self.socket.send(payload_bytes)