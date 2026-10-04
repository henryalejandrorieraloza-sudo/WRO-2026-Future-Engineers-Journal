// Rear motor test: FORWARD -> STOP -> BACKWARD -> STOP (loop)
// Board: Arduino Nano ESP32 | Driver: TB6612FNG | Motor: N20 DC (rear axle)

const int PWMA_PIN = D6;   // speed (PWM)
const int AIN1_PIN = D7;   // direction 1
const int AIN2_PIN = D8;   // direction 2
const int STBY_PIN = D5;   // standby (must be HIGH). If STBY is wired to 3.3V, ignore this pin.

// --- Tunable values ---
const int MOTOR_SPEED  = 150;   // 0-255
const int RUN_TIME_MS  = 2000;  // time running in each direction
const int STOP_TIME_MS = 1000;  // pause between direction changes

void motorForward(int speed) {
  digitalWrite(AIN1_PIN, HIGH);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorBackward(int speed) {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, HIGH);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

void setup() {
  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);

  digitalWrite(STBY_PIN, HIGH);  // enable driver
  motorStop();

  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 5000) delay(10);  // Nano ESP32 native USB

  Serial.println("Rear motor test started");
}

void loop() {
  Serial.println("FORWARD");
  motorForward(MOTOR_SPEED);
  delay(RUN_TIME_MS);

  Serial.println("STOP");
  motorStop();
  delay(STOP_TIME_MS);

  Serial.println("BACKWARD");
  motorBackward(MOTOR_SPEED);
  delay(RUN_TIME_MS);

  Serial.println("STOP");
  motorStop();
  delay(STOP_TIME_MS);
}
