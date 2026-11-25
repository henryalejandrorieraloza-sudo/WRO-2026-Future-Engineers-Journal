// =====================================================================
// OBSTACLE CHALLENGE - Nano driver (v1)
// Parking exit + Open challenge + pillar avoidance + wall centering.
//
// Power on: calibrates everything (4 ToF sensors, gyro bias, encoder set to 0).
// The car does NOT move at power on.
// Press START (D10):
//   1) PARKING EXIT: turns 45 degrees with the gyro, then a PD counter-steer brings
//      the heading back to 0 degrees (parallel to the start), pauses, and reverses
//      PARK_REVERSE_TICKS while holding heading 0.
//   2) OPEN CHALLENGE: drives laps and uses the FIRST floor line the camera sees
//      (blue = turn left, orange = turn right). Each corner is a 90-degree turn
//      measured with the gyro. Between corners the car centers itself between
//      the two side ToF sensors. After a line it drives until the front ToF reads
//      FRONT_TURN_CM, brakes for BRAKE_S, then turns IN REVERSE. After the turn it
//      reverses until the rear ToF reads REAR_STOP_CM.
//      Lines seen within IGNORE_LINE_S after a corner are ignored.
//      After the last corner it drives FINAL_EXTRA_S past the last line and stops.
//   Pillars (camera): red pillars are passed on the right, green pillars on the left.
//   Priority on every straight section:
//     1) BLOCK: a pillar is on the map -> steer to avoid it.
//     2) PASS: a pillar was just passed -> hold the heading for PASS_CM cm.
//     3) CENTER: no pillar -> both side ToF sensors at the same distance.
//   The run has no USB/serial command input: the only way to start is the START button.
//   Camera: camera TX -> Nano RX (D0), 115200 baud. Camera messages start with '$'.
//   Board: Arduino Nano | TB6612: PWMA D6, AIN1 D7, AIN2 D8 | encoder C1 D3, C2 D2
//   Servo D9 | START D10 | LED D13 | BMI160 0x69 | left ToF A3 -> 0x2A | right ToF A2 -> 0x2B
//   rear ToF A0 -> 0x2C | front ToF A1 -> 0x2D
//   Libraries: VL53L1X (Pololu), Servo
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>
#include <math.h>

// #####################################################################
// ##  1. RUN SETTINGS                                            ##
// #####################################################################
const int   LAPS      = 3;            // 1 = one lap (4 corners), 3 = full Open challenge (12 corners)
const int   RUN_PWM   = 70;           // speed on straight sections (0-255)
const int   TURN_PWM  = 70;           // speed while turning (the turn is made in reverse)
const int   REVERSE_PWM = 70;         // speed when reversing after each turn
const float FRONT_TURN_CM = 40.0;     // after a line, drive until the front ToF reads this (cm) or less
const unsigned long MAX_PRE_TURN_MS = 4000;  // safety: if the front ToF never reaches FRONT_TURN_CM, stop
const float BRAKE_S = 0.5;            // brake this long with the wheels straight before turning in reverse (s)
const float REAR_STOP_CM = 20.0;      // reverse until the rear ToF reads this (cm) or less
const float REVERSE_CM  = 30.0;       // fallback: if the rear ToF does not respond, reverse this distance (cm)
const unsigned long MAX_REVERSE_MS = 3000;   // safety: if the reverse is not finished in this time, stop
const bool  ONLY_LOCKED_COLOR = true; // true: only count lines of the color seen first
const float LINE_WAIT_S = 0.75;       // fallback: without a front ToF, drive this long before turning
const float FINAL_EXTRA_S = 0.45;     // after the LAST line: drive this long, then stop

// #####################################################################
// ##  1b. PARKING (leaving the parking spot)                                  ##
// #####################################################################
const float PARK_FIRST_ANGLE_DEG  = 45.0;   // first turn toward the open side (degrees)
const float PARK_RETURN_TOL_DEG   = 1.0;    // counter-steer until within this many degrees of 0 (parallel to the start)
const long  PARK_STRAIGHT_TICKS   = 1;      // straight ticks before the counter-steer
const unsigned long PARK_PAUSE_MS = 500;    // stopped this long before reversing (= BRAKE_S)
const unsigned long PARK_TURN_TIMEOUT_MS = 5000;
const unsigned long PARK_STALL_MS = 1500;   // no encoder pulses = error
const int   PARK_DRIVE_PWM        = 90;     // speed of the first turn
const int   PARK_REVERSE_PWM      = 90;     // speed when reversing out of the parking spot
const int   PARK_REVERSE_SLOW_PWM = 75;     // last 20 % of the reverse move
const long  PARK_REVERSE_TICKS    = 1200;   // confirmed by the team on the car (24 Nov 2026). The 597 ticks calculated from 5 wheel turns (5 x pi x 4.1 cm x 9.27 ticks/cm) are not used

// #####################################################################
// ##  2. PILLARS                                                       ##
// #####################################################################
const float TARGET_CM        = 17.0;  // distance from pillar center to car center when passing it
const float MAX_AIM_DEG      = 60.0;  // maximum steering angle to avoid a pillar
const float LOOK_MIN_CM      = 12.0;  // minimum look-ahead distance
const float ARRIVE_BEFORE_CM = 55.0;  // be already on the safe side this distance before the pillar
const float RETURN_AFTER_CM  = 25.0;  // pillar counts as "passed" once the car is this far beyond it
const float PASS_CM          = 30.0;  // after losing the pillar, hold the heading this distance (> RETURN_AFTER)
const float SAME_PILLAR_CM   = 15.0;  // a detection this close to a stored pillar = the same pillar
const float BLEND            = 0.3;   // how much each message corrects the stored position
const float MAX_SIDE_CM      = 60.0;  // pillars farther to the side than this = noise
const float PILLAR_CM        = 5.0;   // pillar width
const float CAM_FWD_CM       = 11.7;  // rear axle (encoder) -> camera lens

