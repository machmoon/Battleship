#include <SPI.h>
#include <string.h>

#define DEVICE_COUNT 12
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10

#define JOY_X_PIN A0
#define JOY_Y_PIN A1
#define JOY_SW_PIN A5
#define JOY2_X_PIN A3
#define JOY2_Y_PIN A4
#define JOY2_SW_PIN 3
#define MAIN_BTN_PIN 2
#define BUZZER_PIN 7

#if MAIN_BTN_PIN == CLK_PIN
#error "MAIN_BTN_PIN cannot be CLK_PIN (D13). Move button to D2 or another free pin."
#endif

const uint8_t REG_DIGIT0 = 0x01;
const uint8_t REG_DECODE_MODE = 0x09;
const uint8_t REG_INTENSITY = 0x0A;
const uint8_t REG_SCAN_LIMIT = 0x0B;
const uint8_t REG_SHUTDOWN = 0x0C;
const uint8_t REG_DISPLAY_TEST = 0x0F;

// Physical module grid:
// 4 modules across x 3 modules down = 32x24 logical space.
const uint8_t MODULE_COLS = 4;
const uint8_t MODULE_ROWS = 3;
const uint8_t LOGICAL_WIDTH = MODULE_COLS * 8;
const uint8_t LOGICAL_HEIGHT = MODULE_ROWS * 8;
const uint8_t MAX_LOGICAL_WIDTH = 32;

const uint16_t MOVE_REPEAT_MS = 150;
const uint16_t BLINK_MS = 220;

uint8_t matrix_rows[8][DEVICE_COUNT];

// Physical layout reported by you:
// top row:    4,3,2,1
// middle row: 8,7,6,5
// bottom row: 12,11,10,9
uint8_t map_logical_to_device(uint8_t logical_idx) {
  uint8_t row_base = (uint8_t)((logical_idx / MODULE_COLS) * MODULE_COLS);
  uint8_t col = (uint8_t)(logical_idx % MODULE_COLS);
  return (uint8_t)(row_base + (MODULE_COLS - 1 - col));
}

// Per-module 180 rotation flags.
const bool ROT180_DEVICE[DEVICE_COUNT] = {
  false, false, false, false,
  false, false, false, false,
  false, false, false, false
};

bool blink_on = true;
unsigned long last_blink_at = 0;
unsigned long last_move_at = 0;
bool last_joy_button_down = false;
bool last_joy2_button_down = false;
bool last_main_button_down = false;

bool console_powered = false;

bool menu_music_active = false;
uint8_t menu_music_track = 0;
uint8_t menu_music_choice = 0;
unsigned long last_music_switch_at = 0;
const uint16_t MENU_SWITCH_MS = 240;
unsigned long last_debug_at = 0;
const uint16_t DEBUG_PERIOD_MS = 600;

const uint8_t STATE_BOOT = 0;
const uint8_t STATE_MENU = 1;
const uint8_t STATE_BATTLESHIP = 2;
const uint8_t STATE_SNAKE = 3;
const uint8_t STATE_DINO = 4;
const uint8_t STATE_SURF = 5;
const uint8_t STATE_REACT = 6;
const uint8_t STATE_PARKOUR = 7;
const uint8_t STATE_MUSIC = 8;

uint8_t console_state = STATE_BOOT;
unsigned long boot_started_at = 0;

uint8_t menu_index = 0;
const uint8_t MENU_COUNT = 7;

const bool SERIAL_DEBUG = true;

// ---------- battleship ----------
const uint8_t CELL_EMPTY = 0;
const uint8_t CELL_SHIP = 1;
const uint8_t CELL_MISS = 2;
const uint8_t CELL_HIT = 3;

#define B_SIZE 4
const uint8_t B_SHIPS = 4;
uint8_t player_board[B_SIZE][B_SIZE];
uint8_t enemy_board[B_SIZE][B_SIZE];
uint8_t player_shots[B_SIZE][B_SIZE];
uint8_t enemy_shots[B_SIZE][B_SIZE];
uint8_t b_p1_placed = 0;
uint8_t b_p2_placed = 0;
uint8_t b_cursor_x = 0;
uint8_t b_cursor_y = 0;
bool b_game_over = false;
bool b_player_won = false;
uint8_t b_phase = 0;
uint8_t b_winner = 0;
const uint8_t BS_ROW_Y0 = 8;      // render battleship only in middle row (y=8..15)
const uint8_t BS_P1_BOARD_X0 = 0;
const uint8_t BS_P1_SHOTS_X0 = 8;
const uint8_t BS_P2_SHOTS_X0 = 16;
const uint8_t BS_P2_BOARD_X0 = 24;
const uint8_t BS_PHASE_PLACE_P1 = 0;
const uint8_t BS_PHASE_PLACE_P2 = 1;
const uint8_t BS_PHASE_P1_TURN = 2;
const uint8_t BS_PHASE_P2_TURN = 3;
const uint8_t BS_PHASE_GAME_OVER = 4;

// ---------- snake ----------
const uint8_t S_MAX = 220;
int8_t s_x[S_MAX];
int8_t s_y[S_MAX];
uint8_t s_len = 0;
int8_t s_dx = 1;
int8_t s_dy = 0;
int8_t s_food_x = 20;
int8_t s_food_y = 4;
unsigned long s_last_step_at = 0;
const uint16_t S_STEP_MS = 120;
bool s_game_over = false;
uint16_t s_score = 0;
bool s_player_won = false;

// ---------- dino ----------
int8_t dino_y = 6;
int8_t dino_vy = 0;
bool dino_game_over = false;
uint16_t dino_score = 0;
bool dino_obs[MAX_LOGICAL_WIDTH];
unsigned long dino_last_step_at = 0;
const uint16_t DINO_STEP_MS = 95;

// ---------- basic subway surfers ----------
struct SurfObstacle {
  int8_t x;
  int8_t lane;
  bool active;
};

const uint8_t SURF_MAX_OBS = 18;
SurfObstacle surf_obs[SURF_MAX_OBS];
int8_t surf_lane = 1; // 0,1,2
bool surf_game_over = false;
uint16_t surf_score = 0;
unsigned long surf_last_step_at = 0;
const uint16_t SURF_STEP_MS = 110;

// ---------- reaction ----------
int8_t react_target = 0; // 0 up, 1 right, 2 down, 3 left
unsigned long react_round_at = 0;
uint16_t react_window_ms = 1400;
uint16_t react_score = 0;
bool react_game_over = false;

// ---------- parkour ----------
uint8_t park_cols[MAX_LOGICAL_WIDTH];
int8_t park_player_y = 5;
int8_t park_player_vy = 0;
bool park_game_over = false;
uint16_t park_score = 0;
unsigned long park_last_step_at = 0;
const uint16_t PARK_STEP_MS = 100;
uint8_t park_gen_h = 6;

