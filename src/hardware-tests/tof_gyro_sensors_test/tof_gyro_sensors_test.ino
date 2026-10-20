// ===== TOF400C (VL53L1X) x4 + BMI160 test - Arduino Nano (ATmega328P) =====
// Library: "VL53L1X" by Pololu (Library Manager). BMI160 is read directly (no library).
// Wiring: all SDA -> A4, all SCL -> A5 (TOFs + BMI160 on the same I2C bus)
// XSHUT: FRONT=A1, BACK=A0, RIGHT=A2, LEFT=A3  (LOW = off, HIGH = on)
// Output: distances in cm (1 decimal), gyro Z in deg/s, integrated yaw in degrees
// Keep the car STILL during the first seconds (gyro calibration)

#include <Wire.h>
#include <VL53L1X.h>

// ---------- PART 1: configuration ----------
const uint8_t NUM_SENSORS = 4;

enum SensorId { LEFT_S = 0, RIGHT_S = 1, BACK_S = 2, FRONT_S = 3 };

const char*   SENSOR_NAMES[NUM_SENSORS]  = { "LEFT", "RIGHT", "BACK", "FRONT" };
const uint8_t XSHUT_PINS[NUM_SENSORS]    = { A3,     A2,      A0,     A1      };
const uint8_t I2C_ADDRESSES[NUM_SENSORS] = { 0x30,   0x31,    0x32,   0x33    };

const uint32_t TIMING_BUDGET_US = 33000;  // time per measurement (us)
const uint16_t PERIOD_MS        = 40;     // continuous mode period (>= budget)

VL53L1X sensors[NUM_SENSORS];
bool sensorOk[NUM_SENSORS];

// BMI160
const uint8_t BMI160_ADDR        = 0x69;
const uint8_t BMI160_REG_CHIP_ID = 0x00;  // should read 0xD1
const uint8_t BMI160_REG_PMU     = 0x03;  // power mode status
const uint8_t BMI160_REG_GYRO_X  = 0x0C;  // gyro data starts here (X, Y, Z, little endian)
const uint8_t BMI160_REG_CMD     = 0x7E;
const uint8_t BMI160_CHIP_ID     = 0xD1;
const float   GYRO_LSB_PER_DPS   = 16.4;  // default range +-2000 deg/s
const uint16_t CALIB_SAMPLES     = 500;

bool imuOk = false;
float gyroZOffset = 0.0;   // raw units
float yaw = 0.0;           // degrees
unsigned long lastMicros = 0;

// ---------- PART 2: TOF setup ----------
void initSensors() {
  // 1) Turn all sensors OFF so none answers at the default address 0x29
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    pinMode(XSHUT_PINS[i], OUTPUT);
    digitalWrite(XSHUT_PINS[i], LOW);
  }
  delay(20);

  // 2) Turn them ON one by one and give each a unique address
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    digitalWrite(XSHUT_PINS[i], HIGH);
    delay(10);  // boot time

    sensors[i].setTimeout(500);
    if (!sensors[i].init()) {
      Serial.print(F("ERROR: failed to init "));
      Serial.println(SENSOR_NAMES[i]);
      sensorOk[i] = false;
      // Turn it back OFF so it doesn't block 0x29 for the next sensor
      digitalWrite(XSHUT_PINS[i], LOW);
      continue;
    }

    sensors[i].setAddress(I2C_ADDRESSES[i]);
    sensors[i].setDistanceMode(VL53L1X::Short);  // up to ~1.3 m, best vs ambient light
    sensors[i].setMeasurementTimingBudget(TIMING_BUDGET_US);
    sensors[i].startContinuous(PERIOD_MS);
    sensorOk[i] = true;

    Serial.print(F("OK: "));
    Serial.print(SENSOR_NAMES[i]);
    Serial.print(F(" -> 0x"));
    Serial.println(I2C_ADDRESSES[i], HEX);
  }
}

