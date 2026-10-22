// exito_rotundo.ino  ("Exito rotundo" - validated on the track 21 Oct 2026)
// 1) PID (distance + gyro) following the nearest wall (same as Lance Chung), with an
//    integral on the heading loop (KI_HEADING) so it does not drift to one side.
// 2) While driving, the last REF_HISTORY valid wall readings of EACH side are kept.
// 3) If LEFT or RIGHT reads more than SIDE_OPEN_MM for SIDE_CONFIRM valid readings in a
//    row -> that side is saved as the opening (only the first one counts) and the PID
//    switches to the OPPOSITE wall, keeping the AVERAGE of that wall's last REF_HISTORY
//    readings taken BEFORE the opening (so the car does not drift when switching wall).
// 4) After RUN_MS: brake and stay still.
//     - opening seen     -> turn toward the opening (no compare)
//     - no opening seen  -> average COMPARE_SAMPLES readings of each side and choose
//                           the FARTHER side (invalid reading = no wall = far)
// 5) DO_TURN = true  -> 90 degree turn (giro de 90 grados) toward that side, then braked.
//    DO_TURN = false -> no turn: wheels point to the chosen side and it stays braked.
// Board: Arduino Nano (ATmega328P) | I2C: SDA=A4, SCL=A5
// Sensors: TOF400C (VL53L1X, Pololu library) LEFT (XSHUT A3), RIGHT (XSHUT A2)
// Gyro: BMI160 (0x68 or 0x69), read by registers (no library)
// Steering: servo D9 | Rear motor: TB6612FNG (PWMA D6, AIN1 D7, AIN2 D8, STBY D5)

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// ================= SETTINGS (change these) =================
// --- Steering ---
const int  CENTER_ANGLE         = 120;
const int  LEFT_LIMIT           = 180;
const int  RIGHT_LIMIT          = 50;
const bool STEER_LEFT_IS_BIGGER = false;
const int  MAX_STEER_DEG        = 25;     // max steering of the PID
const int  SERVO_SPEED_DPS      = 250;    // servo speed during the PID

// --- Motor ---
const int  MOTOR_SPEED    = 100;    // 0-255
const bool MOTOR_REVERSED = true;
const int  START_DELAY_MS = 2000;

// --- PID: distance -> heading (outer loop) ---
const float K_DIST          = 0.08;
const float KI_DIST         = 0.04;
const float MAX_HEADING_DEG = 5;

// --- PID: heading -> steering (inner loop, gyroscope) ---
const float K_HEADING     = 2.0;
const float K_RATE        = 0.15;
const float KI_HEADING    = 2.5;   // removes a constant steering offset (servo center not exact)
const float MAX_HEADING_I = 10;    // max steering degrees the heading integral can add

// --- Gyro ---
const bool  YAW_REVERSED = false;  // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;   // fine correction: real angle / measured angle

// --- Opening detection (while driving) ---
const int SIDE_OPEN_MM = 2400;   // a side reading MORE than this = opening (240 cm)
const int SIDE_CONFIRM = 3;      // valid readings in a row above SIDE_OPEN_MM
const int REF_HISTORY  = 5;      // last wall readings averaged for the new reference

// --- Timed stop + decide ---
const unsigned long RUN_MS = 1800;     // driving time before stopping
const int  STOP_SETTLE_MS  = 300;      // wait still before measuring (car fully stopped)
const int  COMPARE_SAMPLES = 10;       // readings per side when there was no opening
const bool DO_TURN         = true;     // true = 90 degree turn after deciding, false = only decide

