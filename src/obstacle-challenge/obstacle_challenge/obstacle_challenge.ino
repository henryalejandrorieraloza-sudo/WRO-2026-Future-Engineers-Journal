// =====================================================================
// OBSTACLE CHALLENGE - Arduino Nano, v1
// Combines three behaviours in one sketch:
//   - LAPS: drives around the track by following the FIRST floor line the
//     camera sees (blue = turn left, orange = turn right). Each corner is a
//     90-degree turn measured with the gyroscope. After every turn the car
//     drives forward until the FRONT ToF reads FRONT_TURN_CM, then reverses
//     through the turn, then backs up until the REAR ToF reads REAR_STOP_CM.
//     Once a line has been used, all other lines are ignored for
//     IGNORE_LINE_S seconds. After the last corner the car drives
//     FINAL_EXTRA_S more and stops.
//   - PILLARS: when the camera reports a red or green pillar on the straight,
//     the car steers around it (red pillar: the car passes on the pillar's RIGHT, so the
//     pillar ends up on the car's LEFT; green pillar: the car passes on the pillar's LEFT).
//     When the car has passed the pillar (PASS_CM), it holds that heading for
//     a while and then goes back to centring itself between the walls.
//   - CENTRING: on a straight with no pillar in view, the two side ToF sensors
//     keep the car in the middle of the corridor.
//
// Priority on every straight section:
//   1) BLOCK : a pillar is on the map -> steer to avoid it.
//   2) PASS  : the car has just passed a pillar -> keep the heading for PASS_CM.
//   3) LANE  : no pillar -> both side ToF sensors at the same distance.
// The camera's floor lines are always checked first (they are corners).
//
// Speed: RUN_PWM = 70 on the straights, TURN_PWM = 70 while turning (the turn
// is done in reverse gear), REVERSE_PWM = 70 while backing up after a turn.
// If the car does not turn well at 70, raise TURN_PWM.
//
// Pillar map: it is cleared after every back-up. Each straight section has its
// own coordinate system, with x pointing forward and y pointing left.
//
// Camera: camera TX -> Nano RX (D0), 115200 baud. The camera sends lines that
// start with '$' and end with a newline (see handleCameraLine()).
// Serial monitor: type 'g' + Enter to start, 's' + Enter for an emergency stop.
// Board: Arduino Nano | TB6612: PWMA D6, AIN1 D7, AIN2 D8 | Encoder C1 D3, C2 D2
// Servo D9 | START D10 | LED D13 | BMI160 0x69 | ToF left A3 -> 0x2A | ToF right A2 -> 0x2B
// ToF rear A0 -> 0x2C | ToF front A1 -> 0x2D
// Libraries: VL53L1X (Pololu), Servo
// =====================================================================

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// #####################################################################
// ##  1. RUN SETTINGS                                                  ##
// #####################################################################
const int   LAPS      = 3;            // 1 = one lap (4 corners), 3 = full open round (12 corners)
const int   RUN_PWM   = 70;           // motor speed on the straights (0-255)
const int   TURN_PWM  = 70;           // motor speed while turning (the turn is done in reverse)
const int   REVERSE_PWM = 70;         // motor speed while backing up after each turn
const float FRONT_TURN_CM = 40.0;     // after a line, drive forward until the front ToF reads this (cm) or less
const unsigned long MAX_PRE_TURN_MS = 4000;  // safety: if the front ToF never reaches FRONT_TURN_CM, stop
const float REAR_STOP_CM = 20.0;      // back up until the rear ToF reads this (cm) or less
const float REVERSE_CM  = 30.0;       // fallback: if the rear ToF gives no data, back up this distance (cm)
const unsigned long MAX_REVERSE_MS = 3000;   // safety: if the back-up is not finished in this time, stop
const float STOP_OFFSET_CM = 0;       // (no longer used to stop; kept for reference)
const bool  ONLY_LOCKED_COLOR = true; // true: only count lines of the colour seen first
const float LINE_WAIT_S = 0.75;       // fallback: without a front ToF, drive this long before turning
const float FINAL_EXTRA_S = 0.45;     // after the LAST line: drive this much longer, then stop

// #####################################################################
// ##  2. PILLARS                                                       ##
// #####################################################################
const float TARGET_CM        = 17.0;  // wanted distance between pillar centre and car centre when passing it
const float MAX_AIM_DEG      = 60.0;  // maximum steering target angle used to avoid a pillar
const float LOOK_MIN_CM      = 12.0;  // minimum look-ahead distance for the aiming angle
const float ARRIVE_BEFORE_CM = 55.0;  // be already on the safe side this far before the pillar
const float RETURN_AFTER_CM  = 25.0;  // a pillar counts as "passed" when the car is this far beyond it
const float PASS_CM          = 50.0;  // after losing the pillar, keep the heading for this distance (> RETURN_AFTER)
const float SAME_PILLAR_CM   = 15.0;  // a detection closer than this to a stored pillar is the same pillar
const float BLEND            = 0.3;   // how much each camera message corrects the stored pillar position
const float MAX_SIDE_CM      = 60.0;  // pillars further to the side than this are noise
const float PILLAR_CM        = 5.0;   // pillar width
const float CAM_FWD_CM       = 11.7;  // rear axle (encoder) -> camera lens distance

