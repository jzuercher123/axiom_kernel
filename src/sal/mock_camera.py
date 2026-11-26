import cv2
import time


class MockCamera:
    def __init__(self, device_id=0, width=640, height=480, fps_cap=30):
        self.cap = cv2.VideoCapture(device_id)
        self.width = width
        self.height = height
        self.delay = 1.0 / fps_cap if fps_cap > 0 else 0

    def read_frame(self):
        ret, frame = self.cap.read()

        # Loop video if playing from a file and it reaches the end
        if not ret:
            self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ret, frame = self.cap.read()
            if not ret:
                return None

        frame = cv2.resize(frame, (self.width, self.height))

        # Throttle to simulate physical sensor bandwidth constraints
        if self.delay > 0:
            time.sleep(self.delay)

        return frame

    def release(self):
        self.cap.release()