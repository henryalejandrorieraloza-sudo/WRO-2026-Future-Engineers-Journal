/*
  servo_simple.ino
  -----------------
  Simplest possible steering servo test (on D9): no libraries.
  Generates the servo pulse by hand (50Hz, 0.5-2.5ms pulse).

  Board: Arduino Nano (classic, ATmega328P).
  This hand-made pulse was first written on 22 Sept 2026 because the
  servo libraries did not work with the Nano ESP32 core (journal
  Problem 6). On the classic Nano the standard Servo.h also works.

  CALIBRATED: the real steering CENTER of our old chassis was 120 degrees
  (not 90). With the NEW servo/chassis (Oct 2026) measure it again.
  LEFT/RIGHT below are +/-30 around that center -- adjust
  to the real mechanical limits of the Ackermann steering.
*/

const int PIN_SERVO = 9;   // D9

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
