/*
  servo_calibration.ino
  ---------------------
  Find the real CENTER, LEFT limit and RIGHT limit of the steering
  servo (MG90, 180 degrees) on the new chassis.

  How to use (Serial Monitor, 115200, "New Line"):
    - type a number 0-180 and press Enter -> the servo goes to that angle
    - type  a  -> 1 degree less      type  d  -> 1 degree more
      ("aaaaa" = 5 degrees less, "ddd" = 3 degrees more)
    - type  c  -> back to the current center (START_ANGLE)
  The servo keeps holding the angle until you send a new one.

  Find:
    CENTER = angle where both front wheels point straight ahead
    LEFT / RIGHT = angle where the wheels stop turning (the mechanism
    hits its end). Then use 2-3 degrees LESS than that, so the servo
    never pushes against the end (it heats up and can break).

  Same pulse-by-hand method as servo_simple.ino (no libraries).
*/

const int PIN_SERVO   = D9;
const int START_ANGLE = 120;   // old center, change after measuring

int angle = START_ANGLE;

void sendPulse(int a) {
  int pulseUs = map(a, 0, 180, 500, 2500);
  digitalWrite(PIN_SERVO, HIGH);
  delayMicroseconds(pulseUs);
  digitalWrite(PIN_SERVO, LOW);
  delayMicroseconds(20000 - pulseUs);   // 50 Hz
}

void printAngle() {
  Serial.printf("angle = %d  (pulse %d us)\n", angle, (int)map(angle, 0, 180, 500, 2500));
}

void setup() {
  pinMode(PIN_SERVO, OUTPUT);
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 5000) delay(10);
  Serial.println("SERVO CALIBRATION: type 0-180, or a / d / c");
  printAngle();
}

void loop() {
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s == "c") angle = START_ANGLE;
    else if (s.length() > 0 && (s[0] == 'a' || s[0] == 'd')) {
      // every letter counts: "aaaaa" = 5 degrees less, "ddd" = 3 degrees more
      for (unsigned int i = 0; i < s.length(); i++) {
        if (s[i] == 'a') angle--;
        if (s[i] == 'd') angle++;
      }
    }
    else if (s.length() > 0) angle = s.toInt();
    angle = constrain(angle, 0, 180);
    printAngle();
  }
  sendPulse(angle);   // keep holding the angle
}
