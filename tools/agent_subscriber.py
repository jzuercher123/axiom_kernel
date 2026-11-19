import pathlib
import yaml
import zmq
import numpy as np

DTYPE_MAP = {
    "UInt8": np.uint8,
    "UInt16": np.uint16,
    "Int16": np.int16,
    "Int32": np.int32,
    "Float32": np.float32,
    "Float64": np.float64,
}


def main():
    # 1. Load static physical & calibration context from fleet.yaml
    manifest_path = pathlib.Path(__file__).resolve().parent.parent / "config" / "fleet.yaml"
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = yaml.safe_load(f)

    endpoint = manifest["node"]["transport"]["endpoint"]
    topic = manifest["node"]["transport"]["topic"]
    sensor_cfg = manifest["sensors"][0]
    shape = tuple(sensor_cfg["shape"])
    dtype = DTYPE_MAP[sensor_cfg["dtype"]]

    # 2. Subscribe to the C++ Axiom Kernel node
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.connect(endpoint)
    sub.setsockopt_string(zmq.SUBSCRIBE, topic)

    print(f"[agent] Loaded manifest for '{sensor_cfg['name']}' ({sensor_cfg['physical_domain']})")
    print(f"[agent] Listening on {endpoint} | topic='{topic}' | expected shape={shape}, dtype={dtype.__name__}")
    print("[agent] Start sda_node now...\n")

    received = 0
    try:
        while True:
            part1_topic, part2_header, part3_payload = sub.recv_multipart()
            received += 1

            # Zero-copy view of the raw C++ tensor bytes
            tensor = np.frombuffer(part3_payload, dtype=dtype).reshape(shape)

            print(
                f"[agent] Anomaly #{received} | topic={part1_topic.decode()} | "
                f"header={len(part2_header)}B | tensor={tensor.shape} | "
                f"mean={tensor.mean():.2f} | peak={tensor.max()} {sensor_cfg['value_units']}"
            )
    except KeyboardInterrupt:
        print(f"\n[agent] Stopped. Total verified anomalies received: {received}")
    finally:
        sub.close()
        ctx.term()


if __name__ == "__main__":
    main()