// #####################################################################
// ##  3. FINE TUNING                                                   ##
// #####################################################################
const float KP_HEAD = 2.0;            // servo degrees per degree of heading error
const float KD_HEAD = 0.03;           // damping from the turn rate (gyro)
const float KI_HEAD = 4.0;            // learns the real straight-ahead angle of the servo (centring only)
const float I_MAX   = 25;             // limit of the heading integral term
const float MAX_STEER = 70.0;         // maximum steering command (servo degrees)
const float K_LAT       = 0.03;       // heading degrees per mm of lateral offset between the walls
const float KI_LAT      = 0.02;       // integral gain of the wall-centring correction
const float LAT_I_MAX   = 8;          // limit of the wall-centring integral (degrees)
const float LAT_MAX_DEG = 12;         // maximum heading correction from the wall-centring
const int   SIDE_MAX_MM = 1200;       // a side reading above this means "no usable wall"
const int   SAFE_WALL_MM = 70;        // closer than this to a wall: steer away from it at full strength
const float TURN_PREDICT_S = 0.12;    // end the turn this much earlier (predicted). Raise it if the turn overshoots 90 degrees
const float IGNORE_LINE_S = 4.0;      // after a line is detected, ignore ALL lines for this long (s)
const float MIN_DRIVE_CM = 10;        // do not detect lines in the first cm after START
const float FALLBACK_CM = 50;         // (no longer used; kept for reference)
const float MAX_FINAL_CM = 250;       // safety: if the last line never shows up after the last turn, stop
const unsigned long MAX_TURN_MS = 4000;       // safety: a single turn longer than this stops the car
const unsigned long MAX_STRAIGHT_MS = 8000;   // no corner seen within 8 s on a straight -> stop
const unsigned long STALL_MS = 500;           // motor on but wheels not turning for this long -> stop (wall hit)
const unsigned long CAMERA_TIMEOUT_MS = 500;  // no camera message for this long -> do not drive forward

// #####################################################################
// ##  4. CAR CONSTANTS (same as open_challenge)                        ##
// #####################################################################
const bool  MOTOR_REVERSED   = true;  // flips the motor direction pins
const bool  ENCODER_REVERSED = true;  // flips the encoder count sign
const int   YAW_SIGN         = 1;     // +1: turning left increases the yaw angle
const float GYRO_SCALE       = 1.007; // gyro calibration factor (measured)
const int   STEER_CENTER = 85, STEER_LEFT_MAX = 10, STEER_RIGHT_MAX = 170;   // left = smaller servo angle
const int   RAMP_STEP = 5;            // PWM change per 20 ms (soft start / soft stop)
const float TICKS_PER_CM = 9.27;      // encoder ticks per cm of wheel travel (measured)
const uint8_t BMI = 0x69;             // I2C address of the BMI160 gyroscope
const float GYRO_LSB_PER_DPS = 65.6;  // gyro raw counts per degree per second
const uint32_t TOF_BUDGET_US = 50000; // ToF measurement timing budget (50 ms)
const int PWMA_PIN = 6, AIN1_PIN = 7, AIN2_PIN = 8;   // TB6612FNG motor driver pins
const int ENC_C1_PIN = 3, ENC_C2_PIN = 2;             // encoder channels (C1 = interrupt pin)
const int SERVO_PIN = 9, START_BTN_PIN = 10, LED_PIN = 13;
const int XSHUT_LEFT = A3, XSHUT_RIGHT = A2;          // shutdown pins of the side ToF sensors
const uint8_t ADDR_LEFT = 0x2A, ADDR_RIGHT = 0x2B;    // new I2C addresses given to the side ToF sensors
const int XSHUT_FRONT = A1, XSHUT_REAR = A0;          // shutdown pins of the front and rear ToF sensors
const uint8_t ADDR_REAR = 0x2C, ADDR_FRONT = 0x2D;    // new I2C addresses of the rear and front ToF sensors

// #####################################################################
// ##  5. VARIABLES                                                     ##
// #####################################################################
struct Side {                         // one ToF distance sensor and its last reading
  VL53L1X dev; int xshut; uint8_t addr; const char *name;
  int mm = -1; bool fresh = false; unsigned long lastData = 0;   // last distance (mm), fresh flag, time of last data
};
struct MapPillar { bool used; float x, y; int color; int hits; };   // one pillar on the map; color 1 = red, 2 = green
enum State { WAIT, STRAIGHT, PRE_TURN, TURN, REVERSE, DONE };         // the main state machine
const char *STATE_NAME[] = { "WAIT", "STRAIGHT", "PRE_TURN", "TURN", "REVERSE", "DONE" };
enum LineLock { LOCK_NONE, LOCK_BLUE, LOCK_ORANGE };                 // which line colour the run follows
enum Mode { MODE_LANE, MODE_BLOCK, MODE_PASS };                      // how the steering target is chosen
const char *MODE_NAME[] = { "LANE", "BLOCK", "PASS" };

