# Battleship on MAX7219 4-in-1 + Joystick (Arduino)

This is a full playable mini Battleship game for:
- `HiLetgo MAX7219 Dot Matrix Module 4-in-1 (5-pin)`
- 1 analog joystick module (X, Y, SW)

## 1) Arduino Libraries
Install this library from Arduino Library Manager:
- `MD_MAX72XX` by MajicDesigns

`SPI` is built in.

## 2) Wiring

### MAX7219 4-in-1 module
- `VCC` -> `5V`
- `GND` -> `GND`
- `DIN` -> `D11`
- `CS`  -> `D10`
- `CLK` -> `D13`

### Joystick module
- `VCC` -> `5V`
- `GND` -> `GND`
- `VRx` -> `A0`
- `VRy` -> `A1`
- `SW`  -> `A2`

## 3) Upload
Open:
- `src/Battleship_MAX7219.ino`

Select your board/port, then upload.

## 4) How to Play
- Press joystick button to start.
- Move joystick to move the target cursor on the **right board** (enemy).
- Press joystick button to fire.
- Left board is your ships; right board is enemy state.
- Press joystick button on game-over screen to go back to start.

## 5) Display Legend (each cell is 2x2 pixels)
- Empty: off
- Ship (your board): diagonal pixels
- Miss: 1 pixel
- Hit: full 2x2 block
- Cursor (enemy board): opposite diagonal blinking

## 6) Current Game Rules
- Board size: `4x4`
- Ships per side: `4` single-cell ships
- Enemy AI: random legal shot

If you want, I can add next:
1. Smarter AI (hunt/target)
2. Multi-cell ships with orientation
3. Difficulty levels
4. Score + win/loss counter
# Battleship
