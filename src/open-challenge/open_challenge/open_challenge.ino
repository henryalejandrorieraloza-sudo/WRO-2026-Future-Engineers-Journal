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
// Press START (D10). Do NOT touch it while the LED blinks (gyro calibration).
// There is no USB command during a run (rules 11.6-11.7). Battery ON FIRST, then USB (upload only).
// Board: Arduino Nano | TB6612: PWMA D6, AIN1 D7, AIN2 D8 | Encoder C1 D3, C2 D2 | Servo D9 | START D10
// BMI160 0x69 (A4/A5) | TOF left XSHUT A3 -> 0x2A | TOF right XSHUT A2 -> 0x2B | LED D13
// TOF front XSHUT A1 + TOF rear XSHUT A0: SOLDERED BUT OFF (XSHUT held LOW the whole time, never
//   started, never read). They stay off the I2C bus so they do not clash with left/right at 0x29.
// Libraries: VL53L1X by Pololu, Servo

#include <Wire.h>                     // I2C bus library (gyro and ToF sensors)
#include <VL53L1X.h>                  // library for the VL53L1X distance sensors (TOF400C)
#include <Servo.h>                    // library for the steering servo

// #####################################################################################
// ##  1. RUN SETTINGS  - change these freely for the laps                            ##
// #####################################################################################
// --- Race ---
const int   LAPS     = 3;             // 1 = one lap (4 turns), 3 = the full Open Challenge (12 turns)
const int   RUN_PWM  = 150;           // speed on the straights (0-255). Raise in steps of ~20 if the run is too slow
const int   TURN_PWM = 90;            // speed in the corners. Lower values were too weak to turn the car; 90 is the value in use
const float STOP_OFFSET_CM = 10;      // final stop: + = stop later (further), - = stop earlier. 10 is the value the team uses (24 Nov)

// --- Where the car drives in the corridor (lane) ---
const int   LANE_MODE = 1;            // 0 = CENTER of the corridor (uses both walls)
                                      // 1 = fixed distance to the INNER wall (INNER_TARGET_MM)
const int   INNER_TARGET_MM = 100;    // LANE_MODE 1: distance to keep from the inner wall (mm)
                                      //   smaller = closer to the inner wall. 100 is the value used in the 3-lap run (22 Nov)
const int   CENTER_SHIFT_MM = 200;    // LANE_MODE 0: move the "center" toward the INNER wall by this
                                      //   many mm (0 = exact center, 100 = 10 cm closer to the inner wall)
const int   SAFE_WALL_MM = 70;        // any wall closer than this: steer away at full correction

// --- Corners ---
const int   JUMP_DELTA_MM = 250;      // corner = inner reading this much above its normal distance...
const int   JUMP_ABS_MM   = 1200;     // ...or above this
const int   JUMP_CONFIRM  = 1;        // readings in a row (1 = faster reaction at high speed, more risk)
const float TURN_PREDICT_S = 0.12;    // ends the turn early by (turn rate x this). Overshoots 90 -> raise it
const float IGNORE_AFTER_TURN_CM = 60; // no corner detection right after a turn

// #####################################################################################
// ##  2. TUNING - change only if the car zig-zags, drifts or misses corners          ##
// #####################################################################################
const float KP_HEAD = 2.0;            // straight: servo degrees per degree of heading error (zig-zag -> lower)
const float KI_HEAD = 4.0;            // learns the servo angle that keeps the car straight (no fixed trim)
const float I_MAX   = 25;             // limit of that integral (servo degrees)
const float K_LAT       = 0.03;       // lane: heading degrees per mm of lateral error (100 mm -> 3 deg)
const float KI_LAT      = 0.02;       // lane integral: removes the slow gyro error
const float LAT_I_MAX   = 8;          // limit of the lane integral (degrees)
const float LAT_MAX_DEG = 12;         // max heading change used to move in the corridor
const int   SIDE_MAX_MM = 1200;       // a side reading above this is not a corridor wall
const int   ARM_WALL_MM  = 900;       // after a turn, the inner wall must be seen closer than this...
const int   ARM_READINGS = 5;         // ...this many readings in a row before the next corner counts
const float TURN_LEAD_DEG  = 0;       // extra fixed margin to end the turn earlier (degrees)
const float MIN_DRIVE_CM   = 10;      // no corner detection in the first cm after START
const float FALLBACK_CM    = 50;      // final stop if nothing could be measured
const float MAX_FINAL_CM   = 250;     // safety: never drive more than this after the last turn
const unsigned long MAX_TURN_MS = 4000;       // a turn longer than this -> stop
const unsigned long MAX_STRAIGHT_MS = 8000;   // no corner in 8 s -> stop
const unsigned long STALL_MS = 500;           // motor ON but wheels not turning this long -> stop