Side tofs[2];                         // [0] = LEFT, [1] = RIGHT
Side rearTof;                         // rear ToF (used to stop the back-up)
bool rearOk = false;                  // true if the rear ToF started correctly
Side frontTof;                        // front ToF (used to decide when to turn after a line)
bool frontOk = false;                 // true if the front ToF started correctly
Servo steering;
State state = WAIT;
Mode  mode = MODE_LANE;
volatile long encRaw = 0;             // raw encoder count, changed inside the interrupt
int  pwmNow = 0, pwmTarget = 0, servoNow = STEER_CENTER;
bool tofOk = false, finalPrinted = true;   // finalPrinted: the final report has been printed
bool driveBack = false;               // true = motor drives in reverse gear
float biasDps = 0, yawDeg = 0, rateDps = 0;   // gyro bias, integrated yaw (deg), turn rate (deg/s)
float heading = 0;                    // target heading of the current straight (multiple of 90)
float turnEndCm = 0;                  // odometry when the last back-up ended
float revStartOdo = 0;                // odometry when the back-up started
float d1Cm = -1, lStartCm = -1, stopAtCm = -1;   // odometry at corner 1, at the start of the last lap (report only)
float iTerm = 0, stallOdo = 0;        // heading integral term, odometry used by the stall check
float latDeg = 0, latI = 0;           // wall-centring correction (+ = left) and its integral
float goalLocal = 0;                  // wanted heading relative to the straight (+ = left)
float passLeftCm = 0;                 // cm still to drive in PASS mode
int   dir = 0;                        // +1 = left (blue line), -1 = right (orange line)
int   corners = 0;                    // corners completed so far
bool  finalLeg = false;               // true after the last corner: stop at the next line
bool  finalLineSeen = false;          // the last line has been seen: drive FINAL_EXTRA_S and stop
unsigned long finalLineMs = 0;        // time at which the last line was seen
unsigned long lineIgnoreUntil = 0;    // lines are ignored until this millis() value
LineLock lineLock = LOCK_NONE;        // line colour chosen at the first line
bool  sameLinePrev = false;           // was the locked line visible in the previous message (edge detection)
int   camBlue = 0, camOrange = 0;     // what the camera sees right now (1 = line present)
int   camColor = 0, camDistCm = -1, camLatCm = 0;   // pillar in the last camera message
bool  newCamData = false;             // a new camera message has arrived and is not used yet
unsigned long lastCamMs = 0;          // time of the last valid camera message
char  camBuf[64]; uint8_t camLen = 0; bool inCam = false;   // serial receive buffer for one camera line
unsigned long lastMicros = 0, lastRamp = 0, lastPrint = 0, phaseStart = 0, doneAt = 0;
unsigned long lastCtrl = 0, stallSince = 0, runStart = 0;
char  cmd = 0;                        // last key typed in the serial monitor

// map of the current straight
float carX = 0, carY = 0;             // rear axle position on this straight (cm)
float lastMapOdo = 0;                 // odometry the last time the map was updated
float segCm = 0, lastSegCm = 0;       // distance driven on this straight
MapPillar current = {false, 0, 0, 0, 0};   // the pillar we are avoiding now
MapPillar nextOne = {false, 0, 0, 0, 0};   // the next pillar, if one has been seen ahead

// ---------------- motor / encoder / servo ----------------
// Encoder interrupt: channel C1 rose; C2 tells the direction.
void encoderISR() { if (digitalRead(ENC_C2_PIN)) encRaw++; else encRaw--; }
// Distance driven by the rear wheels since the last reset, in cm (sign set by ENCODER_REVERSED).
float odoCm() { noInterrupts(); long c = encRaw; interrupts(); return (ENCODER_REVERSED ? -c : c) / TICKS_PER_CM; }
// Set the motor PWM and direction. In reverse gear (driveBack) the two direction pins are swapped.
void motorOut(int pwm) {
  pwm = constrain(pwm, 0, 255);
  bool a = pwm > 0, b = false;
  if (MOTOR_REVERSED) { b = a; a = false; }
  if (driveBack) { bool t = a; a = b; b = t; }     // swap direction to drive backwards
  digitalWrite(AIN1_PIN, a ? HIGH : LOW); digitalWrite(AIN2_PIN, b ? HIGH : LOW);
  analogWrite(PWMA_PIN, pwm); pwmNow = pwm;
}
// Active brake: both direction pins high and full PWM.
void brake() { digitalWrite(AIN1_PIN, HIGH); digitalWrite(AIN2_PIN, HIGH); analogWrite(PWMA_PIN, 255); pwmNow = 0; pwmTarget = 0; }
// Move the steering servo, only writing when the angle really changes.
void servoTo(int a) { a = constrain(a, STEER_LEFT_MAX, STEER_RIGHT_MAX); if (a != servoNow) { steering.write(a); servoNow = a; } }

