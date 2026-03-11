#include <SPI.h>
#include <Wire.h>
#include <math.h>

// MAX7219 4-in-1 (32x8)
#define DEVICE_COUNT 12
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10

// Controls
#define JOY_X_PIN A0
#define JOY_Y_PIN A1
#define SHOOT_BTN_PIN 2
#define MINE_BTN_PIN 3

// MPU6050 (I2C on SDA/SCL)
#define MPU_ADDR 0x68

const bool FLIP_X = false;
const bool FLIP_Y = false;

// Beginner mode pacing: slower and easier to read.
const uint16_t MOVE_REPEAT_MS = 170;
const uint16_t BULLET_STEP_MS = 95;
const uint16_t ENEMY_STEP_MS = 340;
const uint16_t ENEMY_FIRE_MS = 1400;
const uint16_t BLINK_MS = 180;
const uint16_t TUTORIAL_STEP_MS = 2500;
const uint16_t RESTART_HOLD_MS = 1200;

const uint8_t MAX_BULLETS = 6;
const uint8_t MAX_MINES = 3;
const uint8_t MAX_ENEMIES = 3;

// MAX7219 registers
const uint8_t REG_DIGIT0 = 0x01;
const uint8_t REG_DECODE_MODE = 0x09;
const uint8_t REG_INTENSITY = 0x0A;
const uint8_t REG_SCAN_LIMIT = 0x0B;
const uint8_t REG_SHUTDOWN = 0x0C;
const uint8_t REG_DISPLAY_TEST = 0x0F;

const uint8_t LOGICAL_WIDTH = 32;
const uint8_t LOGICAL_HEIGHT = 8;

enum GameState : uint8_t {
  STATE_INTRO = 0,
  STATE_TUTORIAL,
  STATE_PLAY,
  STATE_WIN,
  STATE_LOSE
};

struct Bullet {
  bool active;
  bool from_player;
  int8_t x;
  int8_t y;
  int8_t dx;
  int8_t dy;
  uint8_t bounces_left;
};

struct Mine {
  bool active;
  int8_t x;
  int8_t y;
};

uint8_t matrix_rows[8][DEVICE_COUNT];
bool walls[8][32];

GameState game_state = STATE_INTRO;

int8_t player_x = 2;
int8_t player_y = 6;
int8_t enemy_x[MAX_ENEMIES];
int8_t enemy_y[MAX_ENEMIES];
bool enemy_alive[MAX_ENEMIES];

// 8 directions: N, NE, E, SE, S, SW, W, NW
const int8_t dir_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
const int8_t dir_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
uint8_t aim_dir = 2; // right

Bullet bullets[MAX_BULLETS];
Mine player_mines[MAX_MINES];

bool blink_on = true;
unsigned long last_blink_at = 0;
unsigned long last_move_at = 0;
unsigned long last_bullet_at = 0;
unsigned long last_enemy_step_at = 0;
unsigned long last_enemy_fire_at = 0;
unsigned long last_tutorial_at = 0;
unsigned long hold_both_since = 0;
uint8_t tutorial_step = 0;

void send_all(uint8_t reg, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  for (uint8_t i = 0; i < DEVICE_COUNT; i++) {
    SPI.transfer(reg);
    SPI.transfer(data);
  }
  digitalWrite(CS_PIN, HIGH);
}

void send_row(uint8_t reg, const uint8_t data_by_device[DEVICE_COUNT]) {
  digitalWrite(CS_PIN, LOW);
  for (int8_t dev = DEVICE_COUNT - 1; dev >= 0; dev--) {
    SPI.transfer(reg);
    SPI.transfer(data_by_device[dev]);
  }
  digitalWrite(CS_PIN, HIGH);
}

void clear_matrix() {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t dev = 0; dev < DEVICE_COUNT; dev++) {
      matrix_rows[y][dev] = 0;
    }
  }
}

void update_matrix() {
  for (uint8_t y = 0; y < 8; y++) {
    send_row(REG_DIGIT0 + y, matrix_rows[y]);
  }
}

