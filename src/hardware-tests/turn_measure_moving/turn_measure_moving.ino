// =====================================================================
// TURN MEASURE WHILE MOVING - Step 2 of the Obstacle lap (Arduino Nano classic)
// Measures how the car really turns 90 degrees when it arrives MOVING (as in a corner):
//   START -> gyro calibration (car still) -> straight STRAIGHT_CM with the gyro PD
//   -> without stopping, gyro PD turn of 90 degrees (same as the validated 90-degree gyro turn "turn_left_90_pid_brake")
//   -> wheels straight + brake -> numbers saved in EEPROM.
// The car does NOT need the computer: every run is saved in the Nano's memory.
// Plug the USB afterwards and open the Serial Monitor: it prints all saved runs.
//   Runs 1-3 turn LEFT, runs 4-6 turn RIGHT (then they alternate).
//   LED "L" while waiting for START: 1 blink = next run LEFT, 2 blinks = RIGHT.
//   LED on = calibrating the gyro (do not touch the car).
//   Hold START while powering on (or pressing RESET) for 2 s = erase all runs.
// Wiring (team PCB): encoder C1 = D3 | start button D10 | I2C: SDA=A4, SCL=A5
//         Servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8, STBY D5
// UPLOADING: hold the CAMERA RESET button during the whole upload (the camera is on D0).
// =====================================================================

#include <Wire.h>
#include <Servo.h>
#include <EEPROM.h>

// Defined at the top so the Arduino IDE knows it before any function
struct RunResult {
  int8_t dir;            // +1 left, -1 right
  float straightCm;      // encoder cm from START to the start of the turn
  float turnCm;          // encoder cm during the turn
  float brakeCm;         // encoder cm from "wheels straight + brake" until stopped
  float angleStraighten; // degrees turned when the wheels went straight
  float angleFinal;      // degrees turned when fully stopped
  float turnFwd;         // odometry, camera point: forward during the turn (cm)
  float turnSide;        // odometry, camera point: sideways during the turn (cm, + = toward the turn)
  float endFwd;          // odometry, camera point at the end, from its start: forward (cm)
  float endSide;         // ... sideways (cm, + = toward the turn)
};

// ================= SETTINGS (change these) =================
// --- Test ---
const float STRAIGHT_CM   = 40.0;    // straight before the turn
const int   RUNS_PER_SIDE = 3;       // runs 1-3 left, 4-6 right
const int   MAX_RUNS      = 10;      // memory slots

// --- Steering (same as the validated codes) ---
const int  CENTER_ANGLE         = 120;
const int  LEFT_LIMIT           = 180;
const int  RIGHT_LIMIT          = 50;
const bool STEER_LEFT_IS_BIGGER = false;
const int  SERVO_SPEED_DPS      = 300;

// --- Motor ---
const int  DRIVE_SPEED    = 70;      // straight (same as straight_pillars)
const int  TURN_SPEED     = 70;      // most of the turn
const int  SLOW_SPEED     = 55;      // last SLOW_ZONE_DEG of the turn (raise it if the car stalls)
const bool MOTOR_REVERSED = true;
const bool ACTIVE_BRAKE   = true;    // true = HIGH/HIGH brake, false = coast

// --- Straight: heading PD (validated values) ---
const float KP_HEADING = 2.0;
const float KD_HEADING = 0.10;
const float MAX_STEER  = 60.0;

// --- Turn PD (validated 90-degree gyro turn values) ---
const float TURN_ANGLE_DEG = 90.0;
const float K_TURN         = 2.5;
const float K_RATE         = 0.15;
const float MIN_STEER_DEG  = 8.0;
const float SLOW_ZONE_DEG  = 25.0;
const float STOP_LEAD_DEG  = 2.0;    // wheels straight + brake this many degrees BEFORE 90

