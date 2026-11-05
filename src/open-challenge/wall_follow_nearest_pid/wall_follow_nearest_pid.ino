// wall_follow_nearest_pid.ino
// At the start: takes 10 readings of each side sensor, picks the NEAREST wall
// and keeps that same distance all the time with a PID.
// Board: Arduino Nano (classic, ATmega328P) | I2C: SDA=A4, SCL=A5
// Sensors: TOF400C (VL53L1X, Pololu library) LEFT (XSHUT A3), RIGHT (XSHUT A2)
// Steering: servo D9 (pinion + rack) | Rear motor: TB6612FNG (PWMA D6, AIN1 D7, AIN2 D8, STBY D5)

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// ================= SETTINGS (change these) =================
// --- Steering ---
const int CENTER_ANGLE = 120;     // provisional - adjust to the new rack
const int LEFT_LIMIT   = 180;     // provisional - widen slowly
const int RIGHT_LIMIT  = 50;      // provisional

// --- Motor ---
const int  MOTOR_SPEED    = 60;     // 0-255 (raise if the car does not move)
const bool MOTOR_REVERSED = true;   // true = rear motor spins the other way
const int  START_DELAY_MS = 2000;   // time to let go of the car

// --- PID ---
const float KP           = 0.5;    // degrees per mm of error
const float KI           = 0.0;    // start at 0
const float KD           = 0.03;   // degrees per (mm/s)
const int   TOLERANCE_MM = 20;     // +-2 cm: inside this band the servo stays centered
const bool  INVERT_STEER = true;   // true if the servo corrects to the wrong side

// --- TOF ---
const uint8_t  ROI_WIDTH        = 1;       // horizontal
const uint8_t  ROI_HEIGHT       = 6;       // vertical
const uint32_t TIMING_BUDGET_US = 50000;   // 50 ms per measurement
const int      PERIOD_MS        = 50;
const int      MAX_VALID_MM     = 2000;    // above this = "no wall", ignored
const int      REF_SAMPLES      = 10;      // readings averaged for the reference
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

// ---------------- Objects / state ----------------
VL53L1X tofLeft;
VL53L1X tofRight;
Servo steering;

int   wallSide;          // -1 = following LEFT wall, +1 = following RIGHT wall
int   refDistance;       // distance (mm) to keep
int   lastDistance;      // last valid reading
float prevError = 0;
float integral  = 0;
unsigned long prevTime = 0;

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

// ---------------- TOF ----------------
bool startTof(VL53L1X &sensor, int xshutPin, uint8_t address) {
  digitalWrite(xshutPin, HIGH);   // turn this sensor ON
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

// Returns distance in mm, or -1 if the reading is not usable
int readTof(VL53L1X &sensor) {
  sensor.read();
  if (sensor.timeoutOccurred()) return -1;
  int mm = sensor.ranging_data.range_mm;
  VL53L1X::RangeStatus st = sensor.ranging_data.range_status;
  bool okStatus = (st == VL53L1X::RangeValid || st == VL53L1X::SignalFail);
  if (!okStatus || mm <= 0 || mm > MAX_VALID_MM) return -1;
  return mm;
}

// Average of REF_SAMPLES valid readings (-1 if none were valid)
int averageTof(VL53L1X &sensor) {
  long sum = 0;
  int count = 0;
  for (int i = 0; i < REF_SAMPLES; i++) {
    int mm = readTof(sensor);
    if (mm > 0) { sum += mm; count++; }
  }
  return (count == 0) ? -1 : sum / count;
}

void haltWithError(const __FlashStringHelper *msg) {
  motorStop();
  Serial.println(msg);
  while (true) delay(1000);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Motor driver
  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, HIGH);
  motorStop();

  // All TOF sensors off
  pinMode(XSHUT_LEFT, OUTPUT);  digitalWrite(XSHUT_LEFT, LOW);
  pinMode(XSHUT_RIGHT, OUTPUT); digitalWrite(XSHUT_RIGHT, LOW);
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_BACK, OUTPUT);  digitalWrite(XSHUT_BACK, LOW);
  delay(20);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  if (!startTof(tofLeft, XSHUT_LEFT, 0x30))   haltWithError(F("ERROR: LEFT TOF not found"));
  if (!startTof(tofRight, XSHUT_RIGHT, 0x31)) haltWithError(F("ERROR: RIGHT TOF not found"));
  Serial.println(F("TOF LEFT and RIGHT OK"));

  // Servo after the sensors (its current draw can disturb the I2C start-up)
  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

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
  lastDistance = refDistance;

  Serial.print(F("Following "));
  Serial.print(wallSide < 0 ? F("LEFT") : F("RIGHT"));
  Serial.print(F(" wall at "));
  Serial.print(refDistance / 10.0, 1);
  Serial.println(F(" cm"));

  delay(START_DELAY_MS);
  motorForward(MOTOR_SPEED);   // the car starts moving forward here
  prevTime = millis();
}

// ---------------- LOOP ----------------
void loop() {
  int distance = (wallSide < 0) ? readTof(tofLeft) : readTof(tofRight);
  bool valid = (distance > 0);
  if (!valid) distance = lastDistance;   // bad reading -> use the last good one
  lastDistance = distance;

  unsigned long now = millis();
  float dt = (now - prevTime) / 1000.0;
  if (dt <= 0) dt = 0.001;
  prevTime = now;

  float error = distance - refDistance;   // + = too far, - = too close
  int angle = CENTER_ANGLE;

  if (abs(error) <= TOLERANCE_MM) {
    integral = 0;   // inside the band: go straight
  } else {
    integral += error * dt;
    float derivative = (error - prevError) / dt;
    float correction = KP * error + KI * integral + KD * derivative;
    if (INVERT_STEER) correction = -correction;

    // Same convention as the both-walls code: +correction = angle goes UP.
    // Too far from the wall -> steer TOWARD it; too close -> steer AWAY.
    angle = CENTER_ANGLE - wallSide * (int)correction;
  }
  prevError = error;

  angle = constrain(angle, RIGHT_LIMIT, LEFT_LIMIT);
  steering.write(angle);

  Serial.print(F("dist: "));
  Serial.print(distance / 10.0, 1);
  Serial.print(valid ? F(" cm") : F(" cm (old)"));
  Serial.print(F(" | ref: "));
  Serial.print(refDistance / 10.0, 1);
  Serial.print(F(" | error: "));
  Serial.print(error / 10.0, 1);
  Serial.print(F(" | servo: "));
  Serial.println(angle);
}