// ---------- music player ----------
const uint8_t MUSIC_TRACK_COUNT = 3;
uint8_t music_track_idx = 0;
bool music_playing = false;
unsigned long music_last_nav_at = 0;
const uint16_t MUSIC_NAV_MS = 180;

void send_all(uint8_t reg, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  for (uint8_t i = 0; i < DEVICE_COUNT; i++) {
    SPI.transfer(reg);
    SPI.transfer(data);
  }
  digitalWrite(CS_PIN, HIGH);
}

const char* state_name(uint8_t s) {
  switch (s) {
    case STATE_BOOT: return "BOOT";
    case STATE_MENU: return "MENU";
    case STATE_BATTLESHIP: return "BATTLESHIP";
    case STATE_SNAKE: return "SNAKE";
    case STATE_DINO: return "DINO";
    case STATE_SURF: return "SURF";
    case STATE_REACT: return "REACT";
    case STATE_PARKOUR: return "PARKOUR";
    case STATE_MUSIC: return "MUSIC";
    default: return "?";
  }
}

void log_event(const char* msg) {
  if (!SERIAL_DEBUG) return;
  Serial.print("LOG ");
  Serial.print(millis());
  Serial.print("ms: ");
  Serial.println(msg);
}

void emit_event(const char* evt) {
  Serial.println(evt);
}

void debug_snapshot() {
  if (!SERIAL_DEBUG) return;
  unsigned long now = millis();
  if (now - last_debug_at < DEBUG_PERIOD_MS) return;
  last_debug_at = now;
  Serial.print("DBG state=");
  Serial.print(state_name(console_state));
  Serial.print(" power=");
  Serial.print(console_powered ? 1 : 0);
  Serial.print(" main_btn=");
  Serial.print(digitalRead(MAIN_BTN_PIN) == LOW ? 1 : 0);
  Serial.print(" joy_btn=");
  Serial.print(digitalRead(JOY_SW_PIN) == LOW ? 1 : 0);
  Serial.print(" joy2_btn=");
  Serial.print(digitalRead(JOY2_SW_PIN) == LOW ? 1 : 0);
  Serial.print(" joy_x=");
  Serial.print(analogRead(JOY_X_PIN));
  Serial.print(" joy_y=");
  Serial.print(analogRead(JOY_Y_PIN));
  Serial.print(" joy2_x=");
  Serial.print(analogRead(JOY2_X_PIN));
  Serial.print(" joy2_y=");
  Serial.println(analogRead(JOY2_Y_PIN));
}

void send_row(uint8_t reg, const uint8_t data_by_device[DEVICE_COUNT]) {
  digitalWrite(CS_PIN, LOW);
  for (int8_t d = DEVICE_COUNT - 1; d >= 0; d--) {
    SPI.transfer(reg);
    SPI.transfer(data_by_device[d]);
  }
  digitalWrite(CS_PIN, HIGH);
}

void clear_matrix() {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t d = 0; d < DEVICE_COUNT; d++) matrix_rows[y][d] = 0;
  }
}

void update_matrix() {
  for (uint8_t y = 0; y < 8; y++) send_row(REG_DIGIT0 + y, matrix_rows[y]);
}

void set_pixel(int8_t y, int8_t x, bool on) {
  if (x < 0 || x >= LOGICAL_WIDTH || y < 0 || y >= LOGICAL_HEIGHT) return;
  uint8_t panel = (uint8_t)y / 8;       // 0..2 for stacked panels
  uint8_t local_y = (uint8_t)y % 8;
  uint8_t logical_idx = panel * MODULE_COLS + (uint8_t)x / 8;
  if (panel >= MODULE_ROWS || logical_idx >= DEVICE_COUNT) return;
  uint8_t dev = map_logical_to_device(logical_idx);
  if (dev >= DEVICE_COUNT) return;
  uint8_t col = (uint8_t)x % 8;
  if (ROT180_DEVICE[dev]) {
    local_y = 7 - local_y;
    col = 7 - col;
  }
  uint8_t mask = (uint8_t)(1 << (7 - col));
  if (on) matrix_rows[local_y][dev] |= mask;
  else matrix_rows[local_y][dev] &= (uint8_t)~mask;
}

void max7219_init() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin();
  send_all(REG_SCAN_LIMIT, 7);
  send_all(REG_DECODE_MODE, 0);
  send_all(REG_DISPLAY_TEST, 0);
  send_all(REG_INTENSITY, 3);
  send_all(REG_SHUTDOWN, 1);
  clear_matrix();
  update_matrix();
}

void tone_click() {
  tone(BUZZER_PIN, 1650, 20);
  delay(24);
  tone(BUZZER_PIN, 2200, 22);
}

bool joy_button_edge() {
  bool down = (digitalRead(JOY_SW_PIN) == LOW);
  bool edge = (down && !last_joy_button_down);
  last_joy_button_down = down;
  return edge;
}

bool joy2_button_edge() {
  bool down = (digitalRead(JOY2_SW_PIN) == LOW);
  bool edge = (down && !last_joy2_button_down);
  last_joy2_button_down = down;
  return edge;
}

bool main_button_edge() {
  bool down = (digitalRead(MAIN_BTN_PIN) == LOW);
  bool edge = (down && !last_main_button_down);
  last_main_button_down = down;
  return edge;
}

bool action_edge() {
  return joy_button_edge() || main_button_edge();
}

void update_blink() {
  unsigned long now = millis();
  if (now - last_blink_at >= BLINK_MS) {
    blink_on = !blink_on;
    last_blink_at = now;
  }
}

int8_t joy_dir_x() {
  int v = analogRead(JOY_X_PIN);
  if (v < 320) return -1;
  if (v > 700) return 1;
  return 0;
}

int8_t joy_dir_y() {
  int v = analogRead(JOY_Y_PIN);
  if (v < 320) return -1;
  if (v > 700) return 1;
  return 0;
}

int8_t joy2_dir_x() {
  int v = analogRead(JOY2_X_PIN);
  if (v < 320) return -1;
  if (v > 700) return 1;
  return 0;
}

int8_t joy2_dir_y() {
  int v = analogRead(JOY2_Y_PIN);
  if (v < 320) return -1;
  if (v > 700) return 1;
  return 0;
}

int8_t joy_cardinal_dir() {
  int8_t dx = joy_dir_x();
  int8_t dy = joy_dir_y();
  if (dx == 0 && dy == 0) return -1;

  int vx = analogRead(JOY_X_PIN) - 512;
  int vy = analogRead(JOY_Y_PIN) - 512;
  if (abs(vx) > abs(vy)) return (vx > 0) ? 1 : 3;
  return (vy > 0) ? 2 : 0;
}