// --- Safety ---
const unsigned long STRAIGHT_TIMEOUT_MS = 4000;
const unsigned long TURN_TIMEOUT_MS     = 5000;
const unsigned long STOP_SETTLE_MS      = 800;   // time to stop completely after the brake
const unsigned long ERASE_HOLD_MS       = 2000;

// --- Car ---
const float TICKS_PER_CM          = 15.1;  // measured: 980 ticks = 65 cm
const float CAM_FROM_REAR_AXLE_CM = 9.0;   // MEASURE: rear axle -> camera lens, along the car

// --- Gyro ---
const bool  YAW_REVERSED = false;    // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN   = 9;
const int PWMA_PIN    = 6;
const int AIN1_PIN    = 7;
const int AIN2_PIN    = 8;
const int STBY_PIN    = 5;
const int BUTTON_PIN  = 10;
const int ENCODER_PIN = 3;
const int LED_PIN     = LED_BUILTIN;   // D13, the "L" LED on the Nano

// ---------------- Memory layout ----------------
const uint8_t MEMORY_MAGIC = 0x5A;
const int ADDR_MAGIC = 0;
const int ADDR_COUNT = 1;
const int ADDR_RUNS  = 2;

Servo steering;
RunResult rec;                       // the run being measured / printed

// ---------------- Car state ----------------
float yaw = 0, yawRate = 0;          // degrees, + = car turned LEFT
float rearX = 0, rearY = 0;          // cm, rear axle on the map (x forward at START, y + = left)
float traveledCm = 0;
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;
long prevTicks = 0;

volatile long encoderTicks = 0;
void onEncoder() { encoderTicks++; }

long readTicks() {
  noInterrupts();
  long t = encoderTicks;
  interrupts();
  return t;
}
// ===================== end of PART 1 =====================

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {                   // free stop (coast)
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

void motorBrake() {                  // active brake (short brake)
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, 255);
}

// ---------------- BMI160 (registers, same as the validated codes) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

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
  bmiWrite(0x7E, 0x11); delay(50);   // accelerometer: normal mode
  bmiWrite(0x7E, 0x15); delay(100);  // gyroscope: normal mode
  bmiWrite(0x42, 0x2A);              // gyro 400 Hz
  bmiWrite(0x43, 0x02);              // gyro range +-500 deg/s
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

// ---------------- Odometry: encoder distance + gyro heading ----------------
void updateOdometry(float yawBefore) {
  long t = readTicks();
  long dTicks = t - prevTicks;
  prevTicks = t;
  if (dTicks <= 0) return;
  float ds = dTicks / TICKS_PER_CM;
  float h = radians((yawBefore + yaw) * 0.5);    // average heading during this step
  rearX += ds * cos(h);
  rearY += ds * sin(h);
  traveledCm += ds;
}

// Camera point, measured from where the camera was at START (x forward, y + = left)
void cameraPoint(float &cx, float &cy) {
  float h = radians(yaw);
  cx = rearX + CAM_FROM_REAR_AXLE_CM * cos(h) - CAM_FROM_REAR_AXLE_CM;
  cy = rearY + CAM_FROM_REAR_AXLE_CM * sin(h);
}

// One control step: gyro + odometry. Returns dt (s)
float step() {
  float yawBefore = yaw;
  float dt = updateGyro();
  updateOdometry(yawBefore);
  return dt;
}

// ---------------- Steering ----------------
float maxLeftSteer()  { return STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT); }
float maxRightSteer() { return STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE); }

// steerLeft: + = left, - = right (servo degrees away from center)
void setSteering(float steerLeft, float dt) {
  steerLeft = constrain(steerLeft, -maxRightSteer(), maxLeftSteer());
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);
  float maxStep = SERVO_SPEED_DPS * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));
}

void wheelsStraight() {
  servoAngle = CENTER_ANGLE;
  steering.write(CENTER_ANGLE);
}

// ---------------- Memory (EEPROM) ----------------
int savedRuns() { return EEPROM.read(ADDR_COUNT); }