// --- 90 degree turn (same as "giro de 90 grados") ---
const float TURN_ANGLE_DEG       = 90.0;
const int   TURN_STEER_DEG       = 60;    // max wheel angle during the turn
const int   TURN_SERVO_SPEED_DPS = 300;   // servo speed during the turn
const int   SERVO_SETTLE_MS      = 400;   // time for the wheels to reach the turn angle
const int   TURN_SPEED           = 60;    // speed for most of the turn
const int   SLOW_SPEED           = 35;    // speed near the end (raise it if the car stalls)
const float K_TURN               = 2.5;   // steering degrees per degree left to turn
const float TURN_K_RATE          = 0.15;  // steering degrees per (deg/s) of turning speed (brake)
const float MIN_STEER_DEG        = 8.0;   // always steer at least this much until the stop
const float SLOW_ZONE_DEG        = 25.0;  // last degrees done at SLOW_SPEED
const float STOP_LEAD_DEG        = 2.0;   // brake this many degrees BEFORE 90 (inertia)
const unsigned long TURN_TIMEOUT_MS = 8000;   // safety

// --- TOF ---
const uint8_t  ROI_WIDTH        = 1;
const uint8_t  ROI_HEIGHT       = 6;
const uint32_t TIMING_BUDGET_US = 33000;
const int      PERIOD_MS        = 33;
const int      MAX_VALID_MM     = 4000;  // must be bigger than SIDE_OPEN_MM
const int      MAX_FOLLOW_MM    = 2400;  // side readings above this never enter the PID filter
const int      REF_SAMPLES      = 10;
const float    FILTER_ALPHA     = 0.6;
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN = 9;
const int PWMA_PIN  = 6;
const int AIN1_PIN  = 7;
const int AIN2_PIN  = 8;
const int STBY_PIN  = 5;
const int LED_PIN   = 13;     // built-in LED: ON when an opening was seen

const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;
const int XSHUT_FRONT = A1;   // kept OFF
const int XSHUT_BACK  = A0;   // kept OFF

// ---------------- BMI160 ----------------
const float GYRO_SCALE = 65.6;   // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

// ---------------- Objects ----------------
VL53L1X tofLeft;
VL53L1X tofRight;
Servo steering;

// ---------------- Sensor readings ----------------
int  lastLeftMm = -1, lastRightMm = -1;   // -1 = invalid
bool newLeft = false, newRight = false;
float yaw = 0;       // degrees, + = LEFT
float yawRate = 0;   // deg/s, + = turning LEFT

// ---------------- Wall history (last REF_HISTORY wall readings per side) ----------------
int     histLeft[REF_HISTORY];
int     histRight[REF_HISTORY];
uint8_t histCountLeft = 0, histIdxLeft = 0;
uint8_t histCountRight = 0, histIdxRight = 0;

// ---------------- PID state ----------------
int   wallSide;              // -1 = LEFT wall, +1 = RIGHT wall
int   refDistance;
float filtDistance;
float distIntegral = 0;
float headingIntegral = 0;
float targetHeading = 0;
float servoAngle = CENTER_ANGLE;
unsigned long lastDistMillis = 0;

// ---------------- Opening state ----------------
int  sideCountLeft  = 0;
int  sideCountRight = 0;
bool openingSeen = false;    // true once a side opened (first one counts)
int  openSide = 0;           // -1 = opening on the LEFT, +1 = opening on the RIGHT

// ---------------- State ----------------
bool stopped = false;
unsigned long runStartMs = 0;
unsigned long prevMicros = 0;
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

// Active brake: both inputs HIGH
void motorBrake() {
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, 255);
}

void haltWithError(const __FlashStringHelper *msg) {
  motorStop();
  Serial.println(msg);
  while (true) {                       // fast blink = error at start
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
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
  return bmiRead(0x00, &id, 1) && id == 0xD1;
}

bool startGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;
  bmiWrite(0x7E, 0x11); delay(50);    // accelerometer: normal mode
  bmiWrite(0x7E, 0x15); delay(100);   // gyroscope: normal mode
  bmiWrite(0x42, 0x2A);               // gyro 400 Hz
  bmiWrite(0x43, 0x02);               // +-500 deg/s
  delay(50);
  return true;
}

float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_SCALE;
  return YAW_REVERSED ? -dps : dps;
}

