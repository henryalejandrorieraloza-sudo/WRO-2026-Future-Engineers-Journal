#include <Servo.h>

#define PIN_SERVO  9   // D9
#define PIN_BUTTON 10  // D10 (Start button)

Servo steeringServo;

// Configurable sweep parameters
const int MIN_ANGLE = 10;   // Left limit (adjust if linkage collides)
const int MAX_ANGLE = 170;  // Right limit (adjust if linkage collides)
const int CENTER    = 90;   // Reference midpoint
const int STEP      = 2;    // Degrees per step
const int DELAY_MS  = 30;   // Sweep speed (ms between steps)

int currentAngle = CENTER;
int direction = 1; // 1 = towards right, -1 = towards left

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  steeringServo.attach(PIN_SERVO);
  steeringServo.write(CENTER);

  Serial.println("\n=== SERVO SWEEP TEST (PIN D9) ===");
  Serial.println("Servo centered at 90°. Press button on D10 to start sweep...");

  // Wait until the button on D10 is pressed (LOW)
  while (digitalRead(PIN_BUTTON) == HIGH) {
    delay(10);
  }
  delay(250); // Simple debounce
  Serial.println(">>> Starting servo sweep!\n");
}

void loop() {
  steeringServo.write(currentAngle);

  // Print current angle and key reference points to Serial Monitor
  Serial.print("Angle: ");
  Serial.print(currentAngle);
  Serial.print("°");

  if (currentAngle == CENTER) {
    Serial.print("  <-- [CENTER]");
    delay(600); // Brief pause at center to inspect alignment
  } else if (currentAngle <= MIN_ANGLE) {
    Serial.print("  <-- [LEFT LIMIT]");
    delay(600);
  } else if (currentAngle >= MAX_ANGLE) {
    Serial.print("  <-- [RIGHT LIMIT]");
    delay(600);
  }

  Serial.println();

  // Calculate next position
  currentAngle += (STEP * direction);

  // Reverse direction when reaching limits
  if (currentAngle >= MAX_ANGLE) {
    currentAngle = MAX_ANGLE;
    direction = -1;
  } else if (currentAngle <= MIN_ANGLE) {
    currentAngle = MIN_ANGLE;
    direction = 1;
  }

  delay(DELAY_MS);
}