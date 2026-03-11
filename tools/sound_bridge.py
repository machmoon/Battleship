#!/usr/bin/env python3
import argparse
import glob
import os
import re
import subprocess
import sys
import time
from typing import Optional

try:
    import serial
except ImportError:
    print("pyserial is required. Install with: pip install pyserial")
    sys.exit(1)


def pick_port(explicit_port: Optional[str], allow_fallback: bool = True) -> str:
    if explicit_port:
        if os.path.exists(explicit_port):
            return explicit_port
        if not allow_fallback:
            available = ", ".join(list_ports()) or "none"
            raise RuntimeError(f"Port not found: {explicit_port}. Available: {available}")
        print(f"Requested port not found: {explicit_port}. Falling back to auto-detect.")

    candidates = list_ports()
    if not candidates:
        raise RuntimeError("No Arduino-like serial port found. Use --port /dev/cu.usbmodemXXXX")

    candidates.sort()
    return candidates[0]


def list_ports():
    candidates = []
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbmodem*", "/dev/tty.usbserial*"):
        candidates.extend(glob.glob(pattern))
    candidates.sort()
    return candidates


def play_file(path: str, wait: bool = False):
    if not path:
        return None
    if not os.path.exists(path):
        print(f"Sound file missing: {path}")
        return None

    # macOS first
    if subprocess.call(["which", "afplay"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
        if wait:
            subprocess.call(["afplay", path])
            return None
        return subprocess.Popen(["afplay", path])

    # fallback
    if subprocess.call(["which", "ffplay"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
        cmd = ["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", path]
        if wait:
            subprocess.call(cmd)
            return None
        return subprocess.Popen(cmd)

    print("No audio player found (afplay/ffplay).")
    return None


def main():
    ap = argparse.ArgumentParser(description="Play laptop sounds from Arduino serial events")
    ap.add_argument("--port", help="Serial port (e.g., /dev/cu.usbmodem1101)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--root", default="/Users/patliu/Desktop/Coding/Battleship", help="Project root with mp3 files")
    ap.add_argument("--list-ports", action="store_true", help="Print detected serial ports and exit")
    ap.add_argument("--test-event", help="Play one event immediately and exit (example: BATTLE_HIT)")
    ap.add_argument("--audio", type=int, choices=[0, 1, 2, 3], help="Play/stop audio command and exit (0=stop, 1..3=track)")
    ap.add_argument("--print-events", action="store_true", help="Print supported event names and exit")
    args = ap.parse_args()

    root = args.root
    sounds = {
        # Menu / boot
        "BOOT_START": os.path.join(root, "wii-startup-sound.mp3"),
        "MENU_CLICK": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "MENU_MUSIC_1": os.path.join(root, "C418 - Sweden - Minecraft.mp3"),
        "MENU_MUSIC_2": os.path.join(root, "rickroll-made-with-Voicemod.mp3"),
        "MENU_MUSIC_STOP": "",

        # Battleship
        "BATTLE_HIT": os.path.join(root, "fahhh-made-with-Voicemod.mp3"),
        "BATTLE_MISS": os.path.join(root, "kirby-falling-made-with-Voicemod.mp3"),
        "BATTLE_WIN": os.path.join(root, "rick-roll-(never-gonna-give-you-up)-sound-effect-made-with-Voicemod.mp3"),

        # Snake
        "SNAKE_HIT": os.path.join(root, "yippee!-made-with-Voicemod.mp3"),
        "SNAKE_FAIL": os.path.join(root, "sad-hamster-violin-made-with-Voicemod.mp3"),
        "SNAKE_WIN": os.path.join(root, "kirby-victory-made-with-Voicemod.mp3"),

        # Dino
        "DINO_JUMP_OK": os.path.join(root, "ultrakill-parry-sound-made-with-Voicemod.mp3"),
        "DINO_FAIL": os.path.join(root, "screaming-chicken-made-with-Voicemod.mp3"),

        # Surf
        "SURF_FAIL": os.path.join(root, "metal-pipe-falling-(earrape)-made-with-Voicemod.mp3"),

        # Reaction
        "REACT_HIT": os.path.join(root, "oh-no,-our-table,-its-broken-made-with-Voicemod.mp3"),
        "REACT_MISS": os.path.join(root, "bruh-mp3-made-with-Voicemod.mp3"),

        # Legacy aliases
        "HIT": os.path.join(root, "fahhh-made-with-Voicemod.mp3"),
        "MISS": os.path.join(root, "kirby-falling-made-with-Voicemod.mp3"),
        "WIN": os.path.join(root, "rick-roll-(never-gonna-give-you-up)-sound-effect-made-with-Voicemod.mp3"),
    }
    audio_tracks = {
        1: os.path.join(root, "C418 - Sweden - Minecraft.mp3"),
        2: os.path.join(root, "rickroll-made-with-Voicemod.mp3"),
        3: os.path.join(root, "wii-startup-sound.mp3"),
    }

    if args.print_events:
        print("Supported events:")
        for key in sorted(sounds.keys()):
            print(f"  {key}")
        print("Audio commands:")
        print("  AUDIO 0  (stop)")
        print("  AUDIO 1")
        print("  AUDIO 2")
        print("  AUDIO 3")
        return

    if args.list_ports:
        ports = list_ports()
        if not ports:
            print("No Arduino-like ports found.")
        else:
            print("Detected ports:")
            for p in ports:
                print(f"  {p}")
        return

    missing = [k for k, p in sounds.items() if p and not os.path.exists(p)]
    if missing:
        print("Warning: missing sound files for events:")
        for key in missing:
            print(f"  {key}: {sounds[key]}")

    missing_tracks = [idx for idx, p in audio_tracks.items() if not os.path.exists(p)]
    if missing_tracks:
        print("Warning: missing audio tracks:")
        for idx in missing_tracks:
            print(f"  audio {idx}: {audio_tracks[idx]}")

    if args.audio is not None:
        if args.audio == 0:
            print("audio 0 -> stop")
            return
        print(f"audio {args.audio} -> {audio_tracks[args.audio]}")
        play_file(audio_tracks[args.audio], wait=True)
        return

    if args.test_event:
        event = args.test_event.strip().upper()
        if event not in sounds:
            print(f"Unknown event: {event}")
            print("Run with --print-events to see valid names.")
            return
        print(f"Testing event {event}")
        play_file(sounds[event], wait=True)
        time.sleep(0.2)
        return

    menu_music_proc = None
    menu_music_path = ""

    def stop_menu_music():
        nonlocal menu_music_proc, menu_music_path
        if menu_music_proc and menu_music_proc.poll() is None:
            try:
                menu_music_proc.terminate()
                menu_music_proc.wait(timeout=0.8)
            except Exception:
                pass
        menu_music_proc = None
        menu_music_path = ""

    def start_menu_music(path: str):
        nonlocal menu_music_proc, menu_music_path
        if not path:
            return
        if menu_music_proc and menu_music_proc.poll() is None and menu_music_path == path:
            return
        stop_menu_music()
        menu_music_proc = play_file(path)
        menu_music_path = path

    def parse_audio_command(line: str):
        m = re.match(r"^AUDIO(?:\s+|_)([0-3])$", line.strip().upper())
        if not m:
            return None
        return int(m.group(1))

    while True:
        try:
            port = pick_port(args.port, allow_fallback=True)
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

                    if menu_music_proc and menu_music_proc.poll() is not None and menu_music_path:
                        menu_music_proc = play_file(menu_music_path)

                    if not line:
                        continue

                    print(f"EVENT: {line}")
                    audio_cmd = parse_audio_command(line)
                    if audio_cmd is not None:
                        if audio_cmd == 0:
                            stop_menu_music()
                        else:
                            start_menu_music(audio_tracks[audio_cmd])
                        continue

                    if line == "MENU_MUSIC_STOP":
                        stop_menu_music()
                    elif line == "MENU_MUSIC_1":
                        start_menu_music(sounds["MENU_MUSIC_1"])
                    elif line == "MENU_MUSIC_2":
                        start_menu_music(sounds["MENU_MUSIC_2"])
                    elif line in sounds:
                        play_file(sounds[line])
        except serial.SerialException as e:
            print(f"Serial open error: {e}")
        except RuntimeError as e:
            print(str(e))

        stop_menu_music()
        print("Reconnecting in 1.5s...")
        time.sleep(1.5)


if __name__ == "__main__":
    main()