// ---------------- BMI160 (gyro) ----------------
// True (and clears the flag) if the I2C bus timed out since the last check.
bool i2cTimedOut() { if (Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); return true; } return false; }
// Write one register of the BMI160. Returns false on an I2C error or timeout.
bool bmiWrite(uint8_t reg, uint8_t val) { Wire.beginTransmission(BMI); Wire.write(reg); Wire.write(val); return Wire.endTransmission() == 0 && !i2cTimedOut(); }
// Read n bytes starting at register reg. Returns false on an I2C error or timeout.
bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(BMI); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) { i2cTimedOut(); return false; }
  if (Wire.requestFrom(BMI, n) != n) { i2cTimedOut(); return false; }
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return !i2cTimedOut();
}
// Read the gyro Z axis (registers 0x10-0x11, little endian).
bool readGyroZ(int16_t &raw) { uint8_t b[2]; if (!bmiRead(0x10, b, 2)) return false; raw = (int16_t)((b[1] << 8) | b[0]); return true; }
// Reset the BMI160, switch on the gyro and check the chip ID (0xD1) and power mode. Up to 5 tries.
bool bmiStart() {
  for (int i = 0; i < 5; i++) {
    bmiWrite(0x7E, 0xB6); delay(100);             // soft reset
    bmiWrite(0x7E, 0x15); delay(100);             // gyro in normal mode
    bmiWrite(0x43, 0x02); delay(50);              // gyro range setting
    uint8_t id = 0, st = 0;
    if (bmiRead(0x00, &id, 1) && id == 0xD1 && bmiRead(0x03, &st, 1) && ((st >> 2) & 3) == 1) return true;
  }
  return false;
}
// Measure the gyro zero offset (bias) while the car is still. The LED blinks during the measurement.
void calibrate() {
  long sum = 0; int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 700) { int16_t r; readGyroZ(r); delay(5); }   // settle the sensor first
  for (int i = 0; i < 300; i++) {                 // 300 samples
    int16_t r;
    if (readGyroZ(r)) { sum += r; n++; }
    delay(5);
    digitalWrite(LED_PIN, (millis() / 80) % 2);   // blink
  }
  if (n >= 250) biasDps = (sum / (float)n) / GYRO_LSB_PER_DPS;   // need at least 250 good samples
  yawDeg = 0; lastMicros = micros();
  Serial.print(F("Gyro bias ")); Serial.print(biasDps, 3); Serial.print(F(" (")); Serial.print(n); Serial.println(F("/300)"));
  digitalWrite(LED_PIN, HIGH);
}
// Read the gyro and integrate the yaw angle (degrees) from the turn rate.
void updateGyro() {
  unsigned long now = micros(); float dt = (now - lastMicros) / 1000000.0; lastMicros = now;
  int16_t raw;
  if (readGyroZ(raw)) { rateDps = YAW_SIGN * GYRO_SCALE * (raw / GYRO_LSB_PER_DPS - biasDps); yawDeg += rateDps * dt; }
}

// ---------------- side, rear and front ToF sensors ----------------
// Give one ToF its own I2C address (the sensor is first on the default address 0x29, then moved).
bool startOne(Side &t) {
  pinMode(t.xshut, INPUT); delay(10);             // release the sensor from shutdown
  t.dev.setAddress(0x29); t.dev.setTimeout(200);
  if (!t.dev.init()) { i2cTimedOut(); return false; }
  t.dev.setAddress(t.addr);                       // move it to its own address
  t.dev.setDistanceMode(VL53L1X::Long);           // long range mode
  t.dev.setMeasurementTimingBudget(TOF_BUDGET_US);
  t.dev.setROISize(16, 8);                        // region of interest: 16 x 8 SPAD
  t.dev.startContinuous(TOF_BUDGET_US / 1000);
  t.lastData = millis();
  return true;
}
// Keep the rear and front ToF sensors in shutdown (used while the side ones are configured).
void tofExtraOff() {
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);
}
// Start all four ToF sensors one by one, so each one gets its own address. Returns true if both side sensors work.
bool tofStart() {
  tofExtraOff();                      // rear and front stay off while the side sensors are configured
  for (int i = 0; i < 2; i++) { pinMode(tofs[i].xshut, OUTPUT); digitalWrite(tofs[i].xshut, LOW); }
  delay(10);
  bool a = startOne(tofs[0]); bool b = startOne(tofs[1]);   // left and right first
  rearOk  = startOne(rearTof);        // switched on after left and right
  frontOk = startOne(frontTof);       // switched on after the rear one
  if (!rearOk)  { i2cTimedOut(); }
  if (!frontOk) { i2cTimedOut(); }
  return a && b;
}
// Read every ToF that has new data. A sensor that has not answered for 500 ms triggers a full restart.
void updateTof() {
  bool restart = false;
  for (int i = 0; i < 2; i++) {                   // left and right
    Side &t = tofs[i]; t.fresh = false;
    if (tofOk && t.dev.dataReady()) {
      t.dev.read(false);
      if (!i2cTimedOut()) {
        uint16_t v = t.dev.ranging_data.range_mm;
        t.mm = (v > 0 && v < 4000) ? v : 4000;   // 0 or too big = nothing in range (4000 mm)
        t.fresh = true; t.lastData = millis();
      }
    }
    if (millis() - t.lastData > 500) restart = true;
  }
  if (rearOk && rearTof.dev.dataReady()) {        // rear
    rearTof.dev.read(false);
    if (!i2cTimedOut()) {
      uint16_t v = rearTof.dev.ranging_data.range_mm;
      rearTof.mm = (v > 0 && v < 4000) ? v : 4000;
      rearTof.lastData = millis();
    }
  }
  if (frontOk && frontTof.dev.dataReady()) {      // front
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
// Parse one camera line: "$col,x,bottom,ok,bf,br,seq,w,d,lat,of,orow" (12 fields).
// Only the fields used here are stored: bf/of (line seen), col/d/lat (pillar).
void handleCameraLine() {
  camBuf[camLen] = '\0';
  int col, x, b, ok, bf, br, w, d, lat, of, orow;
  unsigned long seq;
  int n = sscanf(camBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                 &w, &d, &lat, &of, &orow);
  if (n == 12) {                          // a complete, valid message
    camBlue = bf; camOrange = of;          // floor lines (corners)
    camColor = col; camDistCm = d; camLatCm = lat;   // pillar
    lastCamMs = millis();
    newCamData = true;
  }
}
// Read the serial port: camera lines start with '$'; a single key + Enter is a command.
void readSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { inCam = true; camLen = 0; continue; }   // a camera line starts
    if (inCam) {
      if (c == '\n') { handleCameraLine(); inCam = false; camLen = 0; }   // end of the camera line
      else if (c != '\r' && camLen < sizeof(camBuf) - 1) camBuf[camLen++] = c;
      continue;
    }
    if (c == '\n' || c == '\r') {         // end of a command line
      if ((cmd == 's' || cmd == 'S') && (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE)) finish(F("STOPPED (s)"));
      else if ((cmd == 'g' || cmd == 'G') && (state == WAIT || state == DONE)) startRun();
      cmd = 0;
    } else cmd = c;
  }
}
// True if the line colour we locked onto is visible now.
bool sameColorVisible() {
  if (lineLock == LOCK_BLUE)   return camBlue != 0;
  if (lineLock == LOCK_ORANGE) return camOrange != 0;
  return false;
}
// True if a line should be counted now (locked colour only, or any colour if ONLY_LOCKED_COLOR is false).
bool lineVisibleForCount() {
  if (ONLY_LOCKED_COLOR) return sameColorVisible();
  return camBlue != 0 || camOrange != 0;
}

