/*
  servo_simple.ino
  -----------------
  Simplest possible steering servo test (on D9): no libraries,
  no ledc. Generates the servo pulse by hand (50Hz, 0.5-2.5ms pulse),
  so it compiles on any ESP32 core version.

  THIS IS THE VERSION THAT WORKED on the Nano ESP32 (22 Sept 2026).
  The ESP32Servo library was not installed, and ledcAttach() does not
  exist in the Nano ESP32's 2.x core, so both earlier versions failed.

  CALIBRATED: the real steering CENTER of our old chassis was 120 degrees
  (not 90). With the NEW servo/chassis (Oct 2026) measure it again.
  LEFT/RIGHT below are +/-30 around that center -- adjust
  to the real mechanical limits of the Ackermann steering.
*/

const int PIN_SERVO = D9;

const int CENTER_ANGLE = 120; // measured on the old chassis - RE-MEASURE with the new servo
const int LEFT_ANGLE   = 90;  // ADJUST
const int RIGHT_ANGLE  = 150; // ADJUST

// send pulses for 'ms' milliseconds so the servo has time to reach the angle
void moveServo(int angle, int ms) {
  int pulseUs = map(angle, 0, 180, 500, 2500);
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    digitalWrite(PIN_SERVO, HIGH);
    delayMicroseconds(pulseUs);
    digitalWrite(PIN_SERVO, LOW);
    delayMicroseconds(20000 - pulseUs);
  }
}

void setup() {
  pinMode(PIN_SERVO, OUTPUT);
}

void loop() {
  moveServo(LEFT_ANGLE, 1000);
  moveServo(CENTER_ANGLE, 1000);
  moveServo(RIGHT_ANGLE, 1000);
  moveServo(CENTER_ANGLE, 1000);
}
