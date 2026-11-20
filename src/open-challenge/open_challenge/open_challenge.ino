// open_challenge.ino (Open Challenge program)
// FULL LAP(S) AND STOP WHERE IT STARTED: drive straight with the gyro, turn 90 deg at every corner
// (4 per lap), and after the last turn stop at the START POSITION.
//
//  HOW IT FINDS THE START POSITION (no rear sensor needed):
//   - D1 = distance from the start to corner 1 detection (end of the start straight's inner wall).
//   - LAPS >= 2 (recommended, exact): on lap 2 the car drives the WHOLE start straight, so it measures
//     LSTART = distance from the end of turn 4 to corner 5 detection on that same straight.
//     After the last turn it drives LSTART - D1 from the end of the turn and brakes = start position.
//     (Same corners, same turns, same straight: no assumption about the track shape.)
//   - LAPS = 1: estimate with the OPPOSITE wall (WALL = straight 3, from "inner wall seen" to corner 3):
//     after the last turn, when it sees the inner wall it drives WALL - D1 more. Less exact (~36 cm off
//     in the first test), because the opposite wall is not always the same length.
//   - If nothing could be measured, it stops FALLBACK_CM after the inner wall is seen.
//   - Safety: if it detects the next corner before that, it stops at once.
//  CORRIDOR CENTER: on the straights, when BOTH side walls are seen, a small heading change
//   (K_LAT, max LAT_MAX_DEG) moves the car to the middle of the corridor (600 and 1000 mm corridors).
//
//  - Straight: gyro heading hold with an INTEGRAL term (learns the real straight servo angle).
//  - Corner 1: the first side with a strong jump is the INNER side (LEFT = counter-clockwise,
//    RIGHT = clockwise). Only that side is watched for the next corners.
//  - Jump: JUMP_DELTA_MM above that side's normal distance OR above JUMP_ABS_MM, JUMP_CONFIRM in a row.
//  - Turn: full steering to the inner side until the PREDICTED yaw (yaw + rate x TURN_PREDICT_S)
//    reaches the new heading (old heading +/- 90). Then straight again (no brake) on the new heading.
//  - No false corners: after a turn, jumps are ignored for IGNORE_AFTER_TURN_CM, and the inner
//    wall must be seen again (closer than ARM_WALL_MM, ARM_READINGS in a row) before the next corner.
//  - Safety: STALLED (wheels blocked) or no corner in MAX_STRAIGHT_MS -> stop.
//
// Put the car in the MIDDLE of a straight, centered and aligned with the walls.
// Press START (D10) or g + Enter. Do NOT touch it while the LED blinks (gyro calibration).
// s + Enter = emergency stop. Battery ON FIRST, then USB.
// Board: Arduino Nano | TB6612: PWMA D6, AIN1 D7, AIN2 D8 | Encoder C1 D3, C2 D2 | Servo D9 | START D10
// BMI160 0x69 (A4/A5) | TOF left XSHUT A3 -> 0x2A | TOF right XSHUT A2 -> 0x2B | LED D13
// TOF front XSHUT A1 + TOF rear XSHUT A0: SOLDERED BUT OFF (XSHUT held LOW the whole time, never
//   started, never read). They stay off the I2C bus so they do not clash with left/right at 0x29.
// Libraries: VL53L1X by Pololu, Servo

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// #####################################################################################
// ##  1. RUN SETTINGS  - change these freely for the laps                            ##
// #####################################################################################
// --- Race ---
const int   LAPS     = 3;              // 1 = one lap (4 turns), 3 = the full Open Challenge (12 turns)
const int   RUN_PWM  = 150;            // speed on the straights (0-255). Tested: 110. Raise in steps of ~20
const int   TURN_PWM = 90;            // speed in the corners (90 was too weak to turn)
const float STOP_OFFSET_CM = 0;        // final stop: + = stop later (further), - = stop earlier

// --- Where the car drives in the corridor (lane) ---
const int   LANE_MODE = 1;             // 0 = CENTER of the corridor (uses both walls)
                                       // 1 = fixed distance to the INNER wall (INNER_TARGET_MM)
