#include <SPI.h>
#include <string.h>

#define DEVICE_COUNT 12
#define DIN_PIN 11
#define CLK_PIN 13
#define CS_PIN 10

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
char line_buf[260];
uint8_t line_len = 0;

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

uint8_t hex_nibble(char c) {
  if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
  if (c >= 'A' && c <= 'F') return (uint8_t)(10 + c - 'A');
  if (c >= 'a' && c <= 'f') return (uint8_t)(10 + c - 'a');
  return 0;
}

void parse_frame_line(char* s) {
  // expected:
  // F,<64 hex chars>   -> 32x8 frame on top panel only
  // F,<192 hex chars>  -> 32x24 frame for 4x3 module grid
  // F,<N hex chars>    -> direct payload for all devices where N=8*DEVICE_COUNT*2
  if (s[0] != 'F' || s[1] != ',') return;
  uint8_t payload_len = (uint8_t)strlen(s + 2);
  uint8_t top_panel_len = (uint8_t)(LOGICAL_WIDTH * 2);
  uint8_t full_frame_len = (uint8_t)(LOGICAL_HEIGHT * MODULE_COLS * 2);

  if (payload_len == top_panel_len || payload_len == 64) {
    uint8_t idx = 2;
    // clear first so old pixels don't stick on other panels
    clear_matrix();
    for (uint8_t y = 0; y < 8; y++) {
      uint8_t cols = (payload_len == 64) ? 4 : MODULE_COLS;
      for (uint8_t d = 0; d < cols; d++) {
        char h = s[idx++];
        char l = s[idx++];
        if (h == 0 || l == 0) return;
        uint8_t b = (uint8_t)((hex_nibble(h) << 4) | hex_nibble(l));
        matrix_rows[y][d] = b;
      }
    }
  } else if (payload_len == full_frame_len && DEVICE_COUNT >= 12) {
    // LOGICAL_HEIGHT rows * MODULE_COLS bytes per row
    uint8_t idx = 2;
    clear_matrix();
    for (uint8_t y = 0; y < LOGICAL_HEIGHT; y++) {
      uint8_t panel = y / 8;        // 0..MODULE_ROWS-1
      uint8_t local_y = y % 8;      // 0..7
      for (uint8_t d = 0; d < MODULE_COLS; d++) {
        char h = s[idx++];
        char l = s[idx++];
        if (h == 0 || l == 0) return;
        uint8_t dev = d + panel * MODULE_COLS;
        matrix_rows[local_y][dev] = (uint8_t)((hex_nibble(h) << 4) | hex_nibble(l));
      }
    }
  } else if (payload_len == (uint8_t)(8 * DEVICE_COUNT * 2)) {
    uint8_t idx = 2;
    for (uint8_t y = 0; y < 8; y++) {
      for (uint8_t d = 0; d < DEVICE_COUNT; d++) {
        char h = s[idx++];
        char l = s[idx++];
        if (h == 0 || l == 0) return;
        matrix_rows[y][d] = (uint8_t)((hex_nibble(h) << 4) | hex_nibble(l));
      }
    }
  } else {
    return;
  }

  update_matrix();
}

void setup() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.begin();
  send_all(REG_SCAN_LIMIT, 7);
  send_all(REG_DECODE_MODE, 0);
  send_all(REG_DISPLAY_TEST, 0);
  send_all(REG_INTENSITY, 1);
  send_all(REG_SHUTDOWN, 1);

  clear_matrix();
  update_matrix();

  Serial.begin(115200);
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line_buf[line_len] = 0;
      if (line_len > 0) parse_frame_line(line_buf);
      line_len = 0;
    } else if (line_len < sizeof(line_buf) - 1) {
      line_buf[line_len++] = c;
    }
  }
}
