// wall_follow_nearest_gyro.ino
// At the start: takes 10 readings of each side sensor, picks the NEAREST wall
// and keeps that same distance all the time.
// Uses the BMI160 gyroscope to keep the car straight, so it does not snake:
//   1) distance error -> a SMALL target heading (max MAX_HEADING_DEG) toward/away from the wall
//   2) gyroscope      -> steers the car to that heading, smoothly
// Validated on the track on 20 October 2026: drives straight and smooth at MOTOR_SPEED 60.
// Board: Arduino Nano (classic, ATmega328P) | I2C: SDA=A4, SCL=A5
// Sensors: TOF400C (VL53L1X, Pololu library) LEFT (XSHUT A3), RIGHT (XSHUT A2)
// Gyro: BMI160 (0x68 or 0x69), read by registers (no library)
// Steering: servo D9 (pinion + rack) | Rear motor: TB6612FNG (PWMA D6, AIN1 D7, AIN2 D8, STBY D5)

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// ================= SETTINGS (change these) =================
// --- Steering ---
const int  CENTER_ANGLE         = 120;    // real center of the rack
const int  LEFT_LIMIT           = 180;
const int  RIGHT_LIMIT          = 50;
const bool STEER_LEFT_IS_BIGGER = false;  // false = matches INVERT_STEER = true that worked before
const int  MAX_STEER_DEG        = 25;     // max steering away from center
const int  SERVO_SPEED_DPS      = 250;    // max servo speed (degrees per second)

// --- Motor ---
const int  MOTOR_SPEED    = 60;     // 0-255
const bool MOTOR_REVERSED = true;
const int  START_DELAY_MS = 2000;

// --- Distance -> heading (outer loop) ---
const float K_DIST          = 0.08;  // degrees of heading per mm of distance error
const float KI_DIST         = 0.02;  // removes a constant offset from the wall distance
const float MAX_HEADING_DEG = 8;     // max angle the car may tilt toward/away from the wall

// --- Heading -> steering (inner loop, gyroscope) ---
const float K_HEADING = 2.0;   // steering degrees per degree of heading error
const float K_RATE    = 0.15;  // steering degrees per (deg/s) of turning speed (brake)
const bool  YAW_REVERSED = false;  // true if yaw goes DOWN when you turn the car LEFT by hand

// --- TOF ---
const uint8_t  ROI_WIDTH        = 1;
const uint8_t  ROI_HEIGHT       = 6;
const uint32_t TIMING_BUDGET_US = 33000;
const int      PERIOD_MS        = 33;
const int      MAX_VALID_MM     = 2000;
const int      REF_SAMPLES      = 10;
const float    FILTER_ALPHA     = 0.6;   // distance filter (0-1, smaller = smoother)
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN = 9;
const int PWMA_PIN  = 6;
const int AIN1_PIN  = 7;
const int AIN2_PIN  = 8;
const int STBY_PIN  = 5;

const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;
const int XSHUT_FRONT = A1;   // kept OFF
const int XSHUT_BACK  = A0;   // kept OFF

// ---------------- BMI160 ----------------
const float GYRO_SCALE = 65.6;   // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

// ---------------- Objects / state ----------------
VL53L1X tofLeft;
VL53L1X tofRight;
Servo steering;

int   wallSide;              // -1 = following LEFT wall, +1 = following RIGHT wall
int   refDistance;           // distance (mm) to keep
float filtDistance;          // filtered distance (mm)
float distIntegral = 0;
float yaw = 0;               // degrees, + = car turned LEFT since the start
float yawRate = 0;           // deg/s, + = turning LEFT
float targetHeading = 0;     // degrees, + = LEFT
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;
unsigned long lastDistMillis = 0;
unsigned long lastPrint = 0;

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

void haltWithError(const __FlashStringHelper *msg) {
  motorStop();
  Serial.println(msg);
  while (true) delay(1000);
}

// ---------------- BMI160 (registers) ----------------
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
  return bmiRead(0x00, &id, 1) && id == 0xD1;   // chip ID
}

bool startGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;
  bmiWrite(0x7E, 0x11); delay(50);    // accelerometer: normal mode
  bmiWrite(0x7E, 0x15); delay(100);   // gyroscope: normal mode
  bmiWrite(0x42, 0x28);               // gyro 100 Hz
  bmiWrite(0x43, 0x02);               // gyro range +-500 deg/s
  delay(50);
  return true;
}

// Raw Z rotation speed in deg/s (no bias), or NAN if the read failed
float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;   // GYR_Z
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_SCALE;
  return YAW_REVERSED ? -dps : dps;
}

void calibrateGyro() {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < 300; i++) {
    float z = readGyroZ();
    if (!isnan(z)) { sum += z; count++; }
    delay(5);
  }
  gyroBias = (count > 0) ? sum / count : 0;
}

// ---------------- TOF ----------------
bool startTof(VL53L1X &sensor, int xshutPin, uint8_t address) {
  digitalWrite(xshutPin, HIGH);
  delay(20);
  sensor.setTimeout(500);
  if (!sensor.init()) return false;
  sensor.setAddress(address);
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(TIMING_BUDGET_US);
  sensor.setROISize(ROI_WIDTH, ROI_HEIGHT);
  sensor.startContinuous(PERIOD_MS);
  return true;
}