// #####################################################################
// ##  3. FINE TUNING                                                 ##
// #####################################################################
const float KP_HEAD = 2.5;            // servo degrees per degree of heading error
const float KD_HEAD = 0.03;           // damping from the turn rate
const float KI_HEAD = 4.0;            // learns the real straight-ahead angle of the servo (only while centering)
const float I_MAX   = 25;
const float MAX_STEER = 70.0;         // maximum steering command (servo degrees)
const float K_LAT       = 0.03;       // heading degrees per mm of off-center error
const float KI_LAT      = 0.02;
const float LAT_I_MAX   = 8;
const float LAT_MAX_DEG = 12;
const int   SIDE_MAX_MM = 1200;       // a side reading larger than this = no usable wall
const int   SAFE_WALL_MM = 70;        // closer than this to a wall: steer fully away from it
const float TURN_PREDICT_S = 0.12;    // ends the turn early. If the car turns past 90 degrees, increase it
const float IGNORE_LINE_S = 6.0;      // after a line is detected, ignore ALL lines for this long (s)
const float MIN_DRIVE_CM = 10;        // ignore lines during the first cm of each segment
const unsigned long MAX_TURN_MS = 4000;
const unsigned long MAX_STRAIGHT_MS = 8000;   // no corner within 8 s -> stop
const unsigned long STALL_MS = 500;
const unsigned long CAMERA_TIMEOUT_MS = 500;  // no camera data -> do not drive

// #####################################################################
// ##  4. CAR CONSTANTS                                          ##
// #####################################################################
const bool  MOTOR_REVERSED   = true;
const bool  ENCODER_REVERSED = true;
const int   YAW_SIGN         = 1;     // +1: turning left increases the yaw
const float GYRO_SCALE       = 1.007;
const int   STEER_CENTER = 85, STEER_RANGE = 80, STEER_LEFT_MAX = STEER_CENTER - STEER_RANGE, STEER_RIGHT_MAX = STEER_CENTER + STEER_RANGE;   // wheels straight = 85; limits 85 +/- 80 = 5 / 165, same as open_challenge. left = smaller angle
const int   RAMP_STEP = 5;
const float TICKS_PER_CM = 9.27;
const uint8_t BMI = 0x69;
const float GYRO_LSB_PER_DPS = 65.6;
const uint32_t TOF_BUDGET_US = 50000;
const int PWMA_PIN = 6, AIN1_PIN = 7, AIN2_PIN = 8;
const int ENC_C1_PIN = 3, ENC_C2_PIN = 2;
const int SERVO_PIN = 9, START_BTN_PIN = 10, LED_PIN = 13;
const int XSHUT_LEFT = A3, XSHUT_RIGHT = A2;
const uint8_t ADDR_LEFT = 0x2A, ADDR_RIGHT = 0x2B;
const int XSHUT_FRONT = A1, XSHUT_REAR = A0;
const uint8_t ADDR_REAR = 0x2C, ADDR_FRONT = 0x2D;

// #####################################################################
// ##  5. VARIABLES                                                     ##
// #####################################################################
struct Side {
  VL53L1X dev; int xshut; uint8_t addr; const char *name;
  int mm = -1; bool fresh = false; unsigned long lastData = 0;
};
struct MapPillar { bool used; float x, y; int color; int hits; };   // color 1 = red, 2 = green
enum State { WAIT, STRAIGHT, PRE_TURN, BRAKE, TURN, REVERSE, DONE };
const char *STATE_NAME[] = { "WAIT", "STRAIGHT", "PRE_TURN", "BRAKE", "TURN", "REVERSE", "DONE" };
enum LineLock { LOCK_NONE, LOCK_BLUE, LOCK_ORANGE };
enum Mode { MODE_LANE, MODE_BLOCK, MODE_PASS };
const char *MODE_NAME[] = { "CENTER", "PILLAR", "PASS" };

// main phases: calibrate (IDLE) -> parking (PARK) -> run (OBSTACLE)
enum Phase { PHASE_IDLE, PHASE_PARK, PHASE_OBSTACLE };
enum ParkStage { PK_WAIT_SIDES, PK_FIRST_TURN, PK_DRIVE_STRAIGHT, PK_RETURN_TURN, PK_PAUSE, PK_REVERSING };
const char *PARK_NAME[] = { "ESPERA", "GIRO1", "RECTO", "CONTRA", "PAUSA", "RETRO" };
Phase phase = PHASE_IDLE;
ParkStage parkStage = PK_WAIT_SIDES;

