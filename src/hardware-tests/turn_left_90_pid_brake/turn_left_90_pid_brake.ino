// turn_left_90_pid_brake.ino
// Turns the car 90 degrees to the LEFT with a PD on the gyro and stops still:
//  - far from 90: wheels at full left, normal speed
//  - close to 90: wheels straighten little by little, slower speed
//  - STOP_LEAD_DEG before 90: wheels center + active brake (stays braked)
// Board: Arduino Nano (classic) | I2C: SDA=A4, SCL=A5
// Gyro: BMI160 (0x68 or 0x69), registers | Servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8, STBY D5

#include <Wire.h>
#include <Servo.h>

// ================= SETTINGS (change these) =================
// --- Steering ---
const int  CENTER_ANGLE         = 120;
const int  LEFT_LIMIT           = 180;
const int  RIGHT_LIMIT          = 50;
const bool STEER_LEFT_IS_BIGGER = false;  // same as the validated code (Lance Chung)
const int  SERVO_SPEED_DPS      = 300;    // max servo speed (degrees per second)
const int  SERVO_SETTLE_MS      = 400;    // time for the wheels to reach full left

// --- Motor ---
const int  MOTOR_SPEED    = 60;     // 0-255, speed for most of the turn
const int  SLOW_SPEED     = 45;     // speed near the end (raise it if the car stalls)
const bool MOTOR_REVERSED = true;
const int  START_DELAY_MS = 2000;

// --- Turn PD ---
const float TURN_ANGLE_DEG = 90.0;
const float K_TURN        = 2.5;    // steering degrees per degree left to turn
const float K_RATE        = 0.15;   // steering degrees per (deg/s) of turning speed (brake)
const float MIN_STEER_DEG = 8.0;    // always steer at least this much until the stop
const float SLOW_ZONE_DEG = 25.0;   // last degrees done at SLOW_SPEED
const float STOP_LEAD_DEG = 2.0;    // brake this many degrees BEFORE 90 (inertia)
const unsigned long TURN_TIMEOUT_MS = 8000;   // safety

// --- Gyro ---
const bool  YAW_REVERSED = false;  // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;   // fine correction: real angle / measured angle
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN = 9;
const int PWMA_PIN  = 6;
const int AIN1_PIN  = 7;
const int AIN2_PIN  = 8;
const int STBY_PIN  = 5;

// ---------------- BMI160 ----------------
const float GYRO_SCALE = 65.6;   // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

Servo steering;
float yaw = 0;                   // degrees, + = car turned LEFT
float yawRate = 0;               // deg/s, + = turning LEFT
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {               // free stop (coast)
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

// Active brake (short brake): both inputs HIGH -> the car stays still
void motorBrake() {
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, 255);
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
  bmiWrite(0x42, 0x2A);               // gyro 400 Hz (was 100 Hz)
  bmiWrite(0x43, 0x02);               // gyro range +-500 deg/s
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
  for (int i = 0; i < 1000; i++) {      // more samples = better bias
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

// ---------------- Steering ----------------
// steerLeft: + = left, - = right (degrees away from center)
void setSteering(float steerLeft, float dt) {
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);

  float maxStep = SERVO_SPEED_DPS * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));
}

// ---------------- Turn ----------------
void turnLeft90() {
  float maxLeft = STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT);

  // 1) Wheels to full left (car still stopped)
  servoAngle = STEER_LEFT_IS_BIGGER ? LEFT_LIMIT : RIGHT_LIMIT;
  steering.write((int)servoAngle);
  delay(SERVO_SETTLE_MS);

  // 2) Motor forward, PD on the yaw
  yaw = 0;
  yawRate = 0;
  prevMicros = micros();
  unsigned long startMs = millis();
  unsigned long lastPrint = 0;
  motorForward(MOTOR_SPEED);

  while (true) {
    float dt = updateGyro();
    float error = TURN_ANGLE_DEG - yaw;               // + = still must turn LEFT

    // Close enough: brake (inertia does the last STOP_LEAD_DEG)
    if (error <= STOP_LEAD_DEG) break;

    if (millis() - startMs > TURN_TIMEOUT_MS) {
      Serial.println(F("TIMEOUT: turn took too long (check YAW_REVERSED)"));
      break;
    }

    // PD steering, at least MIN_STEER_DEG to the left
    float steerLeft = K_TURN * error - K_RATE * yawRate;
    steerLeft = constrain(steerLeft, MIN_STEER_DEG, maxLeft);
    setSteering(steerLeft, dt);

    // Slower near the end
    motorForward(error < SLOW_ZONE_DEG ? SLOW_SPEED : MOTOR_SPEED);

    if (millis() - lastPrint >= 100) {
      lastPrint = millis();
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
  Serial.println(yaw - TURN_ANGLE_DEG, 1);
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

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  if (!startGyro()) {
    Serial.println(F("ERROR: BMI160 not found"));
    while (true) delay(1000);
  }
  Serial.print(F("BMI160 OK at 0x"));
  Serial.println(bmiAddr, HEX);

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  Serial.println(F("Calibrating gyro... do not move the car"));
  calibrateGyro();

  delay(START_DELAY_MS);
  turnLeft90();
}

void loop() {
  motorBrake();   // stays braked. Press RESET to repeat.
}