void calibrateGyro() {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < 1000; i++) {
    float z = readGyroZ();
    if (!isnan(z)) { sum += z; count++; }
    delay(3);
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

// Side sensors: strict (only precise readings)
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
    sensor.read();
    if (sensor.timeoutOccurred()) continue;
    int mm = validMm(sensor);
    if (mm > 0) { sum += mm; count++; }
  }
  return (count == 0) ? -1 : sum / count;
}

// ---------------- Wall history ----------------
// Saves a wall reading in the circular buffer of one side
void pushHistory(int *buf, uint8_t &idx, uint8_t &count, int mm) {
  buf[idx] = mm;
  idx = (idx + 1) % REF_HISTORY;
  if (count < REF_HISTORY) count++;
}

// Average of the saved readings of one side, or -1 if there are none
int historyAverage(int *buf, uint8_t count) {
  if (count == 0) return -1;
  long sum = 0;
  for (uint8_t i = 0; i < count; i++) sum += buf[i];
  return sum / count;
}

// ---------------- Gyro + sensors ----------------
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

void readSensors() {
  newLeft = false;
  newRight = false;
  if (tofLeft.dataReady()) {
    tofLeft.read(false);
    lastLeftMm = validMm(tofLeft);
    newLeft = true;
    // Before the opening: keep the last wall readings (not the far ones)
    if (!openingSeen && lastLeftMm > 0 && lastLeftMm <= SIDE_OPEN_MM)
      pushHistory(histLeft, histIdxLeft, histCountLeft, lastLeftMm);
  }
  if (tofRight.dataReady()) {
    tofRight.read(false);
    lastRightMm = validMm(tofRight);
    newRight = true;
    if (!openingSeen && lastRightMm > 0 && lastRightMm <= SIDE_OPEN_MM)
      pushHistory(histRight, histIdxRight, histCountRight, lastRightMm);
  }
}

// ---------------- Opening detection: remember it + follow the other wall ----------------
void countFar(int mm, int &count) {
  if (mm < 0) return;                  // invalid: ignored
  if (mm > SIDE_OPEN_MM) count++;
  else count = 0;
}

void checkOpening() {
  if (openingSeen) return;             // only the first opening counts

  if (newLeft)  countFar(lastLeftMm,  sideCountLeft);
  if (newRight) countFar(lastRightMm, sideCountRight);

  if (sideCountLeft < SIDE_CONFIRM && sideCountRight < SIDE_CONFIRM) return;

  openSide = (sideCountLeft >= SIDE_CONFIRM) ? -1 : +1;
  openingSeen = true;
  digitalWrite(LED_PIN, HIGH);         // LED ON = opening seen

  // Follow the wall on the OPPOSITE side, at the average of its last readings
  wallSide = -openSide;
  int avg = (wallSide < 0) ? historyAverage(histLeft,  histCountLeft)
                           : historyAverage(histRight, histCountRight);
  if (avg > 0) {
    refDistance  = avg;
    filtDistance = avg;
  } else {
    // No history of that wall: use its current reading if it is a wall
    int mm = (wallSide < 0) ? lastLeftMm : lastRightMm;
    if (mm > 0 && mm <= SIDE_OPEN_MM) { refDistance = mm; filtDistance = mm; }
  }
  distIntegral = 0;
  lastDistMillis = millis();

  Serial.print(F("OPENING seen on the "));
  Serial.print(openSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" -> following "));
  Serial.print(wallSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" wall at "));
  Serial.print(refDistance / 10.0, 1);
  Serial.println(F(" cm (average of its last readings)"));
}