// #####################################################################################
// ##  3. CAR CONSTANTS - measured on this car. Change only if the hardware changes   ##
// #####################################################################################
const bool  MOTOR_REVERSED   = true;  // motor wires not swapped yet (forward in code)
const bool  ENCODER_REVERSED = true;  // encoder counts backward: flip the sign of the odometry
const int   YAW_SIGN         = 1;     // +1: turning LEFT makes yaw go UP
const float GYRO_SCALE       = 1.007; // gyro read ~0.7 % less than the real rotation (3-lap test)
const int   STEER_CENTER = 85, STEER_RANGE = 80, STEER_LEFT_MAX = STEER_CENTER - STEER_RANGE, STEER_RIGHT_MAX = STEER_CENTER + STEER_RANGE;   // wheels straight = 85 (measured 21 Nov). Range 80 = average of 75 (left) and 85 (right) measured for 40 deg -> 5 / 165. LEFT = smaller angle
const int   RAMP_STEP = 5;            // PWM per 20 ms (soft start)
const float TICKS_PER_CM = 9.27;      // encoder ticks per cm, measured 21 Nov WITH THE MOTOR RUNNING (190 ticks = 20.5 cm)
const uint8_t BMI = 0x69;             // I2C address of the BMI160 gyro
const float GYRO_LSB_PER_DPS = 65.6;  // gyro raw counts per degree/second (sensitivity of the range used)
const uint32_t TOF_BUDGET_US = 50000; // ToF measurement time: 50 ms
const int PWMA_PIN = 6, AIN1_PIN = 7, AIN2_PIN = 8;   // TB6612FNG motor driver pins
const int ENC_C1_PIN = 3, ENC_C2_PIN = 2;             // encoder channels (C1 = interrupt, C2 = direction)
const int SERVO_PIN = 9, START_BTN_PIN = 10, LED_PIN = 13;   // servo, start button and status LED
const int XSHUT_LEFT = A3, XSHUT_RIGHT = A2;          // XSHUT (reset) pins of the left and right ToF
const uint8_t ADDR_LEFT = 0x2A, ADDR_RIGHT = 0x2B;    // new I2C addresses given to left and right ToF
const int XSHUT_FRONT = A1, XSHUT_REAR = A0;   // front/rear TOF: kept OFF (XSHUT LOW). Change pins if wired elsewhere
// #####################################################################################

// Types first (before any function), so the Arduino IDE can use them in its prototypes.
struct Side {                         // everything about one side distance sensor
  VL53L1X dev; int xshut; uint8_t addr; const char *name;   // the sensor object, its XSHUT pin, address, name
  int mm = -1; bool fresh = false; unsigned long lastData = 0;   // last distance, is it new?, time of last data
  float normal = -1;          // average distance of this side while there is a wall
  int jumpCount = 0; int nearCount = 0;   // readings in a row that show a jump / that are close
};
enum State { WAIT, STRAIGHT, TURN, DONE };   // program states: waiting, driving straight, turning, finished
const char *STATE_NAME[] = { "WAIT", "STRAIGHT", "TURN", "DONE" };   // names for the printout