void prepareMemory() {
  if (EEPROM.read(ADDR_MAGIC) != MEMORY_MAGIC || EEPROM.read(ADDR_COUNT) > MAX_RUNS) {
    EEPROM.write(ADDR_MAGIC, MEMORY_MAGIC);
    EEPROM.write(ADDR_COUNT, 0);
  }
}

void eraseRuns() { EEPROM.write(ADDR_COUNT, 0); }

void saveRun(int i) {
  EEPROM.put(ADDR_RUNS + i * (int)sizeof(RunResult), rec);
  EEPROM.write(ADDR_COUNT, i + 1);
}

void printRun(int i) {
  EEPROM.get(ADDR_RUNS + i * (int)sizeof(RunResult), rec);
  Serial.print(F("RUN ")); Serial.print(i + 1);
  Serial.print(rec.dir > 0 ? F(" LEFT ") : F(" RIGHT"));
  Serial.print(F(" | straight "));  Serial.print(rec.straightCm, 1);
  Serial.print(F(" | turn "));      Serial.print(rec.turnCm, 1);
  Serial.print(F(" | brake "));     Serial.print(rec.brakeCm, 1);
  Serial.print(F(" cm | angle "));  Serial.print(rec.angleStraighten, 1);
  Serial.print(F(" -> "));          Serial.print(rec.angleFinal, 1);
  Serial.print(F(" | odo turn: fwd ")); Serial.print(rec.turnFwd, 1);
  Serial.print(F(" side "));        Serial.print(rec.turnSide, 1);
  Serial.print(F(" | odo end: fwd ")); Serial.print(rec.endFwd, 1);
  Serial.print(F(" side "));        Serial.println(rec.endSide, 1);
}

void printAllRuns() {
  int n = savedRuns();
  Serial.println();
  Serial.print(F("=== SAVED RUNS: ")); Serial.print(n); Serial.println(F(" ==="));
  for (int i = 0; i < n; i++) printRun(i);
  Serial.println(F("(cm and degrees; side + = toward the turn)"));
}

// ---------------- LED ----------------
// 1 blink per cycle = next run LEFT, 2 blinks = RIGHT
void blinkDirection(int8_t dir) {
  unsigned long t = millis() % 1500;
  bool on = (t < 200) || (dir < 0 && t >= 400 && t < 600);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}
// ===================== end of PART 2 =====================

// ---------------- The test ----------------
int8_t directionForRun(int n) {
  if (n < RUNS_PER_SIDE)     return 1;           // left
  if (n < 2 * RUNS_PER_SIDE) return -1;          // right
  return (n % 2 == 0) ? 1 : -1;                  // extra runs alternate
}

void waitForStart(int8_t dir) {
  while (digitalRead(BUTTON_PIN) == HIGH) blinkDirection(dir);
  delay(50);
  while (digitalRead(BUTTON_PIN) == LOW) {}      // wait until the finger is off
  delay(500);                                    // hand away from the car
}