const int   INNER_TARGET_MM = 100;     // LANE_MODE 1: distance to keep from the inner wall (mm)
                                       //   smaller = closer to the inner wall. Do not go below ~200:
                                       //   in the turn the car could touch the inner corner
const int   CENTER_SHIFT_MM = 200;       // LANE_MODE 0: move the "center" toward the INNER wall by this
                                       //   many mm (0 = exact center, 100 = 10 cm closer to the inner wall)
const int   SAFE_WALL_MM = 70;        // any wall closer than this: steer away at full correction

// --- Corners ---
const int   JUMP_DELTA_MM = 250;       // corner = inner reading this much above its normal distance...
const int   JUMP_ABS_MM   = 1200;       // ...or above this
const int   JUMP_CONFIRM  = 1;         // readings in a row (1 = faster reaction at high speed, more risk)
const float TURN_PREDICT_S = 0.12;     // ends the turn early by (turn rate x this). Overshoots 90 -> raise it
const float IGNORE_AFTER_TURN_CM = 60; // no corner detection right after a turn

// #####################################################################################
// ##  2. TUNING - change only if the car zig-zags, drifts or misses corners          ##
// #####################################################################################
const float KP_HEAD = 2.0;             // straight: servo degrees per degree of heading error (zig-zag -> lower)
const float KI_HEAD = 4.0;             // learns the real straight servo angle (the car pulled left at 120)
const float I_MAX   = 25;              // limit of that integral (servo degrees)
const float K_LAT       = 0.03;        // lane: heading degrees per mm of lateral error (100 mm -> 3 deg)
const float KI_LAT      = 0.02;        // lane integral: removes the slow gyro error
const float LAT_I_MAX   = 8;           // limit of the lane integral (degrees)
const float LAT_MAX_DEG = 12;          // max heading change used to move in the corridor
const int   SIDE_MAX_MM = 1200;        // a side reading above this is not a corridor wall
const int   ARM_WALL_MM  = 900;        // after a turn, the inner wall must be seen closer than this...
const int   ARM_READINGS = 5;          // ...this many readings in a row before the next corner counts
const float TURN_LEAD_DEG  = 0;        // extra fixed margin to end the turn earlier (degrees)
const float MIN_DRIVE_CM   = 10;       // no corner detection in the first cm after START
const float FALLBACK_CM    = 50;       // final stop if nothing could be measured
const float MAX_FINAL_CM   = 250;      // safety: never drive more than this after the last turn
const unsigned long MAX_TURN_MS = 4000;       // a turn longer than this -> stop
const unsigned long MAX_STRAIGHT_MS = 8000;   // no corner in 8 s -> stop
const unsigned long STALL_MS = 500;           // motor ON but wheels not turning this long -> stop

// #####################################################################################
// ##  3. CAR CONSTANTS - measured on this car. Change only if the hardware changes   ##
// #####################################################################################
const bool  MOTOR_REVERSED   = true;   // motor wires not swapped yet (forward in code)
const bool  ENCODER_REVERSED = true;
const int   YAW_SIGN         = 1;      // +1: turning LEFT makes yaw go UP
const float GYRO_SCALE       = 1.007;  // gyro read ~0.7 % less than the real rotation (3-lap test)
const int   STEER_CENTER = 90, STEER_LEFT_MAX = 10, STEER_RIGHT_MAX = 170;   // LEFT = smaller angle
const int   RAMP_STEP = 5;             // PWM per 20 ms (soft start)
const float TICKS_PER_CM = 15.1;
const uint8_t BMI = 0x69;
const float GYRO_LSB_PER_DPS = 65.6;
const uint32_t TOF_BUDGET_US = 50000;
const int PWMA_PIN = 6, AIN1_PIN = 7, AIN2_PIN = 8;
const int ENC_C1_PIN = 3, ENC_C2_PIN = 2;
const int SERVO_PIN = 9, START_BTN_PIN = 10, LED_PIN = 13;
const int XSHUT_LEFT = A3, XSHUT_RIGHT = A2;
const uint8_t ADDR_LEFT = 0x2A, ADDR_RIGHT = 0x2B;
const int XSHUT_FRONT = A1, XSHUT_REAR = A0;   // front/rear TOF: kept OFF (XSHUT LOW). Change pins if wired elsewhere
// #####################################################################################