Side tofs[2];                 // [0] = LEFT, [1] = RIGHT
Servo steering;               // the steering servo object
State state = WAIT;           // current state, starts waiting for START
volatile long encRaw = 0;     // raw encoder count, changed inside the interrupt
int   pwmNow = 0, pwmTarget = 0, servoNow = STEER_CENTER, turnSide = -1;   // motor speed now / wanted, servo angle, turn side
float biasDps = 0, yawDeg = 0, rateDps = 0;   // gyro zero offset, heading in degrees, turn rate in deg/s
unsigned long lastMicros = 0, lastRamp = 0, lastPrint = 0, phaseStart = 0, doneAt = 0;   // timers
bool  tofOk = false, finalPrinted = true;     // ToF working?, final message already printed?
float iTerm = 0, stallOdo = 0; unsigned long lastCtrl = 0, stallSince = 0;   // heading integral, stall check
int   dir = 0, corners = 0;          // dir: +1 inner LEFT (counter-clockwise), -1 inner RIGHT (clockwise)
float heading = 0, turnEndCm = 0;     // heading = target, exact multiple of 90
float d1Cm = -1, armCm = -1, wallCm = -1, stopAtCm = -1, lStartCm = -1;   // start position
float latDeg = 0, latI = 0;           // corridor-center correction (+ = move left) and its integral
unsigned long runStart = 0;           // time when GO was pressed
bool  finalLeg = false;               // true after the last turn: now looking for the start position

// ---------------- motor / encoder / servo ----------------
void encoderISR() { if (digitalRead(ENC_C2_PIN)) encRaw++; else encRaw--; }   // count +1 or -1 depending on channel C2
float odoCm() { noInterrupts(); long c = encRaw; interrupts(); return (ENCODER_REVERSED ? -c : c) / TICKS_PER_CM; }   // distance travelled in cm
void motorOut(int pwm) {              // sets the motor speed (0-255) forward
  pwm = constrain(pwm, 0, 255);       // keep it in range
  bool a = pwm > 0, b = false;        // a = AIN1 wanted high (forward), b = AIN2
  if (MOTOR_REVERSED) { b = a; a = false; }   // swap the two inputs if the motor is wired backwards
  digitalWrite(AIN1_PIN, a ? HIGH : LOW); digitalWrite(AIN2_PIN, b ? HIGH : LOW);   // set the direction pins
  analogWrite(PWMA_PIN, pwm); pwmNow = pwm;   // set the speed and remember it
}
void brake() { digitalWrite(AIN1_PIN, HIGH); digitalWrite(AIN2_PIN, HIGH); analogWrite(PWMA_PIN, 255); pwmNow = 0; pwmTarget = 0; }   // active brake, both inputs high
void servoTo(int a) { a = constrain(a, STEER_LEFT_MAX, STEER_RIGHT_MAX); if (a != servoNow) { steering.write(a); servoNow = a; } }   // move the wheels only if the angle changed

// ---------------- BMI160 ----------------
bool i2cTimedOut() { if (Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); return true; } return false; }   // true if the I2C bus hung (and clear the flag)
bool bmiWrite(uint8_t reg, uint8_t val) { Wire.beginTransmission(BMI); Wire.write(reg); Wire.write(val); return Wire.endTransmission() == 0 && !i2cTimedOut(); }   // write one register, true if OK
bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t n) {   // read n bytes starting at register reg
  Wire.beginTransmission(BMI); Wire.write(reg);     // address the gyro and say which register
  if (Wire.endTransmission(false) != 0) { i2cTimedOut(); return false; }   // no answer: fail
  if (Wire.requestFrom(BMI, n) != n) { i2cTimedOut(); return false; }     // not all bytes came: fail
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();   // store the bytes
  return !i2cTimedOut();              // false if the bus hung during the read
}
bool readGyroZ(int16_t &raw) { uint8_t b[2]; if (!bmiRead(0x10, b, 2)) return false; raw = (int16_t)((b[1] << 8) | b[0]); return true; }   // read gyro Z (register 0x10/0x11) as a signed number
bool bmiStart() {                     // starts the gyro, tries up to 5 times
  for (int i = 0; i < 5; i++) {
    bmiWrite(0x7E, 0xB6); delay(100); // soft reset
    bmiWrite(0x7E, 0x15); delay(100); // power up accelerometer and gyro in normal mode
    bmiWrite(0x43, 0x02); delay(50);  // gyro range setting
    uint8_t id = 0, st = 0;
    if (bmiRead(0x00, &id, 1) && id == 0xD1 && bmiRead(0x03, &st, 1) && ((st >> 2) & 3) == 1) return true;   // chip ID is 0xD1 and gyro is in normal mode: OK
  }
  return false;                       // not found after 5 tries
}
void calibrate() {                    // measures the gyro zero offset while the car is still
  long sum = 0; int n = 0;            // total of the readings and how many were good
  unsigned long t0 = millis();        // start time
  while (millis() - t0 < 700) { int16_t r; readGyroZ(r); delay(5); }   // first 0.7 s: let the gyro settle, values ignored
  for (int i = 0; i < 300; i++) {     // then 300 readings
    int16_t r;
    if (readGyroZ(r)) { sum += r; n++; }   // keep the good ones
    delay(5);                         // about 5 ms each
    digitalWrite(LED_PIN, (millis() / 80) % 2);   // blink the LED while calibrating
  }
  if (n >= 250) biasDps = (sum / (float)n) / GYRO_LSB_PER_DPS;   // offset in deg/s, only if most readings were good
  yawDeg = 0; lastMicros = micros();  // heading starts at zero
  Serial.print(F("Gyro bias ")); Serial.print(biasDps, 3); Serial.print(F(" (")); Serial.print(n); Serial.println(F("/300)"));
  digitalWrite(LED_PIN, HIGH);        // LED on = calibration done
}
void updateGyro() {                   // adds the rotation since the last call to the heading
  unsigned long now = micros(); float dt = (now - lastMicros) / 1000000.0; lastMicros = now;   // time since last call, in seconds
  int16_t raw;
  if (readGyroZ(raw)) { rateDps = YAW_SIGN * GYRO_SCALE * (raw / GYRO_LSB_PER_DPS - biasDps); yawDeg += rateDps * dt; }   // rate in deg/s, then add rate x time
}