bool can_move_now() {
  unsigned long now = millis();
  if (now - last_move_at < MOVE_REPEAT_MS) return false;
  last_move_at = now;
  return true;
}

uint8_t char_col(char ch, uint8_t col) {
  switch (ch) {
    case 'A': { static const uint8_t c[5] = {0x7E,0x09,0x09,0x7E,0x00}; return c[col]; }
    case 'B': { static const uint8_t c[5] = {0x7F,0x49,0x49,0x36,0x00}; return c[col]; }
    case 'C': { static const uint8_t c[5] = {0x3E,0x41,0x41,0x22,0x00}; return c[col]; }
    case 'D': { static const uint8_t c[5] = {0x7F,0x41,0x41,0x3E,0x00}; return c[col]; }
    case 'E': { static const uint8_t c[5] = {0x7F,0x49,0x49,0x41,0x00}; return c[col]; }
    case 'F': { static const uint8_t c[5] = {0x7F,0x09,0x09,0x01,0x00}; return c[col]; }
    case 'G': { static const uint8_t c[5] = {0x3E,0x41,0x51,0x72,0x00}; return c[col]; }
    case 'H': { static const uint8_t c[5] = {0x7F,0x08,0x08,0x7F,0x00}; return c[col]; }
    case 'I': { static const uint8_t c[5] = {0x41,0x7F,0x41,0x00,0x00}; return c[col]; }
    case 'K': { static const uint8_t c[5] = {0x7F,0x08,0x14,0x63,0x00}; return c[col]; }
    case 'L': { static const uint8_t c[5] = {0x7F,0x40,0x40,0x40,0x00}; return c[col]; }
    case 'M': { static const uint8_t c[5] = {0x7F,0x02,0x04,0x02,0x7F}; return c[col]; }
    case 'N': { static const uint8_t c[5] = {0x7F,0x04,0x08,0x7F,0x00}; return c[col]; }
    case 'O': { static const uint8_t c[5] = {0x3E,0x41,0x41,0x3E,0x00}; return c[col]; }
    case 'P': { static const uint8_t c[5] = {0x7F,0x09,0x09,0x06,0x00}; return c[col]; }
    case 'R': { static const uint8_t c[5] = {0x7F,0x09,0x19,0x66,0x00}; return c[col]; }
    case 'S': { static const uint8_t c[5] = {0x26,0x49,0x49,0x32,0x00}; return c[col]; }
    case 'T': { static const uint8_t c[5] = {0x01,0x7F,0x01,0x01,0x00}; return c[col]; }
    case 'U': { static const uint8_t c[5] = {0x3F,0x40,0x40,0x3F,0x00}; return c[col]; }
    case 'V': { static const uint8_t c[5] = {0x1F,0x20,0x40,0x20,0x1F}; return c[col]; }
    case 'W': { static const uint8_t c[5] = {0x7F,0x20,0x10,0x20,0x7F}; return c[col]; }
    case 'Y': { static const uint8_t c[5] = {0x07,0x08,0x70,0x08,0x07}; return c[col]; }
    case ' ': return 0x00;
    default: return 0x00;
  }
}

void draw_text_center_at(const char* text, uint8_t y_base) {
  uint8_t len = (uint8_t)strlen(text);
  uint8_t total_w = len * 6;
  int8_t start_x = (LOGICAL_WIDTH - total_w) / 2;
  if (start_x < 0) start_x = 0;

  for (uint8_t i = 0; i < len; i++) {
    for (uint8_t c = 0; c < 5; c++) {
      uint8_t bits = char_col(text[i], c);
      int8_t x = start_x + i * 6 + c;
      if (x < 0 || x >= LOGICAL_WIDTH) continue;
      for (uint8_t y = 0; y < 7; y++) {
        int16_t yy = (int16_t)y_base + y;
        if (yy < 0 || yy >= LOGICAL_HEIGHT) continue;
        if (bits & (1 << y)) set_pixel((int8_t)yy, x, true);
      }
    }
  }
}

void draw_text_center(const char* text) {
  draw_text_center_at(text, 0);
}

void update_menu_music() {
  if (menu_music_choice >= 3) menu_music_choice = 0;
  if (!menu_music_active) {
    menu_music_active = true;
    menu_music_track = menu_music_choice;
    if (menu_music_track == 0) emit_event("MENU_MUSIC_1");
    else if (menu_music_track == 1) emit_event("MENU_MUSIC_2");
    else emit_event("MENU_MUSIC_3");
    return;
  }

  if (menu_music_track != menu_music_choice) {
    menu_music_track = menu_music_choice;
    if (menu_music_track == 0) emit_event("MENU_MUSIC_1");
    else if (menu_music_track == 1) emit_event("MENU_MUSIC_2");
    else emit_event("MENU_MUSIC_3");
  }
}

void menu_music_stop() {
  if (!menu_music_active) return;
  emit_event("MENU_MUSIC_STOP");
  menu_music_active = false;
  menu_music_track = 0;
  last_music_switch_at = 0;
  noTone(BUZZER_PIN);
}

void enter_menu() {
  console_state = STATE_MENU;
  menu_music_active = false;
  menu_music_track = 0;
  last_music_switch_at = 0;
  emit_event("MENU_CLICK");
}

void launch_game_from_menu() {
  menu_music_stop();
  emit_event("MENU_CLICK");

  if (menu_index == 0) { start_battleship(); console_state = STATE_BATTLESHIP; }
  else if (menu_index == 1) { start_snake(); console_state = STATE_SNAKE; }
  else if (menu_index == 2) { start_dino(); console_state = STATE_DINO; }
  else if (menu_index == 3) { start_surf(); console_state = STATE_SURF; }
  else if (menu_index == 4) { start_react(); console_state = STATE_REACT; }
  else if (menu_index == 5) { start_parkour(); console_state = STATE_PARKOUR; }
  else { start_music_player(); console_state = STATE_MUSIC; }

  if (SERIAL_DEBUG) {
    Serial.print("LOG ");
    Serial.print(millis());
    Serial.print("ms: Launch -> ");
    Serial.println(state_name(console_state));
  }
}

