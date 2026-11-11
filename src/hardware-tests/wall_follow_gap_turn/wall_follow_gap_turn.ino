// wall_follow_gap_turn.ino
// Hardware test for the Open Challenge (2 side TOF sensors + gyro + steering + drive motor).
// What it does:
//  1) Averages 10 readings of each side TOF; the NEAREST wall becomes the one to follow.
//  2) Follows that wall at the start distance (cascade: distance -> target heading -> gyro -> servo).
//  3) When EITHER side sensor sees a gap (reading far above its start distance, GAP_CONFIRM times
//     in a row) -> active brake (AIN1 = AIN2 = HIGH), wheels centered.
//  4) Turns 90 degrees toward the side where the gap appeared (PD on the gyro, brakes early) and stays still.
// The hardware part (pins, TOF init with XSHUT + unique I2C addresses, BMI160 init, servo, motor) is
// the one of test_completo_nano.ino, which works. The control parts come from the validated codes
// (wall_follow_nearest_gyro and turn_left_90_pid_brake).
// Board: Arduino Nano (classic, ATmega328P) | I2C: SDA=A4, SCL=A5
// TOF: 2 x VL53L1X (Pololu library) LEFT XSHUT A3 = 0x30, RIGHT XSHUT A2 = 0x31
// Gyro: BMI160 (0x68 or 0x69), registers | Servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8, STBY D5
// FIRST TEST: lift the car so the wheels do not touch the floor. Keep it STILL during the calibration.

#include <Wire.h>
#include <Servo.h>
#include <VL53L1X.h>

// ================= SETTINGS (change these) =================
// --- Steering ---
const int  CENTER_ANGLE     = 120;   // real center of the rack
const int  FULL_LEFT_ANGLE  = 50;    // servo angle with the wheels at full LEFT (smaller angle = left)
const int  FULL_RIGHT_ANGLE = 180;   // servo angle with the wheels at full RIGHT
const int  MAX_STEER_DEG    = 25;    // max steering away from center while following the wall
const int  SERVO_SPEED_DPS  = 300;   // max servo speed (degrees per second)
const int  SERVO_SETTLE_MS  = 400;   // time for the wheels to reach full lock before the turn

// --- Motor ---
const int  FOLLOW_SPEED   = 100;     // 0-255, speed while following the wall
const int  TURN_SPEED     = 100;     // speed during most of the turn
const int  SLOW_SPEED     = 75;      // speed near the end of the turn (raise it if the car stalls)
const bool MOTOR_REVERSED = false;   // true if the car drives backwards
const int  START_DELAY_MS = 2000;
const int  BRAKE_PAUSE_MS = 300;     // time braked before the turn starts

// --- PID: distance -> heading (outer loop) ---
const float K_DIST          = 0.08;  // degrees of heading per mm of distance error
const float KI_DIST         = 0.02;  // removes a constant offset from the wall distance
const float MAX_HEADING_DEG = 8;     // max angle the car may tilt toward/away from the wall

// --- PID: heading -> steering (inner loop, gyroscope) ---
const float K_HEADING = 2.0;   // steering degrees per degree of heading error
const float K_RATE    = 0.15;  // steering degrees per (deg/s) of turning speed (brake)

// --- GAP detection (either side) ---
const int  GAP_MARGIN_MM  = 500;    // gap = reading more than (start distance + this)
const int  ABS_GAP_MM     = 1200;   // used for a side that gave no valid start distance
const int  GAP_CONFIRM    = 3;      // readings in a row that must look like a gap
const bool INVALID_AS_GAP = true;   // an invalid reading (nothing in range) counts as a gap,
                                    // only for a side that had a valid start distance

// --- 90 degree turn (PD on the yaw) ---
const float TURN_ANGLE_DEG = 90.0;
const float K_TURN         = 2.5;    // steering degrees per degree left to turn
const float MIN_STEER_DEG  = 8.0;    // always steer at least this much until the stop
const float SLOW_ZONE_DEG  = 25.0;   // last degrees done at SLOW_SPEED
const float STOP_LEAD_DEG  = 2.0;    // brake this many degrees BEFORE 90 (inertia)
const unsigned long TURN_TIMEOUT_MS = 8000;   // safety

