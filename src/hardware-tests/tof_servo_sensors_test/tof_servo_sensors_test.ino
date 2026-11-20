/*
  tof_servo_sensors_test.ino
  ---------------------------
  Board: Arduino Nano (classic, ATmega328P). Tests only the side sensors and the steering servo:
    - Steering servo on D9: left -> center -> right -> center
    - 2 side VL53L1X sensors, each one with its own I2C address

  Wiring:
    I2C (sensors):     SDA -> A4,  SCL -> A5
    XSHUT left sensor:  A3
    XSHUT right sensor: A2
    Servo:              D9

  Libraries: "VL53L1X" (Pololu). Wire and Servo come with the Arduino IDE.
*/

#include <Wire.h>
#include <Servo.h>
#include <VL53L1X.h>

// ---------- Pins ----------
const int PIN_SERVO   = 9;
const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;

// ---------- Settings ----------
// Rack-and-pinion steering: real center 120, mechanical limits 50 (right) .. 180 (left).
// This test only moves +-25 degrees, the same range the wall-following sketches use,
// so the servo never pushes against the end of the rack.
const int SERVO_CENTER = 120;
const int SERVO_LEFT   = SERVO_CENTER + 25;   // bigger angle = more to the left
const int SERVO_RIGHT  = SERVO_CENTER - 25;   // smaller angle = more to the right
const unsigned long SERVO_STEP_MS = 1000;

const unsigned long PRINT_MS = 200;

// I2C address of each VL53L1X (0x29 is the factory default)
const uint8_t ADDR_LEFT  = 0x30;
const uint8_t ADDR_RIGHT = 0x31;

// ---------- Objects and state ----------
VL53L1X tofLeft;
VL53L1X tofRight;
Servo steering;

bool leftOk = false;
bool rightOk = false;

const int SERVO_SEQ[4]    = {SERVO_LEFT, SERVO_CENTER, SERVO_RIGHT, SERVO_CENTER};
const char *SERVO_NAME[4] = {"LEFT", "CENTER", "RIGHT", "CENTER"};
int servoStep = 0;
unsigned long lastServoMs = 0;
unsigned long lastPrintMs = 0;

// ---------- VL53L1X sensors ----------
// Turns one sensor on through XSHUT, gives it a new address and configures it
bool initTof(VL53L1X &s, int xshut, uint8_t addr, const char *name) {
  digitalWrite(xshut, HIGH);   // only this sensor is on
  delay(20);
  s.setTimeout(500);
  if (!s.init()) {
    Serial.print(name);
    Serial.println(F(" NOT FOUND"));
    return false;
  }
  s.setAddress(addr);          // from here on it answers at the new address
  s.setDistanceMode(VL53L1X::Long);
  s.setMeasurementTimingBudget(100000);
  s.setROISize(4, 13);
  s.startContinuous(100);
  Serial.print(name);
  Serial.println(F(" OK"));
  return true;
}

// Returns the distance in cm, or -1 if the reading is not valid
float readCm(VL53L1X &s, bool ok) {
  if (!ok) return -1;
  uint16_t mm = s.read();
  if (s.timeoutOccurred()) return -1;
  VL53L1X::RangeStatus st = s.ranging_data.range_status;
  if (st == VL53L1X::RangeValid || (st == VL53L1X::SignalFail && mm > 0)) {
    return mm / 10.0;
  }
  return -1;
}

// ---------- Printing ----------
void printLine(float l, float r) {
  Serial.print(F("LEFT: "));
  if (l < 0) Serial.print(F("--")); else Serial.print(l, 1);
  Serial.print(F(" cm | RIGHT: "));
  if (r < 0) Serial.print(F("--")); else Serial.print(r, 1);
  Serial.print(F(" cm | SERVO: "));
  Serial.println(SERVO_NAME[servoStep]);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Servo
  steering.attach(PIN_SERVO);
  steering.write(SERVO_CENTER);

  // I2C (on the Nano, A4/A5 are SDA/SCL automatically)
  Wire.begin();
  Wire.setWireTimeout(25000, true);   // do not hang if the bus gets stuck (needs Arduino AVR Boards 1.8.3 or newer)

  // Sensors: turn both off, then turn them on one by one
  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(20);
  leftOk  = initTof(tofLeft,  XSHUT_LEFT,  ADDR_LEFT,  "LEFT");
  rightOk = initTof(tofRight, XSHUT_RIGHT, ADDR_RIGHT, "RIGHT");

  Serial.println(F("Ready"));

  unsigned long now = millis();
  lastServoMs = now;
  lastPrintMs = now;
}

// ---------- Loop ----------
void loop() {
  unsigned long nowMs = millis();

  // 1) Servo: left -> center -> right -> center
  if (nowMs - lastServoMs >= SERVO_STEP_MS) {
    lastServoMs = nowMs;
    servoStep = (servoStep + 1) % 4;
    steering.write(SERVO_SEQ[servoStep]);
  }

  // 2) Side sensors + printing
  if (nowMs - lastPrintMs >= PRINT_MS) {
    lastPrintMs = nowMs;
    float l = readCm(tofLeft,  leftOk);
    float r = readCm(tofRight, rightOk);
    printLine(l, r);
  }
}