void draw_boot() {
  unsigned long t = millis() - boot_started_at;
  clear_matrix();

  if (t < 900) {
    uint8_t head = (uint8_t)((t / 30) % LOGICAL_WIDTH);
    for (uint8_t i = 0; i < 10; i++) {
      int8_t x = (int8_t)head - i;
      if (x < 0) x += LOGICAL_WIDTH;
      uint8_t y = (uint8_t)((x + i) % 8);
      set_pixel(y, x, true);
    }
    if (t < 120) tone(BUZZER_PIN, 784, 90);
    else if (t < 260) tone(BUZZER_PIN, 988, 90);
    else if (t < 420) tone(BUZZER_PIN, 1175, 120);
  } else if (t < 1900) {
    uint8_t r = (uint8_t)((t / 140) % 4);
    for (uint8_t x = r; x < LOGICAL_WIDTH - r; x++) {
      set_pixel(r, x, true);
      set_pixel(7 - r, x, true);
    }
    for (uint8_t y = r; y < 8 - r; y++) {
      set_pixel(y, r * 4, true);
      set_pixel(y, (LOGICAL_WIDTH - 1) - r * 4, true);
    }
    if (t > 1250 && t < 1370) tone(BUZZER_PIN, 1319, 110);
    if (t > 1550 && t < 1670) tone(BUZZER_PIN, 1568, 120);
  } else {
    draw_text_center_at("SBII", 8);
    if ((t > 1950 && t < 2060) || (t > 2140 && t < 2260)) tone(BUZZER_PIN, 1760, 100);
  }

  update_matrix();
  if (t > 2600) {
    noTone(BUZZER_PIN);
    enter_menu();
  }
}

void draw_menu() {
  clear_matrix();
  uint8_t header_y = 0;
  uint8_t title_y = (LOGICAL_HEIGHT >= 16) ? (uint8_t)((LOGICAL_HEIGHT / 2) - 4) : 0;
  uint8_t footer_y = (uint8_t)(LOGICAL_HEIGHT - 1);
  uint8_t arrow_y = (LOGICAL_HEIGHT >= 5) ? (uint8_t)(LOGICAL_HEIGHT - 4) : 0;

  // Top panel: header
  draw_text_center_at("MENU", header_y);
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x += 2) set_pixel((int8_t)(header_y + 7), x, true);

  // Middle panel: selected game title
  // Keep labels <= 5 chars so they fit 32px wide cleanly.
  if (menu_index == 0) draw_text_center_at("BTTL", title_y);
  else if (menu_index == 1) draw_text_center_at("SNKE", title_y);
  else if (menu_index == 2) draw_text_center_at("DINO", title_y);
  else if (menu_index == 3) draw_text_center_at("SURF", title_y);
  else if (menu_index == 4) draw_text_center_at("RECT", title_y);
  else if (menu_index == 5) draw_text_center_at("PARK", title_y);
  else draw_text_center_at("MUSC", title_y);

  // Bottom panel: nav arrows + index dots
  if (menu_index > 0 && blink_on) {
    set_pixel(arrow_y - 1, 1, true);
    set_pixel(arrow_y, 0, true); set_pixel(arrow_y, 1, true);
    set_pixel(arrow_y + 1, 1, true);
  }
  if (menu_index + 1 < MENU_COUNT && blink_on) {
    set_pixel(arrow_y - 1, LOGICAL_WIDTH - 2, true);
    set_pixel(arrow_y, LOGICAL_WIDTH - 2, true); set_pixel(arrow_y, LOGICAL_WIDTH - 1, true);
    set_pixel(arrow_y + 1, LOGICAL_WIDTH - 2, true);
  }
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    uint8_t x = 5 + i * 4;
    bool on = (i == menu_index) ? blink_on : true;
    set_pixel(footer_y, x, on);
    set_pixel(footer_y, x + 1, on);
  }

  update_matrix();
}

// -------------------- battleship --------------------
void clear_cell_board(uint8_t b[][B_SIZE]) {
  for (uint8_t y = 0; y < B_SIZE; y++) {
    for (uint8_t x = 0; x < B_SIZE; x++) b[y][x] = CELL_EMPTY;
  }
}

void start_battleship() {
  clear_cell_board(player_board);
  clear_cell_board(enemy_board);
  clear_cell_board(player_shots);
  clear_cell_board(enemy_shots);
  b_p1_placed = 0;
  b_p2_placed = 0;
  b_cursor_x = 0;
  b_cursor_y = 0;
  b_phase = BS_PHASE_PLACE_P1;
  b_winner = 0;
  b_game_over = false;
  b_player_won = false;
}

uint8_t remaining_ships(uint8_t b[][B_SIZE]) {
  uint8_t n = 0;
  for (uint8_t y = 0; y < B_SIZE; y++) {
    for (uint8_t x = 0; x < B_SIZE; x++) if (b[y][x] == CELL_SHIP) n++;
  }
  return n;
}

void draw_bs_cell(uint8_t ox, uint8_t oy, uint8_t s, bool cursor_here) {
  bool p00 = false;
  bool p10 = false;
  bool p01 = false;
  bool p11 = false;

  if (s == CELL_SHIP) {
    p00 = true;
    p11 = true;
  } else if (s == CELL_MISS) {
    p00 = true;
  } else if (s == CELL_HIT) {
    p00 = true;
    p10 = true;
    p01 = true;
    p11 = true;
  }

  if (cursor_here && blink_on) {
    p10 = true;
    p01 = true;
  }

  set_pixel(oy, ox, p00);
  set_pixel(oy, ox + 1, p10);
  set_pixel(oy + 1, ox, p01);
  set_pixel(oy + 1, ox + 1, p11);
}

void draw_bs_shot(uint8_t ox, uint8_t oy, uint8_t s, bool cursor_here) {
  bool p00 = false;
  bool p10 = false;
  bool p01 = false;
  bool p11 = false;

  if (s == CELL_MISS) {
    p00 = true;
  } else if (s == CELL_HIT) {
    p00 = true;
    p10 = true;
    p01 = true;
    p11 = true;
  }

  if (cursor_here && blink_on) {
    p10 = true;
    p01 = true;
  }

  set_pixel(oy, ox, p00);
  set_pixel(oy, ox + 1, p10);
  set_pixel(oy + 1, ox, p01);
  set_pixel(oy + 1, ox + 1, p11);
}

void place_ship_for_player(uint8_t player_id) {
  uint8_t (*board)[B_SIZE] = (player_id == 1) ? player_board : enemy_board;
  uint8_t* placed = (player_id == 1) ? &b_p1_placed : &b_p2_placed;
  if (*placed >= B_SHIPS) return;
  if (board[b_cursor_y][b_cursor_x] != CELL_EMPTY) {
    emit_event("BATTLE_PLACE_ERR");
    return;
  }

  board[b_cursor_y][b_cursor_x] = CELL_SHIP;
  (*placed)++;
  emit_event("BATTLE_PLACE_OK");

  if (*placed < B_SHIPS) return;

  b_cursor_x = 0;
  b_cursor_y = 0;
  if (player_id == 1) b_phase = BS_PHASE_PLACE_P2;
  else b_phase = BS_PHASE_P1_TURN;
}