// --- Gyro ---
const bool  YAW_REVERSED = false;  // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;   // fine correction: real angle / measured angle

// --- TOF (same as test_completo_nano) ---
const uint8_t  ADDR_LEFT        = 0x30;
const uint8_t  ADDR_RIGHT       = 0x31;
const uint8_t  ROI_WIDTH        = 4;
const uint8_t  ROI_HEIGHT       = 13;
const uint32_t TIMING_BUDGET_US = 100000;
const int      PERIOD_MS        = 100;
const int      MAX_VALID_MM     = 4000;
const int      REF_SAMPLES      = 10;
const float    FILTER_ALPHA     = 0.6;   // distance filter (0-1, smaller = smoother)
// ===========================================================

// ---------------- Pins ----------------
const int PIN_STBY  = 5;
const int PIN_PWMA  = 6;
const int PIN_AIN1  = 7;
const int PIN_AIN2  = 8;
const int PIN_SERVO = 9;
const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;

// Sides (used as array indexes)
const int LEFT_SIDE  = 0;
const int RIGHT_SIDE = 1;

// ---------------- BMI160 ----------------
const float GYRO_LSB_PER_DPS = 65.6;   // +-500 dps
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

// ---------------- Objects ----------------
VL53L1X tof[2];
Servo steering;

// ---------------- State ----------------
bool tofOk[2]        = {false, false};
int  lastMm[2]       = {-1, -1};      // -1 = invalid
bool newReading[2]   = {false, false};
int  startMm[2]      = {-1, -1};
int  gapLimitMm[2]   = {0, 0};
int  openCount[2]    = {0, 0};        // readings in a row that look like a gap

int   wallSide;                       // LEFT_SIDE or RIGHT_SIDE
int   refDistance;                    // distance (mm) to keep
float filtDistance;
float distIntegral = 0;
float targetHeading = 0;              // degrees, + = LEFT

float yaw = 0;                        // degrees, + = car turned LEFT
float yawRate = 0;                    // deg/s, + = turning LEFT
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;
unsigned long lastDistMillis = 0;
unsigned long lastPrint = 0;
bool finished = false;

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(PIN_AIN1, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(PIN_AIN2, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PIN_PWMA, speed);
}

void motorStop() {               // free stop (coast)
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  analogWrite(PIN_PWMA, 0);
}

// Active brake (short brake): both inputs HIGH -> the car stays still
void motorBrake() {
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, HIGH);
  analogWrite(PIN_PWMA, 255);
}

void haltWithError(const __FlashStringHelper *msg) {
  motorStop();
  Serial.println(msg);
  while (true) delay(1000);
}

// ---------------- TOF VL53L1X (same as test_completo_nano) ----------------
// Turns one sensor on by XSHUT, gives it a new I2C address and configures it
bool initTof(VL53L1X &s, int xshut, uint8_t addr, const char *name) {
  digitalWrite(xshut, HIGH);   // only this sensor is on
  delay(20);
  s.setTimeout(500);
  if (!s.init()) {
    Serial.print(name);
    Serial.println(F(" NOT FOUND"));
    return false;
  }
  s.setAddress(addr);          // from here it answers at the new address
  s.setDistanceMode(VL53L1X::Long);
  s.setMeasurementTimingBudget(TIMING_BUDGET_US);
  s.setROISize(ROI_WIDTH, ROI_HEIGHT);
  s.startContinuous(PERIOD_MS);
  Serial.print(name);
  Serial.println(F(" OK"));
  return true;
}

// Checks the last measurement (call after read). Returns mm or -1.
int validMm(VL53L1X &s) {
  int mm = s.ranging_data.range_mm;
  VL53L1X::RangeStatus st = s.ranging_data.range_status;
  bool okStatus = (st == VL53L1X::RangeValid || st == VL53L1X::SignalFail);
  if (!okStatus || mm <= 0 || mm > MAX_VALID_MM) return -1;
  return mm;
}

