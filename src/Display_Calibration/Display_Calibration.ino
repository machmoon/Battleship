#include <SPI.h>

#define DEVICE_COUNT 12
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10

#define BTN_PIN A5

const uint8_t MODULE_COLS = 4;
const uint8_t MODULE_ROWS = 3;
const uint8_t LOGICAL_WIDTH = MODULE_COLS * 8;
const uint8_t LOGICAL_HEIGHT = MODULE_ROWS * 8;

const uint8_t REG_DIGIT0 = 0x01;
const uint8_t REG_DECODE_MODE = 0x09;
const uint8_t REG_INTENSITY = 0x0A;
const uint8_t REG_SCAN_LIMIT = 0x0B;
const uint8_t REG_SHUTDOWN = 0x0C;
const uint8_t REG_DISPLAY_TEST = 0x0F;

uint8_t matrix_rows[8][DEVICE_COUNT];
uint8_t cal_mode = 0;
uint8_t probe_idx = 0;
bool blink_on = true;
unsigned long last_blink_at = 0;

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
    for (uint8_t d = 0; d < DEVICE_COUNT; d++) matrix_rows[y][d] = 0;
  }
}

void update_matrix() {
  for (uint8_t y = 0; y < 8; y++) send_row(REG_DIGIT0 + y, matrix_rows[y]);
}