void fire_shot_for_player(uint8_t attacker) {
  uint8_t (*attacker_shots)[B_SIZE] = (attacker == 1) ? player_shots : enemy_shots;
  uint8_t (*defender_board)[B_SIZE] = (attacker == 1) ? enemy_board : player_board;
  uint8_t& shot = attacker_shots[b_cursor_y][b_cursor_x];
  if (shot != CELL_EMPTY) return;

  uint8_t& target = defender_board[b_cursor_y][b_cursor_x];
  if (target == CELL_SHIP) {
    target = CELL_HIT;
    shot = CELL_HIT;
    emit_event("BATTLE_HIT");
  } else {
    if (target == CELL_EMPTY) target = CELL_MISS;
    shot = CELL_MISS;
    emit_event("BATTLE_MISS");
  }

  if (remaining_ships(defender_board) == 0) {
    b_phase = BS_PHASE_GAME_OVER;
    b_winner = attacker;
    b_game_over = true;
    b_player_won = (attacker == 1);
    emit_event("BATTLE_WIN");
    return;
  }

  b_phase = (attacker == 1) ? BS_PHASE_P2_TURN : BS_PHASE_P1_TURN;
}

void draw_battleship() {
  clear_matrix();
  for (uint8_t y = 0; y < B_SIZE; y++) {
    for (uint8_t x = 0; x < B_SIZE; x++) {
      uint8_t py = (uint8_t)(BS_ROW_Y0 + y * 2);
      bool p1_place_cursor = (b_phase == BS_PHASE_PLACE_P1 && x == b_cursor_x && y == b_cursor_y);
      bool p2_place_cursor = (b_phase == BS_PHASE_PLACE_P2 && x == b_cursor_x && y == b_cursor_y);
      bool p1_shot_cursor = (b_phase == BS_PHASE_P1_TURN && x == b_cursor_x && y == b_cursor_y);
      bool p2_shot_cursor = (b_phase == BS_PHASE_P2_TURN && x == b_cursor_x && y == b_cursor_y);

      draw_bs_cell((uint8_t)(BS_P1_BOARD_X0 + x * 2), py, player_board[y][x], p1_place_cursor);
      draw_bs_cell((uint8_t)(BS_P2_BOARD_X0 + x * 2), py, enemy_board[y][x], p2_place_cursor);
      draw_bs_shot((uint8_t)(BS_P1_SHOTS_X0 + x * 2), py, player_shots[y][x], p1_shot_cursor);
      draw_bs_shot((uint8_t)(BS_P2_SHOTS_X0 + x * 2), py, enemy_shots[y][x], p2_shot_cursor);
    }
  }

  if (b_phase == BS_PHASE_GAME_OVER && blink_on) {
    for (uint8_t y = BS_ROW_Y0; y < BS_ROW_Y0 + 8; y++) {
      for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) set_pixel(y, x, true);
    }
  }
  update_matrix();
}

void update_battleship() {
  if (b_phase == BS_PHASE_GAME_OVER) {
    if (action_edge() || joy2_button_edge()) {
      tone_click();
      enter_menu();
      return;
    }
    draw_battleship();
    return;
  }

  bool p2_active = (b_phase == BS_PHASE_PLACE_P2 || b_phase == BS_PHASE_P2_TURN);
  if (can_move_now()) {
    int8_t dx = p2_active ? joy2_dir_x() : joy_dir_x();
    int8_t dy = p2_active ? joy2_dir_y() : joy_dir_y();
    if (dx != 0) b_cursor_x = (uint8_t)constrain((int)b_cursor_x + dx, 0, (int)B_SIZE - 1);
    if (dy != 0) b_cursor_y = (uint8_t)constrain((int)b_cursor_y + dy, 0, (int)B_SIZE - 1);
  }

  bool fire_edge = p2_active ? joy2_button_edge() : action_edge();
  if (fire_edge) {
    tone_click();
    if (b_phase == BS_PHASE_PLACE_P1) place_ship_for_player(1);
    else if (b_phase == BS_PHASE_PLACE_P2) place_ship_for_player(2);
    else if (b_phase == BS_PHASE_P1_TURN) fire_shot_for_player(1);
    else if (b_phase == BS_PHASE_P2_TURN) fire_shot_for_player(2);
  }

  draw_battleship();
}

// -------------------- snake --------------------
bool snake_has(int8_t x, int8_t y) {
  for (uint8_t i = 0; i < s_len; i++) {
    if (s_x[i] == x && s_y[i] == y) return true;
  }
  return false;
}

void snake_spawn_food() {
  for (uint8_t t = 0; t < 90; t++) {
    int8_t x = random(0, LOGICAL_WIDTH);
    int8_t y = random(0, LOGICAL_HEIGHT);
    if (!snake_has(x, y)) {
      s_food_x = x;
      s_food_y = y;
      return;
    }
  }
}

void start_snake() {
  s_len = 5;
  int8_t cx = (int8_t)(LOGICAL_WIDTH / 2);
  int8_t cy = (int8_t)(LOGICAL_HEIGHT / 2);
  for (uint8_t i = 0; i < s_len; i++) {
    s_x[i] = cx - i;
    s_y[i] = cy;
  }
  s_dx = 1;
  s_dy = 0;
  s_last_step_at = millis();
  s_game_over = false;
  s_score = 0;
  s_player_won = false;
  snake_spawn_food();
}

void draw_snake() {
  clear_matrix();
  for (uint8_t i = 0; i < s_len; i++) set_pixel(s_y[i], s_x[i], true);
  if (blink_on) set_pixel(s_food_y, s_food_x, true);
  if (s_game_over && blink_on) for (uint8_t x = 6; x < 26; x++) set_pixel((int8_t)(LOGICAL_HEIGHT / 2), x, true);
  update_matrix();
}

void update_snake() {
  if (!s_game_over) {
    if (can_move_now()) {
      int8_t dx = joy_dir_x();
      int8_t dy = joy_dir_y();
      if (dx != 0 && s_dx == 0) { s_dx = dx; s_dy = 0; }
      if (dy != 0 && s_dy == 0) { s_dy = dy; s_dx = 0; }
    }

    unsigned long now = millis();
    if (now - s_last_step_at >= S_STEP_MS) {
      s_last_step_at = now;
      int8_t nx = s_x[0] + s_dx;
      int8_t ny = s_y[0] + s_dy;

      if (nx < 0 || nx >= LOGICAL_WIDTH || ny < 0 || ny >= LOGICAL_HEIGHT || snake_has(nx, ny)) {
        s_game_over = true;
        s_player_won = false;
        emit_event("SNAKE_FAIL");
      } else {
        for (int8_t i = (int8_t)s_len; i > 0; i--) {
          s_x[i] = s_x[i - 1];
          s_y[i] = s_y[i - 1];
        }
        s_x[0] = nx;
        s_y[0] = ny;

        if (nx == s_food_x && ny == s_food_y) {
          if (s_len < S_MAX - 1) s_len++;
          s_score++;
          emit_event("SNAKE_HIT");
          snake_spawn_food();
          if (s_score >= 15) {
            s_game_over = true;
            s_player_won = true;
            emit_event("SNAKE_WIN");
          }
        }
      }
    }
  } else if (action_edge()) {
    tone_click();
    enter_menu();
    return;
  }

  draw_snake();
}

