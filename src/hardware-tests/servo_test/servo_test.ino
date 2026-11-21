// servo_test.ino
// Hardware test: steering servo on D9. Arduino Nano (classic).
// After the start button (D10) the servo sweeps between MIN_ANGLE and MAX_ANGLE in small steps,
// pausing at the center and at both limits, and prints each angle to the Serial Monitor.
// Use it to find the real center and the mechanical limits of the rack without pushing it too far.

#include <Servo.h>                    // standard library that generates the servo PWM signal

#define PIN_SERVO  9                  // D9: signal wire of the steering servo
#define PIN_BUTTON 10                 // D10: start button (the other leg goes to GND)

Servo steeringServo;                  // the servo object used to move the steering

// Configurable sweep parameters
const int MIN_ANGLE = 10;             // Left limit in degrees (adjust if the linkage collides)
const int MAX_ANGLE = 170;            // Right limit in degrees (adjust if the linkage collides)
const int CENTER    = 90;             // Reference midpoint: wheels straight ahead
const int STEP      = 2;              // Degrees moved per step
const int DELAY_MS  = 30;             // Sweep speed: milliseconds between steps

int currentAngle = CENTER;            // angle sent to the servo right now, starts at the center
int direction = 1;                    // movement direction: 1 = towards the right, -1 = towards the left

void setup() {                        // runs once when the Nano starts
  Serial.begin(115200);               // open the serial port at 115200 baud (Serial Monitor speed)
  while (!Serial && millis() < 3000); // wait up to 3 s for the serial port to be ready

  pinMode(PIN_BUTTON, INPUT_PULLUP);  // button pin with internal pull-up: reads HIGH when released, LOW when pressed
  steeringServo.attach(PIN_SERVO);    // connect the servo object to pin D9
  steeringServo.write(CENTER);        // move the wheels to the center before the test

  Serial.println("\n=== SERVO SWEEP TEST (PIN D9) ===");   // title of the test
  Serial.println("Servo centered at 90°. Press button on D10 to start sweep..."); // instruction

  // Wait until the button on D10 is pressed (LOW)
  while (digitalRead(PIN_BUTTON) == HIGH) {   // while the button is still released
    delay(10);                        // check again every 10 ms
  }
  delay(250);                         // simple debounce: ignore the bounce of the button contacts
  Serial.println(">>> Starting servo sweep!\n");   // confirm that the sweep starts
}

void loop() {                         // runs again and again after setup()
  steeringServo.write(currentAngle);  // send the current angle to the servo

  // Print current angle and key reference points to Serial Monitor
  Serial.print("Angle: ");            // label
  Serial.print(currentAngle);         // the angle in degrees
  Serial.print("°");                  // degree sign

  if (currentAngle == CENTER) {       // at the center?
    Serial.print("  <-- [CENTER]");   // mark it in the text
    delay(600);                       // brief pause at the center to inspect the alignment
  } else if (currentAngle <= MIN_ANGLE) {   // at or past the left limit?
    Serial.print("  <-- [LEFT LIMIT]");     // mark it in the text
    delay(600);                       // pause so the limit can be checked
  } else if (currentAngle >= MAX_ANGLE) {   // at or past the right limit?
    Serial.print("  <-- [RIGHT LIMIT]");    // mark it in the text
    delay(600);                       // pause so the limit can be checked
  }

  Serial.println();                   // end the line

  // Calculate next position
  currentAngle += (STEP * direction); // move one step in the current direction

  // Reverse direction when reaching limits
  if (currentAngle >= MAX_ANGLE) {    // reached or passed the right limit?
    currentAngle = MAX_ANGLE;         // clamp the angle to the right limit
    direction = -1;                   // from now on move towards the left
  } else if (currentAngle <= MIN_ANGLE) {   // reached or passed the left limit?
    currentAngle = MIN_ANGLE;         // clamp the angle to the left limit
    direction = 1;                    // from now on move towards the right
  }

  delay(DELAY_MS);                    // wait before the next step (sets the sweep speed)
}