// Checks the last measurement (call after read). Returns mm or -1.
int validMm(VL53L1X &sensor) {
  int mm = sensor.ranging_data.range_mm;
  VL53L1X::RangeStatus st = sensor.ranging_data.range_status;
  bool okStatus = (st == VL53L1X::RangeValid || st == VL53L1X::SignalFail);
  if (!okStatus || mm <= 0 || mm > MAX_VALID_MM) return -1;
  return mm;
}

int averageTof(VL53L1X &sensor) {
  long sum = 0;
  int count = 0;
  for (int i = 0; i < REF_SAMPLES; i++) {
    sensor.read();   // waits for a new measurement
    if (sensor.timeoutOccurred()) continue;
    int mm = validMm(sensor);
    if (mm > 0) { sum += mm; count++; }
  }
  return (count == 0) ? -1 : sum / count;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, HIGH);
  motorStop();

  pinMode(XSHUT_LEFT, OUTPUT);  digitalWrite(XSHUT_LEFT, LOW);
  pinMode(XSHUT_RIGHT, OUTPUT); digitalWrite(XSHUT_RIGHT, LOW);
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_BACK, OUTPUT);  digitalWrite(XSHUT_BACK, LOW);
  delay(20);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  if (!startGyro()) haltWithError(F("ERROR: BMI160 not found"));
  Serial.print(F("BMI160 OK at 0x"));
  Serial.println(bmiAddr, HEX);

  if (!startTof(tofLeft, XSHUT_LEFT, 0x30))   haltWithError(F("ERROR: LEFT TOF not found"));
  if (!startTof(tofRight, XSHUT_RIGHT, 0x31)) haltWithError(F("ERROR: RIGHT TOF not found"));
  Serial.println(F("TOF LEFT and RIGHT OK"));

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  // The car must be STILL here
  Serial.println(F("Calibrating gyro... do not move the car"));
  calibrateGyro();

  // 10 readings of each side -> the nearest wall becomes the reference
  int left  = averageTof(tofLeft);
  int right = averageTof(tofRight);
  if (left < 0 && right < 0) haltWithError(F("ERROR: no wall seen on either side"));

  if (right < 0 || (left > 0 && left <= right)) {
    wallSide = -1;
    refDistance = left;
  } else {
    wallSide = +1;
    refDistance = right;
  }
  filtDistance = refDistance;

  Serial.print(F("Following "));
  Serial.print(wallSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" wall at "));
  Serial.print(refDistance / 10.0, 1);
  Serial.println(F(" cm"));

  delay(START_DELAY_MS);
  yaw = 0;                       // "straight" = the direction the car points now
  motorForward(MOTOR_SPEED);
  prevMicros = micros();
  lastDistMillis = millis();
}

// ---------------- LOOP ----------------
void loop() {
  // ---- 1) Gyroscope: every loop (fast) ----
  unsigned long nowUs = micros();
  float dt = (nowUs - prevMicros) / 1000000.0;
  prevMicros = nowUs;

  float z = readGyroZ();
  if (!isnan(z)) {
    yawRate = z - gyroBias;
    yaw += yawRate * dt;
  }

  // ---- 2) Distance: only when the TOF has a new measurement ----
  VL53L1X &tof = (wallSide < 0) ? tofLeft : tofRight;
  if (tof.dataReady()) {
    tof.read(false);   // does not wait
    int mm = validMm(tof);
    if (mm > 0) filtDistance = FILTER_ALPHA * mm + (1 - FILTER_ALPHA) * filtDistance;

    unsigned long nowMs = millis();
    float dtDist = (nowMs - lastDistMillis) / 1000.0;
    lastDistMillis = nowMs;

    float error = filtDistance - refDistance;   // + = too far, - = too close

    // Integral, capped so it can add at most MAX_HEADING_DEG
    distIntegral += error * dtDist;
    if (KI_DIST > 0) {
      float maxI = MAX_HEADING_DEG / KI_DIST;
      distIntegral = constrain(distIntegral, -maxI, maxI);
    }

    // Too far -> point TOWARD the wall; too close -> point AWAY.
    // Left wall (wallSide = -1): toward = LEFT = +heading.
    float h = -wallSide * (K_DIST * error + KI_DIST * distIntegral);
    targetHeading = constrain(h, -MAX_HEADING_DEG, MAX_HEADING_DEG);
  }

  // ---- 3) Steering: follow the target heading with the gyroscope ----
  float headingError = targetHeading - yaw;                // + = must turn LEFT
  float steerLeft = K_HEADING * headingError - K_RATE * yawRate;
  steerLeft = constrain(steerLeft, -MAX_STEER_DEG, MAX_STEER_DEG);

  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);

  // Servo speed limit (smooth movement)
  float maxStep = SERVO_SPEED_DPS * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));

  // ---- 4) Print 10 times per second (printing every loop would slow it down) ----
  if (millis() - lastPrint >= 100) {
    lastPrint = millis();
    Serial.print(F("dist: "));
    Serial.print(filtDistance / 10.0, 1);
    Serial.print(F(" | ref: "));
    Serial.print(refDistance / 10.0, 1);
    Serial.print(F(" | target hdg: "));
    Serial.print(targetHeading, 1);
    Serial.print(F(" | yaw: "));
    Serial.print(yaw, 1);
    Serial.print(F(" | servo: "));
    Serial.println((int)(servoAngle + 0.5));
  }
}
