// =====================================================================
// GYRO 90 TEST - measures YAW_SCALE and YAW_REVERSED by hand, on this chassis.
//
// Uses the EXACT SAME register-level BMI160 driver as straight_pillars.ino (earlier sketch, now only in git history) (no
// external gyro library), so whatever this test measures transfers directly to
// the main sketch: same registers, same scale factor, same integration method.
//
// Why this test exists: the gyroscope reads angular RATE (degrees per second),
// not angle. The main sketch integrates that rate over time to get an absolute
// heading ("yaw"), but a real sensor never reads exactly the true rate - there is
// a small gain error, and the sign depends on which way the chip is soldered onto
// the board. Both of those have to be measured on the actual car, not assumed:
//   - YAW_REVERSED: is the sign of the sign right? Turning the car LEFT by hand
//     must make yaw increase. If it decreases, the sign is flipped.
//   - YAW_SCALE: a correction factor so that an INTEGRATED 90-degree hand turn
//     actually reads back as (close to) 90.0 degrees, not something like 80 or 95.
//
// HOW TO USE IT:
//   1. Upload this sketch to the Nano. Open the Serial Monitor at 115200 baud.
//   2. Put the car STILL on the table/floor. Wait for the "ready, press START" message.
//   3. Press the button (D10) and let go of the car IMMEDIATELY (it must stay still).
//      This triggers the gyro calibration (about 1 second, do not touch it) and
//      resets the integrated yaw to 0.0.
//   4. Turn the car BY HAND exactly 90 degrees to the LEFT (use a set square, a
//      book corner, or floor marks for an exact 90 degrees). Do it in one smooth
//      motion, without overshooting and coming back.
//   5. Leave the car still and read the LAST "yaw" value it printed. That number
//      should be 90.0 if everything were perfect. It usually is not.
//
// WHAT TO DO WITH THE NUMBER YOU READ (call it Y):
//   - If Y came out NEGATIVE (yaw went DOWN instead of up when turning left):
//       set YAW_REVERSED = true in straight_pillars.ino (earlier sketch, now only in git history) (it currently is false).
//   - New YAW_SCALE = 90.0 / abs(Y)
//       Example: if Y = 73.5  -> YAW_SCALE = 90/73.5 = 1.224
//       Example: if Y = 108.2 -> YAW_SCALE = 90/108.2 = 0.832
//   - Put that YAW_SCALE (and the correct YAW_REVERSED) into straight_pillars.ino (earlier sketch, now only in git history)
//     and re-upload.
//
//   Repeat the test 2-3 times to confirm Y comes out similar every time (it does
//   not have to be exact - +-5 degrees between tries is fine).
//
// Result measured on this chassis, 21 November 2026: Y = 85 (positive, so
// YAW_REVERSED stays false) for an intended 90-degree turn -> YAW_SCALE = 90/85 = 1.06.
// Both values are already applied in straight_pillars.ino (earlier sketch, now only in git history).
//
// Wiring: same as straight_pillars.ino (earlier sketch, now only in git history) -> I2C: SDA=A4, SCL=A5 | START button on D10.
// =====================================================================

#include <Wire.h>

const int BUTTON_PIN = 10;

// ---------------- BMI160 (same registers as straight_pillars.ino (earlier sketch, now only in git history)) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at the +-500 deg/s range
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

// These two are exactly what this test measures. They start at their "neutral"
// values (no correction applied at all) so that the number printed on screen is
// the RAW integrated yaw, with nothing hidden by a scale factor - that is the
// number the YAW_SCALE = 90 / Y formula above needs.
const bool  YAW_REVERSED = false;
const float YAW_SCALE    = 1.00;

float yaw = 0;
unsigned long prevMicros = 0;

void bmiWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(bmiAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(bmiAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(bmiAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool bmiFound(uint8_t addr) {
  bmiAddr = addr;
  uint8_t id = 0;
  return bmiRead(0x00, &id, 1) && id == 0xD1;   // BMI160's chip-ID register must read back 0xD1
}

bool startGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;   // try both possible I2C addresses
  bmiWrite(0x7E, 0x11); delay(50);   // accelerometer: normal power mode
  bmiWrite(0x7E, 0x15); delay(100);  // gyroscope: normal power mode
  bmiWrite(0x42, 0x2A);              // gyro output data rate: 400 Hz
  bmiWrite(0x43, 0x02);              // gyro full-scale range: +-500 deg/s
  delay(50);
  return true;
}

float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;   // 0x10/0x11 = gyro Z axis, low byte then high byte
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_SCALE;
  return YAW_REVERSED ? -dps : dps;
}

void calibrateGyro() {
  // Averages 1000 samples (~1 second at 1 ms per sample) while the car is held
  // still, to remove the sensor's resting bias before integrating.
  float sum = 0;
  int count = 0;
  for (int i = 0; i < 1000; i++) {
    float z = readGyroZ();
    if (!isnan(z)) { sum += z; count++; }
    delay(1);
  }
  gyroBias = (count > 0) ? sum / count : 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  if (!startGyro()) {
    Serial.println(F("ERROR: BMI160 not found (check SDA=A4, SCL=A5)"));
    while (true) delay(1000);
  }

  Serial.println(F("GYRO 90 TEST ready."));
  Serial.println(F("Leave the car STILL and press the button (D10) to calibrate and zero the yaw."));
  while (digitalRead(BUTTON_PIN) == HIGH) delay(10);   // wait for the button

  Serial.println(F("Calibrating... DO NOT TOUCH the car (1 s)"));
  calibrateGyro();
  yaw = 0;
  prevMicros = micros();
  Serial.println(F("READY. Turn the car by hand, EXACTLY 90 degrees to the LEFT, in one smooth motion."));
  Serial.println(F("When you finish and leave it still, write down the last yaw value printed."));
}

void loop() {
  unsigned long nowUs = micros();
  float dt = (nowUs - prevMicros) / 1000000.0;
  prevMicros = nowUs;

  float z = readGyroZ();
  if (!isnan(z)) {
    float yawRate = (z - gyroBias) * YAW_SCALE;
    yaw += yawRate * dt;
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 100) {         // 10 times per second
    lastPrint = millis();
    Serial.print(F("yaw = "));
    Serial.println(yaw, 1);
  }
}