// -------------------- dino jump --------------------
void start_dino() {
  dino_y = (int8_t)(LOGICAL_HEIGHT - 2);
  dino_vy = 0;
  dino_game_over = false;
  dino_score = 0;
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) dino_obs[x] = false;
  dino_last_step_at = millis();
}

void draw_dino() {
  clear_matrix();
  int8_t ground_y = (int8_t)(LOGICAL_HEIGHT - 1);
  int8_t obstacle_top = (int8_t)(LOGICAL_HEIGHT - 3);
  int8_t obstacle_mid = (int8_t)(LOGICAL_HEIGHT - 2);
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) set_pixel(ground_y, x, true);
  set_pixel(dino_y, 4, true);
  if (dino_y > 0) set_pixel(dino_y - 1, 4, true);

  for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) {
    if (dino_obs[x]) {
      set_pixel(obstacle_mid, x, true);
      set_pixel(obstacle_top, x, true);
    }
  }

  uint8_t bars = (uint8_t)min((int)(dino_score / 10), 8);
  for (uint8_t i = 0; i < bars; i++) set_pixel(0, i, true);
  if (dino_game_over && blink_on) for (uint8_t x = 10; x < 22; x++) set_pixel(0, x, true);
  update_matrix();
}

void update_dino() {
  int8_t stand_y = (int8_t)(LOGICAL_HEIGHT - 2);
  int8_t min_y = (int8_t)(LOGICAL_HEIGHT - 12);
  int8_t hit_y = (int8_t)(LOGICAL_HEIGHT - 3);
  if (!dino_game_over) {
    if (action_edge() && dino_y >= stand_y) {
      dino_vy = -4;
      tone_click();
      emit_event("DINO_JUMP_OK");
    }

    unsigned long now = millis();
    if (now - dino_last_step_at >= DINO_STEP_MS) {
      dino_last_step_at = now;
      for (uint8_t x = 0; x + 1 < LOGICAL_WIDTH; x++) dino_obs[x] = dino_obs[x + 1];
      dino_obs[LOGICAL_WIDTH - 1] = (random(0, 100) < 22);

      dino_y += dino_vy;
      dino_vy += 1;
      if (dino_y > stand_y) { dino_y = stand_y; dino_vy = 0; }
      if (dino_y < min_y) dino_y = min_y;

      bool hit = dino_obs[4] && (dino_y >= hit_y);
      if (hit) {
        dino_game_over = true;
        emit_event("DINO_FAIL");
      }
      else dino_score++;
    }
  } else if (action_edge()) {
    tone_click();
    enter_menu();
    return;
  }

  draw_dino();
}

// -------------------- subway basic --------------------
void start_surf() {
  surf_lane = 1;
  surf_game_over = false;
  surf_score = 0;
  for (uint8_t i = 0; i < SURF_MAX_OBS; i++) surf_obs[i].active = false;
  surf_last_step_at = millis();
}

void surf_spawn() {
  for (uint8_t i = 0; i < SURF_MAX_OBS; i++) {
    if (!surf_obs[i].active) {
      surf_obs[i].active = true;
      surf_obs[i].x = LOGICAL_WIDTH - 1;
      surf_obs[i].lane = random(0, 3);
      return;
    }
  }
}

void draw_surf() {
  clear_matrix();
  int8_t lane_y[3] = {
    (int8_t)(LOGICAL_HEIGHT / 4),
    (int8_t)(LOGICAL_HEIGHT / 2),
    (int8_t)((LOGICAL_HEIGHT * 3) / 4)
  };
  int8_t sep_a = (int8_t)((lane_y[0] + lane_y[1]) / 2);
  int8_t sep_b = (int8_t)((lane_y[1] + lane_y[2]) / 2);
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x += 2) {
    set_pixel(sep_a, x, true);
    set_pixel(sep_b, x, true);
  }

  int8_t py = lane_y[surf_lane];
  set_pixel(py, 4, true);
  set_pixel(py, 5, true);
  set_pixel(py - 1, 4, true);
  set_pixel(py + 1, 4, true);

  for (uint8_t i = 0; i < SURF_MAX_OBS; i++) {
    if (!surf_obs[i].active) continue;
    int8_t y = lane_y[surf_obs[i].lane];
    set_pixel(y, surf_obs[i].x, true);
    set_pixel(y - 1, surf_obs[i].x, true);
    set_pixel(y + 1, surf_obs[i].x, true);
  }

  uint8_t bars = (uint8_t)min((int)(surf_score / 12), 8);
  for (uint8_t i = 0; i < bars; i++) set_pixel(0, i, true);
  if (surf_game_over && blink_on) for (uint8_t x = 10; x < 22; x++) set_pixel((int8_t)(LOGICAL_HEIGHT - 1), x, true);
  update_matrix();
}

void update_surf() {
  if (!surf_game_over) {
    if (can_move_now()) {
      int8_t dy = joy_dir_y();
      if (dy < 0) surf_lane = max(0, surf_lane - 1);
      if (dy > 0) surf_lane = min(2, surf_lane + 1);
    }

    unsigned long now = millis();
    if (now - surf_last_step_at >= SURF_STEP_MS) {
      surf_last_step_at = now;
      for (uint8_t i = 0; i < SURF_MAX_OBS; i++) {
        if (!surf_obs[i].active) continue;
        surf_obs[i].x--;
        if (surf_obs[i].x < 0) surf_obs[i].active = false;
      }

      if (random(0, 100) < 35) surf_spawn();

      for (uint8_t i = 0; i < SURF_MAX_OBS; i++) {
        if (!surf_obs[i].active) continue;
        if ((surf_obs[i].x == 4 || surf_obs[i].x == 5) && surf_obs[i].lane == surf_lane) {
          surf_game_over = true;
          emit_event("SURF_FAIL");
        }
      }

      if (!surf_game_over) surf_score++;
    }
  } else if (action_edge()) {
    tone_click();
    enter_menu();
    return;
  }

  draw_surf();
}

