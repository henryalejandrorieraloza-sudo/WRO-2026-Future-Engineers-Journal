/*
  drive_until_left_open.ino
  -------------------------
  The robot drives forward and STOPS when the LEFT TOF reads
  more than STOP_DISTANCE_CM (the left wall ended).

  Board: Arduino Nano ESP32
  Motor driver TB6612FNG: PWMA=D6, AIN1=D7, AIN2=D8, STBY=D5 (check!)
  LEFT TOF400C (VL53L1X): XSHUT=A3, I2C SDA=A4, SCL=A5
  Library: "VL53L1X" by Pololu

  Only the LEFT sensor is used: the other 3 TOF are kept OFF
  (XSHUT LOW), so the left one can stay at the default address 0x29.

  Safety: it waits START_DELAY_MS after reset before moving,
  so you have time to put the robot on the floor.
*/

#include <Wire.h>
#include <VL53L1X.h>

// ---------- settings ----------
const float STOP_DISTANCE_CM = 170.0;  // stop when LEFT > this
const int   CONFIRM_READINGS = 2;      // readings in a row above the limit (avoids one bad spike)
const bool  NO_TARGET_MEANS_OPEN = true; // if the sensor sees nothing (too far), count it as "open"
const int   MOTOR_SPEED      = -70;    // 0-255
const unsigned long START_DELAY_MS = 3000;

// ---------- pins ----------
const int PWMA = D6, AIN1 = D7, AIN2 = D8, STBY = D5;
const int XSHUT_LEFT  = A3;
const int XSHUT_OTHERS[3] = {A2, A0, A1};   // RIGHT, BACK, FRONT -> kept off

VL53L1X leftSensor;
int  aboveCount = 0;
bool stopped    = false;

void motorForward(int speed) {
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, speed);
}

void motorBrake() {              // short brake: stops fast
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, 0);
}

void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 2000) delay(10);

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT); pinMode(STBY, OUTPUT);
  motorBrake();

  // other sensors OFF, left sensor ON
  for (int i = 0; i < 3; i++) { pinMode(XSHUT_OTHERS[i], OUTPUT); digitalWrite(XSHUT_OTHERS[i], LOW); }
  pinMode(XSHUT_LEFT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);  delay(20);
  digitalWrite(XSHUT_LEFT, HIGH); delay(20);

  Wire.begin(A4, A5);
  leftSensor.setTimeout(500);
  if (!leftSensor.init()) {
    Serial.println("ERROR: LEFT sensor not found. The robot will NOT move.");
    stopped = true;
    return;
  }
  leftSensor.setDistanceMode(VL53L1X::Long);
  leftSensor.setMeasurementTimingBudget(50000);   // 50 ms -> faster reaction
  leftSensor.startContinuous(50);

  Serial.printf("LEFT OK. Starting in %lu s...\n", START_DELAY_MS / 1000);
  delay(START_DELAY_MS);
  motorForward(MOTOR_SPEED);
  Serial.println("GO");
}

void loop() {
  if (stopped) return;

  uint16_t mm = leftSensor.read();
  VL53L1X::RangeStatus status = leftSensor.ranging_data.range_status;

  bool open = false;
  if (leftSensor.timeoutOccurred()) {
    Serial.println("LEFT: TIMEOUT");          // no answer: do not count it
  } else if (status == VL53L1X::RangeValid || (status == VL53L1X::SignalFail && mm > 0)) {
    float cm = mm / 10.0;
    open = (cm > STOP_DISTANCE_CM);
    Serial.printf("LEFT: %.1f cm\n", cm);
  } else {
    // sensor sees nothing useful (often: nothing close enough)
    open = NO_TARGET_MEANS_OPEN;
    Serial.printf("LEFT: --- (%s)\n", VL53L1X::rangeStatusToString(status));
  }

  aboveCount = open ? aboveCount + 1 : 0;

  if (aboveCount >= CONFIRM_READINGS) {
    motorBrake();
    stopped = true;
    Serial.println("STOP: left side is open");
  }
}