Side tofs[2];                         // [0] = LEFT, [1] = RIGHT
Side rearTof;                         // rear ToF (for reversing)
bool rearOk = false;
Side frontTof;                        // front ToF (decides when to turn after a line)
bool frontOk = false;
Servo steering;
State state = WAIT;
Mode  mode = MODE_LANE;
volatile long encRaw = 0;
int  pwmNow = 0, pwmTarget = 0, servoNow = STEER_CENTER;
bool tofOk = false, finalPrinted = true;
bool driveBack = false;               // true = motor running in reverse
float biasDps = 0, yawDeg = 0, rateDps = 0;
float heading = 0;                    // target heading of this segment (multiple of 90)
float turnEndCm = 0;                  // odometry when the last reverse move ended
float revStartOdo = 0;                // odometry when reversing started
float d1Cm = -1, lStartCm = -1, stopAtCm = -1;
float iTerm = 0, stallOdo = 0;
float latDeg = 0, latI = 0;           // centering correction (+ = left)
float goalLocal = 0;                  // desired heading relative to the segment (+ = left)
float passLeftCm = 0;                 // cm still needed to finish passing the pillar
int   dir = 0;                        // +1 = left (blue line), -1 = right (orange line)
int   corners = 0;
bool  finalLeg = false;
bool  finalLineSeen = false;          // last line already seen: drive FINAL_EXTRA_S, then stop
unsigned long finalLineMs = 0;        // time the last line was seen
unsigned long lineIgnoreUntil = 0;    // lines are ignored until this time (millis)
LineLock lineLock = LOCK_NONE;
bool  sameLinePrev = false;
int   camBlue = 0, camOrange = 0;     // lines: what the camera sees now
int   camColor = 0, camDistCm = -1, camLatCm = 0;   // pillar in the last camera message
bool  newCamData = false;
unsigned long lastCamMs = 0;
char  camBuf[64]; uint8_t camLen = 0; bool inCam = false;
unsigned long lastMicros = 0, lastRamp = 0, lastPrint = 0, phaseStart = 0, doneAt = 0;
unsigned long lastCtrl = 0, stallSince = 0, runStart = 0;

// parking variables
float parkOutboundSign = 1.0f;        // +1 / -1: side the car left the parking spot on
int   parkReturnSteer = STEER_CENTER; // initial counter-steer angle
long  parkStraightStart = 0;          // encoder value when the short straight started
long  parkRevStart = 0;               // encoder value when reversing started
long  parkLastCount = 0;              // last distance seen while reversing
unsigned long parkStageMs = 0;        // start time of the current parking stage
unsigned long parkLastPulseMs = 0;    // last encoder pulse while reversing

// map of the current segment
float carX = 0, carY = 0;             // rear axle position in the segment (cm)
float lastMapOdo = 0;                 // odometry when the map was last updated
float segCm = 0, lastSegCm = 0;       // distance driven in this segment
MapPillar current = {false, 0, 0, 0, 0};
MapPillar nextOne = {false, 0, 0, 0, 0};

// ---------------- motor / encoder / servo ----------------
void encoderISR() { if (digitalRead(ENC_C2_PIN)) encRaw++; else encRaw--; }
long readEncRaw() { noInterrupts(); long c = encRaw; interrupts(); return c; }
float odoCm() { long c = readEncRaw(); return (ENCODER_REVERSED ? -c : c) / TICKS_PER_CM; }
void motorOut(int pwm) {
  pwm = constrain(pwm, 0, 255);
  bool a = pwm > 0, b = false;
  if (MOTOR_REVERSED) { b = a; a = false; }
  if (driveBack) { bool t = a; a = b; b = t; }     // reverse the direction for reversing
  digitalWrite(AIN1_PIN, a ? HIGH : LOW); digitalWrite(AIN2_PIN, b ? HIGH : LOW);
  analogWrite(PWMA_PIN, pwm); pwmNow = pwm;
}
void brake() { digitalWrite(AIN1_PIN, HIGH); digitalWrite(AIN2_PIN, HIGH); analogWrite(PWMA_PIN, 255); pwmNow = 0; pwmTarget = 0; }
void servoTo(int a) { a = constrain(a, STEER_LEFT_MAX, STEER_RIGHT_MAX); if (a != servoNow) { steering.write(a); servoNow = a; } }

// ---------------- BMI160 (gyro) ----------------
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
    bmiWrite(0x7E, 0xB6); delay(100);
    bmiWrite(0x7E, 0x15); delay(100);
    bmiWrite(0x43, 0x02); delay(50);
    uint8_t id = 0, st = 0;
    if (bmiRead(0x00, &id, 1) && id == 0xD1 && bmiRead(0x03, &st, 1) && ((st >> 2) & 3) == 1) return true;
  }
  return false;
}
void calibrate() {
  long sum = 0; int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 700) { int16_t r; readGyroZ(r); delay(5); }
  for (int i = 0; i < 300; i++) {
    int16_t r;
    if (readGyroZ(r)) { sum += r; n++; }
    delay(5);
    digitalWrite(LED_PIN, (millis() / 80) % 2);
  }
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

// ---------------- side, rear and front ToF ----------------
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
void tofExtraOff() {
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);
}
bool tofStart() {
  tofExtraOff();                      // rear and front sensors off while left and right are configured
  for (int i = 0; i < 2; i++) { pinMode(tofs[i].xshut, OUTPUT); digitalWrite(tofs[i].xshut, LOW); }
  delay(10);
  bool a = startOne(tofs[0]); bool b = startOne(tofs[1]);
  rearOk  = startOne(rearTof);        // powered on after left and right
  frontOk = startOne(frontTof);       // after the rear sensor
  if (!rearOk)  { i2cTimedOut(); }
  if (!frontOk) { i2cTimedOut(); }
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
        t.mm = (v > 0 && v < 4000) ? v : 4000;
        t.fresh = true; t.lastData = millis();
      }
    }
    if (millis() - t.lastData > 500) restart = true;
  }
  if (rearOk && rearTof.dev.dataReady()) {
    rearTof.dev.read(false);
    if (!i2cTimedOut()) {
      uint16_t v = rearTof.dev.ranging_data.range_mm;
      rearTof.mm = (v > 0 && v < 4000) ? v : 4000;
      rearTof.lastData = millis();
    }
  }
  if (frontOk && frontTof.dev.dataReady()) {
    frontTof.dev.read(false);
    if (!i2cTimedOut()) {
      uint16_t v = frontTof.dev.ranging_data.range_mm;
      frontTof.mm = (v > 0 && v < 4000) ? v : 4000;
      frontTof.lastData = millis();
    }
  }
  if (restart) {
    tofOk = tofStart();
    Serial.print(F("!!! TOF RESTART -> ")); Serial.println(tofOk ? F("OK") : F("FAILED"));
    tofs[0].lastData = tofs[1].lastData = millis();
  }
}