void set_pixel(int8_t y, int8_t x, bool on) {
  if (x < 0 || x >= LOGICAL_WIDTH || y < 0 || y >= LOGICAL_HEIGHT) return;
  uint8_t panel = (uint8_t)y / 8;
  uint8_t local_y = (uint8_t)y % 8;
  uint8_t logical_idx = panel * MODULE_COLS + (uint8_t)x / 8;
  if (panel >= MODULE_ROWS || logical_idx >= DEVICE_COUNT) return;
  uint8_t dev = map_logical_to_device(logical_idx);
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

bool button_edge() {
  static bool last_state = HIGH;
  bool now_state = (digitalRead(BTN_PIN) == LOW);
  bool edge = (now_state && !last_state);
  last_state = now_state;
  return edge;
}

void print_mapping_table() {
  Serial.println("Mapping table (logical -> device):");
  for (uint8_t row = 0; row < MODULE_ROWS; row++) {
    for (uint8_t col = 0; col < MODULE_COLS; col++) {
      uint8_t logical_idx = row * MODULE_COLS + col;
      uint8_t dev = map_logical_to_device(logical_idx);
      Serial.print("L");
      Serial.print(logical_idx);
      Serial.print("->D");
      Serial.print(dev + 1);
      Serial.print(ROT180_DEVICE[dev] ? "(R) " : "(N) ");
    }
    Serial.println();
  }
}

void draw_digit_3x5(uint8_t x, uint8_t y, uint8_t d) {
  static const uint8_t font[10][3] = {
    {0x1F, 0x11, 0x1F},
    {0x00, 0x1F, 0x00},
    {0x1D, 0x15, 0x17},
    {0x15, 0x15, 0x1F},
    {0x07, 0x04, 0x1F},
    {0x17, 0x15, 0x1D},
    {0x1F, 0x15, 0x1D},
    {0x01, 0x01, 0x1F},
    {0x1F, 0x15, 0x1F},
    {0x17, 0x15, 0x1F}
  };

  if (d > 9) return;
  for (uint8_t cx = 0; cx < 3; cx++) {
    uint8_t bits = font[d][cx];
    for (uint8_t cy = 0; cy < 5; cy++) {
      if (bits & (1 << cy)) set_pixel((int8_t)(y + cy), (int8_t)(x + cx), true);
    }
  }
}

void draw_module_border(uint8_t x0, uint8_t y0) {
  for (uint8_t x = 0; x < 8; x++) {
    set_pixel((int8_t)y0, (int8_t)(x0 + x), true);
    set_pixel((int8_t)(y0 + 7), (int8_t)(x0 + x), true);
  }
  for (uint8_t y = 0; y < 8; y++) {
    set_pixel((int8_t)(y0 + y), (int8_t)x0, true);
    set_pixel((int8_t)(y0 + y), (int8_t)(x0 + 7), true);
  }
}

void draw_numbered_modules() {
  // Show addressed device IDs on the 4x3 module grid.
  for (uint8_t panel = 0; panel < MODULE_ROWS; panel++) {
    for (uint8_t col = 0; col < MODULE_COLS; col++) {
      uint8_t logical_idx = panel * MODULE_COLS + col;
      uint8_t dev = map_logical_to_device(logical_idx);
      uint8_t id = (uint8_t)(dev + 1);
      uint8_t x0 = col * 8;
      uint8_t y0 = panel * 8;

      draw_module_border(x0, y0);

      if (id < 10) {
        draw_digit_3x5(x0 + 2, y0 + 1, id);
      } else {
        draw_digit_3x5(x0 + 0, y0 + 1, id / 10);
        draw_digit_3x5(x0 + 4, y0 + 1, id % 10);
      }

      if (blink_on) set_pixel((int8_t)y0, (int8_t)x0, true);
    }
  }
}

void draw_probe_module() {
  uint8_t row = probe_idx / MODULE_COLS;
  uint8_t col = probe_idx % MODULE_COLS;
  uint8_t logical_idx = row * MODULE_COLS + col;
  uint8_t dev = map_logical_to_device(logical_idx);
  uint8_t x0 = col * 8;
  uint8_t y0 = row * 8;

  draw_module_border(x0, y0);
  set_pixel((int8_t)(y0 + 1), (int8_t)(x0 + 1), true);
  set_pixel((int8_t)(y0 + 1), (int8_t)(x0 + 2), true);
  set_pixel((int8_t)(y0 + 2), (int8_t)(x0 + 1), true);
  set_pixel((int8_t)(y0 + 6), (int8_t)(x0 + 6), blink_on);

  if (dev + 1 < 10) draw_digit_3x5(x0 + 2, y0 + 1, dev + 1);
}

void draw_big_vertical_circle() {
  const int16_t cx = 16;
  const int16_t cy = 12;
  const int16_t rx = 10;
  const int16_t ry = 11;
  const int32_t rx2 = (int32_t)rx * rx;
  const int32_t ry2 = (int32_t)ry * ry;

  for (int16_t y = 0; y < LOGICAL_HEIGHT; y++) {
    for (int16_t x = 0; x < LOGICAL_WIDTH; x++) {
      int32_t dx = x - cx;
      int32_t dy = y - cy;
      int32_t value = (dx * dx * 100) / rx2 + (dy * dy * 100) / ry2;
      if (value >= 90 && value <= 110) set_pixel((int8_t)y, (int8_t)x, true);
    }
  }
}

void draw_screen() {
  clear_matrix();
  if (cal_mode == 0) draw_numbered_modules();
  else if (cal_mode == 1) draw_big_vertical_circle();
  else draw_probe_module();
  update_matrix();
}

void print_help() {
  Serial.println("Serial commands:");
  Serial.println("  h: help");
  Serial.println("  m: print mapping table");
  Serial.println("  0: mode NUMBERS");
  Serial.println("  1: mode CIRCLE");
  Serial.println("  2: mode PROBE");
  Serial.println("  n: next probe module");
  Serial.println("  p: previous probe module");
}

void report_probe() {
  uint8_t row = probe_idx / MODULE_COLS;
  uint8_t col = probe_idx % MODULE_COLS;
  uint8_t logical_idx = row * MODULE_COLS + col;
  uint8_t dev = map_logical_to_device(logical_idx);
  Serial.print("PROBE idx=");
  Serial.print(probe_idx);
  Serial.print(" logical=");
  Serial.print(logical_idx);
  Serial.print(" row=");
  Serial.print(row);
  Serial.print(" col=");
  Serial.print(col);
  Serial.print(" device=");
  Serial.print(dev + 1);
  Serial.print(" rot180=");
  Serial.println(ROT180_DEVICE[dev] ? 1 : 0);
}

void handle_serial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == 'h' || c == 'H') {
      print_help();
    } else if (c == 'm' || c == 'M') {
      print_mapping_table();
    } else if (c == '0' || c == '1' || c == '2') {
      cal_mode = (uint8_t)(c - '0');
      Serial.print("Calibration mode -> ");
      if (cal_mode == 0) Serial.println("NUMBERS");
      else if (cal_mode == 1) Serial.println("CIRCLE");
      else {
        Serial.println("PROBE");
        report_probe();
      }
    } else if (c == 'n' || c == 'N') {
      probe_idx = (uint8_t)((probe_idx + 1) % DEVICE_COUNT);
      report_probe();
    } else if (c == 'p' || c == 'P') {
      probe_idx = (probe_idx == 0) ? (DEVICE_COUNT - 1) : (uint8_t)(probe_idx - 1);
      report_probe();
    }
  }
}

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  max7219_init();
  Serial.println("Display calibration ready.");
  Serial.println("Mode 0: show addressed module numbers");
  Serial.println("Mode 1: big vertical circle");
  Serial.println("Mode 2: single-module probe");
  Serial.println("Press button to toggle mode.");
  print_help();
  print_mapping_table();
}

void loop() {
  unsigned long now = millis();
  if (now - last_blink_at >= 300) {
    blink_on = !blink_on;
    last_blink_at = now;
  }

  if (button_edge()) {
    cal_mode = (uint8_t)((cal_mode + 1) % 3);
    Serial.print("Calibration mode -> ");
    if (cal_mode == 0) Serial.println("NUMBERS");
    else if (cal_mode == 1) Serial.println("CIRCLE");
    else {
      Serial.println("PROBE");
      report_probe();
    }
  }

  handle_serial();
  draw_screen();
}