// Types first (before any function), so the Arduino IDE can use them in its prototypes.
struct Side {
  VL53L1X dev; int xshut; uint8_t addr; const char *name;
  int mm = -1; bool fresh = false; unsigned long lastData = 0;
  float normal = -1;          // average distance of this side while there is a wall
  int jumpCount = 0; int nearCount = 0;
};
enum State { WAIT, STRAIGHT, TURN, DONE };
const char *STATE_NAME[] = { "WAIT", "STRAIGHT", "TURN", "DONE" };

Side tofs[2];                 // [0] = LEFT, [1] = RIGHT
Servo steering;
State state = WAIT;
volatile long encRaw = 0;
int   pwmNow = 0, pwmTarget = 0, servoNow = STEER_CENTER, turnSide = -1;
float biasDps = 0, yawDeg = 0, rateDps = 0;
unsigned long lastMicros = 0, lastRamp = 0, lastPrint = 0, phaseStart = 0, doneAt = 0;
bool  tofOk = false, finalPrinted = true;
float iTerm = 0, stallOdo = 0; unsigned long lastCtrl = 0, stallSince = 0;
int   dir = 0, corners = 0;          // dir: +1 inner LEFT (counter-clockwise), -1 inner RIGHT (clockwise)
float heading = 0, turnEndCm = 0;     // heading = target, exact multiple of 90
float d1Cm = -1, armCm = -1, wallCm = -1, stopAtCm = -1, lStartCm = -1;   // start position
float latDeg = 0, latI = 0;           // corridor-center correction (+ = move left) and its integral
unsigned long runStart = 0;
bool  finalLeg = false;
char  cmd = 0;

// ---------------- motor / encoder / servo ----------------
void encoderISR() { if (digitalRead(ENC_C2_PIN)) encRaw++; else encRaw--; }
float odoCm() { noInterrupts(); long c = encRaw; interrupts(); return (ENCODER_REVERSED ? -c : c) / TICKS_PER_CM; }
void motorOut(int pwm) {
  pwm = constrain(pwm, 0, 255);
  bool a = pwm > 0, b = false;
  if (MOTOR_REVERSED) { b = a; a = false; }
  digitalWrite(AIN1_PIN, a ? HIGH : LOW); digitalWrite(AIN2_PIN, b ? HIGH : LOW);
  analogWrite(PWMA_PIN, pwm); pwmNow = pwm;
}
void brake() { digitalWrite(AIN1_PIN, HIGH); digitalWrite(AIN2_PIN, HIGH); analogWrite(PWMA_PIN, 255); pwmNow = 0; pwmTarget = 0; }
void servoTo(int a) { a = constrain(a, STEER_LEFT_MAX, STEER_RIGHT_MAX); if (a != servoNow) { steering.write(a); servoNow = a; } }

// ---------------- BMI160 ----------------
bool i2cTimedOut() { if (Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); return true; } return false; }
bool bmiWrite(uint8_t reg, uint8_t val) { Wire.beginTransmission(BMI); Wire.write(reg); Wire.write(val); return Wire.endTransmission() == 0 && !i2cTimedOut(); }
bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(BMI); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) { i2cTimedOut(); return false; }
  if (Wire.requestFrom(BMI, n) != n) { i2cTimedOut(); return false; }
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return !i2cTimedOut();
}
bool readGyroZ(int16_t &raw) { uint8_t b[2]; if (!bmiRead(0x10, b, 2)) return false; raw = (int16_t)((b[1] << 8) | b[0]); return true; }
bool bmiStart() {
  for (int i = 0; i < 5; i++) {
    bmiWrite(0x7E, 0xB6); delay(100); bmiWrite(0x7E, 0x15); delay(100); bmiWrite(0x43, 0x02); delay(50);
    uint8_t id = 0, st = 0;
    if (bmiRead(0x00, &id, 1) && id == 0xD1 && bmiRead(0x03, &st, 1) && ((st >> 2) & 3) == 1) return true;
  }
  return false;
}
void calibrate() {
  long sum = 0; int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 700) { int16_t r; readGyroZ(r); delay(5); }
  for (int i = 0; i < 300; i++) { int16_t r; if (readGyroZ(r)) { sum += r; n++; } delay(5); digitalWrite(LED_PIN, (millis() / 80) % 2); }
  if (n >= 250) biasDps = (sum / (float)n) / GYRO_LSB_PER_DPS;
  yawDeg = 0; lastMicros = micros();
  Serial.print(F("Gyro bias ")); Serial.print(biasDps, 3); Serial.print(F(" (")); Serial.print(n); Serial.println(F("/300)"));
  digitalWrite(LED_PIN, HIGH);
}
void updateGyro() {
  unsigned long now = micros(); float dt = (now - lastMicros) / 1000000.0; lastMicros = now;
  int16_t raw;
  if (readGyroZ(raw)) { rateDps = YAW_SIGN * GYRO_SCALE * (raw / GYRO_LSB_PER_DPS - biasDps); yawDeg += rateDps * dt; }
}