// ---------------- camera ----------------
void handleCameraLine() {
  camBuf[camLen] = '\0';
  int col, x, b, ok, bf, br, w, d, lat, of, orow;
  unsigned long seq;
  int n = sscanf(camBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                 &w, &d, &lat, &of, &orow);
  if (n == 12) {
    camBlue = bf; camOrange = of;          // lines (corners)
    camColor = col; camDistCm = d; camLatCm = lat;   // pillar
    lastCamMs = millis();
    newCamData = true;
  }
}
bool canStartRun() {                       // START can be pressed: no run is active
  return phase == PHASE_IDLE || (phase == PHASE_OBSTACLE && state == DONE);
}
void readSerialInput() {
  // Reads the camera messages that arrive on the serial port (Nano D0).
  // There is no command input: the run can only be started with the START button.
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { inCam = true; camLen = 0; continue; }     // '$' starts a camera message
    if (inCam) {
      if (c == '\n') { handleCameraLine(); inCam = false; camLen = 0; }   // end of message: parse it
      else if (c != '\r' && camLen < sizeof(camBuf) - 1) camBuf[camLen++] = c;   // collect the characters
    }
  }
}
bool sameColorVisible() {
  if (lineLock == LOCK_BLUE)   return camBlue != 0;
  if (lineLock == LOCK_ORANGE) return camOrange != 0;
  return false;
}
bool lineVisibleForCount() {
  if (ONLY_LOCKED_COLOR) return sameColorVisible();
  return camBlue != 0 || camOrange != 0;
}

// ---------------- local heading and segment map ----------------
float yawL() { return yawDeg - heading; }     // deviation from the segment heading (+ = left)
float camPosX() { return carX + CAM_FWD_CM * cos(radians(yawL())); }
float camPosY() { return carY + CAM_FWD_CM * sin(radians(yawL())); }

void updateMap() {
  float o = odoCm();
  float ds = o - lastMapOdo; lastMapOdo = o;
  float h = radians(yawL());
  carX += ds * cos(h);
  carY += ds * sin(h);
  segCm += ds;
}

void relativeToCar(float px, float py, float &fwd, float &left) {
  float h = radians(yawL());
  float dx = px - camPosX(), dy = py - camPosY();
  fwd  =  dx * cos(h) + dy * sin(h);
  left = -dx * sin(h) + dy * cos(h);
}

// Places the pillar from the last camera message on the segment map
void onCameraPillar() {
  if (camColor == 0 || camDistCm <= 0) return;
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;

  float fwd  = camDistCm + PILLAR_CM / 2;        // to the pillar center
  float left = -camLatCm;                        // camera: + = right; map: + = left
  float h = radians(yawL());
  float cx = camPosX(), cy = camPosY();
  float px = cx + fwd * cos(h) - left * sin(h);
  float py = cy + fwd * sin(h) + left * cos(h);

  MapPillar *slots[2] = { &current, &nextOne };
  for (int i = 0; i < 2; i++) {
    MapPillar *p = slots[i];
    if (p->used && p->color == camColor && hypot(p->x - px, p->y - py) < SAME_PILLAR_CM) {
      p->x += BLEND * (px - p->x);
      p->y += BLEND * (py - p->y);
      p->hits++;
      return;
    }
  }
  MapPillar fresh = {true, px, py, camColor, 1};
  if (!current.used) {
    current = fresh;
    Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN"));
  } else if (px > current.x + SAME_PILLAR_CM) {
    nextOne = fresh;
  }
}

// Side to pass the pillar on: red on the right, green on the left
float goalFor(int color, float pillarY) {
  float cy = camPosY();
  if (color == 1) return min(cy, pillarY - TARGET_CM);
  return max(cy, pillarY + TARGET_CM);
}

// Returns true and the local heading if a pillar must be avoided now
bool pillarGoal(float &goalOut) {
  float fwd = 0, left = 0;
  if (current.used) {
    relativeToCar(current.x, current.y, fwd, left);
    if (fwd < -RETURN_AFTER_CM) {
      Serial.println(F(">> PILLAR PASSED"));
      current = nextOne;
      nextOne.used = false;
      if (current.used) relativeToCar(current.x, current.y, fwd, left);
    }
  }
  if (!current.used) return false;

  bool beside = fwd < 0;                          // already level with the pillar
  const MapPillar *target = beside ? (nextOne.used ? &nextOne : NULL) : &current;
  if (target == NULL) return false;

  float goalY = goalFor(target->color, target->y);
  float toFace = (target->x - PILLAR_CM / 2) - camPosX() - ARRIVE_BEFORE_CM;
  float look = max(LOOK_MIN_CM, toFace);

  if (beside) {                                   // level with the pillar: never turn back toward it
    if (current.color == 1) goalY = min(goalY, camPosY());
    else                    goalY = max(goalY, camPosY());
  }
  goalOut = constrain(degrees(atan2(goalY - camPosY(), look)), -MAX_AIM_DEG, MAX_AIM_DEG);
  return true;
}