// ---------- PART 3: BMI160 setup ----------
void bmiWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BMI160_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t bmiRead(uint8_t reg) {
  Wire.beginTransmission(BMI160_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI160_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

int16_t readGyroZRaw() {
  Wire.beginTransmission(BMI160_ADDR);
  Wire.write(BMI160_REG_GYRO_X);
  Wire.endTransmission(false);
  Wire.requestFrom(BMI160_ADDR, (uint8_t)6);
  uint8_t data[6] = {0};
  for (uint8_t i = 0; i < 6 && Wire.available(); i++) data[i] = Wire.read();
  return (int16_t)((data[5] << 8) | data[4]);  // Z = bytes 4 (LSB) and 5 (MSB)
}

void initIMU() {
  uint8_t id = bmiRead(BMI160_REG_CHIP_ID);
  Serial.print(F("BMI160 chip ID: 0x"));
  Serial.println(id, HEX);
  if (id != BMI160_CHIP_ID) {
    Serial.println(F("ERROR: BMI160 not found (expected 0xD1)"));
    imuOk = false;
    return;
  }

  bmiWrite(BMI160_REG_CMD, 0x11);  // accelerometer -> normal mode
  delay(5);
  bmiWrite(BMI160_REG_CMD, 0x15);  // gyroscope -> normal mode
  delay(100);

  uint8_t pmu = bmiRead(BMI160_REG_PMU);
  Serial.print(F("BMI160 PMU status: 0x"));
  Serial.println(pmu, HEX);        // 0x14 = accel and gyro both in normal mode

  // Gyro Z offset calibration (car must be still)
  Serial.println(F("Calibrating gyro... keep the car still"));
  long sum = 0;
  for (uint16_t i = 0; i < CALIB_SAMPLES; i++) {
    sum += readGyroZRaw();
    delay(2);
  }
  gyroZOffset = (float)sum / CALIB_SAMPLES;
  Serial.print(F("Gyro Z offset (raw): "));
  Serial.println(gyroZOffset);

  imuOk = true;
  lastMicros = micros();
}

// ---------- PART 4: I2C scan + main loop ----------
void scanI2C() {
  Serial.println(F("I2C scan:"));
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  found 0x"));
      Serial.println(addr, HEX);  // expect 0x30-0x33 (TOF) and 0x69 (BMI160)
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();          // classic Nano: SDA=A4, SCL=A5 are fixed, no pin arguments
  Wire.setClock(400000); // fast I2C

  Serial.println(F("\n=== TOF400C x4 + BMI160 test ==="));
  initSensors();
  initIMU();
  scanI2C();
  Serial.println(F("Reading..."));
}

void loop() {
  // Format "NAME:value" also works with Tools > Serial Plotter
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial.print(SENSOR_NAMES[i]);
    Serial.print(':');

    if (!sensorOk[i]) {
      Serial.print(-1);  // sensor never initialized
    } else {
      uint16_t distanceMm = sensors[i].read();  // waits for new data (mm)
      if (sensors[i].timeoutOccurred()) {
        Serial.print(-2);  // timeout
      } else if (sensors[i].ranging_data.range_status != VL53L1X::RangeValid) {
        Serial.print(0);   // invalid reading (out of range, signal fail, etc.)
      } else {
        float distanceCm = distanceMm / 10.0;
        Serial.print(distanceCm, 1);  // 1 decimal, e.g. 34.5
      }
    }
    Serial.print(' ');
  }

  // Gyro Z rate and integrated yaw
  if (imuOk) {
    unsigned long now = micros();
    float dt = (now - lastMicros) / 1000000.0;  // seconds
    lastMicros = now;

    float gyroZ = (readGyroZRaw() - gyroZOffset) / GYRO_LSB_PER_DPS;  // deg/s
    yaw += gyroZ * dt;

    Serial.print(F("GZ:"));
    Serial.print(gyroZ, 1);
    Serial.print(F(" YAW:"));
    Serial.println(yaw, 1);
  } else {
    Serial.println(F("GZ:-1 YAW:-1"));
  }
}