// ---------------- Compare both sides (car stopped, averaged) ----------------
// Returns +1 (turn LEFT) or -1 (turn RIGHT): toward the farther side.
// Invalid readings count as "no wall" = MAX_VALID_MM (far).
// Both sides invalid every time: away from the followed wall.
int dirByWalls() {
  long sumL = 0, sumR = 0;
  int validL = 0, validR = 0;
  for (int i = 0; i < COMPARE_SAMPLES; i++) {
    tofLeft.read();                                  // waits for a new measurement
    int l = tofLeft.timeoutOccurred() ? -1 : validMm(tofLeft);
    tofRight.read();
    int r = tofRight.timeoutOccurred() ? -1 : validMm(tofRight);

    if (l < 0) sumL += MAX_VALID_MM; else { sumL += l; validL++; }
    if (r < 0) sumR += MAX_VALID_MM; else { sumR += r; validR++; }
  }
  long avgL = sumL / COMPARE_SAMPLES;
  long avgR = sumR / COMPARE_SAMPLES;

  Serial.print(F("COMPARE avg L: "));
  Serial.print(avgL / 10.0, 1);
  Serial.print(F(" cm ("));
  Serial.print(validL);
  Serial.print(F(" valid) | R: "));
  Serial.print(avgR / 10.0, 1);
  Serial.print(F(" cm ("));
  Serial.print(validR);
  Serial.println(F(" valid)"));

  if (validL == 0 && validR == 0) return (wallSide < 0) ? -1 : +1;
  return (avgL >= avgR) ? +1 : -1;
}

// ---------------- Servo with speed limit (steerLeft: + = LEFT) ----------------
void setSteering(float steerLeft, float dt, int speedDps) {
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);

  float maxStep = speedDps * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));
}

// Wheels to the max steering of one side right away (dir = +1 LEFT, -1 RIGHT)
void pointWheels(int dir) {
  float maxLeft  = STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT);
  float maxRight = STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE);
  float maxSide  = min((float)TURN_STEER_DEG, dir > 0 ? maxLeft : maxRight);
  float t0 = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? dir * maxSide : -dir * maxSide);
  servoAngle = constrain(t0, RIGHT_LIMIT, LEFT_LIMIT);
  steering.write((int)(servoAngle + 0.5));
}

// ---------------- 90 degree turn ----------------
// dir = +1 turns LEFT, -1 turns RIGHT
void turn90(int dir) {
  float maxLeft  = STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT);
  float maxRight = STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE);
  float maxSide  = min((float)TURN_STEER_DEG, dir > 0 ? maxLeft : maxRight);

  // 1) Car braked, wheels toward the turn side
  motorBrake();
  pointWheels(dir);
  delay(SERVO_SETTLE_MS);

  // 2) Motor forward, PD on the yaw
  prevMicros = micros();
  float startYaw = yaw;
  unsigned long startMs = millis();
  motorForward(TURN_SPEED);

  while (true) {
    float dt = updateGyro();
    float turned = dir * (yaw - startYaw);
    float error = TURN_ANGLE_DEG - turned;           // + = still must turn

    if (error <= STOP_LEAD_DEG) break;
    if (millis() - startMs > TURN_TIMEOUT_MS) {
      Serial.println(F("TIMEOUT: turn took too long"));
      break;
    }

    float steer = K_TURN * error - TURN_K_RATE * (dir * yawRate);
    steer = constrain(steer, MIN_STEER_DEG, maxSide);
    setSteering(dir * steer, dt, TURN_SERVO_SPEED_DPS);

    motorForward(error < SLOW_ZONE_DEG ? SLOW_SPEED : TURN_SPEED);
  }

  // 3) Brake and wheels straight
  motorBrake();
  servoAngle = CENTER_ANGLE;
  steering.write(CENTER_ANGLE);
}

// ---------------- Time is up: stop and decide ----------------
// Returns true when it happened (car stays braked afterwards)
bool checkTimeUp() {
  if (millis() - runStartMs <= RUN_MS) return false;

  // 1) Stop and stay still
  motorBrake();
  servoAngle = CENTER_ANGLE;
  steering.write(CENTER_ANGLE);
  delay(STOP_SETTLE_MS);

  // 2) Decide: opening seen -> go there; otherwise compare both sides
  int dir;
  if (openingSeen) {
    dir = (openSide < 0) ? +1 : -1;
    Serial.print(F("TIME UP -> opening seen, going "));
  } else {
    dir = dirByWalls();
    Serial.print(F("TIME UP -> no opening, compared walls, going "));
  }
  Serial.println(dir > 0 ? F("LEFT") : F("RIGHT"));

  // 3) Turn, or only point the wheels to the chosen side
  if (DO_TURN) turn90(dir);
  else         pointWheels(dir);

  stopped = true;
  return true;
}