void set_pixel(int8_t y, int8_t x, bool on) {
  if (x < 0 || x >= LOGICAL_WIDTH || y < 0 || y >= LOGICAL_HEIGHT) return;

  uint8_t draw_x = FLIP_X ? (31 - x) : x;
  uint8_t draw_y = FLIP_Y ? (7 - y) : y;

  uint8_t dev = draw_x / 8;
  uint8_t col = draw_x % 8;
  uint8_t bit_mask = (uint8_t)(1 << (7 - col));

  if (on) matrix_rows[draw_y][dev] |= bit_mask;
  else matrix_rows[draw_y][dev] &= (uint8_t)~bit_mask;
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

void mpu_init() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

bool mpu_read_accel(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return false;

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
  return true;
}

bool in_bounds(int8_t x, int8_t y) {
  return (x >= 0 && x < LOGICAL_WIDTH && y >= 0 && y < LOGICAL_HEIGHT);
}

bool is_blocked(int8_t x, int8_t y) {
  if (!in_bounds(x, y)) return true;
  return walls[y][x];
}

bool any_enemy_alive() {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    if (enemy_alive[i]) return true;
  }
  return false;
}

void reset_level() {
  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      walls[y][x] = false;
    }
  }

  // Protection cover (compact wall blocks, no full outer border).
  for (uint8_t x = 6; x <= 8; x++) walls[3][x] = true;
  for (uint8_t x = 10; x <= 12; x++) walls[5][x] = true;
  for (uint8_t x = 15; x <= 17; x++) walls[2][x] = true;
  for (uint8_t x = 19; x <= 21; x++) walls[4][x] = true;
  for (uint8_t x = 24; x <= 26; x++) walls[3][x] = true;
  walls[1][13] = true;
  walls[6][18] = true;

  player_x = 2;
  player_y = 6;
  enemy_x[0] = 28; enemy_y[0] = 1; enemy_alive[0] = true;
  enemy_x[1] = 27; enemy_y[1] = 6; enemy_alive[1] = true;
  enemy_x[2] = 22; enemy_y[2] = 1; enemy_alive[2] = true;
  aim_dir = 2;

  for (uint8_t i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (uint8_t i = 0; i < MAX_MINES; i++) player_mines[i].active = false;

  last_move_at = millis();
  last_bullet_at = millis();
  last_enemy_step_at = millis();
  last_enemy_fire_at = millis();
}

bool edge_pressed(uint8_t pin) {
  static bool last_shoot = HIGH;
  static bool last_mine = HIGH;

  bool current = digitalRead(pin);
  bool pressed = false;

  if (pin == SHOOT_BTN_PIN) {
    pressed = (last_shoot == HIGH && current == LOW);
    last_shoot = current;
  } else if (pin == MINE_BTN_PIN) {
    pressed = (last_mine == HIGH && current == LOW);
    last_mine = current;
  }
  return pressed;
}

int8_t axis_dir(int value) {
  if (value < 320) return -1;
  if (value > 700) return 1;
  return 0;
}

void update_aim_from_mpu() {
  int16_t ax, ay, az;
  if (!mpu_read_accel(ax, ay, az)) return;

  float fax = (float)ax;
  float fay = (float)ay;
  float faz = (float)az;

  float roll = atan2f(fay, faz) * 57.2958f;
  float pitch = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * 57.2958f;

  int8_t hx = 0;
  int8_t hy = 0;

  if (roll > 15) hx = 1;
  else if (roll < -15) hx = -1;

  if (pitch > 15) hy = 1;
  else if (pitch < -15) hy = -1;

  if (hx == 0 && hy == 0) return;

  if (hx == 0 && hy < 0) aim_dir = 0;
  else if (hx > 0 && hy < 0) aim_dir = 1;
  else if (hx > 0 && hy == 0) aim_dir = 2;
  else if (hx > 0 && hy > 0) aim_dir = 3;
  else if (hx == 0 && hy > 0) aim_dir = 4;
  else if (hx < 0 && hy > 0) aim_dir = 5;
  else if (hx < 0 && hy == 0) aim_dir = 6;
  else if (hx < 0 && hy < 0) aim_dir = 7;
}

void try_move_player() {
  unsigned long now = millis();
  if (now - last_move_at < MOVE_REPEAT_MS) return;

  int8_t dx = axis_dir(analogRead(JOY_X_PIN));
  int8_t dy = axis_dir(analogRead(JOY_Y_PIN));
  if (dx == 0 && dy == 0) return;

  int8_t nx = player_x + dx;
  int8_t ny = player_y + dy;

  bool enemy_on_target = false;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    if (enemy_alive[i] && nx == enemy_x[i] && ny == enemy_y[i]) {
      enemy_on_target = true;
      break;
    }
  }

  if (!is_blocked(nx, ny) && !enemy_on_target) {
    player_x = nx;
    player_y = ny;
  }
  last_move_at = now;
}

