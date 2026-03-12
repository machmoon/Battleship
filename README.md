# SBII LED Matrix Console (32x24, 1 Joystick)

This project runs a mini console on 12 MAX7219 chips (three 4-in-1 modules).

Current main sketch:
- `src/Game_Console/Game_Console.ino`

## Display Layout (Important)
Your calibrated mapping is:
- Top row: `4,3,2,1`
- Middle row: `8,7,6,5`
- Bottom row: `12,11,10,9`

Logical screen size is `32x24`.

## Hardware Wiring

### MAX7219 chain (12 chips total)
- Arduino `D11` -> `DIN` (first module in chain)
- Arduino `D13` -> `CLK`
- Arduino `D10` -> `CS`
- 5V -> all module `VCC`
- GND -> all module `GND`
- Chain `DOUT -> DIN` between modules

### Controller 1 (P1 / global)
- `VRx` -> `A0`
- `VRy` -> `A1`
- `SW` -> `A5`
- `VCC` -> `5V`
- `GND` -> `GND`

### Other IO
- Buzzer: `D7` (+), `GND` (-)

## Upload
1. Open Arduino IDE.
2. Select board/port.
3. Upload `src/Game_Console/Game_Console.ino`.

## Boot + Menu
- Standby screen: rectangle with `SBII`.
- Press joystick button (`A5`) to power on and boot.
- In menu:
  - Controller 1 left/right changes app.
  - Controller 1 button (`A5`) enters app.

Menu apps:
1. Battleship
2. Snake
3. Dino
4. Surf
5. Reaction
6. Parkour
7. Music Player

## Game Controls

### Battleship (middle row only, 32x8)
Layout:
- `x 0..7`: P1 board
- `x 8..15`: P1 shots
- `x 16..23`: P2 shots
- `x 24..31`: P2 board

Flow:
- P1 places 4 ships
- P2 places 4 ships
- P1 turn
- P2 turn

Controls:
- `A0/A1` move, `A5` confirm (all phases).
- Game over: `A5` returns to menu.

### Snake / Dino / Surf / Reaction / Parkour
- Controlled by Controller 1 (`A0/A1`, `A5`).
- Press action after game over to return to menu.
- These games now use the full `32x24` display area.

### Music Player
- Enter from menu item `MUSC`.
- Controller 1 up/down (`A1`) changes track.
- Controller 1 button (`A5`) selects this song as **menu background music**.
- Move joystick left (`A0`) to exit to menu.
- Tracks:
  - `MINE` -> `C418 - Sweden - Minecraft.mp3`
  - `OWA` -> `Lil_Tecca_-_OWA_OWA_(mp3.pm).mp3`
  - `LITE` -> `2hollis_-_light_(mp3.pm).mp3`
 - After selecting, menu will use that song (`MENU_MUSIC_1/2/3`).

## Debugging
- Open Serial Monitor at `115200` for debug snapshots/events.
- For display mapping checks, upload:
  - `src/Display_Calibration/Display_Calibration.ino`

## Optional PC Sound Bridge
If you want external PC sounds from serial events:
- `python3 tools/sound_bridge.py`

Manual audio command examples:
- `python3 tools/sound_bridge.py --audio 1`
- `python3 tools/sound_bridge.py --audio 2`
- `python3 tools/sound_bridge.py --audio 3`
- `python3 tools/sound_bridge.py --audio 0` (stop/no-op and exit)
- `audio 1/2/3` now map to `Minecraft / OWA OWA / light`.

Audio output devices (macOS):
- Install helper: `brew install switchaudio-osx`
- List devices: `python3 tools/sound_bridge.py --list-output-devices`
- Set device: `python3 tools/sound_bridge.py --output-device \"MacBook Pro Speakers\"`
- Set device by id: `python3 tools/sound_bridge.py --output-device-id 1`
- Set + run bridge: `python3 tools/sound_bridge.py --output-device \"MacBook Pro Speakers\" --port /dev/cu.usbmodemXXXX`

When running continuously, it also accepts serial commands:
- `AUDIO 1`, `AUDIO 2`, `AUDIO 3`, `AUDIO 0`

## Optional Screen Mirror
- Arduino receiver: `src/Screen_Mirror_Receiver/Screen_Mirror_Receiver.ino`
- Sender:
  - `python3 tools/screen_to_matrix.py --port <PORT> --fps 15 --width 32 --height 24`
