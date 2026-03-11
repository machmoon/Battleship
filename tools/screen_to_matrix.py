#!/usr/bin/env python3
"""
Capture the computer screen, downsample, threshold to on/off,
and stream frames to an Arduino MAX7219 receiver.

Protocol sent each frame:
  F,<hex payload>\n
Row-major bytes in hex:
- 32x8  => 64 hex chars
- 32x24 => 192 hex chars (for 4x3 module layout)
"""

import argparse
import time

from PIL import ImageGrab, ImageOps
import serial


def frame_to_hex(img_luma):
    w, h = img_luma.size
    pixels = img_luma.load()
    row_bytes = []

    for y in range(h):
        for block in range(w // 8):
            b = 0
            for col in range(8):
                x = block * 8 + col
                on = pixels[x, y] > 127
                if on:
                    b |= 1 << (7 - col)
            row_bytes.append(b)

    return "".join(f"{b:02X}" for b in row_bytes)


def main():
    parser = argparse.ArgumentParser(description="Mirror PC screen to MAX7219 via serial")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/cu.usbmodem1101")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--fps", type=float, default=15.0)
    parser.add_argument("--width", type=int, default=32, help="Output width (multiple of 8)")
    parser.add_argument("--height", type=int, default=24, help="Output height (24 for 4x3 module layout)")
    parser.add_argument("--invert", action="store_true", help="Invert black/white mapping")
    args = parser.parse_args()

    if args.width % 8 != 0:
        raise SystemExit("--width must be a multiple of 8")

    ser = serial.Serial(args.port, args.baud, timeout=0)
    print(f"Streaming {args.width}x{args.height} to {args.port} @ {args.baud}")

    frame_dt = 1.0 / max(1.0, args.fps)
    next_t = time.time()

    try:
        while True:
            shot = ImageGrab.grab(all_screens=True)
            gray = ImageOps.grayscale(shot).resize((args.width, args.height))
            if args.invert:
                gray = ImageOps.invert(gray)

            payload = frame_to_hex(gray)
            ser.write(f"F,{payload}\n".encode("ascii"))

            next_t += frame_dt
            sleep_t = next_t - time.time()
            if sleep_t > 0:
                time.sleep(sleep_t)
            else:
                next_t = time.time()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
