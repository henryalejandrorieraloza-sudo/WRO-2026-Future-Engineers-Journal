// Gyroscope test: BMI160 yaw (Z axis) angle
// Board: Arduino Nano ESP32 | IMU: BMI160 (SDA=A4, SCL=A5)
// No libraries: reads BMI160 registers directly over I2C.
// Keep the robot STILL during calibration, then rotate it by hand.

#include <Wire.h>

// --- BMI160 registers ---
const uint8_t REG_CHIP_ID   = 0x00;  // should read 0xD1
const uint8_t REG_GYR_DATA  = 0x0C;  // X_L, X_H, Y_L, Y_H, Z_L, Z_H
const uint8_t REG_GYR_CONF  = 0x42;
const uint8_t REG_GYR_RANGE = 0x43;
const uint8_t REG_CMD       = 0x7E;

const uint8_t BMI160_CHIP_ID = 0xD1;

// --- Tunable values ---
const int   CALIBRATION_SAMPLES = 500;   // samples to measure the offset (robot still)
const float GYRO_SENSITIVITY    = 65.6;  // LSB per deg/s for +-500 deg/s range
const int   PRINT_INTERVAL_MS   = 100;   // how often to print

uint8_t bmiAddress = 0;      // detected automatically (0x68 or 0x69)
float gyroZOffset  = 0.0;    // raw offset measured at rest
float yawAngle     = 0.0;    // integrated angle in degrees
unsigned long lastMicros = 0;
unsigned long lastPrint  = 0;

// Writes one byte to a register. Returns true if OK.
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(bmiAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// Reads 'length' bytes starting at 'reg'. Returns true if OK.
bool readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length) {
  Wire.beginTransmission(bmiAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(bmiAddress, length) != length) return false;
  for (uint8_t i = 0; i < length; i++) buffer[i] = Wire.read();
  return true;
}

// Reads the raw Z rotation speed. Returns true if OK.
bool readGyroZRaw(int16_t &gz) {
  uint8_t data[6];
  if (!readRegisters(REG_GYR_DATA, data, 6)) return false;
  gz = (int16_t)((data[5] << 8) | data[4]);
  return true;
}

// Looks for the BMI160 at 0x68 and 0x69
bool findBMI160() {
  uint8_t candidates[2] = {0x68, 0x69};
  for (int i = 0; i < 2; i++) {
    bmiAddress = candidates[i];
    uint8_t id = 0;
    if (readRegisters(REG_CHIP_ID, &id, 1) && id == BMI160_CHIP_ID) {
      Serial.print("BMI160 found at 0x");
      Serial.println(bmiAddress, HEX);
      return true;
    }
  }
  return false;
}

bool initBMI160() {
  if (!writeRegister(REG_CMD, 0xB6)) return false;   // soft reset
  delay(100);
  if (!writeRegister(REG_CMD, 0x15)) return false;   // gyro -> normal mode
  delay(100);
  if (!writeRegister(REG_GYR_CONF, 0x28)) return false;   // 100 Hz, normal filter
  if (!writeRegister(REG_GYR_RANGE, 0x02)) return false;  // +-500 deg/s
  delay(50);
  return true;
}

void calibrateGyro() {
  Serial.println("Calibrating... keep the robot STILL");
  long sum = 0;
  int validSamples = 0;
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    int16_t gz;
    if (readGyroZRaw(gz)) {
      sum += gz;
      validSamples++;
    }
    delay(5);
  }
  gyroZOffset = (validSamples > 0) ? (float)sum / validSamples : 0.0;
  Serial.print("Calibration done. Z offset (raw): ");
  Serial.println(gyroZOffset);
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 5000) delay(10);  // Nano ESP32 native USB

  Wire.begin();              // A4 = SDA, A5 = SCL
  Wire.setClock(400000);
  Wire.setTimeOut(50);       // don't hang if the I2C bus gets stuck

  if (!findBMI160()) {
    Serial.println("ERROR: BMI160 not found. Check wiring (SDA=A4, SCL=A5, VCC, GND).");
    while (true) delay(1000);
  }
  if (!initBMI160()) {
    Serial.println("ERROR: could not configure BMI160.");
    while (true) delay(1000);
  }

  calibrateGyro();

  Serial.println("Ready. Rotate the robot by hand and watch the yaw angle.");
  lastMicros = micros();
}

void loop() {
  int16_t gz;
  if (readGyroZRaw(gz)) {
    unsigned long now = micros();
    float dt = (now - lastMicros) / 1000000.0;   // seconds since last reading
    lastMicros = now;

    float rateZ = (gz - gyroZOffset) / GYRO_SENSITIVITY;  // deg/s
    yawAngle += rateZ * dt;                               // integrate -> degrees

    if (millis() - lastPrint >= PRINT_INTERVAL_MS) {
      lastPrint = millis();
      Serial.print("Rate Z: ");
      Serial.print(rateZ, 1);
      Serial.print(" deg/s | Yaw: ");
      Serial.print(yawAngle, 1);
      Serial.println(" deg");
    }
  } else {
    Serial.println("I2C read error");
    delay(100);
  }

  delay(5);  // ~200 readings per second
}