// Average of REF_SAMPLES readings (car still). -1 if fewer than half were valid.
int averageMm(int side) {
  if (!tofOk[side]) return -1;
  long sum = 0;
  int count = 0;
  for (int i = 0; i < REF_SAMPLES; i++) {
    tof[side].read();                       // waits for a new measurement
    if (tof[side].timeoutOccurred()) continue;
    int mm = validMm(tof[side]);
    if (mm > 0) { sum += mm; count++; }
  }
  if (count < REF_SAMPLES / 2) return -1;
  return (int)(sum / count);
}

// Non-blocking: reads a sensor only when it has a new measurement
void readSensors() {
  for (int i = 0; i < 2; i++) {
    newReading[i] = false;
    if (!tofOk[i]) continue;
    if (tof[i].dataReady()) {
      tof[i].read(false);
      lastMm[i] = validMm(tof[i]);
      newReading[i] = true;
    }
  }
}

// ---------------- BMI160 (registers, same init as test_completo_nano) ----------------
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

bool initGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) {
    Serial.println(F("BMI160 NOT FOUND"));
    return false;
  }
  bmiWrite(0x7E, 0xB6); delay(100);   // soft reset
  bmiWrite(0x7E, 0x15); delay(100);   // gyro normal mode
  bmiWrite(0x42, 0x2A);               // gyro 400 Hz
  bmiWrite(0x43, 0x02);               // +-500 dps
  delay(50);

  float sum = 0;
  int count = 0;
  for (int i = 0; i < 400; i++) {
    float z = readGyroZ();
    if (!isnan(z)) { sum += z; count++; }
    delay(5);
  }
  gyroBias = (count > 0) ? sum / count : 0;
  Serial.println(F("BMI160 OK"));
  return true;
}

// Raw Z rotation speed in deg/s (no bias), or NAN if the read failed
float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;   // GYR_Z
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_LSB_PER_DPS;
  return YAW_REVERSED ? -dps : dps;
}

float updateGyro() {
  unsigned long nowUs = micros();
  float dt = (nowUs - prevMicros) / 1000000.0;
  prevMicros = nowUs;
  float z = readGyroZ();
  if (!isnan(z)) {
    yawRate = (z - gyroBias) * YAW_SCALE;
    yaw += yawRate * dt;
  }
  return dt;
}

// ---------------- Steering ----------------
// steerLeft: + = left, - = right (degrees away from center)
void setSteering(float steerLeft, float dt) {
  float target = CENTER_ANGLE - steerLeft;                    // smaller angle = left
  target = constrain(target, FULL_LEFT_ANGLE, FULL_RIGHT_ANGLE);

  float maxStep = SERVO_SPEED_DPS * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));
}

// ---------------- Wall following (cascade, same as wall_follow_nearest_gyro) ----------------
void updateWallPid() {
  if (!newReading[wallSide]) return;

  int mm = lastMm[wallSide];
  // Far or invalid readings (a wall that is ending) never enter the filter
  if (mm > 0 && mm <= gapLimitMm[wallSide]) {
    filtDistance = FILTER_ALPHA * mm + (1 - FILTER_ALPHA) * filtDistance;
  }

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
  // Left wall: toward = LEFT = +heading. Right wall: toward = RIGHT = -heading.
  float wallSign = (wallSide == LEFT_SIDE) ? -1.0 : 1.0;
  float h = -wallSign * (K_DIST * error + KI_DIST * distIntegral);
  targetHeading = constrain(h, -MAX_HEADING_DEG, MAX_HEADING_DEG);
}

void updateFollowSteering(float dt) {
  float headingError = targetHeading - yaw;                    // + = must turn LEFT
  float steerLeft = K_HEADING * headingError - K_RATE * yawRate;
  steerLeft = constrain(steerLeft, -MAX_STEER_DEG, MAX_STEER_DEG);
  setSteering(steerLeft, dt);
}