// -------------------- reaction --------------------
void react_next_round() {
  react_target = random(0, 4);
  react_round_at = millis();
  if (react_window_ms > 550) react_window_ms -= 25;
}

void start_react() {
  react_score = 0;
  react_window_ms = 1400;
  react_game_over = false;
  react_next_round();
}

void draw_react_arrow(int8_t dir) {
  int8_t cx = (int8_t)(LOGICAL_WIDTH / 2);
  int8_t cy = (int8_t)(LOGICAL_HEIGHT / 2);
  if (dir == 0) {
    set_pixel(cy - 2, cx, true);
    set_pixel(cy - 1, cx - 1, true); set_pixel(cy - 1, cx, true); set_pixel(cy - 1, cx + 1, true);
    for (int8_t y = cy; y <= cy + 2; y++) set_pixel(y, cx, true);
  } else if (dir == 2) {
    set_pixel(cy + 2, cx, true);
    set_pixel(cy + 1, cx - 1, true); set_pixel(cy + 1, cx, true); set_pixel(cy + 1, cx + 1, true);
    for (int8_t y = cy - 2; y <= cy; y++) set_pixel(y, cx, true);
  } else if (dir == 1) {
    set_pixel(cy, cx + 2, true);
    set_pixel(cy - 1, cx + 1, true); set_pixel(cy, cx + 1, true); set_pixel(cy + 1, cx + 1, true);
    for (int8_t x = cx - 2; x <= cx; x++) set_pixel(cy, x, true);
  } else {
    set_pixel(cy, cx - 2, true);
    set_pixel(cy - 1, cx - 1, true); set_pixel(cy, cx - 1, true); set_pixel(cy + 1, cx - 1, true);
    for (int8_t x = cx; x <= cx + 2; x++) set_pixel(cy, x, true);
  }
}

void draw_react() {
  clear_matrix();
  draw_react_arrow(react_target);

  uint8_t bars = (uint8_t)min((int)react_score, 8);
  for (uint8_t i = 0; i < bars; i++) set_pixel(0, i, true);

  uint16_t elapsed = (uint16_t)(millis() - react_round_at);
  uint8_t time_bar = (elapsed >= react_window_ms) ? 0 : (uint8_t)(8 - (elapsed * 8 / react_window_ms));
  for (uint8_t i = 0; i < time_bar; i++) set_pixel((int8_t)(LOGICAL_HEIGHT - 1), (LOGICAL_WIDTH - 1) - i, true);

  if (react_game_over && blink_on) for (uint8_t x = 10; x < 22; x++) set_pixel(0, x, true);
  update_matrix();
}

void update_react() {
  if (!react_game_over) {
    int8_t got = joy_cardinal_dir();
    if (got >= 0) {
      if (got == react_target) {
        react_score++;
        tone_click();
        emit_event("REACT_HIT");
        react_next_round();
      } else {
        react_game_over = true;
        emit_event("REACT_MISS");
      }
    }
    if ((millis() - react_round_at) > react_window_ms) {
      react_game_over = true;
      emit_event("REACT_MISS");
    }
  } else if (action_edge()) {
    tone_click();
    enter_menu();
    return;
  }

  draw_react();
}

// -------------------- parkour --------------------
void start_parkour() {
  park_player_y = (int8_t)(LOGICAL_HEIGHT - 3);
  park_player_vy = 0;
  park_game_over = false;
  park_score = 0;
  park_last_step_at = millis();
  park_gen_h = (uint8_t)(LOGICAL_HEIGHT - 4);
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) park_cols[x] = (uint8_t)(LOGICAL_HEIGHT - 4);
}

void park_shift_left() {
  for (uint8_t x = 0; x + 1 < LOGICAL_WIDTH; x++) park_cols[x] = park_cols[x + 1];
}

void park_spawn_col() {
  if (random(0, 100) < 18) {
    park_cols[LOGICAL_WIDTH - 1] = 0;
    return;
  }

  int8_t delta = (int8_t)random(-1, 2);
  int8_t nh = (int8_t)park_gen_h + delta;
  int8_t min_h = (int8_t)(LOGICAL_HEIGHT - 9);
  int8_t max_h = (int8_t)(LOGICAL_HEIGHT - 2);
  if (nh < min_h) nh = min_h;
  if (nh > max_h) nh = max_h;
  park_gen_h = (uint8_t)nh;
  park_cols[LOGICAL_WIDTH - 1] = park_gen_h;
}

void draw_parkour() {
  clear_matrix();
  for (uint8_t x = 0; x < LOGICAL_WIDTH; x++) {
    uint8_t h = park_cols[x];
    if (h == 0) continue;
    for (uint8_t y = h; y < LOGICAL_HEIGHT; y++) set_pixel(y, x, true);
  }

  set_pixel(park_player_y, 4, true);
  if (park_player_y > 0) set_pixel(park_player_y - 1, 4, true);

  uint8_t bars = (uint8_t)min((int)(park_score / 12), 8);
  for (uint8_t i = 0; i < bars; i++) set_pixel(0, i, true);
  if (park_game_over && blink_on) for (uint8_t x = 10; x < 22; x++) set_pixel(0, x, true);

  update_matrix();
}

void update_parkour() {
  if (!park_game_over) {
    uint8_t ground_h = park_cols[4];
    int8_t stand_y = (ground_h == 0) ? (int8_t)(LOGICAL_HEIGHT - 1) : (int8_t)ground_h - 1;
    bool on_ground = (ground_h != 0 && park_player_y >= stand_y && park_player_vy >= 0);

    if ((action_edge() || joy_dir_y() < 0) && on_ground) {
      park_player_vy = -3;
      tone_click();
    }

    unsigned long now = millis();
    if (now - park_last_step_at >= PARK_STEP_MS) {
      park_last_step_at = now;

      park_shift_left();
      park_spawn_col();

      park_player_y += park_player_vy;
      park_player_vy += 1;
      if (park_player_vy > 3) park_player_vy = 3;
      if (park_player_y < 0) park_player_y = 0;

      ground_h = park_cols[4];
      stand_y = (ground_h == 0) ? (int8_t)(LOGICAL_HEIGHT - 1) : (int8_t)ground_h - 1;
      if (ground_h != 0 && park_player_y >= stand_y) {
        park_player_y = stand_y;
        park_player_vy = 0;
      }

      if (ground_h == 0 && park_player_y >= (int8_t)(LOGICAL_HEIGHT - 1)) park_game_over = true;
      if (!park_game_over) park_score++;
    }
  } else if (action_edge()) {
    tone_click();
    enter_menu();
    return;
  }

  draw_parkour();
}

