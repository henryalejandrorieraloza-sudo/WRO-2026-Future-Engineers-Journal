#include <Wire.h>
#include <VL53L1X.h>

// TOF400C XSHUT control pins
#define PIN_SHUT_LEFT  A3
#define PIN_SHUT_RIGHT A2
#define PIN_SHUT_REAR  A0
#define PIN_SHUT_FRONT A1

// Start button pin
#define PIN_BUTTON     10  // D10

// Assigned unique I2C addresses
#define ADDR_TOF_LEFT  0x30
#define ADDR_TOF_RIGHT 0x31
#define ADDR_TOF_REAR  0x32
#define ADDR_TOF_FRONT 0x33
#define ADDR_BMI160    0x69  

// BMI160 internal registers
#define BMI160_CHIP_ID_REG    0x00
#define BMI160_GYRO_Z_L_REG   0x10
#define BMI160_GYRO_RANGE_REG 0x43
#define BMI160_CMD_REG        0x7E

VL53L1X sensorLeft;
VL53L1X sensorRight;
VL53L1X sensorRear;
VL53L1X sensorFront;

// Variables for Yaw calculation
float yaw = 0.0;
float gz_offset = 0.0;
unsigned long previousTime = 0;

// --- Native I2C functions for BMI160 ---
void writeBMIRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDR_BMI160);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readBMIByte(uint8_t reg) {
  Wire.beginTransmission(ADDR_BMI160);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR_BMI160, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0;
}

int16_t readBMIGyroZ() {
  Wire.beginTransmission(ADDR_BMI160);
  Wire.write(BMI160_GYRO_Z_L_REG);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR_BMI160, (uint8_t)2);
  
  if (Wire.available() >= 2) {
    uint8_t lowByte = Wire.read();
    uint8_t highByte = Wire.read();
    return (int16_t)((highByte << 8) | lowByte);
  }
  return 0;
}

