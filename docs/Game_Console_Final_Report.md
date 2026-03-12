# Game Console Final Report (Concise)

## 1. Project Summary
We built a 2-player Battleship game on Arduino using MAX7219 LED matrices and joystick input.
Players place ships, take turns firing, and receive hit/miss feedback on the LED display until one side wins.

## 2. Core Functionality
- 2-player ship placement and turn-based firing
- Hit/miss resolution with win detection
- Cursor control with joystick input
- LED matrix rendering for boards and feedback
- Optional PC audio event bridge

## 3. Hardware Used
- Arduino Uno/Nano (lab kit)
- 3x MAX7219 4-in-1 modules (12 chips total, logical 32x24)
- 2x joystick modules (A0/A1/A5 and A3/A4/D3)
- Buzzer (optional)
- Breadboard + jumper wires

## 4. Lab Timeline (5 Sessions)
- Lab 1: scope, wiring plan, component checks
- Lab 2: display bring-up + joystick input test
- Lab 3: Battleship board/turn/shot logic
- Lab 4: UI/readability and feedback improvements
- Lab 5: integration, bug fixing, demo prep

## 5. Software Design
State-machine architecture:
- `INTRO`
- `PLACE_SHIPS_P1`
- `PLACE_SHIPS_P2`
- `P1_TURN`
- `P2_TURN`
- `GAME_OVER`

Why this worked:
- clean phase transitions
- predictable control flow
- easy debugging of turn logic and win conditions

## 6. Circuit Wiring (Main)
MAX7219:
- `D11 -> DIN`
- `D10 -> CS`
- `D13 -> CLK`
- `5V -> VCC`
- `GND -> GND`
- chained `DOUT -> DIN`

Joystick 1:
- `VRx -> A0`, `VRy -> A1`, `SW -> A5` (or A2 fallback)

Joystick 2:
- `VRx -> A3`, `VRy -> A4`, `SW -> D3`

Buzzer:
- `D7 -> signal`, `GND -> GND`

## 7. Testing Summary
We validated:
- input reliability (movement + press edges)
- display mapping and orientation
- valid placement and turn switching
- hit/miss correctness and repeated-shot handling
- win condition + restart behavior

Result: stable repeated gameplay with reliable board updates.

## 8. Challenges and Fixes
- Small LED resolution -> compact symbols and board encoding
- Input noise/repeats -> dead-zone + edge detection + cooldowns
- Multi-step game flow -> explicit state machine

## 9. Final Outcome
- Fully playable embedded 2-player Battleship
- Low-cost hardware, real-time interaction
- Strong integration of input + rendering + game logic

## 10. Future Improvements
- Multi-length ships with orientation
- stronger animations/transitions
- match score tracking / best-of series
- richer sound and accessibility cues
