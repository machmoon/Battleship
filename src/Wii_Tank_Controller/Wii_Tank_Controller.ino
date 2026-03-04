#include <Wire.h>
#include <math.h>

#define JOY_X_PIN A0
#define JOY_Y_PIN A1
#define SHOOT_BTN_PIN 2
#define MINE_BTN_PIN 3
#define MPU_ADDR 0x68

const uint16_t SEND_PERIOD_MS = 20;

bool last_shoot_down = false;
bool last_mine_down = false;
unsigned long last_send_at = 0;

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

uint8_t read_aim_dir() {
  int16_t ax, ay, az;
  if (!mpu_read_accel(ax, ay, az)) return 2;

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

  if (hx == 0 && hy == 0) return 2;

  if (hx == 0 && hy < 0) return 0;
  if (hx > 0 && hy < 0) return 1;
  if (hx > 0 && hy == 0) return 2;
  if (hx > 0 && hy > 0) return 3;
  if (hx == 0 && hy > 0) return 4;
  if (hx < 0 && hy > 0) return 5;
  if (hx < 0 && hy == 0) return 6;
  return 7;
}

void setup() {
  pinMode(SHOOT_BTN_PIN, INPUT_PULLUP);
  pinMode(MINE_BTN_PIN, INPUT_PULLUP);

  Wire.begin();
  mpu_init();

  Serial.begin(115200);
}

void loop() {
  unsigned long now = millis();
  if (now - last_send_at < SEND_PERIOD_MS) return;
  last_send_at = now;

  int joy_x = analogRead(JOY_X_PIN);
  int joy_y = analogRead(JOY_Y_PIN);

  bool shoot_down = (digitalRead(SHOOT_BTN_PIN) == LOW);
  bool mine_down = (digitalRead(MINE_BTN_PIN) == LOW);

  bool shoot_edge = (shoot_down && !last_shoot_down);
  bool mine_edge = (mine_down && !last_mine_down);

  last_shoot_down = shoot_down;
  last_mine_down = mine_down;

  uint8_t aim_dir = read_aim_dir();

  // C,jx,jy,shoot_down,mine_down,shoot_edge,mine_edge,aim_dir
  Serial.print("C,");
  Serial.print(joy_x);
  Serial.print(',');
  Serial.print(joy_y);
  Serial.print(',');
  Serial.print(shoot_down ? 1 : 0);
  Serial.print(',');
  Serial.print(mine_down ? 1 : 0);
  Serial.print(',');
  Serial.print(shoot_edge ? 1 : 0);
  Serial.print(',');
  Serial.print(mine_edge ? 1 : 0);
  Serial.print(',');
  Serial.println(aim_dir);
}