void spawn_bullet(bool from_player, int8_t x, int8_t y, int8_t dx, int8_t dy) {
  for (uint8_t i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].from_player = from_player;
      bullets[i].x = x;
      bullets[i].y = y;
      bullets[i].dx = dx;
      bullets[i].dy = dy;
      bullets[i].bounces_left = from_player ? 1 : 0;
      return;
    }
  }
}

void try_shoot_player() {
  if (!edge_pressed(SHOOT_BTN_PIN)) return;

  int8_t dx = dir_dx[aim_dir];
  int8_t dy = dir_dy[aim_dir];
  int8_t sx = player_x + dx;
  int8_t sy = player_y + dy;
  if (!in_bounds(sx, sy)) return;
  spawn_bullet(true, sx, sy, dx, dy);
}

void try_place_mine() {
  if (!edge_pressed(MINE_BTN_PIN)) return;

  for (uint8_t i = 0; i < MAX_MINES; i++) {
    if (player_mines[i].active && player_mines[i].x == player_x && player_mines[i].y == player_y) return;
  }

  for (uint8_t i = 0; i < MAX_MINES; i++) {
    if (!player_mines[i].active) {
      player_mines[i].active = true;
      player_mines[i].x = player_x;
      player_mines[i].y = player_y;
      return;
    }
  }
}

void explode_at(int8_t ex, int8_t ey) {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    if (enemy_alive[i] && abs(enemy_x[i] - ex) <= 1 && abs(enemy_y[i] - ey) <= 1) {
      enemy_alive[i] = false;
    }
  }

  if (abs(player_x - ex) <= 1 && abs(player_y - ey) <= 1) {
    game_state = STATE_LOSE;
  }

  for (uint8_t i = 0; i < MAX_MINES; i++) {
    if (player_mines[i].active && abs(player_mines[i].x - ex) <= 1 && abs(player_mines[i].y - ey) <= 1) {
      player_mines[i].active = false;
    }
  }

  if (!any_enemy_alive()) {
    game_state = STATE_WIN;
  }
}

void update_bullets() {
  unsigned long now = millis();
  if (now - last_bullet_at < BULLET_STEP_MS) return;
  last_bullet_at = now;

  for (uint8_t i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;

    int8_t nx = bullets[i].x + bullets[i].dx;
    int8_t ny = bullets[i].y + bullets[i].dy;

    bool bx = is_blocked(nx, bullets[i].y);
    bool by = is_blocked(bullets[i].x, ny);

    if (bx || by || is_blocked(nx, ny)) {
      if (bullets[i].bounces_left == 0) {
        bullets[i].active = false;
        continue;
      }
      if (bx) bullets[i].dx = -bullets[i].dx;
      if (by) bullets[i].dy = -bullets[i].dy;
      if (!bx && !by) {
        bullets[i].dx = -bullets[i].dx;
        bullets[i].dy = -bullets[i].dy;
      }
      bullets[i].bounces_left--;
      nx = bullets[i].x + bullets[i].dx;
      ny = bullets[i].y + bullets[i].dy;
      if (is_blocked(nx, ny)) {
        bullets[i].active = false;
        continue;
      }
    }

    bullets[i].x = nx;
    bullets[i].y = ny;

    bool hit_enemy = false;
    for (uint8_t e = 0; e < MAX_ENEMIES; e++) {
      if (enemy_alive[e] && bullets[i].x == enemy_x[e] && bullets[i].y == enemy_y[e]) {
        bullets[i].active = false;
        enemy_alive[e] = false;
        hit_enemy = true;
        break;
      }
    }
    if (hit_enemy) {
      if (!any_enemy_alive()) game_state = STATE_WIN;
      continue;
    }

    if (bullets[i].x == player_x && bullets[i].y == player_y) {
      bullets[i].active = false;
      game_state = STATE_LOSE;
      continue;
    }

    for (uint8_t m = 0; m < MAX_MINES; m++) {
      if (player_mines[m].active && player_mines[m].x == bullets[i].x && player_mines[m].y == bullets[i].y) {
        bullets[i].active = false;
        explode_at(player_mines[m].x, player_mines[m].y);
        break;
      }
    }
  }
}

