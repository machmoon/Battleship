# BATTLESHIP ON ARDUINO (MAX7219 + JOYSTICK)
**ECE 5 Winter 2026**  
**Team:** Patrick Liu, Yuanna

---

## PROJECT OVERVIEW
We built a 2-player Battleship game using an Arduino, a MAX7219 8x32 LED matrix, and a joystick input module.  
Players place ships, take turns firing, and get immediate hit/miss feedback on the LED display until one side wins.

---

## DESIGN GOALS
- Build a complete interactive game on constrained hardware.
- Keep controls simple (single joystick + button flow).
- Make game state readable on a low-resolution matrix.
- Ensure reliable gameplay (valid placement, turn logic, win detection).

---

## HARDWARE
- Arduino (Uno/Nano, lab kit)
- HiLetgo MAX7219 Dot Matrix 4-in-1 (8x32)
- Joystick module (A0, A1, button)
- Jumper wires + breadboard
- Optional buzzer/speaker output for event sounds

### Wiring Used
- MAX7219: `DIN->D11`, `CS->D10`, `CLK->D13`, `VCC->5V`, `GND->GND`
- Joystick: `VRx->A0`, `VRy->A1`, `SW->A2`, `VCC->5V`, `GND->GND`

---

## SOFTWARE ARCHITECTURE
State machine design:
1. `INTRO`
2. `PLAYER_1_PLACE_SHIPS`
3. `PLAYER_2_PLACE_SHIPS`
4. `PLAYER_1_TURN`
5. `PLAYER_2_TURN`
6. `GAME_OVER`

Core modules:
- Input handling (joystick movement + button edge detection)
- LED rendering layer (cell-to-pixel mapping)
- Board logic (placement, hit/miss recording)
- Turn management and win condition checks
- Optional sound/event output

---

## GAMEPLAY FLOW
1. Start screen appears.
2. Player 1 places ships.
3. Player 2 places ships.
4. Players alternate shots.
5. Hits/misses are shown on shot boards.
6. Game ends when all ships of one player are sunk.

---

## DESIGN CHALLENGES + SOLUTIONS
**Challenge:** Very limited display resolution (8x32).  
**Solution:** Compact board encoding and distinct pixel patterns for ships/hits/misses/cursor.

**Challenge:** Input noise and accidental repeats.  
**Solution:** Dead zones + debounce/edge-detection timing.

**Challenge:** Multi-step game flow for 2 players.  
**Solution:** Explicit state machine with clean transitions and reset behavior.

---

## TESTING
We tested:
- Joystick directional accuracy and button press reliability
- Ship placement validity (no overlap/out-of-bounds)
- Shot resolution (hit/miss and already-shot handling)
- Turn switching correctness
- Win detection and restart loop

Result: Stable gameplay across repeated runs with consistent rendering and controls.

---

## LAB TIMELINE (5 SESSIONS)
- **Lab 1:** Project scope, wiring plan, component validation
- **Lab 2:** Matrix bring-up + joystick input read
- **Lab 3:** Core Battleship logic + board data structures
- **Lab 4:** UI improvements (readability, feedback, states)
- **Lab 5:** Full integration, debugging, final demo prep

---

## RESULTS
- Functional 2-player Battleship on embedded hardware
- Clear game progression from intro to game-over
- Real-time visual feedback with low hardware cost
- Demonstrates embedded systems integration: input + display + game logic

---

## FUTURE IMPROVEMENTS
- Multiple ship sizes and orientation placement
- Better transition screens and animations
- Score history / best-of series mode
- More audio feedback and accessibility cues

---

## FIGURE PLACEMENT GUIDE (FOR FINAL POSTER)
- **Figure A (top-right):** Full prototype photo with labels
- **Figure B (middle-left):** Wiring schematic
- **Figure C (middle-right):** Gameplay frames (placement, hit, miss, win)
- **Figure D (bottom-left):** Software state diagram

> Replace this markdown with your course-provided 20"x30" template and copy each section into poster text boxes.
