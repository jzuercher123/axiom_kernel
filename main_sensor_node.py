import yaml
import cv2
import time
import uuid
import datetime
from src.sal.mock_camera import MockCamera
from src.ipc.publisher import ZMQPublisher
from src.crypto.hash_mock import generate_hardware_hash


def load_config(path="config/sensor_config.yaml"):
    with open(path, 'r') as f:
        return yaml.safe_load(f)


def run():
    config = load_config()
    cam = MockCamera(
        device_id=config['sensor']['device'],
        width=config['sensor']['width'],
        height=config['sensor']['height'],
        fps_cap=config['sensor']['fps_cap']
    )
    pub = ZMQPublisher(address=config['ipc']['pub_address'])

    print(f"Axiom Kernel: Sensor Node active on {config['ipc']['pub_address']}")
    time.sleep(1)  # Allow subscriber time to connect

    frame_id = 0
    # Generate a unique session ID for this recording run
    session_id = str(uuid.uuid4())

    while True:
        frame = cam.read_frame()
        if frame is None:
            break

        # Encode frame to memory buffer for transmission
        _, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()

        # Generate cryptographic provenance
        crypto_hash = generate_hardware_hash(frame_bytes)

        # Generate ISO 8601 UTC timestamp for FITS compatibility
        timestamp_utc = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%S.%fZ")

        # Axiom Provenance Protocol Metadata (FITS Compliant)
        metadata = {
            "SYS_FRM": frame_id,
            "SYS_SESS": session_id,
            "DATE_OBS": timestamp_utc,
            "EXPTIME": config['sensor'].get('delay', 0.033),  # Approx exposure time based on FPS
            "INSTRUME": "IMX462-MOCK",  # Will change to IMX462-STARVIS when hardware arrives
            "NAXIS1": config['sensor']['width'],
            "NAXIS2": config['sensor']['height'],
            "BITPIX": 8,
            "LOC_LAT": 40.4842,  # Hardcoded dev location (Bloomington, IL)
            "LOC_LON": -88.9936,  # Hardcoded dev location
            "LOC_ELEV": 243.0,
            "PROVHASH": crypto_hash,
            "PROVSIG": "mock_ecdsa_signature_placeholder",  # Hardware signature placeholder
            "DEV_ID": "AK-NODE-001"
        }

        # Broadcast over ZeroMQ IPC
        pub.publish(metadata, frame_bytes)
        frame_id += 1

    cam.release()


if __name__ == "__main__":
    run()