// ---------------- both TOF ----------------
bool startOne(Side &t) {              // starts one side sensor and gives it its address
  pinMode(t.xshut, INPUT); delay(10); // release its reset pin: the sensor turns on at address 0x29
  t.dev.setAddress(0x29); t.dev.setTimeout(200);   // default address, 200 ms timeout
  if (!t.dev.init()) { i2cTimedOut(); return false; }   // no sensor: fail
  t.dev.setAddress(t.addr);           // move it to its own address (0x2A or 0x2B)
  t.dev.setDistanceMode(VL53L1X::Long);            // long range mode
  t.dev.setMeasurementTimingBudget(TOF_BUDGET_US); // 50 ms per measurement
  t.dev.setROISize(16, 8);            // use a 16 x 8 zone of the sensor: avoids the side reflections
  t.dev.startContinuous(TOF_BUDGET_US / 1000);     // measure continuously, every 50 ms
  t.lastData = millis();              // remember that it just started
  return true;
}
// Front and rear TOF are not used: hold their XSHUT LOW so they stay in reset (off the I2C bus).
// If XSHUT were left floating they would wake up at 0x29 and break the left/right TOF start.
void tofExtraOff() {
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);   // front sensor held in reset
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);   // rear sensor held in reset
}
bool tofStart() {                     // starts the left and right sensors one after the other
  tofExtraOff();                      // keep front/rear OFF (also on a TOF restart)
  for (int i = 0; i < 2; i++) { pinMode(tofs[i].xshut, OUTPUT); digitalWrite(tofs[i].xshut, LOW); }   // reset both side sensors
  delay(10);                          // wait for them to power down
  bool a = startOne(tofs[0]); bool b = startOne(tofs[1]);   // start LEFT, then RIGHT (each gets its own address)
  return a && b;                      // true only if both work
}
void updateTof() {                    // reads the new distances, and restarts the sensors if one stops answering
  bool restart = false;               // becomes true if a sensor has been silent too long
  for (int i = 0; i < 2; i++) {       // LEFT and RIGHT
    Side &t = tofs[i]; t.fresh = false;   // no new value yet
    if (tofOk && t.dev.dataReady()) {   // the sensor has a new measurement
      t.dev.read(false);              // read it (false = do not wait)
      if (!i2cTimedOut()) {           // the read worked
        uint16_t v = t.dev.ranging_data.range_mm;   // distance in mm
        t.mm = (v > 0 && v < 4000) ? v : 4000;    // 0 or >= 4000 = nothing in range = far
        t.fresh = true; t.lastData = millis();   // mark as new
      }
    }
    if (millis() - t.lastData > 500) restart = true;   // no data for 0.5 s: something is wrong
  }
  if (restart) {                      // restart both sensors
    tofOk = tofStart();               // try to start them again
    Serial.print(F("!!! TOF RESTART -> ")); Serial.println(tofOk ? F("OK") : F("FAILED"));
    tofs[0].lastData = tofs[1].lastData = millis();   // give them time before the next check
  }
}

