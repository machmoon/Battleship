#include <SPI.h>
#include <Wire.h>
#include <math.h>

// MAX7219 4-in-1 (8x32)
#define DEVICE_COUNT 12
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10

// MPU6050 on I2C
#define MPU_ADDR 0x68

const bool FLIP_X = false;
const bool FLIP_Y = false;

const uint8_t REG_DIGIT0 = 0x01;
const uint8_t REG_DECODE_MODE = 0x09;
const uint8_t REG_INTENSITY = 0x0A;
const uint8_t REG_SCAN_LIMIT = 0x0B;
const uint8_t REG_SHUTDOWN = 0x0C;
const uint8_t REG_DISPLAY_TEST = 0x0F;

const uint8_t WORLD_W = 32;
const uint8_t WORLD_H = 8;
const uint8_t WATER_PARTICLES = 70;
const uint16_t STEP_MS = 70;

uint8_t matrix_rows[WORLD_H][DEVICE_COUNT];
bool water[WORLD_H][WORLD_W];
bool next_water[WORLD_H][WORLD_W];
bool walls[WORLD_H][WORLD_W];

unsigned long last_step_at = 0;

float roll_zero = 0.0f;
float pitch_zero = 0.0f;

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
  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t d = 0; d < DEVICE_COUNT; d++) {
      matrix_rows[y][d] = 0;
    }
  }
}

void update_matrix() {
  for (uint8_t y = 0; y < WORLD_H; y++) {
    send_row(REG_DIGIT0 + y, matrix_rows[y]);
  }
}

void set_pixel(int8_t y, int8_t x, bool on) {
  if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return;

  uint8_t draw_x = FLIP_X ? (WORLD_W - 1 - x) : x;
  uint8_t draw_y = FLIP_Y ? (WORLD_H - 1 - y) : y;

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
  send_all(REG_INTENSITY, 2);
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

void compute_roll_pitch(float &roll, float &pitch) {
  int16_t ax, ay, az;
  if (!mpu_read_accel(ax, ay, az)) {
    roll = 0;
    pitch = 0;
    return;
  }

  float fax = (float)ax;
  float fay = (float)ay;
  float faz = (float)az;

  roll = atan2f(fay, faz) * 57.2958f;
  pitch = atan2f(-fax, sqrtf(fay * fay + faz * faz)) * 57.2958f;
}

void calibrate_zero() {
  float r_sum = 0;
  float p_sum = 0;
  const uint8_t samples = 30;

  for (uint8_t i = 0; i < samples; i++) {
    float r, p;
    compute_roll_pitch(r, p);
    r_sum += r;
    p_sum += p;
    delay(20);
  }

  roll_zero = r_sum / samples;
  pitch_zero = p_sum / samples;
}

bool in_bounds(int8_t x, int8_t y) {
  return x >= 0 && x < WORLD_W && y >= 0 && y < WORLD_H;
}

bool blocked(int8_t x, int8_t y) {
  if (!in_bounds(x, y)) return true;
  return walls[y][x];
}

void clear_world() {
  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t x = 0; x < WORLD_W; x++) {
      water[y][x] = false;
      next_water[y][x] = false;
      walls[y][x] = false;
    }
  }
}

void setup_walls() {
  // Minimal container so we keep pixels for water, not a full border.
  for (uint8_t x = 0; x < WORLD_W; x++) {
    walls[WORLD_H - 1][x] = true;
  }
  for (uint8_t y = 2; y < WORLD_H; y++) {
    walls[y][0] = true;
    walls[y][WORLD_W - 1] = true;
  }

  // A few baffles so the flow looks more interesting.
  for (uint8_t y = 3; y <= 5; y++) walls[y][10] = true;
  for (uint8_t y = 2; y <= 4; y++) walls[y][20] = true;
  for (uint8_t x = 24; x <= 27; x++) walls[5][x] = true;
}