// Centering between walls: returns the heading correction (+ = left)
float laneGoal(float dt) {
  bool okL = tofs[0].mm > 0 && tofs[0].mm < SIDE_MAX_MM;
  bool okR = tofs[1].mm > 0 && tofs[1].mm < SIDE_MAX_MM;
  if (okL && okR) {
    float latErrMm = (tofs[0].mm - tofs[1].mm) / 2.0;   // + = closer to the right wall -> steer left
    latI = constrain(latI + KI_LAT * latErrMm * dt, -LAT_I_MAX, LAT_I_MAX);
    latDeg = constrain(K_LAT * latErrMm + latI, -LAT_MAX_DEG, LAT_MAX_DEG);
  } else {
    latDeg = latI;
  }
  if (tofs[0].mm > 0 && tofs[0].mm < SAFE_WALL_MM) latDeg = -LAT_MAX_DEG;
  if (tofs[1].mm > 0 && tofs[1].mm < SAFE_WALL_MM) latDeg =  LAT_MAX_DEG;
  return latDeg;
}

// ---------------- corner = line ----------------
void onCorner(float odo) {
  lineIgnoreUntil = millis() + IGNORE_LINE_S * 1000.0;   // ignore all lines for IGNORE_LINE_S
  int n = corners + 1;
  Serial.print(F(">>> ESQUINA ")); Serial.print(n);
  Serial.print(F(" (line) at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
  if (n == 1) { d1Cm = odo; Serial.print(F(">>> D1 = ")); Serial.print(d1Cm, 1); Serial.println(F(" cm")); }
  if (n == 5 && lStartCm < 0) {
    lStartCm = odo - turnEndCm;
    Serial.print(F(">>> LSTART = ")); Serial.print(lStartCm, 1); Serial.println(F(" cm (end of turn 4 -> corner 5)"));
  }
  if (finalLeg) {                                 // last line: keep going FINAL_EXTRA_S, then stop (in runTest)
    finalLineSeen = true;
    finalLineMs = millis();
    Serial.println(F(">>> LAST LINE detected: driving FINAL_EXTRA_S, then stopping"));
    return;
  }
  state = PRE_TURN; phaseStart = millis(); pwmTarget = RUN_PWM;   // drive until FRONT_TURN_CM, then turn
}

// ---------------- stop ----------------
void finish(const __FlashStringHelper *why) {
  brake(); servoTo(STEER_CENTER); state = DONE; doneAt = millis(); finalPrinted = false;
  Serial.print(F(">>> ")); Serial.print(why);
  Serial.print(F(" | yaw ")); Serial.print(yawDeg, 1);
  Serial.print(F(" | odo ")); Serial.print(odoCm(), 1); Serial.println(F(" cm"));
}

// ---------------- PARKING ----------------
// Parking stop: back to IDLE, START must be pressed again
void parkStop(const __FlashStringHelper *why) {
  motorOut(0); driveBack = false; servoTo(STEER_CENTER);
  phase = PHASE_IDLE;
  Serial.print(F(">>> PARKING STOP: ")); Serial.println(why);
}

// Starts leaving the parking spot (START button). No recalibration: that was done at power-on.
void startPark() {
  if (!tofOk) {
    Serial.println(F(">>> Not starting: a side ToF is missing (check the wiring and restart)"));
    return;
  }
  Serial.println(F(">>> PARKING: leaving the parking space"));
  while (Serial.available()) Serial.read();
  inCam = false; camLen = 0;
  yawDeg = 0; heading = 0;                 // the parking heading is the car's straight-ahead heading
  servoTo(STEER_CENTER);
  driveBack = false; motorOut(0);
  parkStage = PK_WAIT_SIDES; parkStageMs = millis();
  runStart = millis();
  phase = PHASE_PARK;
}

// Waits for both side sensors and picks the exit side (the side with more space)
void parkChooseDirection() {
  unsigned long now = millis();
  bool freshL = tofs[0].mm > 0 && now - tofs[0].lastData < 300;
  bool freshR = tofs[1].mm > 0 && now - tofs[1].lastData < 300;
  if (!freshL || !freshR) return;                 // wait for data from both side sensors
  int left = tofs[0].mm, right = tofs[1].mm;
  if (left == right) return;                      // same reading: wait for another one

  bool turnLeft = left > right;                   // more space on the left = turn left
  parkReturnSteer = turnLeft ? STEER_RIGHT_MAX : STEER_LEFT_MAX;   // counter-steer to the other side
  servoTo(turnLeft ? STEER_LEFT_MAX : STEER_RIGHT_MAX);
  Serial.println(turnLeft ? F("PARK: first turn LEFT") : F("PARK: first turn RIGHT"));
  delay(75);                                      // let the servo move before rolling

  driveBack = false;
  motorOut(PARK_DRIVE_PWM);
  parkStageMs = now;
  parkStage = PK_FIRST_TURN;
}

void parkStartReverse() {
  parkRevStart = readEncRaw();
  parkLastCount = 0;
  parkLastPulseMs = millis();
  driveBack = true;                               // motor in reverse
  parkStage = PK_REVERSING;
  motorOut(PARK_REVERSE_PWM);
  Serial.println(F("PARK: retrocedo"));
}

void parkUpdateReverse() {
  long traveled = labs(readEncRaw() - parkRevStart);
  unsigned long now = millis();
  if (traveled >= PARK_REVERSE_TICKS) {           // distance reached: end of parking
    motorOut(0); driveBack = false;
    Serial.print(F("PARK: reverse done, ticks ")); Serial.println(traveled);
    Serial.println(F(">>> OBSTACLE CHALLENGE: starting the round"));
    startObstacle();
    return;
  }
  if (traveled != parkLastCount) {                // a new encoder pulse arrived
    parkLastCount = traveled;
    parkLastPulseMs = now;
  } else if (now - parkLastPulseMs >= PARK_STALL_MS) {
    parkStop(F("reverse: no encoder pulses"));
    return;
  }
  if (PARK_REVERSE_TICKS - traveled <= PARK_REVERSE_TICKS / 5) motorOut(PARK_REVERSE_SLOW_PWM);
}

// Steers toward 0 degrees with the gyro (same idea as the run).
// When reversing, the steering goes to the opposite side.
void parkSteerToZero(bool reversing) {
  float steer = KP_HEAD * (-yawDeg) - KD_HEAD * rateDps;
  steer = constrain(steer, -MAX_STEER, MAX_STEER);
  servoTo(reversing ? STEER_CENTER + (int)steer : STEER_CENTER - (int)steer);
}

// Parking state machine (called every loop while phase == PHASE_PARK)
void runPark() {
  unsigned long now = millis();
  switch (parkStage) {
    case PK_WAIT_SIDES:
      parkChooseDirection();
      break;

    case PK_FIRST_TURN:                           // turning toward the open side
      if (fabs(yawDeg) >= PARK_FIRST_ANGLE_DEG) {
        parkOutboundSign = yawDeg >= 0.0f ? 1.0f : -1.0f;
        servoTo(STEER_CENTER);                    // wheels straight for a moment
        parkStraightStart = readEncRaw();
        parkStageMs = now;
        parkStage = PK_DRIVE_STRAIGHT;
        Serial.println(F("PARK: 45 grados. Ruedas centradas."));
      } else if (now - parkStageMs >= PARK_TURN_TIMEOUT_MS) {
        parkStop(F("first turn too long"));
      }
      break;

    case PK_DRIVE_STRAIGHT:                       // one very short straight section
      if (labs(readEncRaw() - parkStraightStart) >= PARK_STRAIGHT_TICKS) {
        servoTo(parkReturnSteer);                 // initial counter-steer
        parkStageMs = now;
        parkStage = PK_RETURN_TURN;
        Serial.println(F("PARK: contravolante"));
      } else if (now - parkStageMs >= PARK_STALL_MS) {
        parkStop(F("no encoder pulses (check D3)"));
      }
      break;

    case PK_RETURN_TURN:                          // PD counter-steer until 0 degrees (parallel to the start)
      parkSteerToZero(false);
      if (fabs(yawDeg + rateDps * TURN_PREDICT_S) <= PARK_RETURN_TOL_DEG) {
        motorOut(0);
        servoTo(STEER_CENTER);
        parkStageMs = now;
        parkStage = PK_PAUSE;
        Serial.println(F("PARK: heading 0 (parallel to the start). Stopped before reversing."));
      } else if (now - parkStageMs >= PARK_TURN_TIMEOUT_MS) {
        parkStop(F("contravolante demasiado largo"));
      }
      break;

    case PK_PAUSE:                                // stopped 0.5 s before reversing
      if (now - parkStageMs >= PARK_PAUSE_MS) parkStartReverse();
      break;

    case PK_REVERSING:                            // reverses while holding the heading at 0
      parkSteerToZero(true);
      parkUpdateReverse();
      break;
  }
}

// ---------------- OPEN CHALLENGE ----------------
// Starts the camera-guided run. Called when the parking stage ends.
// No recalibration: the reference heading is the parking heading (0 degrees).
void startObstacle() {
  noInterrupts(); encRaw = 0; interrupts();       // run odometry reset to 0
  driveBack = false;
  dir = 0; corners = 0; heading = 0; turnEndCm = 0; revStartOdo = 0;
  d1Cm = -1; lStartCm = -1; stopAtCm = -1; finalLeg = false;
  finalLineSeen = false; finalLineMs = 0; lineIgnoreUntil = 0;
  lineLock = LOCK_NONE; sameLinePrev = false;
  latDeg = 0; latI = 0; iTerm = 0; goalLocal = 0; passLeftCm = 0;
  mode = MODE_LANE;
  carX = carY = 0; segCm = 0; lastSegCm = 0; lastMapOdo = 0;
  current.used = false; nextOne.used = false; newCamData = false;
  stallOdo = 0; stallSince = millis(); runStart = millis();
  state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
  phase = PHASE_OBSTACLE;
  Serial.println(F(">>> GO"));
}

// ---------------- main logic ----------------
void runTest() {
  if (state == STRAIGHT) {
    float dt = (millis() - lastCtrl) / 1000.0; lastCtrl = millis();
    float odo = odoCm();
    bool camOk = (millis() - lastCamMs) <= CAMERA_TIMEOUT_MS;
    pwmTarget = camOk ? RUN_PWM : 0;

    updateMap();
    float dSeg = segCm - lastSegCm; lastSegCm = segCm;

    // 0) LAST LINE seen FINAL_EXTRA_S ago: stop
    if (finalLineSeen && millis() - finalLineMs >= FINAL_EXTRA_S * 1000.0) {
      finish(F(">>> LAST LINE + drive-on -> STOP"));
      return;
    }

    // 1) TOP PRIORITY: LINE = CORNER -> turn (the turn happens even if a pillar is visible)
    if (camOk && !finalLineSeen) {
      if (lineLock == LOCK_NONE) {
        if (odo >= MIN_DRIVE_CM && (camBlue || camOrange)) {
          lineLock = camOrange ? LOCK_ORANGE : LOCK_BLUE;
          dir = (lineLock == LOCK_BLUE) ? 1 : -1;
          sameLinePrev = true;
          Serial.println(lineLock == LOCK_BLUE ? F(">> FIRST BLUE LINE: turn LEFT")
                                               : F(">> FIRST ORANGE LINE: turn RIGHT"));
          onCorner(odo);
          return;
        }
      } else {
        bool now = lineVisibleForCount();
        bool rising = now && !sameLinePrev;
        sameLinePrev = now;                                    // se actualiza siempre, aunque se ignore
        if (rising && millis() >= lineIgnoreUntil) { onCorner(odo); return; }
      }
    }

    // 2) SECOND PRIORITY: PILLARS. Steer to avoid a pillar if needed; if one was just passed, hold the heading; otherwise center
    if (newCamData) { newCamData = false; if (camOk) onCameraPillar(); }
    float g = 0;
    if (pillarGoal(g)) {
      goalLocal = g; passLeftCm = PASS_CM; mode = MODE_BLOCK;
    } else if (passLeftCm > 0) {
      passLeftCm -= dSeg; mode = MODE_PASS;           // goalLocal se queda igual
    } else {
      goalLocal = laneGoal(dt); mode = MODE_LANE;
    }

    float err = goalLocal - yawL();
    if (mode == MODE_LANE && pwmNow >= RUN_PWM) iTerm = constrain(iTerm + KI_HEAD * err * dt, -I_MAX, I_MAX);
    float steer = KP_HEAD * err - KD_HEAD * rateDps + iTerm;
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    // 3) AFTER THE LAST TURN: keep going to the last line. Distance-based safety.
    if (finalLeg && !finalLineSeen) {
      if (!finalLeg && millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }
    }
    if (millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }

  } else if (state == PRE_TURN) {
    // line seen: drive straight holding the heading, ignoring pillars and lines,
    // until the front ToF reads FRONT_TURN_CM (or, without a front ToF, after LINE_WAIT_S)
    newCamData = false;
    pwmTarget = RUN_PWM;
    float steer = KP_HEAD * (-yawL()) - KD_HEAD * rateDps;
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    bool frontFresh = frontOk && frontTof.mm > 0 && (millis() - frontTof.lastData < 300);
    bool readyToTurn = frontOk
                       ? (frontFresh && frontTof.mm <= FRONT_TURN_CM * 10)
                       : (millis() - phaseStart >= LINE_WAIT_S * 1000.0);

    if (readyToTurn) {
      // brake with the wheels straight, then turn in reverse
      brake();
      state = BRAKE; phaseStart = millis();
    } else if (millis() - phaseStart > MAX_PRE_TURN_MS) {
      finish(F("FRONT never reached FRONT_TURN_CM -> STOP"));
    }

  } else if (state == BRAKE) {
    // brake for BRAKE_S with the wheels straight before reversing
    newCamData = false;
    brake();
    servoTo(STEER_CENTER);
    if (millis() - phaseStart >= BRAKE_S * 1000.0) {
      driveBack = true;
      state = TURN; phaseStart = millis(); pwmTarget = TURN_PWM;
    }

  } else if (state == TURN) {
    newCamData = false;                            // no pillars are mapped while turning
    // When reversing, the steering goes to the opposite side: to turn left, steer right
    servoTo(dir > 0 ? STEER_RIGHT_MAX : STEER_LEFT_MAX);
    float predicted = dir * (yawDeg + rateDps * TURN_PREDICT_S - heading);
    if (predicted >= 90) {
      heading += dir * 90.0;
      corners++;
      if (corners >= 4 * LAPS) {
        finalLeg = true;
        stopAtCm = -1;                             // no longer stop based on distance
        Serial.println(F(">>> FINAL: drive to the last line, then FINAL_EXTRA_S and stop"));
      }
      // after the turn: reverse until the rear ToF reads REAR_STOP_CM
      brake();
      servoTo(STEER_CENTER);
      revStartOdo = odoCm();
      driveBack = true;
      state = REVERSE; phaseStart = millis(); pwmTarget = REVERSE_PWM;
      goalLocal = 0; passLeftCm = 0; mode = MODE_LANE;
      current.used = false; nextOne.used = false;
      Serial.print(F(">>> GIRO ")); Serial.print(corners); Serial.print(F(" hecho | yaw ")); Serial.print(yawDeg, 1);
      Serial.print(F(" -> heading ")); Serial.println(heading, 0);
    } else if (millis() - phaseStart > MAX_TURN_MS) {
      finish(F("TURN TOO LONG -> STOP"));
    }

  } else if (state == REVERSE) {
    newCamData = false;                            // no pillars are mapped and no lines are counted while reversing
    servoTo(STEER_CENTER);
    // Stop with the rear ToF (fresh reading, under 300 ms old); without data, use odometry
    bool rearFresh = rearOk && rearTof.mm > 0 && (millis() - rearTof.lastData < 300);
    bool reached = rearFresh
                   ? (rearTof.mm <= REAR_STOP_CM * 10)
                   : (odoCm() <= revStartOdo - REVERSE_CM);
    if (reached) {
      brake();
      driveBack = false;
      // new segment: map, pillars and line counting start here
      carX = carY = 0; segCm = 0; lastSegCm = 0; lastMapOdo = odoCm();
      turnEndCm = odoCm();
      goalLocal = 0; mode = MODE_LANE;
      sameLinePrev = lineVisibleForCount();
      state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
      Serial.println(F(">>> REVERSE done -> driving on"));
    } else if (millis() - phaseStart > MAX_REVERSE_MS) {
      finish(F("REVERSE TOO LONG -> STOP"));
    }
  }

  // safety stop: motor on but wheels not turning
  if (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE) {
    float o = odoCm();
    if (fabs(o - stallOdo) > 1.0 || pwmNow < 60) { stallOdo = o; stallSince = millis(); }
    else if (millis() - stallSince > STALL_MS) finish(F("STALLED: wheels not turning (hit a wall?) -> STOP"));
  }

  if (state == DONE && !finalPrinted && millis() - doneAt > 1000) {
    finalPrinted = true; motorOut(0);
    Serial.print(F(">>> FINAL yaw: ")); Serial.print(yawDeg, 1);
    Serial.print(F(" | heading ")); Serial.print(heading, 0);
    Serial.print(F(" | esquinas ")); Serial.print(corners);
    Serial.print(F(" | D1 ")); Serial.print(d1Cm, 1);
    Serial.print(F(" | LSTART ")); Serial.print(lStartCm, 1);
    Serial.print(F(" | tiempo ")); Serial.print((doneAt - runStart) / 1000.0, 1); Serial.println(F(" s"));
  }
}

// ---------------- setup / loop ----------------
void setup() {
  pinMode(PWMA_PIN, OUTPUT); pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT); motorOut(0);
  tofExtraOff();
  pinMode(START_BTN_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_C1_PIN, INPUT_PULLUP); pinMode(ENC_C2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_C1_PIN), encoderISR, RISING);
  steering.attach(SERVO_PIN); steering.write(STEER_CENTER);
  Serial.begin(115200); delay(500);
  Serial.println(F("\nBOOT - PARKING + OBSTACLE CHALLENGE"));
  Wire.begin(); Wire.setClock(100000); Wire.setWireTimeout(25000, true);
  tofs[0].xshut = XSHUT_LEFT;  tofs[0].addr = ADDR_LEFT;  tofs[0].name = "LEFT";
  tofs[1].xshut = XSHUT_RIGHT; tofs[1].addr = ADDR_RIGHT; tofs[1].name = "RIGHT";
  rearTof.xshut = XSHUT_REAR;  rearTof.addr = ADDR_REAR;  rearTof.name = "REAR";
  frontTof.xshut = XSHUT_FRONT; frontTof.addr = ADDR_FRONT; frontTof.name = "FRONT";
  for (int i = 0; i < 5 && !tofOk; i++) { tofOk = tofStart(); if (!tofOk) delay(300); }
  bool g = bmiStart();
  Serial.println(g ? F("BMI160 OK") : F("WARNING: BMI160 not ready (battery ON first?)"));
  Serial.println(tofOk ? F("Both TOF OK") : F("WARNING: a TOF is missing"));
  Serial.println(rearOk ? F("REAR TOF OK") : F("WARNING: REAR TOF missing (reverse by odometry)"));
  Serial.println(frontOk ? F("FRONT TOF OK") : F("WARNING: FRONT TOF missing (turn by time)"));
  calibrate();                                    // calibrate the gyro at power-on
  noInterrupts(); encRaw = 0; interrupts();
  phase = PHASE_IDLE;
  Serial.println(F("Ready. Press START to leave the parking spot and run the course."));
}