// ---------------- local heading and straight map ----------------
float yawL() { return yawDeg - heading; }     // deviation from the straight's heading (+ = left)
// Position of the camera lens on the map (the camera is CAM_FWD_CM in front of the rear axle).
float camPosX() { return carX + CAM_FWD_CM * cos(radians(yawL())); }
float camPosY() { return carY + CAM_FWD_CM * sin(radians(yawL())); }

// Move the car on the map using the distance driven since the last call.
void updateMap() {
  float o = odoCm();
  float ds = o - lastMapOdo; lastMapOdo = o;
  float h = radians(yawL());
  carX += ds * cos(h);
  carY += ds * sin(h);
  segCm += ds;
}

// Convert a map point into the car's frame: fwd = ahead of the camera, left = left of the camera.
void relativeToCar(float px, float py, float &fwd, float &left) {
  float h = radians(yawL());
  float dx = px - camPosX(), dy = py - camPosY();
  fwd  =  dx * cos(h) + dy * sin(h);
  left = -dx * sin(h) + dy * cos(h);
}

// Place the pillar from the last camera message on the map (or update the one already stored).
void onCameraPillar() {
  if (camColor == 0 || camDistCm <= 0) return;      // no pillar in this message
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;  // too far to the side: noise

  float fwd  = camDistCm + PILLAR_CM / 2;        // distance to the pillar centre
  float left = -camLatCm;                        // camera: + = right; map: + = left
  float h = radians(yawL());
  float cx = camPosX(), cy = camPosY();
  float px = cx + fwd * cos(h) - left * sin(h);  // pillar position on the map
  float py = cy + fwd * sin(h) + left * cos(h);

  MapPillar *slots[2] = { &current, &nextOne };
  for (int i = 0; i < 2; i++) {
    MapPillar *p = slots[i];
    if (p->used && p->color == camColor && hypot(p->x - px, p->y - py) < SAME_PILLAR_CM) {
      p->x += BLEND * (px - p->x);               // same pillar seen again: move the stored position a little
      p->y += BLEND * (py - p->y);
      p->hits++;
      return;
    }
  }
  MapPillar fresh = {true, px, py, camColor, 1};   // a new pillar
  if (!current.used) {
    current = fresh;
    Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN"));
  } else if (px > current.x + SAME_PILLAR_CM) {
    nextOne = fresh;                             // a pillar further ahead: keep it as the next one
  }
}

// Side to pass a pillar on: red on its RIGHT (the pillar ends up on the car's left), green on its LEFT.
float goalFor(int color, float pillarY) {
  float cy = camPosY();
  if (color == 1) return min(cy, pillarY - TARGET_CM);   // red: car passes on the pillar's RIGHT (target y below the pillar)
  return max(cy, pillarY + TARGET_CM);                    // green: car passes on the pillar's LEFT (target y above the pillar)
}