// ---------------- test ----------------
void finish(const __FlashStringHelper *why) {   // stops the car and prints why
  brake(); servoTo(STEER_CENTER); state = DONE; doneAt = millis(); finalPrinted = false;   // brake, wheels straight, state DONE
  Serial.print(F(">>> ")); Serial.print(why);   // reason for stopping
  Serial.print(F(" | yaw ")); Serial.print(yawDeg, 1);   // heading at the end
  Serial.print(F(" | odo ")); Serial.print(odoCm(), 1); Serial.println(F(" cm"));   // distance travelled
}
void startRun() {                     // called when START is pressed
  Serial.println(F("Calibrating gyro: do NOT touch the car..."));
  servoTo(STEER_CENTER); delay(300);  // wheels straight before calibrating
  calibrate();                        // measure the gyro offset
  noInterrupts(); encRaw = 0; interrupts();   // start the encoder count from zero
  for (int i = 0; i < 2; i++) { tofs[i].normal = -1; tofs[i].jumpCount = 0; tofs[i].nearCount = 0; }   // forget old walls
  turnSide = -1; state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM;   // start driving straight
  dir = 0; corners = 0; heading = 0; turnEndCm = -1000;   // no inner side yet, no corners, heading 0
  d1Cm = -1; armCm = -1; wallCm = -1; stopAtCm = -1; lStartCm = -1; finalLeg = false; latDeg = 0; latI = 0; runStart = millis();   // clear the start-position data
  iTerm = 0; lastCtrl = millis(); stallOdo = 0; stallSince = millis();   // reset the controllers and the stall check
  Serial.println(F(">>> GO"));
}