// Returns true if the run finished normally (then it is saved)
bool runTest(int8_t dir) {
  yaw = 0; yawRate = 0;
  rearX = 0; rearY = 0; traveledCm = 0;
  prevTicks = readTicks();
  prevMicros = micros();
  wheelsStraight();
  rec.dir = dir;

  // 1) Straight with the heading PD
  unsigned long t0 = millis();
  motorForward(DRIVE_SPEED);
  while (traveledCm < STRAIGHT_CM) {
    float dt = step();
    float steer = KP_HEADING * (0 - yaw) - KD_HEADING * yawRate;
    setSteering(constrain(steer, -MAX_STEER, MAX_STEER), dt);
    if (millis() - t0 > STRAIGHT_TIMEOUT_MS) {
      motorStop(); wheelsStraight();
      Serial.println(F("TIMEOUT in the straight (motor or encoder?) - run NOT saved"));
      return false;
    }
  }
  rec.straightCm = traveledCm;
  float turnStartCm = traveledCm;
  float cx0, cy0;
  cameraPoint(cx0, cy0);

  // 2) Turn without stopping: gyro PD, at least MIN_STEER_DEG toward the turn
  float maxSteer = (dir > 0) ? maxLeftSteer() : maxRightSteer();
  unsigned long ts = millis();
  bool ok = true;
  while (true) {
    float dt = step();
    float turned = dir * yaw;                    // degrees turned toward the turn side
    float rate   = dir * yawRate;
    float error  = TURN_ANGLE_DEG - turned;
    if (error <= STOP_LEAD_DEG) break;
    if (millis() - ts > TURN_TIMEOUT_MS) { ok = false; break; }
    float steer = K_TURN * error - K_RATE * rate;
    steer = constrain(steer, MIN_STEER_DEG, maxSteer);
    setSteering(dir * steer, dt);
    motorForward(error < SLOW_ZONE_DEG ? SLOW_SPEED : TURN_SPEED);
  }

  // 3) Wheels straight + brake at the same time
  rec.angleStraighten = dir * yaw;
  rec.turnCm = traveledCm - turnStartCm;
  float cx1, cy1;
  cameraPoint(cx1, cy1);
  rec.turnFwd  = cx1 - cx0;
  rec.turnSide = dir * (cy1 - cy0);
  float brakeStartCm = traveledCm;
  wheelsStraight();
  if (ACTIVE_BRAKE) motorBrake(); else motorStop();

  unsigned long tb = millis();
  while (millis() - tb < STOP_SETTLE_MS) step();  // keep measuring until it stops
  motorStop();                                   // release: the car can be pushed by hand
  rec.brakeCm = traveledCm - brakeStartCm;
  rec.angleFinal = dir * yaw;
  float ex, ey;
  cameraPoint(ex, ey);
  rec.endFwd  = ex;
  rec.endSide = dir * ey;

  if (!ok) {
    Serial.println(F("TIMEOUT in the turn (check YAW_REVERSED) - run NOT saved"));
    return false;
  }
  return true;
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

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoder, RISING);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);
  if (!startGyro()) {
    Serial.println(F("ERROR: BMI160 not found"));
    while (true) { digitalWrite(LED_PIN, (millis() / 100) % 2); }   // fast blink = error
  }

  steering.attach(SERVO_PIN, 500, 2500);
  wheelsStraight();

  prepareMemory();

  // START held at power on -> erase all runs
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long t = millis();
    while (digitalRead(BUTTON_PIN) == LOW && millis() - t < ERASE_HOLD_MS) {}
    if (millis() - t >= ERASE_HOLD_MS) {
      eraseRuns();
      Serial.println(F(">> ALL RUNS ERASED"));
      for (int i = 0; i < 10; i++) { digitalWrite(LED_PIN, i % 2); delay(100); }
      while (digitalRead(BUTTON_PIN) == LOW) {}
      delay(300);
    }
  }

  printAllRuns();
}

// ---------------- LOOP ----------------
void loop() {
  int n = savedRuns();
  if (n >= MAX_RUNS) {
    Serial.println(F("MEMORY FULL: hold START + RESET for 2 s to erase"));
    while (true) { digitalWrite(LED_PIN, (millis() / 100) % 2); }
  }

  int8_t dir = directionForRun(n);
  Serial.println();
  Serial.print(F("Next: RUN ")); Serial.print(n + 1);
  Serial.println(dir > 0 ? F(" LEFT. Car on the mark, press START") : F(" RIGHT. Car on the mark, press START"));
  waitForStart(dir);

  digitalWrite(LED_PIN, HIGH);
  Serial.println(F("Calibrating gyro... do not touch the car"));
  calibrateGyro();
  digitalWrite(LED_PIN, LOW);

  if (runTest(dir)) {
    saveRun(n);
    printRun(n);
  }
}