// ---------------- both TOF ----------------
bool startOne(Side &t) {
  pinMode(t.xshut, INPUT); delay(10);
  t.dev.setAddress(0x29); t.dev.setTimeout(200);
  if (!t.dev.init()) { i2cTimedOut(); return false; }
  t.dev.setAddress(t.addr);
  t.dev.setDistanceMode(VL53L1X::Long);
  t.dev.setMeasurementTimingBudget(TOF_BUDGET_US);
  t.dev.setROISize(16, 8);
  t.dev.startContinuous(TOF_BUDGET_US / 1000);
  t.lastData = millis();
  return true;
}
// Front and rear TOF are not used: hold their XSHUT LOW so they stay in reset (off the I2C bus).
// If XSHUT were left floating they would wake up at 0x29 and break the left/right TOF start.
void tofExtraOff() {
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);
}
bool tofStart() {
  tofExtraOff();                                  // keep front/rear OFF (also on a TOF restart)
  for (int i = 0; i < 2; i++) { pinMode(tofs[i].xshut, OUTPUT); digitalWrite(tofs[i].xshut, LOW); }
  delay(10);
  bool a = startOne(tofs[0]); bool b = startOne(tofs[1]);
  return a && b;
}
void updateTof() {
  bool restart = false;
  for (int i = 0; i < 2; i++) {
    Side &t = tofs[i]; t.fresh = false;
    if (tofOk && t.dev.dataReady()) {
      t.dev.read(false);
      if (!i2cTimedOut()) {
        uint16_t v = t.dev.ranging_data.range_mm;
        t.mm = (v > 0 && v < 4000) ? v : 4000;    // 0 or >= 4000 = nothing in range = far
        t.fresh = true; t.lastData = millis();
      }
    }
    if (millis() - t.lastData > 500) restart = true;
  }
  if (restart) {
    tofOk = tofStart();
    Serial.print(F("!!! TOF RESTART -> ")); Serial.println(tofOk ? F("OK") : F("FAILED"));
    tofs[0].lastData = tofs[1].lastData = millis();
  }
}

// ---------------- test ----------------
void finish(const __FlashStringHelper *why) {
  brake(); servoTo(STEER_CENTER); state = DONE; doneAt = millis(); finalPrinted = false;
  Serial.print(F(">>> ")); Serial.print(why);
  Serial.print(F(" | yaw ")); Serial.print(yawDeg, 1);
  Serial.print(F(" | odo ")); Serial.print(odoCm(), 1); Serial.println(F(" cm"));
}
void startRun() {
  Serial.println(F("Calibrating gyro: do NOT touch the car..."));
  servoTo(STEER_CENTER); delay(300);
  calibrate();
  noInterrupts(); encRaw = 0; interrupts();
  for (int i = 0; i < 2; i++) { tofs[i].normal = -1; tofs[i].jumpCount = 0; tofs[i].nearCount = 0; }
  turnSide = -1; state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM;
  dir = 0; corners = 0; heading = 0; turnEndCm = -1000;
  d1Cm = -1; armCm = -1; wallCm = -1; stopAtCm = -1; lStartCm = -1; finalLeg = false; latDeg = 0; latI = 0; runStart = millis();
  iTerm = 0; lastCtrl = millis(); stallOdo = 0; stallSince = millis();
  Serial.println(F(">>> GO"));
}