// ---------------- Gap detection ----------------
// Returns LEFT_SIDE / RIGHT_SIDE when that side confirmed a gap, or -1
int checkGap() {
  for (int i = 0; i < 2; i++) {
    if (!newReading[i]) continue;
    int mm = lastMm[i];

    if (mm > gapLimitMm[i]) {
      openCount[i]++;                                 // far: looks like a gap
    } else if (mm < 0) {
      if (INVALID_AS_GAP && startMm[i] > 0) openCount[i]++;   // nothing in range: looks like a gap
      // otherwise an invalid reading is ignored
    } else {
      openCount[i] = 0;                               // wall seen: start again
    }
  }

  bool leftGap  = openCount[LEFT_SIDE]  >= GAP_CONFIRM;
  bool rightGap = openCount[RIGHT_SIDE] >= GAP_CONFIRM;
  if (!leftGap && !rightGap) return -1;
  if (leftGap && rightGap) return (openCount[LEFT_SIDE] > openCount[RIGHT_SIDE]) ? LEFT_SIDE : RIGHT_SIDE;
  return leftGap ? LEFT_SIDE : RIGHT_SIDE;
}

// ---------------- 90 degree turn (same as turn_left_90_pid_brake, both directions) ----------------
// dir: +1 = LEFT, -1 = RIGHT
void turn90(int dir) {
  float maxSteer = (dir > 0) ? abs(CENTER_ANGLE - FULL_LEFT_ANGLE) : abs(FULL_RIGHT_ANGLE - CENTER_ANGLE);

  // 1) Wheels to full lock (car still stopped)
  servoAngle = (dir > 0) ? FULL_LEFT_ANGLE : FULL_RIGHT_ANGLE;
  steering.write((int)servoAngle);
  delay(SERVO_SETTLE_MS);

  // 2) Motor forward, PD on the yaw
  yaw = 0;
  yawRate = 0;
  prevMicros = micros();
  unsigned long startMs = millis();
  unsigned long lastTurnPrint = 0;
  motorForward(TURN_SPEED);

  while (true) {
    float dt = updateGyro();
    float turned = dir * yaw;                          // degrees turned toward the gap side
    float error = TURN_ANGLE_DEG - turned;             // degrees still to turn

    // Close enough: brake (inertia does the last STOP_LEAD_DEG)
    if (error <= STOP_LEAD_DEG) break;

    if (millis() - startMs > TURN_TIMEOUT_MS) {
      Serial.println(F("TIMEOUT: turn took too long (check YAW_REVERSED)"));
      break;
    }

    // PD steering, at least MIN_STEER_DEG toward the gap side
    float steer = K_TURN * error - K_RATE * dir * yawRate;
    steer = constrain(steer, MIN_STEER_DEG, maxSteer);
    setSteering(dir * steer, dt);

    // Slower near the end
    motorForward(error < SLOW_ZONE_DEG ? SLOW_SPEED : TURN_SPEED);

    if (millis() - lastTurnPrint >= 100) {
      lastTurnPrint = millis();
      Serial.print(F("yaw: "));
      Serial.print(yaw, 1);
      Serial.print(F(" | error: "));
      Serial.print(error, 1);
      Serial.print(F(" | servo: "));
      Serial.println((int)(servoAngle + 0.5));
    }
  }

  // 3) Brake and wheels straight at the same time
  motorBrake();
  servoAngle = CENTER_ANGLE;
  steering.write(CENTER_ANGLE);

  // Measure the final angle after the car fully stops
  unsigned long t = millis();
  while (millis() - t < 700) updateGyro();
  Serial.print(F("DONE. Final yaw: "));
  Serial.print(yaw, 1);
  Serial.print(F(" | difference: "));
  Serial.println(dir * yaw - TURN_ANGLE_DEG, 1);
}

// Gap confirmed: brake, then turn toward that side
void handleGap(int side) {
  motorBrake();
  servoAngle = CENTER_ANGLE;
  steering.write(CENTER_ANGLE);

  Serial.print(F("GAP: "));
  Serial.print(side == LEFT_SIDE ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" read "));
  if (lastMm[side] < 0) Serial.print(F("---")); else Serial.print(lastMm[side] / 10.0, 1);
  Serial.print(F(" cm (limit "));
  Serial.print(gapLimitMm[side] / 10.0, 1);
  Serial.println(F(" cm) -> braked, turning to that side"));

  delay(BRAKE_PAUSE_MS);                  // let the car stop completely
  turn90(side == LEFT_SIDE ? +1 : -1);
  finished = true;
}

