import cv2

class KinematicIsolator:
    def __init__(self, history=500, var_threshold=16, detect_shadows=False):
        self.back_sub = cv2.createBackgroundSubtractorMOG2(
            history=history,
            varThreshold=var_threshold,
            detectShadows=detect_shadows
        )

    def apply(self, frame):
        """Returns the foreground mask highlighting moving kinematics."""
        return self.back_sub.apply(frame)