void runTest() {
  if (state == STRAIGHT) {
    // corridor center: only with both walls learned (never right after a turn) and both close enough
    bool okL = tofs[0].normal > 0 && tofs[0].mm < SIDE_MAX_MM, okR = tofs[1].normal > 0 && tofs[1].mm < SIDE_MAX_MM;
    float dt = (millis() - lastCtrl) / 1000.0; lastCtrl = millis();
    // lateral error in mm: + = the car is RIGHT of where it should be (it must move left)
    bool haveLane = false; float latErrMm = 0;
    int in = dir > 0 ? 0 : 1;                                      // inner side (valid once dir != 0)
    bool okIn = (in == 0) ? okL : okR;
    if (LANE_MODE == 1 && dir != 0 && okIn) {                      // fixed distance to the inner wall
      latErrMm = (in == 0) ? (tofs[0].mm - INNER_TARGET_MM) : (INNER_TARGET_MM - tofs[1].mm);
      haveLane = true;
    } else if (okL && okR) {                                       // center (shifted toward the inner wall)
      latErrMm = (tofs[0].mm - tofs[1].mm) / 2.0 + dir * CENTER_SHIFT_MM;
      haveLane = true;
    }
    if (haveLane) {
      latI = constrain(latI + KI_LAT * latErrMm * dt, -LAT_I_MAX, LAT_I_MAX);   // kept between straights
      latDeg = constrain(K_LAT * latErrMm + latI, -LAT_MAX_DEG, LAT_MAX_DEG);
    } else latDeg = latI;                                           // no usable wall: keep the learned bias
    if (tofs[0].mm > 0 && tofs[0].mm < SAFE_WALL_MM) latDeg = -LAT_MAX_DEG;    // too close to the LEFT wall: go right
    if (tofs[1].mm > 0 && tofs[1].mm < SAFE_WALL_MM) latDeg =  LAT_MAX_DEG;    // too close to the RIGHT wall: go left
    float err = heading + latDeg - yawDeg;
    if (pwmNow >= RUN_PWM) iTerm = constrain(iTerm + KI_HEAD * err * dt, -I_MAX, I_MAX);   // learn only at speed
    servoTo(STEER_CENTER - (int)(KP_HEAD * err + iTerm));
    float odo = odoCm();
    for (int i = 0; i < 2; i++) {
      Side &t = tofs[i];
      if (!t.fresh) continue;
      if (t.normal < 0) {                                       // learn the wall only after ARM_READINGS close readings
        t.nearCount = (t.mm < ARM_WALL_MM) ? t.nearCount + 1 : 0;
        if (t.nearCount >= ARM_READINGS) {
          t.normal = t.mm;
          if (dir != 0 && i == (dir > 0 ? 0 : 1)) {           // the INNER wall starts here
            armCm = odo;
            Serial.print(F(">>> inner wall seen at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
            if (finalLeg && stopAtCm < 0) {
              float d = (wallCm > 0 && d1Cm >= 0) ? max(wallCm - d1Cm, 0.0f) : FALLBACK_CM;
              stopAtCm = odo + d;
              Serial.print(F(">>> FINAL: drive ")); Serial.print(d, 1); Serial.print(F(" cm more (WALL "));
              Serial.print(wallCm, 1); Serial.print(F(" - D1 ")); Serial.print(d1Cm, 1); Serial.println(F(")"));
            }
          }
        }
        continue;
      }
      if (t.mm > t.normal + JUMP_DELTA_MM || t.mm > JUMP_ABS_MM) t.jumpCount++;
      else { t.jumpCount = 0; t.normal = 0.8 * t.normal + 0.2 * t.mm; }          // average only while there is a wall
    }
    int side = -1;
    if (odo >= MIN_DRIVE_CM) {
      if (dir == 0) {                                   // corner 1: the side that opens is the inner side
        for (int i = 0; i < 2; i++) if (tofs[i].jumpCount >= JUMP_CONFIRM && tofs[1 - i].jumpCount == 0) side = i;
      } else {                                          // next corners: only the inner side, once armed
        int in = dir > 0 ? 0 : 1;
        if (odo - turnEndCm >= IGNORE_AFTER_TURN_CM && tofs[in].normal > 0 && tofs[in].jumpCount >= JUMP_CONFIRM) side = in;
      }
    }
    if (side >= 0) {
      Side &t = tofs[side];
      if (dir == 0) dir = (side == 0) ? 1 : -1;
      Serial.print(F(">>> CORNER ")); Serial.print(corners + 1); Serial.print(F(" on ")); Serial.print(t.name);
      Serial.print(F(": normal ")); Serial.print((int)t.normal); Serial.print(F(" mm -> ")); Serial.print(t.mm);
      Serial.print(F(" mm at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
      if (corners == 0) { d1Cm = odo; Serial.print(F(">>> D1 = ")); Serial.print(d1Cm, 1); Serial.println(F(" cm")); }
      if (corners == 2 && armCm > 0) { wallCm = odo - armCm; Serial.print(F(">>> WALL = ")); Serial.print(wallCm, 1); Serial.println(F(" cm")); }
      if (corners == 4) { lStartCm = odo - turnEndCm; Serial.print(F(">>> LSTART = ")); Serial.print(lStartCm, 1); Serial.println(F(" cm (turn 4 end -> corner 5)")); }
      if (finalLeg) finish(F("SAFETY: next corner reached before the start position -> STOP"));
      else { turnSide = side; state = TURN; phaseStart = millis(); pwmTarget = TURN_PWM; }
    }
    if (state == STRAIGHT && finalLeg) {
      if (stopAtCm > 0 && odo >= stopAtCm) finish(F("BACK AT THE START POSITION -> STOP"));
      else if (odo - turnEndCm > MAX_FINAL_CM) finish(F("SAFETY: final distance too long -> STOP"));
    }
    if (state == STRAIGHT && millis() - phaseStart > MAX_STRAIGHT_MS) finish(F("NO CORNER in 8 s -> STOP"));
  } else if (state == TURN) {
    servoTo(dir > 0 ? STEER_LEFT_MAX : STEER_RIGHT_MAX);
    float predicted = dir * (yawDeg + rateDps * TURN_PREDICT_S - heading);   // degrees turned at the end
    if (predicted >= 90 - TURN_LEAD_DEG) {
      heading += dir * 90.0; corners++; turnEndCm = odoCm(); armCm = -1;
      if (corners >= 4 * LAPS) {
        finalLeg = true;
        if (lStartCm > 0 && d1Cm >= 0) {                  // LAPS >= 2: exact, measured on the start straight
          float d = max(lStartCm - d1Cm + STOP_OFFSET_CM, 0.0f);
          stopAtCm = turnEndCm + d;
          Serial.print(F(">>> FINAL: drive ")); Serial.print(d, 1); Serial.print(F(" cm from the turn end (LSTART "));
          Serial.print(lStartCm, 1); Serial.print(F(" - D1 ")); Serial.print(d1Cm, 1); Serial.println(F(")"));
        }
      }
      for (int i = 0; i < 2; i++) { tofs[i].normal = -1; tofs[i].jumpCount = 0; tofs[i].nearCount = 0; }
      state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
      Serial.print(F(">>> TURN ")); Serial.print(corners); Serial.print(F(" done at rate ")); Serial.print(rateDps, 0);
      Serial.print(F(" deg/s, yaw ")); Serial.print(yawDeg, 1); Serial.print(F(" -> new heading ")); Serial.println(heading, 0);
    }
    else if (millis() - phaseStart > MAX_TURN_MS) finish(F("TURN TOO LONG -> STOP"));
  }
  if (state == STRAIGHT || state == TURN) {                       // stall: wheels blocked
    float o = odoCm();
    if (fabs(o - stallOdo) > 1.0 || pwmNow < 60) { stallOdo = o; stallSince = millis(); }
    else if (millis() - stallSince > STALL_MS) finish(F("STALLED: wheels not turning (hit a wall?) -> STOP"));
  }
  if (state == DONE && !finalPrinted && millis() - doneAt > 1000) {
    finalPrinted = true; motorOut(0);
    Serial.print(F(">>> FINAL yaw after 1 s: ")); Serial.print(yawDeg, 1);
    Serial.print(F(" | heading ")); Serial.print(heading, 0);
    Serial.print(F(" | error ")); Serial.print(yawDeg - heading, 1);
    Serial.print(F(" | corners turned ")); Serial.print(corners);
    Serial.print(F(" | D1 ")); Serial.print(d1Cm, 1); Serial.print(F(" | WALL ")); Serial.print(wallCm, 1);
    Serial.print(F(" | LSTART ")); Serial.print(lStartCm, 1);
    Serial.print(F(" | time ")); Serial.print((doneAt - runStart) / 1000.0, 1); Serial.println(F(" s"));
  }
}

void setup() {
  pinMode(PWMA_PIN, OUTPUT); pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT); motorOut(0);
  tofExtraOff();                                  // front/rear TOF OFF before anything touches I2C
  pinMode(START_BTN_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_C1_PIN, INPUT_PULLUP); pinMode(ENC_C2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_C1_PIN), encoderISR, RISING);
  steering.attach(SERVO_PIN); steering.write(STEER_CENTER);
  Serial.begin(115200); delay(500);
  Serial.println(F("\nBOOT - LAP TEST (stop at the start)"));
  Wire.begin(); Wire.setClock(100000); Wire.setWireTimeout(25000, true);
  tofs[0].xshut = XSHUT_LEFT;  tofs[0].addr = ADDR_LEFT;  tofs[0].name = "LEFT";
  tofs[1].xshut = XSHUT_RIGHT; tofs[1].addr = ADDR_RIGHT; tofs[1].name = "RIGHT";
  for (int i = 0; i < 5 && !tofOk; i++) { tofOk = tofStart(); if (!tofOk) delay(300); }
  bool g = bmiStart();
  Serial.println(g ? F("BMI160 OK") : F("WARNING: BMI160 not ready (battery ON first?)"));
  Serial.println(tofOk ? F("Both TOF OK") : F("WARNING: a TOF is missing"));
  calibrate();
  Serial.println(F("Ready. Press START or g + Enter. s + Enter = stop."));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if ((cmd == 's' || cmd == 'S') && (state == STRAIGHT || state == TURN)) finish(F("STOPPED (s)"));
      else if ((cmd == 'g' || cmd == 'G') && (state == WAIT || state == DONE)) startRun();
      cmd = 0;
    } else cmd = c;
  }
  if ((state == WAIT || state == DONE) && digitalRead(START_BTN_PIN) == LOW) {
    delay(30);
    if (digitalRead(START_BTN_PIN) == LOW) { while (digitalRead(START_BTN_PIN) == LOW) delay(5); delay(300); startRun(); }
  }
  updateGyro();
  updateTof();
  runTest();

  unsigned long now = millis();
  if (now - lastRamp >= 20) {
    lastRamp = now;
    int target = (state == STRAIGHT || state == TURN) ? pwmTarget : 0;
    if (state == DONE) { /* keep the brake */ }
    else if (pwmNow < target) motorOut(min(pwmNow + RAMP_STEP, target));
    else if (pwmNow > target) motorOut(target);
  }
  if (now - lastPrint >= 100 && state != WAIT) {
    lastPrint = now;
    Serial.print(STATE_NAME[state]);
    Serial.print(F("\tc ")); Serial.print(corners);
    Serial.print(F("\thead ")); Serial.print(heading, 0);
    Serial.print(F("\todo ")); Serial.print(odoCm(), 1);
    Serial.print(F("\tyaw ")); Serial.print(yawDeg, 1);
    Serial.print(F("\tservo ")); Serial.print(servoNow);
    Serial.print(F("\tI ")); Serial.print(iTerm, 1);
    Serial.print(F("\tlat ")); Serial.print(latDeg, 1); Serial.print('/'); Serial.print(latI, 1);
    Serial.print(F("\tL ")); Serial.print(tofs[0].mm); Serial.print(F(" (")); Serial.print((int)tofs[0].normal); Serial.print(')');
    Serial.print(F("\tR ")); Serial.print(tofs[1].mm); Serial.print(F(" (")); Serial.print((int)tofs[1].normal); Serial.print(')');
    Serial.print(F("\tpwm ")); Serial.println(pwmNow);
  }
}