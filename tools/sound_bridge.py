#!/usr/bin/env python3
import argparse
import glob
import os
import subprocess
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial is required. Install with: pip install pyserial")
    sys.exit(1)


def pick_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port

    candidates = []
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbmodem*", "/dev/tty.usbserial*"):
        candidates.extend(glob.glob(pattern))

    if not candidates:
        raise RuntimeError("No Arduino-like serial port found. Use --port /dev/cu.usbmodemXXXX")

    candidates.sort()
    return candidates[0]


def play_file(path: str):
    if not path:
        return
    if not os.path.exists(path):
        print(f"Sound file missing: {path}")
        return

    # macOS first
    if subprocess.call(["which", "afplay"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
        subprocess.Popen(["afplay", path])
        return

    # fallback
    if subprocess.call(["which", "ffplay"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
        subprocess.Popen(["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", path])
        return

    print("No audio player found (afplay/ffplay).")


def main():
    ap = argparse.ArgumentParser(description="Play laptop sounds from Arduino serial events")
    ap.add_argument("--port", help="Serial port (e.g., /dev/cu.usbmodem1101)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hit", default="/Users/patliu/Desktop/Coding/Battleship/fawk-made-with-Voicemod.mp3")
    ap.add_argument("--miss", default="/Users/patliu/Desktop/Coding/Battleship/vine-boom-sound-effect-made-with-Voicemod.mp3")
    ap.add_argument("--win", default="/Users/patliu/Desktop/Coding/Battleship/rickroll-made-with-Voicemod.mp3")
    ap.add_argument("--error", default="")
    args = ap.parse_args()

    sounds = {
        "HIT": args.hit,
        "MISS": args.miss,
        "WIN": args.win,
        "ERROR": args.error,
    }

    while True:
        try:
            port = pick_port(args.port)
            print(f"Listening on {port} @ {args.baud}")
            with serial.Serial(port, args.baud, timeout=0.2) as ser:
                # allow board reset on serial open
                time.sleep(2.0)
                while True:
                    try:
                        line = ser.readline().decode(errors="ignore").strip().upper()
                    except serial.SerialException as e:
                        print(f"Serial read error: {e}")
                        break

                    if not line:
                        continue

                    print(f"EVENT: {line}")
                    if line in sounds:
                        play_file(sounds[line])
        except serial.SerialException as e:
            print(f"Serial open error: {e}")
        except RuntimeError as e:
            print(str(e))

        print("Reconnecting in 1.5s...")
        time.sleep(1.5)


if __name__ == "__main__":
    main()
