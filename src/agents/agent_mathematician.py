import zmq
import argparse
import numpy as np
import cv2


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipc", type=str, default="tcp://localhost:5555", help="ZeroMQ IPC Bus")
    return parser.parse_args()


def run_agent():
    args = parse_args()

    # 1. Connect to the Universal Bus
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect(args.ipc)
    socket.setsockopt_string(zmq.SUBSCRIBE, "")

    print(f"[MATHEMATICIAN] Subscribed to {args.ipc}. Awaiting data...")

    while True:
        # 2. Ingest the standardized Axiom payload
        metadata = socket.recv_json()
        frame_bytes = socket.recv()

        # 3. Reconstruct the physical matrix
        nparr = np.frombuffer(frame_bytes, np.uint8)
        frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        # 4. Agent-specific logic goes here.
        # (e.g., PySINDy analyzing the bounding box coordinates to derive equations)

        frame_id = metadata.get("SYS_FRM", "UNKNOWN")
        hash_prefix = metadata.get("PROVHASH", "UNKNOWN")[:8]

        print(f"[MATHEMATICIAN] Processed Frame {frame_id} (Hash: {hash_prefix}). Extracting dynamics...")


if __name__ == "__main__":
    run_agent()