void update_enemy() {
  if (!any_enemy_alive() || game_state != STATE_PLAY) return;

  unsigned long now = millis();

  if (now - last_enemy_step_at >= ENEMY_STEP_MS) {
    last_enemy_step_at = now;

    for (uint8_t e = 0; e < MAX_ENEMIES; e++) {
      if (!enemy_alive[e]) continue;

      int8_t dx = 0;
      int8_t dy = 0;
      if (player_x > enemy_x[e]) dx = 1;
      else if (player_x < enemy_x[e]) dx = -1;
      if (player_y > enemy_y[e]) dy = 1;
      else if (player_y < enemy_y[e]) dy = -1;

      int8_t nx = enemy_x[e] + dx;
      int8_t ny = enemy_y[e] + dy;

      bool occupied_by_enemy = false;
      for (uint8_t k = 0; k < MAX_ENEMIES; k++) {
        if (k != e && enemy_alive[k] && enemy_x[k] == nx && enemy_y[k] == ny) {
          occupied_by_enemy = true;
          break;
        }
      }

      if (!is_blocked(nx, ny) && !(nx == player_x && ny == player_y) && !occupied_by_enemy) {
        enemy_x[e] = nx;
        enemy_y[e] = ny;
      }
    }
  }

  if (now - last_enemy_fire_at >= ENEMY_FIRE_MS) {
    last_enemy_fire_at = now;

    for (uint8_t e = 0; e < MAX_ENEMIES; e++) {
      if (!enemy_alive[e]) continue;
      int8_t dx = 0;
      int8_t dy = 0;
      if (player_x > enemy_x[e]) dx = 1;
      else if (player_x < enemy_x[e]) dx = -1;
      if (player_y > enemy_y[e]) dy = 1;
      else if (player_y < enemy_y[e]) dy = -1;

      if (dx != 0 || dy != 0) {
        spawn_bullet(false, enemy_x[e] + dx, enemy_y[e] + dy, dx, dy);
      }
    }
  }

  for (uint8_t m = 0; m < MAX_MINES; m++) {
    if (!player_mines[m].active) continue;
    for (uint8_t e = 0; e < MAX_ENEMIES; e++) {
      if (enemy_alive[e] && player_mines[m].x == enemy_x[e] && player_mines[m].y == enemy_y[e]) {
        explode_at(enemy_x[e], enemy_y[e]);
        player_mines[m].active = false;
      }
    }
  }
}

void update_blink() {
  unsigned long now = millis();
  if (now - last_blink_at >= BLINK_MS) {
    blink_on = !blink_on;
    last_blink_at = now;
  }
}

void draw_intro() {
  clear_matrix();
  // Tank icon + pulse cue.
  set_pixel(2, 4, true); set_pixel(2, 5, true);
  set_pixel(3, 4, true); set_pixel(3, 5, true);
  set_pixel(1, 5, true);
  set_pixel(4, 3, true); set_pixel(4, 6, true);

  if (blink_on) {
    for (uint8_t x = 24; x < 30; x++) set_pixel(6, x, true);
  }
  update_matrix();
}

void draw_tutorial_step(uint8_t step) {
  clear_matrix();
  for (uint8_t x = 0; x < 32; x++) {
    set_pixel(0, x, true);
    set_pixel(7, x, true);
  }

  if (step == 0) {
    // MOVE
    set_pixel(3, 4, true); set_pixel(3, 5, true); set_pixel(3, 6, true);
    set_pixel(2, 5, true); set_pixel(4, 5, true);
    set_pixel(3, 22, true); set_pixel(3, 23, true);
    set_pixel(4, 22, true); set_pixel(4, 23, true);
  } else if (step == 1) {
    // AIM
    set_pixel(4, 8, true); set_pixel(4, 9, true);
    set_pixel(5, 8, true); set_pixel(5, 9, true);
    set_pixel(4, 11, true); set_pixel(3, 13, true); set_pixel(2, 15, true);
  } else if (step == 2) {
    // SHOOT
    set_pixel(4, 6, true); set_pixel(4, 7, true);
    set_pixel(5, 6, true); set_pixel(5, 7, true);
    set_pixel(4, 10, true); set_pixel(4, 12, true); set_pixel(4, 14, true);
  } else {
    // MINE
    set_pixel(4, 10, true); set_pixel(4, 11, true); set_pixel(4, 12, true);
    set_pixel(3, 11, true); set_pixel(5, 11, true);
    set_pixel(3, 20, true); set_pixel(3, 21, true);
    set_pixel(4, 20, true); set_pixel(4, 21, true);
  }

  if (blink_on) {
    set_pixel(6, 29, true);
    set_pixel(6, 30, true);
  }
  update_matrix();
}