void start_music_player() {
  music_track_idx = menu_music_choice;
  music_playing = false;
  music_last_nav_at = 0;
  emit_event("MUSIC_STOP");
  noTone(BUZZER_PIN);
}

void draw_music_player() {
  clear_matrix();
  draw_text_center_at("MUSC", 0);

  if (music_track_idx == 0) draw_text_center_at("MINE", 8);
  else if (music_track_idx == 1) draw_text_center_at("OWA", 8);
  else draw_text_center_at("LITE", 8);

  for (uint8_t i = 0; i < MUSIC_TRACK_COUNT; i++) {
    uint8_t x = (uint8_t)(10 + i * 4);
    bool on = (i == music_track_idx) ? true : blink_on;
    set_pixel(23, x, on);
    set_pixel(23, (uint8_t)(x + 1), on);
    if (i == menu_music_choice) set_pixel(22, x, true);
  }

  if (music_playing) {
    set_pixel(16, 1, true);
    set_pixel(17, 1, true);
    set_pixel(18, 1, true);
    set_pixel(17, 2, true);
    set_pixel(17, 3, true);
  } else {
    if (blink_on) {
      set_pixel(16, 1, true);
      set_pixel(17, 1, true);
      set_pixel(18, 1, true);
      set_pixel(16, 3, true);
      set_pixel(17, 3, true);
      set_pixel(18, 3, true);
    }
  }

  update_matrix();
}

void update_music_player() {
  unsigned long now = millis();
  if (now - music_last_nav_at >= MUSIC_NAV_MS) {
    int8_t dy = joy_dir_y();
    if (dy != 0) {
      music_last_nav_at = now;
      if (dy < 0) {
        music_track_idx = (music_track_idx == 0) ? (MUSIC_TRACK_COUNT - 1) : (uint8_t)(music_track_idx - 1);
      } else {
        music_track_idx = (uint8_t)((music_track_idx + 1) % MUSIC_TRACK_COUNT);
      }
      emit_event("MUSIC_TRACK_CHANGE");
      if (music_playing) {
        if (music_track_idx == 0) emit_event("MENU_MUSIC_1");
        else if (music_track_idx == 1) emit_event("MENU_MUSIC_2");
        else emit_event("MENU_MUSIC_3");
      }
      tone_click();
    }
  }

  bool joy_press = joy_button_edge();
  bool main_press = main_button_edge();
  if (joy_press) {
    music_playing = true;
    menu_music_choice = music_track_idx;
    if (menu_music_choice == 0) emit_event("MENU_MUSIC_1");
    else if (menu_music_choice == 1) emit_event("MENU_MUSIC_2");
    else emit_event("MENU_MUSIC_3");
    emit_event("MUSIC_SELECT");
    tone_click();
  }

  if (main_press) {
    music_playing = false;
    emit_event("MUSIC_STOP");
    noTone(BUZZER_PIN);
    tone_click();
    enter_menu();
    return;
  }

  draw_music_player();
}

void setup() {
  pinMode(MAIN_BTN_PIN, INPUT_PULLUP);
  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(JOY2_SW_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  if (SERIAL_DEBUG) {
    Serial.begin(115200);
    delay(60);
    Serial.println();
    Serial.println("=== SBII Console Debug Start ===");
    Serial.print("Pins -> DIN:");
    Serial.print(DIN_PIN);
    Serial.print(" CS:");
    Serial.print(CS_PIN);
    Serial.print(" CLK:");
    Serial.print(CLK_PIN);
    Serial.print(" MAIN_BTN:");
    Serial.print(MAIN_BTN_PIN);
    Serial.print(" JOY_BTN:");
    Serial.print(JOY_SW_PIN);
    Serial.print(" JOY2_BTN:");
    Serial.print(JOY2_SW_PIN);
    Serial.print(" BUZZER:");
    Serial.println(BUZZER_PIN);
    Serial.print("DEVICE_COUNT=");
    Serial.println(DEVICE_COUNT);
    Serial.println("Expected: button on D2->GND, not on D13.");
  }

  max7219_init();
  randomSeed(analogRead(A3));

  console_powered = false;
  console_state = STATE_BOOT;
  clear_matrix();
  update_matrix();
  log_event("Console initialized; waiting for power button.");
}

void loop() {
  update_blink();
  debug_snapshot();

  if (!console_powered) {
    clear_matrix();
    const uint8_t x0 = 2;
    const uint8_t y0 = 2;
    const uint8_t x1 = LOGICAL_WIDTH - 3;
    const uint8_t y1 = LOGICAL_HEIGHT - 3;

    for (uint8_t x = x0; x <= x1; x++) {
      set_pixel(y0, x, true);
      set_pixel(y1, x, true);
    }
    for (uint8_t y = y0; y <= y1; y++) {
      set_pixel(y, x0, true);
      set_pixel(y, x1, true);
    }

    draw_text_center_at("SBII", (uint8_t)((LOGICAL_HEIGHT - 7) / 2));
    if (blink_on) {
      set_pixel(y0, x0, true);
      set_pixel(y0, x1, true);
      set_pixel(y1, x0, true);
      set_pixel(y1, x1, true);
    }
    update_matrix();

    if (main_button_edge()) {
      log_event("Power button edge detected; entering boot.");
      console_powered = true;
      boot_started_at = millis();
      console_state = STATE_BOOT;
      emit_event("BOOT_START");
    }
    return;
  }

  if (console_state == STATE_BOOT) {
    draw_boot();
    if (millis() - boot_started_at > 2600) {
      log_event("Boot complete -> MENU");
    }
    return;
  }

  if (console_state == STATE_MENU) {
    update_menu_music();

    if (can_move_now()) {
      int8_t dx = joy_dir_x();
      if (dx != 0) {
        int8_t next = (int8_t)menu_index + dx;
        if (next >= 0 && next < MENU_COUNT) {
          menu_index = (uint8_t)next;
          tone_click();
          emit_event("MENU_CLICK");
          if (SERIAL_DEBUG) {
            Serial.print("LOG ");
            Serial.print(millis());
            Serial.print("ms: Menu index -> ");
            Serial.println(menu_index);
          }
        }
      }
    }

    if (action_edge()) {
      tone_click();
      launch_game_from_menu();
    }

    draw_menu();
    return;
  }

  if (console_state == STATE_BATTLESHIP) { update_battleship(); return; }
  if (console_state == STATE_SNAKE) { update_snake(); return; }
  if (console_state == STATE_DINO) { update_dino(); return; }
  if (console_state == STATE_SURF) { update_surf(); return; }
  if (console_state == STATE_REACT) { update_react(); return; }
  if (console_state == STATE_PARKOUR) { update_parkour(); return; }
  if (console_state == STATE_MUSIC) { update_music_player(); return; }
}
