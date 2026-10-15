// servo_only_test.ino
// Moves ONLY the steering servo: center -> left -> center -> right, slowly, forever.
// Board: Arduino Nano (classic, ATmega328P) | Servo on D9
// Use it to check the new pinion + rack steering: the servo must not hum or push at the ends.

#include <Servo.h>

// ================= SETTINGS (change these) =================
const int CENTER_ANGLE = 120;   // provisional - adjust to the new rack
const int LEFT_LIMIT   = 180;   // provisional - widen slowly
const int RIGHT_LIMIT  = 50;    // provisional
const int STEP_DELAY_MS = 15;   // ms per degree (bigger = slower)
const int HOLD_MS       = 1000; // time it stays at each position
// ===========================================================

const int SERVO_PIN = 9;
Servo steering;
int currentAngle = CENTER_ANGLE;

// Move one degree at a time to the target angle
void moveTo(int target) {
  while (currentAngle != target) {
    currentAngle += (target > currentAngle) ? 1 : -1;
    steering.write(currentAngle);
    delay(STEP_DELAY_MS);
  }
  Serial.print(F("Servo at: "));
  Serial.println(currentAngle);
  delay(HOLD_MS);
}

void setup() {
  Serial.begin(115200);
  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);
  delay(1000);
}

void loop() {
  moveTo(LEFT_LIMIT);
  moveTo(CENTER_ANGLE);
  moveTo(RIGHT_LIMIT);
  moveTo(CENTER_ANGLE);
}