void draw_game() {
  clear_matrix();

  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      if (walls[y][x]) set_pixel(y, x, true);
    }
  }

  for (uint8_t i = 0; i < MAX_MINES; i++) {
    if (player_mines[i].active && blink_on) {
      int8_t mx = player_mines[i].x;
      int8_t my = player_mines[i].y;
      set_pixel(my, mx, true);
      set_pixel(my - 1, mx, true);
      set_pixel(my + 1, mx, true);
      set_pixel(my, mx - 1, true);
      set_pixel(my, mx + 1, true);
    }
  }

  for (uint8_t i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active && (blink_on || bullets[i].from_player)) {
      set_pixel(bullets[i].y, bullets[i].x, true);
    }
  }

  // Enemy tanks: hollow-looking 2x2
  for (uint8_t e = 0; e < MAX_ENEMIES; e++) {
    if (!enemy_alive[e]) continue;
    set_pixel(enemy_y[e], enemy_x[e], true);
    set_pixel(enemy_y[e], enemy_x[e] + 1, true);
    set_pixel(enemy_y[e] + 1, enemy_x[e], true);
    set_pixel(enemy_y[e] + 1, enemy_x[e] + 1, true);
    if (!blink_on) set_pixel(enemy_y[e], enemy_x[e], false);
  }

  // Player tank: solid 2x2
  set_pixel(player_y, player_x, true);
  set_pixel(player_y, player_x + 1, true);
  set_pixel(player_y + 1, player_x, true);
  set_pixel(player_y + 1, player_x + 1, true);

  // Aim ray based on MPU direction; stops at wall/edge.
  int8_t ray_dx = dir_dx[aim_dir];
  int8_t ray_dy = dir_dy[aim_dir];
  int8_t ray_x = player_x + ray_dx;
  int8_t ray_y = player_y + ray_dy;
  uint8_t step = 0;
  while (in_bounds(ray_x, ray_y) && !is_blocked(ray_x, ray_y)) {
    // Dashed ray so bullets/tanks are still readable.
    if ((step % 2 == 0) || blink_on) {
      set_pixel(ray_y, ray_x, true);
    }
    ray_x += ray_dx;
    ray_y += ray_dy;
    step++;
  }

  update_matrix();
}

void draw_end(bool won) {
  clear_matrix();
  if (blink_on) {
    for (uint8_t y = 0; y < 8; y++) {
      for (uint8_t x = 0; x < 32; x++) {
        if (won) {
          if ((x + y) % 2 == 0) set_pixel(y, x, true);
        } else {
          if ((x + y) % 2 == 1) set_pixel(y, x, true);
        }
      }
    }
  }
  update_matrix();
}

void setup() {
  pinMode(SHOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(MINE_BTN_PIN, INPUT_PULLUP);

  Wire.begin();
  mpu_init();
  max7219_init();

  reset_level();
}

void loop() {
  update_blink();

  // Hold both buttons to restart quickly.
  bool shoot_down = (digitalRead(SHOOT_BTN_PIN) == LOW);
  bool mine_down = (digitalRead(MINE_BTN_PIN) == LOW);
  if (shoot_down && mine_down) {
    if (hold_both_since == 0) hold_both_since = millis();
    if (millis() - hold_both_since >= RESTART_HOLD_MS) {
      reset_level();
      game_state = STATE_PLAY;
      hold_both_since = 0;
      return;
    }
  } else {
    hold_both_since = 0;
  }

  if (game_state == STATE_INTRO) {
    draw_intro();
    if (edge_pressed(SHOOT_BTN_PIN) || edge_pressed(MINE_BTN_PIN)) {
      tutorial_step = 0;
      last_tutorial_at = millis();
      game_state = STATE_TUTORIAL;
    }
    return;
  }

  if (game_state == STATE_TUTORIAL) {
    draw_tutorial_step(tutorial_step);
    unsigned long now = millis();
    if (edge_pressed(SHOOT_BTN_PIN) || edge_pressed(MINE_BTN_PIN) || (now - last_tutorial_at >= TUTORIAL_STEP_MS)) {
      last_tutorial_at = now;
      tutorial_step++;
      if (tutorial_step >= 4) {
        reset_level();
        game_state = STATE_PLAY;
      }
    }
    return;
  }

  if (game_state == STATE_PLAY) {
    try_move_player();
    update_aim_from_mpu();
    try_shoot_player();
    try_place_mine();
    update_enemy();
    update_bullets();
    draw_game();
    return;
  }

  if (game_state == STATE_WIN) {
    draw_end(true);
    if (edge_pressed(SHOOT_BTN_PIN)) {
      reset_level();
      game_state = STATE_PLAY;
    }
    return;
  }

  if (game_state == STATE_LOSE) {
    draw_end(false);
    if (edge_pressed(SHOOT_BTN_PIN)) {
      reset_level();
      game_state = STATE_PLAY;
    }
  }
}