// ---------------- Print 10 times per second ----------------
void printStatus() {
  if (millis() - lastPrint < 100) return;
  lastPrint = millis();
  Serial.print(F("L: "));
  if (lastMm[LEFT_SIDE] < 0) Serial.print(F("---")); else Serial.print(lastMm[LEFT_SIDE] / 10.0, 1);
  Serial.print(F(" ("));
  Serial.print(openCount[LEFT_SIDE]);
  Serial.print(F(") | R: "));
  if (lastMm[RIGHT_SIDE] < 0) Serial.print(F("---")); else Serial.print(lastMm[RIGHT_SIDE] / 10.0, 1);
  Serial.print(F(" ("));
  Serial.print(openCount[RIGHT_SIDE]);
  Serial.print(F(") | wall: "));
  Serial.print(wallSide == LEFT_SIDE ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" | yaw: "));
  Serial.print(yaw, 1);
  Serial.print(F(" | servo: "));
  Serial.println((int)(servoAngle + 0.5));
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Motor
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);        // takes the TB6612 out of standby
  motorStop();                         // motor stopped while calibrating

  // Servo
  steering.attach(PIN_SERVO, 500, 2500);
  steering.write(CENTER_ANGLE);

  // I2C (on the Nano, A4/A5 are SDA/SCL automatically)
  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);    // I2C timeout: 25 ms, so a stuck bus cannot freeze the car

  // Sensors: turn both off and turn them on one by one
  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(20);
  tofOk[LEFT_SIDE]  = initTof(tof[LEFT_SIDE],  XSHUT_LEFT,  ADDR_LEFT,  "LEFT");
  tofOk[RIGHT_SIDE] = initTof(tof[RIGHT_SIDE], XSHUT_RIGHT, ADDR_RIGHT, "RIGHT");
  if (!tofOk[LEFT_SIDE] && !tofOk[RIGHT_SIDE]) haltWithError(F("ERROR: no TOF found"));

  // Gyro (the car must be STILL here)
  Serial.println(F("Calibrating gyro: DO NOT MOVE THE CAR"));
  if (!initGyro()) haltWithError(F("ERROR: BMI160 not found"));

  // Start distances: the nearest wall becomes the one to follow
  startMm[LEFT_SIDE]  = averageMm(LEFT_SIDE);
  startMm[RIGHT_SIDE] = averageMm(RIGHT_SIDE);
  bool leftValid  = startMm[LEFT_SIDE]  > 0;
  bool rightValid = startMm[RIGHT_SIDE] > 0;
  if (!leftValid && !rightValid) haltWithError(F("ERROR: no wall seen on either side"));

  if (!rightValid || (leftValid && startMm[LEFT_SIDE] <= startMm[RIGHT_SIDE])) wallSide = LEFT_SIDE;
  else wallSide = RIGHT_SIDE;
  refDistance = startMm[wallSide];
  filtDistance = refDistance;

  for (int i = 0; i < 2; i++) {
    gapLimitMm[i] = (startMm[i] > 0) ? startMm[i] + GAP_MARGIN_MM : ABS_GAP_MM;
  }

  Serial.print(F("Following "));
  Serial.print(wallSide == LEFT_SIDE ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" wall at "));
  Serial.print(refDistance / 10.0, 1);
  Serial.println(F(" cm"));
  Serial.print(F("Gap limits: LEFT "));
  Serial.print(gapLimitMm[LEFT_SIDE] / 10.0, 1);
  Serial.print(F(" cm | RIGHT "));
  Serial.print(gapLimitMm[RIGHT_SIDE] / 10.0, 1);
  Serial.println(F(" cm"));
  Serial.println(F("Ready"));

  delay(START_DELAY_MS);
  yaw = 0;                       // "straight" = the direction the car points now
  motorForward(FOLLOW_SPEED);
  prevMicros = micros();
  lastDistMillis = millis();
}

// ---------------- LOOP ----------------
void loop() {
  if (finished) {
    motorBrake();                // stays braked. Press RESET to repeat.
    return;
  }

  float dt = updateGyro();
  readSensors();

  int gapSide = checkGap();
  if (gapSide >= 0) {
    handleGap(gapSide);
    return;
  }

  updateWallPid();
  updateFollowSteering(dt);
  printStatus();
}