bool initBMI160() {
  writeBMIRegister(BMI160_CMD_REG, 0xB6); // Soft-reset
  delay(100);

  if (readBMIByte(BMI160_CHIP_ID_REG) != 0xD1) {
    return false;
  }

  writeBMIRegister(BMI160_CMD_REG, 0x15); // Power up gyroscope in normal mode
  delay(100);

  writeBMIRegister(BMI160_GYRO_RANGE_REG, 0x03); // Set range to ±250 deg/s
  delay(10);

  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial.println("\n=== TEST: 4x TOF400C (FRONT, LEFT, RIGHT, REAR) + NATIVE BMI160 ===");

  // Set XSHUT pins as outputs and shut down all 4 ToF sensors
  pinMode(PIN_SHUT_LEFT, OUTPUT);
  pinMode(PIN_SHUT_RIGHT, OUTPUT);
  pinMode(PIN_SHUT_REAR, OUTPUT);
  pinMode(PIN_SHUT_FRONT, OUTPUT);
  
  digitalWrite(PIN_SHUT_LEFT, LOW);
  digitalWrite(PIN_SHUT_RIGHT, LOW);
  digitalWrite(PIN_SHUT_REAR, LOW);
  digitalWrite(PIN_SHUT_FRONT, LOW);
  delay(20);

  // Initialize I2C bus (A4 = SDA, A5 = SCL)
  Wire.begin();
  Wire.setClock(100000);

  // 1. Configure LEFT ToF (A3 -> 0x30)
  digitalWrite(PIN_SHUT_LEFT, HIGH);
  delay(20);
  sensorLeft.setTimeout(500);
  if (!sensorLeft.init()) {
    Serial.println("❌ ERROR: Left ToF (A3) not detected.");
  } else {
    sensorLeft.setAddress(ADDR_TOF_LEFT);
    sensorLeft.setDistanceMode(VL53L1X::Short);
    sensorLeft.setMeasurementTimingBudget(50000);
    sensorLeft.startContinuous(50);
    Serial.println("✅ Left ToF ready (0x30)");
  }

  // 2. Configure RIGHT ToF (A2 -> 0x31)
  digitalWrite(PIN_SHUT_RIGHT, HIGH);
  delay(20);
  sensorRight.setTimeout(500);
  if (!sensorRight.init()) {
    Serial.println("❌ ERROR: Right ToF (A2) not detected.");
  } else {
    sensorRight.setAddress(ADDR_TOF_RIGHT);
    sensorRight.setDistanceMode(VL53L1X::Short);
    sensorRight.setMeasurementTimingBudget(50000);
    sensorRight.startContinuous(50);
    Serial.println("✅ Right ToF ready (0x31)");
  }

  // 3. Configure REAR ToF (A0 -> 0x32)
  digitalWrite(PIN_SHUT_REAR, HIGH);
  delay(20);
  sensorRear.setTimeout(500);
  if (!sensorRear.init()) {
    Serial.println("❌ ERROR: Rear ToF (A0) not detected.");
  } else {
    sensorRear.setAddress(ADDR_TOF_REAR);
    sensorRear.setDistanceMode(VL53L1X::Short);
    sensorRear.setMeasurementTimingBudget(50000);
    sensorRear.startContinuous(50);
    Serial.println("✅ Rear ToF ready (0x32)");
  }

  // 4. Configure FRONT ToF (A1 -> 0x33)
  digitalWrite(PIN_SHUT_FRONT, HIGH);
  delay(20);
  sensorFront.setTimeout(500);
  if (!sensorFront.init()) {
    Serial.println("❌ ERROR: Front ToF (A1) not detected.");
  } else {
    sensorFront.setAddress(ADDR_TOF_FRONT);
    sensorFront.setDistanceMode(VL53L1X::Short);
    sensorFront.setMeasurementTimingBudget(50000);
    sensorFront.startContinuous(50);
    Serial.println("✅ Front ToF ready (0x33)");
  }

  // 5. Configure BMI160 directly via registers
  if (!initBMI160()) {
    Serial.println("❌ ERROR: BMI160 not responding or wrong ID (expected 0xD1).");
    while (1);
  }
  Serial.println("✅ BMI160 ready (ID 0xD1 verified)");

  // Static offset calibration
  Serial.println("Calibrating gyroscope... KEEP SENSOR STILL.");
  long gzSum = 0;
  const int SAMPLES = 200;

  for (int i = 0; i < SAMPLES; i++) {
    gzSum += readBMIGyroZ();
    delay(10);
  }

  gz_offset = (gzSum / (float)SAMPLES) / 131.2;

  Serial.print("✅ Calibration complete. Offset: ");
  Serial.print(gz_offset, 3);
  Serial.println(" deg/s");
  Serial.println("\n>>> Press button on D10 to start reading sensors...");

  // Wait until the button on D10 is pressed (LOW)
  while (digitalRead(PIN_BUTTON) == HIGH) {
    delay(10);
  }
  delay(250); // Simple debounce

  // Reset Yaw and timer right when the button is pressed
  yaw = 0.0;
  previousTime = millis();
  Serial.println(">>> Starting sensor readings!\n");
}

void loop() {
  // Read distances from all 4 ToF sensors
  uint16_t distFront = sensorFront.read();
  uint16_t distLeft  = sensorLeft.read();
  uint16_t distRight = sensorRight.read();
  uint16_t distRear  = sensorRear.read();

  // Read BMI160 Z-axis directly
  int16_t gz_raw = readBMIGyroZ();
  float gz_dps = (gz_raw / 131.2) - gz_offset;

  // Calculate delta time (dt)
  unsigned long currentTime = millis();
  float dt = (currentTime - previousTime) / 1000.0;
  previousTime = currentTime;

  // Deadband filter against static drift
  if (abs(gz_dps) < 0.25) {
    gz_dps = 0.0;
  }

  // Integrate angular velocity to obtain Yaw angle
  yaw += gz_dps * dt;

  // Normalize angle between -180° and +180°
  if (yaw > 180.0) yaw -= 360.0;
  if (yaw < -180.0) yaw += 360.0;

  // Print results
  Serial.print("FRONT: ");
  if (sensorFront.timeoutOccurred()) Serial.print("ERR");
  else { Serial.print(distFront); Serial.print("mm"); }

  Serial.print(" | LEFT: ");
  if (sensorLeft.timeoutOccurred()) Serial.print("ERR");
  else { Serial.print(distLeft); Serial.print("mm"); }

  Serial.print(" | RIGHT: ");
  if (sensorRight.timeoutOccurred()) Serial.print("ERR");
  else { Serial.print(distRight); Serial.print("mm"); }

  Serial.print(" | REAR: ");
  if (sensorRear.timeoutOccurred()) Serial.print("ERR");
  else { Serial.print(distRear); Serial.print("mm"); }

  Serial.print(" | YAW: ");
  Serial.print(yaw, 1);
  Serial.println("°");

  delay(40);
}