// ---------------- PID ----------------
void updatePid() {
  bool newFollowed = (wallSide < 0) ? newLeft : newRight;
  if (!newFollowed) return;

  int mm = (wallSide < 0) ? lastLeftMm : lastRightMm;
  // Far readings (a wall that is ending) never enter the filter
  if (mm > 0 && mm <= MAX_FOLLOW_MM) filtDistance = FILTER_ALPHA * mm + (1 - FILTER_ALPHA) * filtDistance;

  unsigned long nowMs = millis();
  float dtDist = (nowMs - lastDistMillis) / 1000.0;
  lastDistMillis = nowMs;

  float error = filtDistance - refDistance;   // + = too far, - = too close

  distIntegral += error * dtDist;
  if (KI_DIST > 0) {
    float maxI = MAX_HEADING_DEG / KI_DIST;
    distIntegral = constrain(distIntegral, -maxI, maxI);
  }

  float h = -wallSide * (K_DIST * error + KI_DIST * distIntegral);
  targetHeading = constrain(h, -MAX_HEADING_DEG, MAX_HEADING_DEG);
}

void updateSteering(float dt) {
  float headingError = targetHeading - yaw;          // + = must turn LEFT

  // Integral of the heading error: cancels a constant offset (e.g. servo center not exact)
  headingIntegral += headingError * dt;
  if (KI_HEADING > 0) {
    float maxI = MAX_HEADING_I / KI_HEADING;
    headingIntegral = constrain(headingIntegral, -maxI, maxI);
  }

  float steerLeft = K_HEADING * headingError
                  + KI_HEADING * headingIntegral
                  - K_RATE * yawRate;
  steerLeft = constrain(steerLeft, -MAX_STEER_DEG, MAX_STEER_DEG);
  setSteering(steerLeft, dt, SERVO_SPEED_DPS);
}

// ---------------- Print 10 times per second ----------------
void printStatus() {
  if (millis() - lastPrint < 100) return;
  lastPrint = millis();
  Serial.print(F("wall: "));
  Serial.print(wallSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" ref "));
  Serial.print(refDistance / 10.0, 1);
  Serial.print(F(" | L: "));
  if (lastLeftMm < 0) Serial.print(F("---")); else Serial.print(lastLeftMm / 10.0, 1);
  Serial.print(F(" ("));
  Serial.print(sideCountLeft);
  Serial.print(F(") | R: "));
  if (lastRightMm < 0) Serial.print(F("---")); else Serial.print(lastRightMm / 10.0, 1);
  Serial.print(F(" ("));
  Serial.print(sideCountRight);
  Serial.print(F(") | opening: "));
  if (!openingSeen) Serial.print(F("no"));
  else Serial.print(openSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" | yaw: "));
  Serial.print(yaw, 1);
  Serial.print(F(" | hI: "));
  Serial.println(KI_HEADING * headingIntegral, 1);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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
  if (!startTof(tofLeft, XSHUT_LEFT, 0x30))   haltWithError(F("ERROR: LEFT TOF not found"));
  if (!startTof(tofRight, XSHUT_RIGHT, 0x31)) haltWithError(F("ERROR: RIGHT TOF not found"));

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  calibrateGyro();                     // car must be STILL

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

  delay(START_DELAY_MS);
  yaw = 0;
  headingIntegral = 0;
  motorForward(MOTOR_SPEED);
  prevMicros = micros();
  lastDistMillis = millis();
  runStartMs = millis();               // starts the RUN_MS timer
}

// ---------------- LOOP ----------------
void loop() {
  if (stopped) { motorBrake(); return; }   // stays braked

  float dt = updateGyro();
  readSensors();

  checkOpening();                  // opening: remember it + follow the other wall at its recent distance
  if (checkTimeUp()) return;       // RUN_MS passed -> stop, decide (opening or compare), turn

  updatePid();
  updateSteering(dt);

  printStatus();
}