// Returns true and the local steering target (goalOut, degrees) if a pillar has to be avoided now.
bool pillarGoal(float &goalOut) {
  float fwd = 0, left = 0;
  if (current.used) {
    relativeToCar(current.x, current.y, fwd, left);
    if (fwd < -RETURN_AFTER_CM) {                 // the car is far enough past it: it is passed
      Serial.println(F(">> PILLAR PASSED"));
      current = nextOne;                           // the next pillar becomes the current one
      nextOne.used = false;
      if (current.used) relativeToCar(current.x, current.y, fwd, left);
    }
  }
  if (!current.used) return false;

  bool beside = fwd < 0;                          // the car is already level with the pillar
  const MapPillar *target = beside ? (nextOne.used ? &nextOne : NULL) : &current;   // when beside, aim at the next pillar
  if (target == NULL) return false;

  float goalY = goalFor(target->color, target->y);
  float toFace = (target->x - PILLAR_CM / 2) - camPosX() - ARRIVE_BEFORE_CM;   // distance to start the manoeuvre
  float look = max(LOOK_MIN_CM, toFace);          // look-ahead distance for the aiming angle

  if (beside) {                                   // level with the pillar: never steer back towards it
    if (current.color == 1) goalY = min(goalY, camPosY());
    else                    goalY = max(goalY, camPosY());
  }
  goalOut = constrain(degrees(atan2(goalY - camPosY(), look)), -MAX_AIM_DEG, MAX_AIM_DEG);   // aim angle, clamped
  return true;
}

// Wall centring: returns the heading correction (+ = left) that keeps the car between the two walls.
float laneGoal(float dt) {
  bool okL = tofs[0].mm > 0 && tofs[0].mm < SIDE_MAX_MM;   // a usable left reading
  bool okR = tofs[1].mm > 0 && tofs[1].mm < SIDE_MAX_MM;   // a usable right reading
  if (okL && okR) {
    float latErrMm = (tofs[0].mm - tofs[1].mm) / 2.0;   // + = closer to the right wall -> steer left
    latI = constrain(latI + KI_LAT * latErrMm * dt, -LAT_I_MAX, LAT_I_MAX);
    latDeg = constrain(K_LAT * latErrMm + latI, -LAT_MAX_DEG, LAT_MAX_DEG);
  } else {
    latDeg = latI;                                // one wall missing: keep the last integral value
  }
  if (tofs[0].mm > 0 && tofs[0].mm < SAFE_WALL_MM) latDeg = -LAT_MAX_DEG;   // too close to the left wall: steer right
  if (tofs[1].mm > 0 && tofs[1].mm < SAFE_WALL_MM) latDeg =  LAT_MAX_DEG;   // too close to the right wall: steer left
  return latDeg;
}

