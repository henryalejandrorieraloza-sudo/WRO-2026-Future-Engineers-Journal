// motor_encoder_test.ino
// Hardware test: N20 drive motor (TB6612FNG) + Hall encoder. Arduino Nano (classic).
// Press the start button (D10); the car runs the sequence slow forward -> fast forward -> brake ->
// reverse -> stopped, and prints the encoder pulses, wheel turns and RPM every 200 ms.
// Use it to check the motor direction (MOTOR_REVERSED), the encoder direction and the pulses per turn.
// Put the car on a stand (wheels in the air) the first time.

// --- TB6612FNG driver pins ---
#define PIN_PWMA     6               // D6: PWM pin, sets the motor speed
#define PIN_AIN1     7               // D7: direction input 1
#define PIN_AIN2     8               // D8: direction input 2

// --- Encoder pins (hardware interrupts on the Nano: D2 and D3) ---
#define PIN_ENC_A    2               // D2 (INT0): encoder channel A, causes the interrupt
#define PIN_ENC_B    3               // D3 (INT1): encoder channel B, tells the direction

// --- Start button ---
#define PIN_BUTTON   10              // D10: start button (with internal pull-up)

// --- Motor and encoder settings ---
// Set to 'true' if the motor turns the wrong way
const bool MOTOR_REVERSED = false;   // true flips the sign of every speed command

// Pulses per revolution of the output shaft of the gearmotor. 374 is only the example value of a
// typical gearmotor (1:34 gearbox, 11 PPR encoder): measure it for the real motor
// (turn the wheel by hand exactly one revolution and read the pulse count).
const float PULSES_PER_REV = 374.0;  // encoder pulses for one full turn of the wheel

// Variable modified inside the interrupt
volatile long pulseCount = 0;        // total pulses counted; "volatile" because the interrupt changes it

// Variables for the speed (RPM) calculation
long previousPulses = 0;             // pulse count at the last RPM update
unsigned long previousTime = 0;      // time (ms) of the last RPM update

// --- Encoder interrupt service routine (ISR) ---
void readEncoder() {                 // runs on every rising edge of channel A
  // On a rising edge of channel A, check channel B to know the direction
  if (digitalRead(PIN_ENC_B) == HIGH) {   // B is high: the wheel turns one way
    pulseCount++;                    // count one step forward
  } else {                           // B is low: the wheel turns the other way
    pulseCount--;                    // count one step backward
  }
}

// --- TB6612FNG motor control: speed from -255 (reverse) to 255 (forward), 0 = active brake ---
void moveMotor(int speed) {          // sets the motor to a speed between -255 and 255
  if (MOTOR_REVERSED) {              // the motor is wired backwards?
    speed = -speed;                  // flip the direction in software
  }

  if (speed > 0) {                   // forward
    digitalWrite(PIN_AIN1, HIGH);    // direction: AIN1 high ...
    digitalWrite(PIN_AIN2, LOW);     // ... AIN2 low = forward
    analogWrite(PIN_PWMA, constrain(speed, 0, 255));   // set the forward speed (0-255)
  } else if (speed < 0) {            // reverse
    digitalWrite(PIN_AIN1, LOW);     // direction: AIN1 low ...
    digitalWrite(PIN_AIN2, HIGH);    // ... AIN2 high = reverse
    analogWrite(PIN_PWMA, constrain(-speed, 0, 255));  // use the positive value as PWM
  } else {                           // speed 0
    // Active brake (both inputs HIGH)
    digitalWrite(PIN_AIN1, HIGH);    // both direction inputs high = the driver brakes the motor
    digitalWrite(PIN_AIN2, HIGH);
    analogWrite(PIN_PWMA, 0);        // no PWM while braking
  }
}