void loop() {
  readSerialInput();

  if (canStartRun() && digitalRead(START_BTN_PIN) == LOW) {
    delay(30);
    if (digitalRead(START_BTN_PIN) == LOW) {
      while (digitalRead(START_BTN_PIN) == LOW) delay(5);
      delay(300);
      startPark();
    }
  }

  updateGyro();
  updateTof();

  if (phase == PHASE_PARK) {
    runPark();
  } else if (phase == PHASE_OBSTACLE) {
    runTest();

    unsigned long now = millis();                 // speed ramp (only during the run)
    if (now - lastRamp >= 20) {
      lastRamp = now;
      bool moving = (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE);
      int target = moving ? pwmTarget : 0;
      driveBack = (state == TURN || state == REVERSE);   // turning and reversing run the motor in reverse
      if (state == DONE) { /* brake */ }
      else if (pwmNow < target) motorOut(min(pwmNow + RAMP_STEP, target));
      else if (pwmNow > target) motorOut(target);
    }
  }

  unsigned long now = millis();
  if (now - lastPrint >= 100 && phase != PHASE_IDLE) {
    lastPrint = now;
    if (phase == PHASE_PARK) {
      Serial.print(F("PARK ")); Serial.print(PARK_NAME[parkStage]);
      Serial.print(F("\tyaw ")); Serial.print(yawDeg, 1);
      Serial.print(F("\tL ")); Serial.print(tofs[0].mm);
      Serial.print(F("\tR ")); Serial.print(tofs[1].mm);
      Serial.print(F("\tenc ")); Serial.print(readEncRaw());
      Serial.print(F("\tpwm ")); Serial.println(pwmNow);
    } else {
      Serial.print(STATE_NAME[state]);
      Serial.print(F("\tc ")); Serial.print(corners);
      Serial.print(F("\tyawL ")); Serial.print(yawL(), 1);
      Serial.print(F("\tgoal ")); Serial.print(goalLocal, 1);
      Serial.print(F("\t")); Serial.print(MODE_NAME[mode]);
      Serial.print(F("\todo ")); Serial.print(odoCm(), 1);
      Serial.print(F("\tL ")); Serial.print(tofs[0].mm);
      Serial.print(F("\tR ")); Serial.print(tofs[1].mm);
      Serial.print(F("\tFR ")); Serial.print(frontTof.mm);
      Serial.print(F("\tRA ")); Serial.print(rearTof.mm);
      Serial.print(F("\tpillar ")); Serial.print(current.used ? (current.color == 1 ? F("R") : F("G")) : F("-"));
      Serial.print(F("\tline B/O ")); Serial.print(camBlue); Serial.print('/'); Serial.print(camOrange);
      Serial.print(F("\tlock ")); Serial.print((int)lineLock);
      Serial.print(F("\tpwm ")); Serial.println(pwmNow);
    }
  }
}