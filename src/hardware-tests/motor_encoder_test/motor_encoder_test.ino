// motor_encoder_test.ino
// Hardware test: N20 drive motor (TB6612FNG) + Hall encoder. Arduino Nano (classic).
// Press the start button (D10); the car runs the sequence slow forward -> fast forward -> brake ->
// reverse -> stopped, and prints the encoder pulses, wheel turns and RPM every 200 ms.
// Use it to check the motor direction (MOTOR_REVERSED), the encoder direction and the pulses per turn.
// Put the car on a stand (wheels in the air) the first time.

// --- TB6612FNG driver pins ---
#define PIN_PWMA     6   // D6 (PWM)
#define PIN_AIN1     7   // D7
#define PIN_AIN2     8   // D8

// --- Encoder pins (hardware interrupts on the Nano: D2 and D3) ---
#define PIN_ENC_A    2   // D2 (INT0)
#define PIN_ENC_B    3   // D3 (INT1)

// --- Start button ---
#define PIN_BUTTON   10  // D10

// --- Motor and encoder settings ---
// Set to 'true' if the motor turns the wrong way
const bool MOTOR_REVERSED = false;

// Pulses per revolution of the output shaft of the gearmotor. 374 is only the example value of a
// typical gearmotor (1:34 gearbox, 11 PPR encoder): measure it for the real motor
// (turn the wheel by hand exactly one revolution and read the pulse count).
const float PULSES_PER_REV = 374.0;

// Variable modified inside the interrupt
volatile long pulseCount = 0;

// Variables for the speed (RPM) calculation
long previousPulses = 0;
unsigned long previousTime = 0;

// --- Encoder interrupt service routine (ISR) ---
void readEncoder() {
  // On a rising edge of channel A, check channel B to know the direction
  if (digitalRead(PIN_ENC_B) == HIGH) {
    pulseCount++;
  } else {
    pulseCount--;
  }
}

// --- TB6612FNG motor control: speed from -255 (reverse) to 255 (forward), 0 = active brake ---
void moveMotor(int speed) {
  if (MOTOR_REVERSED) {
    speed = -speed;
  }

  if (speed > 0) {
    digitalWrite(PIN_AIN1, HIGH);
    digitalWrite(PIN_AIN2, LOW);
    analogWrite(PIN_PWMA, constrain(speed, 0, 255));
  } else if (speed < 0) {
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, HIGH);
    analogWrite(PIN_PWMA, constrain(-speed, 0, 255));
  } else {
    // Active brake (both inputs HIGH)
    digitalWrite(PIN_AIN1, HIGH);
    digitalWrite(PIN_AIN2, HIGH);
    analogWrite(PIN_PWMA, 0);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  // Motor pins
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  moveMotor(0);

  // Encoder pins with the internal pull-up
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);

  // Hardware interrupt on D2 (channel A)
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), readEncoder, RISING);

  // Start button
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial.println("\n=== MOTOR (TB6612FNG) + ENCODER TEST ===");
  Serial.println("Press the button on D10 to start the test sequence...");

  while (digitalRead(PIN_BUTTON) == HIGH) {
    delay(10);
  }
  delay(250); // Debounce

  pulseCount = 0;
  previousPulses = 0;
  previousTime = millis();
  Serial.println(">>> Starting test!\n");
}

void runPhase(const char* phaseName, int pwmValue, unsigned long durationMs) {
  Serial.print("--- ");
  Serial.print(phaseName);
  Serial.print(" (PWM: ");
  Serial.print(pwmValue);
  Serial.println(") ---");

  moveMotor(pwmValue);
  unsigned long phaseStart = millis();

  while (millis() - phaseStart < durationMs) {
    unsigned long now = millis();
    if (now - previousTime >= 200) { // Update every 200 ms
      float dtMinutes = (now - previousTime) / 60000.0;

      // Read the volatile variable safely
      noInterrupts();
      long currentPulses = pulseCount;
      interrupts();

      long deltaPulses = currentPulses - previousPulses;
      float rpm = (deltaPulses / PULSES_PER_REV) / dtMinutes;

      Serial.print("Total pulses: ");
      Serial.print(currentPulses);
      Serial.print("  |  Turns: ");
      Serial.print(currentPulses / PULSES_PER_REV, 2);
      Serial.print("  |  Speed: ");
      Serial.print(rpm, 1);
      Serial.println(" RPM");

      previousPulses = currentPulses;
      previousTime = now;
    }
  }
}

void loop() {
  // Automatic test sequence
  runPhase("1. SLOW FORWARD", 110, 2500);
  runPhase("2. FAST FORWARD", 210, 2500);
  runPhase("3. BRAKE", 0, 1500);
  runPhase("4. REVERSE", -140, 2500);
  runPhase("5. STOPPED", 0, 2000);

  Serial.println("\n=== Cycle complete. Repeating in 2 seconds... ===\n");
}
