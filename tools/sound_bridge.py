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


def have_switch_audio_source() -> bool:
    return subprocess.call(
        ["which", "SwitchAudioSource"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ) == 0


def list_output_devices():
    if not have_switch_audio_source():
        raise RuntimeError(
            "SwitchAudioSource not found. Install with: brew install switchaudio-osx"
        )
    out = subprocess.check_output(["SwitchAudioSource", "-a", "-t", "output"], text=True)
    return [line.strip() for line in out.splitlines() if line.strip()]


def set_output_device(name: str):
    if not have_switch_audio_source():
        raise RuntimeError(
            "SwitchAudioSource not found. Install with: brew install switchaudio-osx"
        )
    subprocess.check_call(["SwitchAudioSource", "-s", name, "-t", "output"])


def set_output_device_by_id(idx: int):
    devices = list_output_devices()
    if idx < 1 or idx > len(devices):
        raise RuntimeError(f"Invalid output device id: {idx}. Valid range: 1..{len(devices)}")
    set_output_device(devices[idx - 1])
    return devices[idx - 1]


def main():
    ap = argparse.ArgumentParser(description="Play laptop sounds from Arduino serial events")
    ap.add_argument("--port", help="Serial port (e.g., /dev/cu.usbmodem1101)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--root", default="/Users/patliu/Desktop/Coding/Battleship", help="Project root with mp3 files")
    ap.add_argument("--list-ports", action="store_true", help="Print detected serial ports and exit")
    ap.add_argument("--list-output-devices", action="store_true", help="List audio output devices and exit (macOS, SwitchAudioSource)")
    ap.add_argument("--output-device", help="Audio output device name (macOS, SwitchAudioSource)")
    ap.add_argument("--output-device-id", type=int, help="Audio output device id from --list-output-devices (1-based)")
    ap.add_argument("--test-event", help="Play one event immediately and exit (example: BATTLE_HIT)")
    ap.add_argument("--audio", type=int, choices=[0, 1, 2, 3], help="Play/stop audio command and exit (0=stop, 1..3=track)")
    ap.add_argument("--camera-device-id", type=int, help="Enable Dino camera control with device id (e.g. 0 or 1)")
    ap.add_argument("--camera-jump-threshold", type=float, default=18.0, help="Camera jump threshold in pixels")
    ap.add_argument("--camera-cooldown-ms", type=int, default=450, help="Minimum ms between camera jump commands")
    ap.add_argument("--camera-show", action="store_true", help="Show camera debug window")
    ap.add_argument("--print-events", action="store_true", help="Print supported event names and exit")
    args = ap.parse_args()

    root = args.root
    sounds = {
        # Menu / boot
        "BOOT_START": os.path.join(root, "wii-startup-sound.mp3"),
        "MENU_CLICK": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "MENU_MUSIC_1": os.path.join(root, "C418 - Sweden - Minecraft.mp3"),
        "MENU_MUSIC_2": os.path.join(root, "Lil_Tecca_-_OWA_OWA_(mp3.pm).mp3"),
        "MENU_MUSIC_3": os.path.join(root, "2hollis_-_light_(mp3.pm).mp3"),
        "MENU_MUSIC_STOP": "",
        "MUSIC_PLAY_1": os.path.join(root, "C418 - Sweden - Minecraft.mp3"),
        "MUSIC_PLAY_2": os.path.join(root, "Lil_Tecca_-_OWA_OWA_(mp3.pm).mp3"),
        "MUSIC_PLAY_3": os.path.join(root, "2hollis_-_light_(mp3.pm).mp3"),
        "MUSIC_STOP": "",
        "MUSIC_PLAY": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "MUSIC_PAUSE": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "MUSIC_SELECT": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "MUSIC_TRACK_CHANGE": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),

        # Battleship
        "BATTLE_HIT": os.path.join(root, "fahhh-made-with-Voicemod.mp3"),
        "BATTLE_MISS": os.path.join(root, "kirby-falling-made-with-Voicemod.mp3"),
        "BATTLE_WIN": os.path.join(root, "rick-roll-(never-gonna-give-you-up)-sound-effect-made-with-Voicemod.mp3"),
        "BATTLE_PLACE_OK": os.path.join(root, "wii---button-press-made-with-Voicemod.mp3"),
        "BATTLE_PLACE_ERR": os.path.join(root, "bruh-mp3-made-with-Voicemod.mp3"),

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
        2: os.path.join(root, "Lil_Tecca_-_OWA_OWA_(mp3.pm).mp3"),
        3: os.path.join(root, "2hollis_-_light_(mp3.pm).mp3"),
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

    if args.list_output_devices:
        try:
            devices = list_output_devices()
        except RuntimeError as e:
            print(str(e))
            return
        print("Audio output devices:")
        for i, d in enumerate(devices, start=1):
            print(f"  {i}. {d}")
        return

    if args.output_device_id is not None:
        try:
            selected = set_output_device_by_id(args.output_device_id)
            print(f"Audio output device [{args.output_device_id}] set to: {selected}")
        except RuntimeError as e:
            print(str(e))
            return
        except subprocess.CalledProcessError:
            print(f"Failed to set output device id: {args.output_device_id}")
            return

    if args.output_device:
        try:
            set_output_device(args.output_device)
            print(f"Audio output device set to: {args.output_device}")
        except RuntimeError as e:
            print(str(e))
            return
        except subprocess.CalledProcessError:
            print(f"Failed to set output device: {args.output_device}")
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
    cam = None
    face_cascade = None
    baseline_y = None
    last_cam_jump_at = 0.0

    if args.camera_device_id is not None:
        try:
            import cv2  # type: ignore
        except ImportError:
            print("opencv-python is required for camera mode. Install with: pip install opencv-python")
            return

        cam = cv2.VideoCapture(args.camera_device_id)
        if not cam.isOpened():
            print(f"Failed to open camera device {args.camera_device_id}")
            return
        face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
        print(f"Camera mode ON (device {args.camera_device_id})")

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

    def extract_event_key(line: str):
        raw = line.strip().upper()
        if not raw:
            return None
        if raw in sounds:
            return raw

        token = re.split(r"[\s,:]+", raw, maxsplit=1)[0]
        if token in sounds:
            return token

        for key in sounds.keys():
            if key and key in raw:
                return key
        return None

    while True:
        try:
            port = pick_port(args.port, allow_fallback=True)
            print(f"Listening on {port} @ {args.baud}")
            with serial.Serial(port, args.baud, timeout=0.03) as ser:
                # allow board reset on serial open
                time.sleep(2.0)
                while True:
                    if cam is not None and face_cascade is not None:
                        ok, frame = cam.read()
                        if ok:
                            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                            faces = face_cascade.detectMultiScale(
                                gray,
                                scaleFactor=1.1,
                                minNeighbors=5,
                                minSize=(40, 40),
                            )
                            if len(faces) > 0:
                                x, y, w, h = max(faces, key=lambda r: r[2] * r[3])
                                center_y = y + h * 0.5
                                if baseline_y is None:
                                    baseline_y = center_y
                                else:
                                    baseline_y = baseline_y * 0.96 + center_y * 0.04

                                dy = baseline_y - center_y
                                now = time.time()
                                if dy >= args.camera_jump_threshold and (now - last_cam_jump_at) * 1000.0 >= args.camera_cooldown_ms:
                                    try:
                                        ser.write(b"DINO_JUMP\n")
                                        last_cam_jump_at = now
                                        print("CAMERA: DINO_JUMP")
                                    except serial.SerialException:
                                        pass

                                if args.camera_show:
                                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                                    cv2.putText(frame, f"dy={dy:.1f}", (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                            if args.camera_show:
                                cv2.imshow("Sound Bridge Camera", frame)
                                key = cv2.waitKey(1) & 0xFF
                                if key == 27 or key == ord("q"):
                                    raise KeyboardInterrupt

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

                    event_key = extract_event_key(line)
                    if event_key is None:
                        continue

                    if event_key == "MENU_MUSIC_STOP":
                        stop_menu_music()
                    elif event_key == "MENU_MUSIC_1":
                        start_menu_music(sounds["MENU_MUSIC_1"])
                    elif event_key == "MENU_MUSIC_2":
                        start_menu_music(sounds["MENU_MUSIC_2"])
                    elif event_key == "MENU_MUSIC_3":
                        start_menu_music(sounds["MENU_MUSIC_3"])
                    elif event_key == "MUSIC_STOP":
                        stop_menu_music()
                    elif event_key == "MUSIC_PLAY_1":
                        start_menu_music(sounds["MUSIC_PLAY_1"])
                    elif event_key == "MUSIC_PLAY_2":
                        start_menu_music(sounds["MUSIC_PLAY_2"])
                    elif event_key == "MUSIC_PLAY_3":
                        start_menu_music(sounds["MUSIC_PLAY_3"])
                    elif event_key in sounds:
                        play_file(sounds[event_key])
        except serial.SerialException as e:
            print(f"Serial open error: {e}")
        except KeyboardInterrupt:
            break
        except RuntimeError as e:
            print(str(e))

        stop_menu_music()
        print("Reconnecting in 1.5s...")
        time.sleep(1.5)

    if cam is not None:
        cam.release()
        try:
            import cv2  # type: ignore
            cv2.destroyAllWindows()
        except Exception:
            pass


if __name__ == "__main__":
    main()