void seed_water() {
  uint8_t placed = 0;
  while (placed < WATER_PARTICLES) {
    uint8_t x = random(1, WORLD_W - 1);
    uint8_t y = random(0, WORLD_H - 3);

    if (!walls[y][x] && !water[y][x]) {
      water[y][x] = true;
      placed++;
    }
  }
}

void gravity_from_tilt(int8_t &gx, int8_t &gy) {
  float roll, pitch;
  compute_roll_pitch(roll, pitch);

  roll -= roll_zero;
  pitch -= pitch_zero;

  gx = 0;
  gy = 0;

  if (roll > 10) gx = 1;
  else if (roll < -10) gx = -1;

  if (pitch > 10) gy = 1;
  else if (pitch < -10) gy = -1;

  if (gx == 0 && gy == 0) gy = 1;
}

bool can_move_to(int8_t x, int8_t y) {
  if (!in_bounds(x, y)) return false;
  if (walls[y][x]) return false;
  if (next_water[y][x]) return false;
  return true;
}

void step_water() {
  int8_t gx, gy;
  gravity_from_tilt(gx, gy);

  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t x = 0; x < WORLD_W; x++) {
      next_water[y][x] = false;
    }
  }

  int8_t y_start = (gy > 0) ? (WORLD_H - 1) : 0;
  int8_t y_end = (gy > 0) ? -1 : WORLD_H;
  int8_t y_step = (gy > 0) ? -1 : 1;

  int8_t x_start = (gx > 0) ? (WORLD_W - 1) : 0;
  int8_t x_end = (gx > 0) ? -1 : WORLD_W;
  int8_t x_step = (gx > 0) ? -1 : 1;

  for (int8_t y = y_start; y != y_end; y += y_step) {
    for (int8_t x = x_start; x != x_end; x += x_step) {
      if (!water[y][x]) continue;

      bool moved = false;

      int8_t candidates_x[6];
      int8_t candidates_y[6];
      uint8_t c = 0;

      candidates_x[c] = x + gx;
      candidates_y[c] = y + gy;
      c++;

      candidates_x[c] = x;
      candidates_y[c] = y + gy;
      c++;

      candidates_x[c] = x + gx;
      candidates_y[c] = y;
      c++;

      int8_t side = (random(0, 2) == 0) ? -1 : 1;
      candidates_x[c] = x + side;
      candidates_y[c] = y + gy;
      c++;

      candidates_x[c] = x - side;
      candidates_y[c] = y + gy;
      c++;

      candidates_x[c] = x + side;
      candidates_y[c] = y;
      c++;

      for (uint8_t i = 0; i < c; i++) {
        int8_t nx = candidates_x[i];
        int8_t ny = candidates_y[i];

        if (!can_move_to(nx, ny)) continue;
        if (water[ny][nx]) continue;

        next_water[ny][nx] = true;
        moved = true;
        break;
      }

      if (!moved) {
        if (can_move_to(x, y)) {
          next_water[y][x] = true;
        }
      }
    }
  }

  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t x = 0; x < WORLD_W; x++) {
      water[y][x] = next_water[y][x];
    }
  }
}

void draw_world() {
  clear_matrix();

  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t x = 0; x < WORLD_W; x++) {
      if (walls[y][x]) {
        set_pixel(y, x, true);
      }
    }
  }

  for (uint8_t y = 0; y < WORLD_H; y++) {
    for (uint8_t x = 0; x < WORLD_W; x++) {
      if (water[y][x]) {
        set_pixel(y, x, true);
      }
    }
  }

  update_matrix();
}

void setup() {
  Wire.begin();
  mpu_init();

  max7219_init();

  randomSeed(analogRead(A3));

  clear_world();
  setup_walls();
  seed_water();

  delay(300);
  calibrate_zero();

  last_step_at = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - last_step_at >= STEP_MS) {
    last_step_at = now;
    step_water();
    draw_world();
  }
}
