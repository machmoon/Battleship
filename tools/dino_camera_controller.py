#!/usr/bin/env python3
import argparse
import glob
import os
import sys
import time
from typing import Optional

try:
    import cv2
except ImportError:
    print("opencv-python is required. Install with: pip install opencv-python")
    sys.exit(1)

try:
    import serial
except ImportError:
    print("pyserial is required. Install with: pip install pyserial")
    sys.exit(1)


def list_ports():
    ports = []
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbmodem*", "/dev/tty.usbserial*"):
        ports.extend(glob.glob(pattern))
    ports.sort()
    return ports


def pick_port(explicit: Optional[str]) -> str:
    if explicit:
        if os.path.exists(explicit):
            return explicit
        raise RuntimeError(f"Port not found: {explicit}")
    ports = list_ports()
    if not ports:
        raise RuntimeError("No serial port found. Pass --port /dev/cu.usbmodemXXXX")
    return ports[0]


def main():
    ap = argparse.ArgumentParser(description="Control Dino jump using camera motion")
    ap.add_argument("--port", help="Arduino serial port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--device-id", type=int, default=0, help="Camera device id, e.g. 0 or 1")
    ap.add_argument("--jump-threshold", type=float, default=18.0, help="Pixels above baseline to trigger jump")
    ap.add_argument("--cooldown-ms", type=int, default=450, help="Minimum ms between jumps")
    ap.add_argument("--show", action="store_true", help="Show camera debug window")
    ap.add_argument("--list-ports", action="store_true")
    args = ap.parse_args()

    if args.list_ports:
        for p in list_ports():
            print(p)
        return

    port = pick_port(args.port)
    print(f"Opening serial {port} @ {args.baud}")
    ser = serial.Serial(port, args.baud, timeout=0.05)
    time.sleep(2.0)

    cam = cv2.VideoCapture(args.device_id)
    if not cam.isOpened():
        print(f"Failed to open camera device {args.device_id}")
        return

    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    baseline_y = None
    last_jump_at = 0.0

    print("Running. Press Ctrl+C to stop.")
    try:
        while True:
            ok, frame = cam.read()
            if not ok:
                time.sleep(0.02)
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(40, 40))

            jump_sent = False
            if len(faces) > 0:
                # Use largest face for stability.
                x, y, w, h = max(faces, key=lambda r: r[2] * r[3])
                center_y = y + h * 0.5

                if baseline_y is None:
                    baseline_y = center_y
                else:
                    # Slow baseline follow so quick upward movement stands out.
                    baseline_y = baseline_y * 0.96 + center_y * 0.04

                now = time.time()
                dy = baseline_y - center_y
                if dy >= args.jump_threshold and (now - last_jump_at) * 1000.0 >= args.cooldown_ms:
                    ser.write(b"DINO_JUMP\n")
                    last_jump_at = now
                    jump_sent = True
                    print("JUMP")

                if args.show:
                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                    cv2.putText(frame, f"dy={dy:.1f}", (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                    cv2.putText(frame, f"baseline={baseline_y:.1f}", (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 200, 0), 2)

            if args.show:
                if jump_sent:
                    cv2.putText(frame, "JUMP SENT", (10, 80), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 80, 255), 2)
                cv2.imshow("Dino Camera Controller", frame)
                key = cv2.waitKey(1) & 0xFF
                if key == 27 or key == ord('q'):
                    break

    except KeyboardInterrupt:
        pass
    finally:
        cam.release()
        cv2.destroyAllWindows()
        ser.close()


if __name__ == "__main__":
    main()
