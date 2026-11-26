import os
import cv2
import numpy as np
from astropy.io import fits


class FitsExporter:
    def __init__(self, output_dir="data/output_fits"):
        self.output_dir = output_dir
        # Ensure the output directory exists
        os.makedirs(self.output_dir, exist_ok=True)

    def export_frame(self, frame: np.ndarray, metadata: dict) -> str:
        """
        Converts an OpenCV image matrix and JSON metadata into a FITS dataset.
        """
        # 1. Format the Image Data for Astrophysics
        # FITS standards heavily prefer 2D grayscale arrays for sensor telemetry.
        # If the OpenCV frame is 3-channel BGR, we convert it to 1-channel grayscale.
        if len(frame.shape) == 3:
            frame_fits = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        else:
            frame_fits = frame

        # 2. Create the Primary Header Data Unit (HDU)
        hdu = fits.PrimaryHDU(frame_fits)
        hdr = hdu.header

        # 3. Inject the Axiom Provenance Protocol Metadata
        # FITS requires keys to be <= 8 characters and uppercase,
        # which your JSON schema already perfectly accommodates.
        for key, value in metadata.items():
            try:
                hdr[key] = value
            except ValueError as e:
                print(f"FITS Header Error on {key}: {e}")

        # 4. Generate a Unique, Timestamped Filename
        # Replace colons in the ISO 8601 string to prevent file system errors
        safe_time = metadata.get('DATE_OBS', 'UNKNOWN').replace(':', '')
        dev_id = metadata.get('DEV_ID', 'NODE')

        filename = f"AXIOM_{dev_id}_{safe_time}.fits"
        filepath = os.path.join(self.output_dir, filename)

        # 5. Write to Disk
        hdu.writeto(filepath, overwrite=True)
        return filepath