void setup() {                       // runs once when the Nano starts
  Serial.begin(115200);              // serial port at 115200 baud
  while (!Serial && millis() < 3000);   // wait up to 3 s for the serial port

  // Motor pins
  pinMode(PIN_PWMA, OUTPUT);         // PWM pin is an output
  pinMode(PIN_AIN1, OUTPUT);         // direction pin 1 is an output
  pinMode(PIN_AIN2, OUTPUT);         // direction pin 2 is an output
  moveMotor(0);                      // start with the motor braked / stopped

  // Encoder pins with the internal pull-up
  pinMode(PIN_ENC_A, INPUT_PULLUP);  // encoder A with pull-up (reads HIGH when idle)
  pinMode(PIN_ENC_B, INPUT_PULLUP);  // encoder B with pull-up

  // Hardware interrupt on D2 (channel A)
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), readEncoder, RISING);   // call readEncoder() on every rising edge of A

  // Start button
  pinMode(PIN_BUTTON, INPUT_PULLUP); // button with pull-up: HIGH = released, LOW = pressed

  Serial.println("\n=== MOTOR (TB6612FNG) + ENCODER TEST ===");   // title
  Serial.println("Press the button on D10 to start the test sequence...");   // instruction

  while (digitalRead(PIN_BUTTON) == HIGH) {   // wait while the button is not pressed
    delay(10);                       // check every 10 ms
  }
  delay(250);                        // debounce: ignore the button bounce

  pulseCount = 0;                    // start counting from zero
  previousPulses = 0;                // the first RPM reading starts from zero too
  previousTime = millis();           // remember the start time
  Serial.println(">>> Starting test!\n");   // confirm the start
}

void runPhase(const char* phaseName, int pwmValue, unsigned long durationMs) {   // runs one part of the sequence
  Serial.print("--- ");              // header line, part 1
  Serial.print(phaseName);           // name of the phase, for example "2. FAST FORWARD"
  Serial.print(" (PWM: ");           // part 2
  Serial.print(pwmValue);            // the PWM value used in this phase
  Serial.println(") ---");           // end of the header line

  moveMotor(pwmValue);               // apply the speed for this phase
  unsigned long phaseStart = millis();   // remember when the phase started

  while (millis() - phaseStart < durationMs) {   // keep looping until the phase time is over
    unsigned long now = millis();    // current time
    if (now - previousTime >= 200) { // Update every 200 ms
      float dtMinutes = (now - previousTime) / 60000.0;   // elapsed time since the last update, in minutes

      // Read the volatile variable safely
      noInterrupts();                // stop interrupts so the counter does not change while reading
      long currentPulses = pulseCount;   // copy the pulse count
      interrupts();                  // allow interrupts again

      long deltaPulses = currentPulses - previousPulses;   // pulses counted since the last update
      float rpm = (deltaPulses / PULSES_PER_REV) / dtMinutes;   // turns per minute = RPM

      Serial.print("Total pulses: ");    // label
      Serial.print(currentPulses);       // total pulses since the start
      Serial.print("  |  Turns: ");      // label
      Serial.print(currentPulses / PULSES_PER_REV, 2);   // total turns, two decimals
      Serial.print("  |  Speed: ");      // label
      Serial.print(rpm, 1);              // speed in RPM, one decimal
      Serial.println(" RPM");            // unit and end of line

      previousPulses = currentPulses;    // remember the count for the next RPM calculation
      previousTime = now;                // remember the time for the next RPM calculation
    }
  }
}

void loop() {                        // runs again and again after setup()
  // Automatic test sequence
  runPhase("1. SLOW FORWARD", 110, 2500);   // slow forward for 2.5 s
  runPhase("2. FAST FORWARD", 210, 2500);   // fast forward for 2.5 s
  runPhase("3. BRAKE", 0, 1500);            // active brake for 1.5 s
  runPhase("4. REVERSE", -140, 2500);       // reverse for 2.5 s
  runPhase("5. STOPPED", 0, 2000);          // motor stopped for 2 s

  Serial.println("\n=== Cycle complete. Repeating in 2 seconds... ===\n");   // end of the cycle message
}