// ---------------- corner = line ----------------
// Called when a floor line is seen. Starts the corner sequence (or finishes the run after the last line).
void onCorner(float odo) {
  lineIgnoreUntil = millis() + IGNORE_LINE_S * 1000.0;   // ignore ALL lines for IGNORE_LINE_S
  int n = corners + 1;
  Serial.print(F(">>> CORNER ")); Serial.print(n);
  Serial.print(F(" (line) at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
  if (n == 1) { d1Cm = odo; Serial.print(F(">>> D1 = ")); Serial.print(d1Cm, 1); Serial.println(F(" cm")); }
  if (n == 5 && lStartCm < 0) {
    lStartCm = odo - turnEndCm;
    Serial.print(F(">>> LSTART = ")); Serial.print(lStartCm, 1); Serial.println(F(" cm (end of turn 4 -> corner 5)"));
  }
  if (finalLeg) {                                 // last line: drive FINAL_EXTRA_S and stop (see runTest)
    finalLineSeen = true;
    finalLineMs = millis();
    Serial.println(F(">>> LAST LINE detected: driving FINAL_EXTRA_S, then stop"));
    return;
  }
  state = PRE_TURN; phaseStart = millis(); pwmTarget = RUN_PWM;   // keep driving until FRONT_TURN_CM, then turn
}

// ---------------- stop ----------------
// Brake, centre the steering and enter DONE. The final report is printed about 1 s later.
void finish(const __FlashStringHelper *why) {
  brake(); servoTo(STEER_CENTER); state = DONE; doneAt = millis(); finalPrinted = false;
  Serial.print(F(">>> ")); Serial.print(why);
  Serial.print(F(" | yaw ")); Serial.print(yawDeg, 1);
  Serial.print(F(" | odo ")); Serial.print(odoCm(), 1); Serial.println(F(" cm"));
}

// Start a run: calibrate the gyro (car must be still), reset all counters and drive off.
void startRun() {
  Serial.println(F("Calibrating gyro: do NOT touch the car..."));
  servoTo(STEER_CENTER); delay(300);
  calibrate();
  noInterrupts(); encRaw = 0; interrupts();        // zero the encoder
  while (Serial.available()) Serial.read();        // drop any old serial input
  inCam = false; camLen = 0;
  state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
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
  Serial.println(F(">>> GO"));
}

// ---------------- main logic (state machine) ----------------
// Called every loop. Runs the current state: STRAIGHT, PRE_TURN, TURN, REVERSE or DONE.
void runTest() {
  if (state == STRAIGHT) {
    float dt = (millis() - lastCtrl) / 1000.0; lastCtrl = millis();
    float odo = odoCm();
    bool camOk = (millis() - lastCamMs) <= CAMERA_TIMEOUT_MS;   // camera alive?
    pwmTarget = camOk ? RUN_PWM : 0;                            // no camera -> do not drive

    updateMap();
    float dSeg = segCm - lastSegCm; lastSegCm = segCm;

    // 0) LAST LINE seen FINAL_EXTRA_S ago: stop
    if (finalLineSeen && millis() - finalLineMs >= FINAL_EXTRA_S * 1000.0) {
      finish(F(">>> LAST LINE + extra drive -> STOP"));
      return;
    }

    // 1) TOP PRIORITY: LINE = CORNER -> turn (even if a pillar is in view)
    if (camOk && !finalLineSeen) {
      if (lineLock == LOCK_NONE) {                  // no line locked yet: the first line sets the direction
        if (odo >= MIN_DRIVE_CM && (camBlue || camOrange)) {
          lineLock = camOrange ? LOCK_ORANGE : LOCK_BLUE;
          dir = (lineLock == LOCK_BLUE) ? 1 : -1;   // blue = left, orange = right
          sameLinePrev = true;
          Serial.println(lineLock == LOCK_BLUE ? F(">> FIRST LINE BLUE: turning LEFT")
                                               : F(">> FIRST LINE ORANGE: turning RIGHT"));
          onCorner(odo);
          return;
        }
      } else {
        bool now = lineVisibleForCount();
        bool rising = now && !sameLinePrev;         // a new line appears (rising edge)
        sameLinePrev = now;                         // always updated, even when ignored
        if (rising && millis() >= lineIgnoreUntil) { onCorner(odo); return; }
      }
    }

    // 2) SECOND PRIORITY: PILLARS. Avoid if needed; keep the heading if just passed; otherwise centre between the walls
    if (newCamData) { newCamData = false; if (camOk) onCameraPillar(); }
    float g = 0;
    if (pillarGoal(g)) {
      goalLocal = g; passLeftCm = PASS_CM; mode = MODE_BLOCK;     // avoid the pillar
    } else if (passLeftCm > 0) {
      passLeftCm -= dSeg; mode = MODE_PASS;           // just passed: goalLocal stays the same
    } else {
      goalLocal = laneGoal(dt); mode = MODE_LANE;     // no pillar: centre between the walls
    }

    float err = goalLocal - yawL();                   // heading error
    if (mode == MODE_LANE && pwmNow >= RUN_PWM) iTerm = constrain(iTerm + KI_HEAD * err * dt, -I_MAX, I_MAX);   // integral only in lane mode
    float steer = KP_HEAD * err - KD_HEAD * rateDps + iTerm;   // PID-like steering (P + gyro damping + I)
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    // 3) After the last turn: keep going until the last line. Safety by time.
    if (finalLeg && !finalLineSeen) {
      if (!finalLeg && millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }
    }
    if (millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }

  } else if (state == PRE_TURN) {
    // Line seen: keep straight on the current heading, ignoring pillars and lines,
    // until the front ToF reads FRONT_TURN_CM (or, without a front ToF, after LINE_WAIT_S)
    newCamData = false;
    pwmTarget = RUN_PWM;
    float steer = KP_HEAD * (-yawL()) - KD_HEAD * rateDps;   // hold the heading only
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    bool frontFresh = frontOk && frontTof.mm > 0 && (millis() - frontTof.lastData < 300);
    bool readyToTurn = frontOk
                       ? (frontFresh && frontTof.mm <= FRONT_TURN_CM * 10)   // mm
                       : (millis() - phaseStart >= LINE_WAIT_S * 1000.0);

    if (readyToTurn) {
      // switch to reverse gear: brake first, then turn while backing up
      brake();
      driveBack = true;
      state = TURN; phaseStart = millis(); pwmTarget = TURN_PWM;
    } else if (millis() - phaseStart > MAX_PRE_TURN_MS) {
      finish(F("FRONT never reached FRONT_TURN_CM -> STOP"));
    }

  } else if (state == TURN) {
    newCamData = false;                            // no pillars are mapped while turning
    // When reversing, the wheels turn the opposite way: for a left turn, steer right
    servoTo(dir > 0 ? STEER_RIGHT_MAX : STEER_LEFT_MAX);
    float predicted = dir * (yawDeg + rateDps * TURN_PREDICT_S - heading);   // yaw turned so far (+ = in the turn direction)
    if (predicted >= 90) {                          // 90 degrees done (predicted a little early)
      heading += dir * 90.0;
      corners++;
      if (corners >= 4 * LAPS) {                    // all corners done: go for the last line
        finalLeg = true;
        stopAtCm = -1;                             // no longer stop by distance
        Serial.println(F(">>> FINAL: drive to the last line, then FINAL_EXTRA_S, then stop"));
      }
      // after the turn: back up until REAR_STOP_CM on the rear ToF
      brake();
      servoTo(STEER_CENTER);
      revStartOdo = odoCm();
      driveBack = true;
      state = REVERSE; phaseStart = millis(); pwmTarget = REVERSE_PWM;
      goalLocal = 0; passLeftCm = 0; mode = MODE_LANE;
      current.used = false; nextOne.used = false;   // new straight, new map
      Serial.print(F(">>> TURN ")); Serial.print(corners); Serial.print(F(" done | yaw ")); Serial.print(yawDeg, 1);
      Serial.print(F(" -> heading ")); Serial.println(heading, 0);
    } else if (millis() - phaseStart > MAX_TURN_MS) {
      finish(F("TURN TOO LONG -> STOP"));
    }

  } else if (state == REVERSE) {
    newCamData = false;                            // no pillars are mapped and no lines counted while backing up
    servoTo(STEER_CENTER);
    // Stop with the rear ToF (fresh, under 300 ms old); without data, use the odometry
    bool rearFresh = rearOk && rearTof.mm > 0 && (millis() - rearTof.lastData < 300);
    bool reached = rearFresh
                   ? (rearTof.mm <= REAR_STOP_CM * 10)
                   : (odoCm() <= revStartOdo - REVERSE_CM);
    if (reached) {
      brake();
      driveBack = false;
      // new straight: map, pillars and line count start here
      carX = carY = 0; segCm = 0; lastSegCm = 0; lastMapOdo = odoCm();
      turnEndCm = odoCm();
      goalLocal = 0; mode = MODE_LANE;
      sameLinePrev = lineVisibleForCount();
      state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
      Serial.println(F(">>> BACK-UP done -> driving on"));
    } else if (millis() - phaseStart > MAX_REVERSE_MS) {
      finish(F("REVERSE TOO LONG -> STOP"));
    }
  }

  // safety stop: motor on but wheels not moving
  if (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE) {
    float o = odoCm();
    if (fabs(o - stallOdo) > 1.0 || pwmNow < 60) { stallOdo = o; stallSince = millis(); }
    else if (millis() - stallSince > STALL_MS) finish(F("STALLED: wheels not turning (hit a wall?) -> STOP"));
  }

  // final report, printed once, about 1 s after stopping
  if (state == DONE && !finalPrinted && millis() - doneAt > 1000) {
    finalPrinted = true; motorOut(0);
    Serial.print(F(">>> FINAL yaw: ")); Serial.print(yawDeg, 1);
    Serial.print(F(" | heading ")); Serial.print(heading, 0);
    Serial.print(F(" | corners ")); Serial.print(corners);
    Serial.print(F(" | D1 ")); Serial.print(d1Cm, 1);
    Serial.print(F(" | LSTART ")); Serial.print(lStartCm, 1);
    Serial.print(F(" | time ")); Serial.print((doneAt - runStart) / 1000.0, 1); Serial.println(F(" s"));
  }
}

// ---------------- setup / loop ----------------
// Runs once at power-on: configure pins, start the sensors and the gyro, calibrate the gyro.
void setup() {
  pinMode(PWMA_PIN, OUTPUT); pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT); motorOut(0);
  tofExtraOff();
  pinMode(START_BTN_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_C1_PIN, INPUT_PULLUP); pinMode(ENC_C2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_C1_PIN), encoderISR, RISING);
  steering.attach(SERVO_PIN); steering.write(STEER_CENTER);
  Serial.begin(115200); delay(500);
  Serial.println(F("\nBOOT - OBSTACLE CHALLENGE"));
  Wire.begin(); Wire.setClock(100000); Wire.setWireTimeout(25000, true);   // I2C at 100 kHz, 25 ms bus timeout
  tofs[0].xshut = XSHUT_LEFT;  tofs[0].addr = ADDR_LEFT;  tofs[0].name = "LEFT";
  tofs[1].xshut = XSHUT_RIGHT; tofs[1].addr = ADDR_RIGHT; tofs[1].name = "RIGHT";
  rearTof.xshut = XSHUT_REAR;  rearTof.addr = ADDR_REAR;  rearTof.name = "REAR";
  frontTof.xshut = XSHUT_FRONT; frontTof.addr = ADDR_FRONT; frontTof.name = "FRONT";
  for (int i = 0; i < 5 && !tofOk; i++) { tofOk = tofStart(); if (!tofOk) delay(300); }   // up to 5 tries
  bool g = bmiStart();
  Serial.println(g ? F("BMI160 OK") : F("WARNING: BMI160 not ready (battery ON first?)"));
  Serial.println(tofOk ? F("Both TOF OK") : F("WARNING: a TOF is missing"));
  Serial.println(rearOk ? F("REAR TOF OK") : F("WARNING: REAR TOF missing (reverse by odometry)"));
  Serial.println(frontOk ? F("FRONT TOF OK") : F("WARNING: FRONT TOF missing (turn by time)"));
  calibrate();
  Serial.println(F("Ready. Press START or g + Enter. s + Enter = stop."));
}

// Main loop: read inputs (serial, start button, gyro, ToF), run the state machine, ramp the motor, print status.
void loop() {
  readSerialInput();

  if ((state == WAIT || state == DONE) && digitalRead(START_BTN_PIN) == LOW) {   // start button pressed
    delay(30);                                    // debounce
    if (digitalRead(START_BTN_PIN) == LOW) {
      while (digitalRead(START_BTN_PIN) == LOW) delay(5);   // wait for release
      delay(300);
      startRun();
    }
  }

  updateGyro();
  updateTof();
  runTest();

  unsigned long now = millis();
  if (now - lastRamp >= 20) {                     // motor ramp every 20 ms (soft start / soft stop)
    lastRamp = now;
    bool moving = (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE);
    int target = moving ? pwmTarget : 0;
    driveBack = (state == TURN || state == REVERSE);   // turn and back-up run in reverse gear
    if (state == DONE) { /* brake */ }
    else if (pwmNow < target) motorOut(min(pwmNow + RAMP_STEP, target));
    else if (pwmNow > target) motorOut(target);
  }

  if (now - lastPrint >= 100 && state != WAIT) {  // status line every 100 ms
    lastPrint = now;
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
