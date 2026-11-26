import yaml
import cv2
import numpy as np
from src.ipc.subscriber import ZMQSubscriber
from src.vision.background_sub import KinematicIsolator
from src.data.fits_exporter import FitsExporter


def load_config(path="config/sensor_config.yaml"):
    with open(path, 'r') as f:
        return yaml.safe_load(f)


def run():
    config = load_config()
    sub = ZMQSubscriber(address=config['ipc']['sub_address'])
    isolator = KinematicIsolator()
    fits_out = FitsExporter()

    print(f"Axiom Kernel: Processing Node active...")

    while True:
        metadata, frame_bytes = sub.receive()

        nparr = np.frombuffer(frame_bytes, np.uint8)
        frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

        # 1. Run kinematic isolation filter
        fg_mask = isolator.apply(frame)

        # 2. Clean the mask (remove minor sensor static)
        _, thresh = cv2.threshold(fg_mask, 244, 255, cv2.THRESH_BINARY)

        # 3. Extract Contours (grouping pixels into objects)
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        anomaly_detected = False

        # 4. Loop through detected objects
        for contour in contours:
            area = cv2.contourArea(contour)

            # Ignore tiny clusters of noise (adjust this threshold based on sensor)
            if area > 500:
                anomaly_detected = True

                # Calculate the Bounding Box
                x, y, w, h = cv2.boundingRect(contour)

                # Calculate the Centroid (exact x, y center of the mass)
                cx = x + (w // 2)
                cy = y + (h // 2)

                # Draw UI Overlays on the raw frame for real-time monitoring
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                cv2.circle(frame, (cx, cy), 4, (0, 0, 255), -1)
                cv2.putText(frame, f"Tracking: {cx}, {cy}", (x, y - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

                print(f"[{metadata['DATE_OBS']}] Target Locked at X:{cx} Y:{cy}")

        # 5. Export FITS dataset only if a valid object was tracked
        if anomaly_detected:
            # Uncomment below when you are ready to write to disk
            # fits_out.export_frame(frame, metadata)
            pass

        # Display the live UI
        cv2.imshow("Axiom Kernel: Raw Feed with Tracking", frame)
        cv2.imshow("Axiom Kernel: Kinematic Isolation", fg_mask)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()


if __name__ == "__main__":
    run()
