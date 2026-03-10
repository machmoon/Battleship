# LED Matrix Game Console (Beginner Setup)

This project turns an Arduino + MAX7219 LED matrices into a mini game console.

It includes:
- Boot screen (`SBII`)
- Menu + games (Battleship, Snake, Dino, Surf, Reaction, Parkour)
- Optional PC screen mirroring mode

---

## 1) What You Need
- Arduino (Uno/Nano)
- Two MAX7219 4-in-1 LED matrix modules (stacked/chained)
- Joystick module
- 1 push button (main power/action button)
- Passive buzzer (optional but recommended)
- Jumper wires
- Stable 5V power

---

## 2) Wiring (Exact Pins)

### LED Displays (2 modules, chained)
- Arduino `D11` -> `DIN` on Display 1
- Display 1 `DOUT` -> `DIN` on Display 2
- Arduino `D10` -> `CS` on both displays
- Arduino `D13` -> `CLK` on both displays
- Arduino `5V` -> `VCC` on both displays
- Arduino `GND` -> `GND` on both displays

Important:
- All grounds must be shared (Arduino + both displays + all modules)
- Two displays need more current than one, so use a solid 5V source

### Joystick
- `VRx` -> `A0`
- `VRy` -> `A1`
- `SW` -> `A2`
- `VCC` -> `5V`
- `GND` -> `GND`

### Main Button
- One leg -> `D2`
- Other leg -> `GND`

### Buzzer
- `+` -> `D7`
- `-` -> `GND`

---

## 3) Upload the Main Console
Open and upload:
- `src/Game_Console/Game_Console.ino`

After upload:
1. Press the main button on `D2` to turn on.
2. Watch boot animation (`SBII`).
3. Use joystick left/right in menu.
4. Press joystick button (`A2`) or main button (`D2`) to select.

---

## 4) Game Controls
- Joystick: movement/menu navigation
- `A2` joystick button: select/action
- `D2` main button: also works as action/select in games

---

## 5) Optional: Mirror Computer Screen to LEDs

### Arduino side
Upload:
- `src/Screen_Mirror_Receiver/Screen_Mirror_Receiver.ino`

### Computer side
Install Python packages:
- `pip install pillow pyserial`

Run:
- `python3 tools/screen_to_matrix.py --port /dev/cu.usbmodem1101 --fps 15`

---

## 6) If It Doesn’t Work
- Close Arduino Serial Monitor before running Python tools
- Only one app can use the serial port at a time
- Double-check `DIN/CS/CLK` pins (`11/10/13`)
- Ensure Display 1 `DOUT` really goes to Display 2 `DIN`
- Ensure all `GND` lines are connected together

---

## 7) Key Files
- Main console: `src/Game_Console/Game_Console.ino`
- Original Battleship sketch: `src/Battleship_MAX7219/Battleship_MAX7219.ino`
- Screen mirror receiver: `src/Screen_Mirror_Receiver/Screen_Mirror_Receiver.ino`
- Screen mirror sender: `tools/screen_to_matrix.py`