void runTest() {                      // the main logic: called every loop
  if (state == STRAIGHT) {            // driving straight along a corridor
    // corridor center: only with both walls learned (never right after a turn) and both close enough
    bool okL = tofs[0].normal > 0 && tofs[0].mm < SIDE_MAX_MM, okR = tofs[1].normal > 0 && tofs[1].mm < SIDE_MAX_MM;   // is each wall usable?
    float dt = (millis() - lastCtrl) / 1000.0; lastCtrl = millis();   // time since the last control step, in seconds
    // lateral error in mm: + = the car is RIGHT of where it should be (it must move left)
    bool haveLane = false; float latErrMm = 0;   // is there a usable lane reading? and how far off
    int in = dir > 0 ? 0 : 1;                                      // inner side (valid once dir != 0)
    bool okIn = (in == 0) ? okL : okR;               // is the inner wall usable?
    if (LANE_MODE == 1 && dir != 0 && okIn) {                      // fixed distance to the inner wall
      latErrMm = (in == 0) ? (tofs[0].mm - INNER_TARGET_MM) : (INNER_TARGET_MM - tofs[1].mm);   // error vs the wanted distance
      haveLane = true;
    } else if (okL && okR) {                                       // center (shifted toward the inner wall)
      latErrMm = (tofs[0].mm - tofs[1].mm) / 2.0 + dir * CENTER_SHIFT_MM;   // distance to the middle of the corridor
      haveLane = true;
    }
    if (haveLane) {                   // a usable wall: correct the lane
      latI = constrain(latI + KI_LAT * latErrMm * dt, -LAT_I_MAX, LAT_I_MAX);   // integral term, kept between straights
      latDeg = constrain(K_LAT * latErrMm + latI, -LAT_MAX_DEG, LAT_MAX_DEG);   // heading change needed, limited
    } else latDeg = latI;                                           // no usable wall: keep the learned bias
    if (tofs[0].mm > 0 && tofs[0].mm < SAFE_WALL_MM) latDeg = -LAT_MAX_DEG;    // too close to the LEFT wall: go right
    if (tofs[1].mm > 0 && tofs[1].mm < SAFE_WALL_MM) latDeg =  LAT_MAX_DEG;    // too close to the RIGHT wall: go left
    float err = heading + latDeg - yawDeg;      // heading error: where we want to point minus where we point
    if (pwmNow >= RUN_PWM) iTerm = constrain(iTerm + KI_HEAD * err * dt, -I_MAX, I_MAX);   // learn the straight angle only at full speed
    servoTo(STEER_CENTER - (int)(KP_HEAD * err + iTerm));   // steer: proportional + integral correction
    float odo = odoCm();              // distance travelled so far
    for (int i = 0; i < 2; i++) {     // check LEFT and RIGHT
      Side &t = tofs[i];
      if (!t.fresh) continue;         // no new value: skip
      if (t.normal < 0) {             // no wall learned yet on this side
        t.nearCount = (t.mm < ARM_WALL_MM) ? t.nearCount + 1 : 0;   // count close readings in a row
        if (t.nearCount >= ARM_READINGS) {   // enough close readings: the wall is there
          t.normal = t.mm;            // this distance is the normal distance of this wall
          if (dir != 0 && i == (dir > 0 ? 0 : 1)) {           // the INNER wall starts here
            armCm = odo;              // remember where the inner wall was seen
            Serial.print(F(">>> inner wall seen at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
            if (finalLeg && stopAtCm < 0) {   // last leg and the stop point is not set yet
              float d = (wallCm > 0 && d1Cm >= 0) ? max(wallCm - d1Cm, 0.0f) : FALLBACK_CM;   // LAPS = 1: WALL - D1, else fallback
              stopAtCm = odo + d;     // stop this far after the inner wall is seen
              Serial.print(F(">>> FINAL: drive ")); Serial.print(d, 1); Serial.print(F(" cm more (WALL "));
              Serial.print(wallCm, 1); Serial.print(F(" - D1 ")); Serial.print(d1Cm, 1); Serial.println(F(")"));
            }
          }
        }
        continue;                     // next side
      }
      if (t.mm > t.normal + JUMP_DELTA_MM || t.mm > JUMP_ABS_MM) t.jumpCount++;   // reading jumped away from the wall: count it
      else { t.jumpCount = 0; t.normal = 0.8 * t.normal + 0.2 * t.mm; }          // normal reading: reset the count and update the average
    }
    int side = -1;                    // which side shows a corner (-1 = none)
    if (odo >= MIN_DRIVE_CM) {        // not in the first cm after START
      if (dir == 0) {                 // corner 1: the side that opens is the inner side
        for (int i = 0; i < 2; i++) if (tofs[i].jumpCount >= JUMP_CONFIRM && tofs[1 - i].jumpCount == 0) side = i;
      } else {                        // next corners: only the inner side, once armed
        int in = dir > 0 ? 0 : 1;     // the inner side
        if (odo - turnEndCm >= IGNORE_AFTER_TURN_CM && tofs[in].normal > 0 && tofs[in].jumpCount >= JUMP_CONFIRM) side = in;
      }
    }
    if (side >= 0) {                  // a corner was seen
      Side &t = tofs[side];
      if (dir == 0) dir = (side == 0) ? 1 : -1;   // the first corner sets the inner side (LEFT = +1, RIGHT = -1)
      Serial.print(F(">>> CORNER ")); Serial.print(corners + 1); Serial.print(F(" on ")); Serial.print(t.name);
      Serial.print(F(": normal ")); Serial.print((int)t.normal); Serial.print(F(" mm -> ")); Serial.print(t.mm);
      Serial.print(F(" mm at ")); Serial.print(odo, 1); Serial.println(F(" cm"));
      if (corners == 0) { d1Cm = odo; Serial.print(F(">>> D1 = ")); Serial.print(d1Cm, 1); Serial.println(F(" cm")); }   // D1: start to corner 1
      if (corners == 2 && armCm > 0) { wallCm = odo - armCm; Serial.print(F(">>> WALL = ")); Serial.print(wallCm, 1); Serial.println(F(" cm")); }   // WALL: inner wall seen -> corner 3
      if (corners == 4) { lStartCm = odo - turnEndCm; Serial.print(F(">>> LSTART = ")); Serial.print(lStartCm, 1); Serial.println(F(" cm (turn 4 end -> corner 5)")); }   // LSTART: turn 4 end -> corner 5
      if (finalLeg) finish(F("SAFETY: next corner reached before the start position -> STOP"));   // a corner in the last leg: stop, something is wrong
      else { turnSide = side; state = TURN; phaseStart = millis(); pwmTarget = TURN_PWM; }   // otherwise start the 90 degree turn
    }
    if (state == STRAIGHT && finalLeg) {   // last leg: check the stop point
      if (stopAtCm > 0 && odo >= stopAtCm) finish(F("BACK AT THE START POSITION -> STOP"));   // reached the start position
      else if (odo - turnEndCm > MAX_FINAL_CM) finish(F("SAFETY: final distance too long -> STOP"));   // driving too long
    }
    if (state == STRAIGHT && millis() - phaseStart > MAX_STRAIGHT_MS) finish(F("NO CORNER in 8 s -> STOP"));   // no corner for 8 s: stop
  } else if (state == TURN) {         // turning 90 degrees toward the inner side
    servoTo(dir > 0 ? STEER_LEFT_MAX : STEER_RIGHT_MAX);   // full steering toward the inner side
    float predicted = dir * (yawDeg + rateDps * TURN_PREDICT_S - heading);   // degrees turned at the end, predicted a little ahead
    if (predicted >= 90 - TURN_LEAD_DEG) {   // turned about 90 degrees
      heading += dir * 90.0; corners++; turnEndCm = odoCm(); armCm = -1;   // new heading, count the corner, remember where the turn ended
      if (corners >= 4 * LAPS) {      // the last turn of the run was done
        finalLeg = true;              // from now on we look for the start position
        if (lStartCm > 0 && d1Cm >= 0) {   // LAPS >= 2: exact, measured on the start straight
          float d = max(lStartCm - d1Cm + STOP_OFFSET_CM, 0.0f);   // distance from the turn end to the start position
          stopAtCm = turnEndCm + d;   // odometry value where to stop
          Serial.print(F(">>> FINAL: drive ")); Serial.print(d, 1); Serial.print(F(" cm from the turn end (LSTART "));
          Serial.print(lStartCm, 1); Serial.print(F(" - D1 ")); Serial.print(d1Cm, 1); Serial.println(F(")"));
        }
      }
      for (int i = 0; i < 2; i++) { tofs[i].normal = -1; tofs[i].jumpCount = 0; tofs[i].nearCount = 0; }   // forget the walls: they are learned again after the turn
      state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();   // back to straight
      Serial.print(F(">>> TURN ")); Serial.print(corners); Serial.print(F(" done at rate ")); Serial.print(rateDps, 0);
      Serial.print(F(" deg/s, yaw ")); Serial.print(yawDeg, 1); Serial.print(F(" -> new heading ")); Serial.println(heading, 0);
    }
    else if (millis() - phaseStart > MAX_TURN_MS) finish(F("TURN TOO LONG -> STOP"));   // the turn takes too long: stop
  }
  if (state == STRAIGHT || state == TURN) {   // stall check: motor on but the wheels do not move
    float o = odoCm();                // current distance
    if (fabs(o - stallOdo) > 1.0 || pwmNow < 60) { stallOdo = o; stallSince = millis(); }   // moving, or motor too weak to check: reset the timer
    else if (millis() - stallSince > STALL_MS) finish(F("STALLED: wheels not turning (hit a wall?) -> STOP"));   // not moving for 0.5 s: stop
  }
  if (state == DONE && !finalPrinted && millis() - doneAt > 1000) {   // 1 s after stopping, print the final report once
    finalPrinted = true; motorOut(0);  // motor off for good
    Serial.print(F(">>> FINAL yaw after 1 s: ")); Serial.print(yawDeg, 1);
    Serial.print(F(" | heading ")); Serial.print(heading, 0);
    Serial.print(F(" | error ")); Serial.print(yawDeg - heading, 1);
    Serial.print(F(" | corners turned ")); Serial.print(corners);
    Serial.print(F(" | D1 ")); Serial.print(d1Cm, 1); Serial.print(F(" | WALL ")); Serial.print(wallCm, 1);
    Serial.print(F(" | LSTART ")); Serial.print(lStartCm, 1);
    Serial.print(F(" | time ")); Serial.print((doneAt - runStart) / 1000.0, 1); Serial.println(F(" s"));
  }
}

void setup() {                        // runs once when the Nano starts
  pinMode(PWMA_PIN, OUTPUT); pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT); motorOut(0);   // motor pins as outputs, motor off
  tofExtraOff();                      // front/rear TOF OFF before anything touches I2C
  pinMode(START_BTN_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT);   // start button with pull-up, LED output
  pinMode(ENC_C1_PIN, INPUT_PULLUP); pinMode(ENC_C2_PIN, INPUT_PULLUP);   // encoder pins with pull-ups
  attachInterrupt(digitalPinToInterrupt(ENC_C1_PIN), encoderISR, RISING);   // count on every rising edge of C1
  steering.attach(SERVO_PIN); steering.write(STEER_CENTER);   // servo on, wheels straight
  Serial.begin(115200); delay(500);   // serial port at 115200 baud
  Serial.println(F("\nBOOT - LAP TEST (stop at the start)"));
  Wire.begin(); Wire.setClock(100000); Wire.setWireTimeout(25000, true);   // I2C at 100 kHz, bus reset after 25 ms of hang
  tofs[0].xshut = XSHUT_LEFT;  tofs[0].addr = ADDR_LEFT;  tofs[0].name = "LEFT";    // LEFT sensor settings
  tofs[1].xshut = XSHUT_RIGHT; tofs[1].addr = ADDR_RIGHT; tofs[1].name = "RIGHT";   // RIGHT sensor settings
  for (int i = 0; i < 5 && !tofOk; i++) { tofOk = tofStart(); if (!tofOk) delay(300); }   // try up to 5 times to start both sensors
  bool g = bmiStart();                // start the gyro
  Serial.println(g ? F("BMI160 OK") : F("WARNING: BMI160 not ready (battery ON first?)"));
  Serial.println(tofOk ? F("Both TOF OK") : F("WARNING: a TOF is missing"));
  calibrate();                        // gyro offset at boot too
  Serial.println(F("Ready. Press START to run."));
}

void loop() {                         // runs again and again after setup()
  if ((state == WAIT || state == DONE) && digitalRead(START_BTN_PIN) == LOW) {   // START button pressed?
    delay(30);                        // debounce
    if (digitalRead(START_BTN_PIN) == LOW) { while (digitalRead(START_BTN_PIN) == LOW) delay(5); delay(300); startRun(); }   // wait for release, then start
  }
  updateGyro();                       // heading update
  updateTof();                        // distance update
  runTest();                          // the program logic

  unsigned long now = millis();
  if (now - lastRamp >= 20) {         // every 20 ms: change the motor speed slowly (soft start)
    lastRamp = now;
    int target = (state == STRAIGHT || state == TURN) ? pwmTarget : 0;   // wanted speed: driving speed, or zero when stopped
    if (state == DONE) { /* keep the brake */ }   // when done the brake stays on
    else if (pwmNow < target) motorOut(min(pwmNow + RAMP_STEP, target));   // speed up by one step
    else if (pwmNow > target) motorOut(target);   // slow down at once
  }
  if (now - lastPrint >= 100 && state != WAIT) {   // every 100 ms, while running
    lastPrint = now;
    Serial.print(STATE_NAME[state]);  // current state
    Serial.print(F("\tc ")); Serial.print(corners);   // corners done
    Serial.print(F("\thead ")); Serial.print(heading, 0);   // target heading
    Serial.print(F("\todometry ")); Serial.print(odoCm(), 1);     // distance travelled
    Serial.print(F("\tyaw ")); Serial.print(yawDeg, 1);      // real heading
    Serial.print(F("\tservo ")); Serial.print(servoNow);     // servo angle now
    Serial.print(F("\tI ")); Serial.print(iTerm, 1);         // heading integral
    Serial.print(F("\tlat ")); Serial.print(latDeg, 1); Serial.print('/'); Serial.print(latI, 1);   // lane correction and its integral
    Serial.print(F("\tL ")); Serial.print(tofs[0].mm); Serial.print(F(" (")); Serial.print((int)tofs[0].normal); Serial.print(')');   // left distance and its normal
    Serial.print(F("\tR ")); Serial.print(tofs[1].mm); Serial.print(F(" (")); Serial.print((int)tofs[1].normal); Serial.print(')');   // right distance and its normal
    Serial.print(F("\tpwm ")); Serial.println(pwmNow);   // motor speed now
  }
}
