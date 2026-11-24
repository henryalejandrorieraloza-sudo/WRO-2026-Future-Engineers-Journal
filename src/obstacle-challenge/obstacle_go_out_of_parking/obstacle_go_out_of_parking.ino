// =====================================================================
// GO OUT OF PARKING - Arduino Nano (Obstacle Challenge)
// The car is parked nose-to-wall between two walls. It must:
//   1) measure the free space on its LEFT and RIGHT with two ToF sensors,
//   2) turn toward the side with MORE space (first turn, 45 degrees, by gyro),
//   3) straighten the wheels for a short moment,
//   4) counter-steer back to about 10 degrees,
//   5) stop, wait, and REVERSE straight for a measured number of encoder ticks.
// Only the rear wheels drive (TB6612FNG, PWMA). Steering is a servo.
// The gyro (BMI160) gives the heading, the encoder (on pin 3) gives the distance.
// Set CALIBRATE_ENCODER = true first to measure REVERSE_TARGET_TICKS.
// =====================================================================

#include <Arduino.h>      // core Arduino functions (pinMode, digitalWrite, Serial, ...)
#include <Wire.h>         // I2C bus, used by the gyro and the ToF sensors
#include <Servo.h>        // standard servo library for the steering
#include <VL53L1X.h>      // library for the VL53L1X time-of-flight distance sensors
#include <math.h>         // fabs() and other math functions

// ---------- Set these after measuring your encoder ----------
const bool CALIBRATE_ENCODER = false;             // true = only count encoder ticks (motor stays off)
const unsigned long REVERSE_TARGET_TICKS = 597;   // encoder ticks for FIVE wheel turns: 5 x pi x 41 mm x 9.27 ticks/cm (calculated)

// ---------- Steering and motor ----------
const byte SERVO_PIN = 9;         // Nano pin that sends the signal to the steering servo
const byte LEFT_LOCK = 10;        // servo angle for full LEFT lock (wheels turned left)
const byte CENTER = 85;           // servo angle for wheels straight ahead (measured 21 Nov on this chassis)
const byte RIGHT_LOCK = 170;      // servo angle for full RIGHT lock (wheels turned right)

const byte PWMA = 6;              // TB6612FNG PWM pin: sets the motor speed
const byte AIN1 = 7;              // TB6612FNG direction pin 1
const byte AIN2 = 8;              // TB6612FNG direction pin 2

const byte DRIVE_PWM = 90;        // motor speed for the first turn (0-255)
const byte REVERSE_PWM = 90;      // motor speed for reversing (0-255)
const byte REVERSE_SLOW_PWM = 75; // slower reverse speed used near the end of the target

// ---------- Sensors ----------
const byte ENCODER_PIN = 3;       // pin of the wheel encoder (interrupt pin on the Nano)
const byte REAR_XSHUT = A0;       // shutdown pin of the rear ToF sensor (not used in this sketch)
const byte FRONT_XSHUT = A1;      // shutdown pin of the front ToF sensor (not used in this sketch)
const byte RIGHT_XSHUT = A2;      // shutdown pin of the right ToF sensor (used to give it its own address)
const byte LEFT_XSHUT = A3;       // shutdown pin of the left ToF sensor (used to give it its own address)

// ---------- Maneuver ----------
const float FIRST_ANGLE_DEG = 45.0f;        // how far (degrees) the car turns to leave the parking
const float RETURN_ANGLE_DEG = 10.0f;       // when the yaw gets this close to 0 the car stops turning
const unsigned long STRAIGHT_TICKS = 1;     // encoder ticks to drive straight before countersteering
const unsigned long PAUSE_BEFORE_REVERSE_MS = 500;  // wait this long with the motor off before reversing
const unsigned long TURN_TIMEOUT_MS = 5000;         // a turn that takes longer than this is an error
const unsigned long ENCODER_STALL_MS = 1500;        // no wheel pulses for this long = something is wrong
const unsigned long GYRO_PERIOD_US = 5000;          // read the gyro at most every 5000 us (200 times/s)

Servo steering;                   // the steering servo object
VL53L1X leftTof, rightTof;        // the two distance sensors, left and right

enum Stage {                      // the steps of the maneuver, in order
  WAIT_FOR_SIDES,                 // measuring the left and right distances
  FIRST_TURN,                     // turning 45 degrees toward the open side
  DRIVE_STRAIGHT,                 // rolling forward a tiny bit with wheels centered
  RETURN_TURN,                    // countersteering back to about 10 degrees
  PAUSE_BEFORE_REVERSE,           // stopped, waiting before reverse
  REVERSING,                      // driving backward on the encoder count
  FINISHED                        // done or stopped because of an error
};

Stage stage = FINISHED;           // current step; starts as FINISHED until setup() succeeds
byte gyroAddress = 0;             // I2C address of the BMI160 (0x68 or 0x69), found at startup
byte returnSteering = CENTER;     // servo angle used for the countersteer
float gyroBias = 0.0f;            // average gyro reading when the car is still (zero offset)
float yawDeg = 0.0f;              // heading change since the turn started, in degrees
float outboundSign = 1.0f;        // +1 if the car turned right, -1 if left (sign of the first turn)

volatile unsigned long encoderTicks = 0;  // total wheel pulses; "volatile" because the interrupt changes it
unsigned long straightStartTicks = 0;     // tick count when the straight part started
unsigned long reverseStartTicks = 0;      // tick count when reversing started (ignores earlier ticks)
unsigned long lastReverseCount = 0;       // last traveled value seen while reversing
unsigned long lastReversePulseMs = 0;     // time of the last new encoder pulse while reversing
unsigned long stageStartMs = 0;           // time when the current stage started (for timeouts)
unsigned long lastGyroUs = 0;             // time of the last gyro update, in microseconds

// Interrupt routine for the wheel encoder. Runs on every rising edge; counts one tick.
void encoderRise() {              // interrupt routine: runs on every rising edge of the encoder
  ++encoderTicks;                 // count one wheel pulse
}

unsigned long readTicks() {       // returns the tick count safely
  noInterrupts();                 // stop interrupts so the value is not changed while reading
  unsigned long count = encoderTicks; // copy the counter
  interrupts();                   // allow interrupts again
  return count;                   // give back the copy
}

// Used only when CALIBRATE_ENCODER is true: counts encoder ticks with the motor off, so the ticks for five
// wheel turns can be measured by hand.
void runCalibration() {           // used only when CALIBRATE_ENCODER is true
  if (!Serial.available()) return;  // no key pressed: do nothing
  char command = Serial.read();     // read one character sent from the computer

  if (command == 'z' || command == 'Z') {   // 'z' = zero the counter
    noInterrupts();               // protect the change of the counter
    encoderTicks = 0;             // reset the count
    interrupts();                 // re-enable interrupts
    Serial.println(F("Zeroed. Rotate the wheel five turns, then send p."));  // tell the user what to do
  } else if (command == 'p' || command == 'P') {  // 'p' = print the count
    Serial.print(F("Encoder count: "));           // label
    Serial.println(readTicks());                   // print the current count
  }
}

// Stops the motor and the steering, prints the reason and the state, and sets the final stop flag.
void stopCar(const __FlashStringHelper *reason) {  // stops everything and prints why
  analogWrite(PWMA, 0);           // motor speed to zero
  steering.write(CENTER);         // wheels straight
  stage = FINISHED;               // no more steps will run
  Serial.println(reason);         // print the reason (text stored in flash memory)
}

// ---------- BMI160 ----------
// Reads 'count' bytes starting at register 'reg' of the BMI160 over I2C.
bool readRegisters(byte reg, byte *data, byte count) {  // read 'count' bytes starting at register 'reg'
  Wire.beginTransmission(gyroAddress);  // address the gyro
  Wire.write(reg);                      // send the register number
  if (Wire.endTransmission(false) != 0) return false;   // repeated start: keep the bus; fail if no answer
  if (Wire.requestFrom(gyroAddress, count) != count) return false; // ask for the bytes; fail if not all arrive

  for (byte i = 0; i < count; ++i) {    // read each byte
    data[i] = Wire.read();              // store it in the buffer
  }
  return true;                          // success
}

// Writes one byte to a BMI160 register over I2C. Returns false if the write failed.
bool writeRegister(byte reg, byte value) {  // write one byte into a gyro register
  Wire.beginTransmission(gyroAddress);  // address the gyro
  Wire.write(reg);                      // which register
  Wire.write(value);                    // which value
  return Wire.endTransmission() == 0;   // true if the gyro acknowledged
}

// Reads the rotation speed around the vertical (Z) axis and returns it in degrees per second.
bool readGyroZ(float &rate) {     // reads the rotation speed around the vertical axis (Z)
  byte data[2];                   // two bytes: low and high
  if (!readRegisters(0x10, data, 2)) return false; // register 0x10 = gyro Z, fail if read fails
  int16_t raw = (int16_t)(((uint16_t)data[1] << 8) | data[0]); // join high and low byte into a signed number
  rate = raw / 65.6f;             // convert raw counts to degrees per second (sensitivity of this range)
  return true;                    // success
}

// Finds and starts the BMI160 (tries the two possible I2C addresses), then measures its zero offset with the car still.
bool initGyro() {                 // finds and starts the BMI160, then measures its zero offset
  const byte addresses[] = {0x69, 0x68};  // the two possible I2C addresses of the BMI160
  bool found = false;             // becomes true when the chip answers

  for (byte address : addresses) {        // try each address
    gyroAddress = address;                // use this address for the next reads
    byte id = 0;                          // will hold the chip ID
    if (readRegisters(0x00, &id, 1) && id == 0xD1) {  // register 0x00 = chip ID, BMI160 uses 0xD1
      found = true;               // chip found
      break;                      // stop searching
    }
  }
  if (!found) return false;       // no BMI160 on the bus

  if (!writeRegister(0x7E, 0x15)) return false;  // command register: start the accelerometer
  delay(100);                     // wait for the sensor to start
  if (!writeRegister(0x42, 0x29)) return false;  // gyro configuration (output data rate / bandwidth)
  delay(2);                       // short wait
  if (!writeRegister(0x43, 0x02)) return false;  // gyro range setting (sensitivity used by the 65.6 divisor)
  delay(50);                      // let the gyro settle

  Serial.println(F("Calibrating gyro. Keep the car still."));  // tell the user to keep still
  float sum = 0.0f;               // total of the readings
  const int samples = 250;        // how many readings to average
  for (int i = 0; i < samples; ++i) {   // take all the readings
    float rate;                   // one reading
    if (!readGyroZ(rate)) return false; // fail if a read fails
    sum += rate;                  // add it to the total
    delay(5);                     // about 1.25 s in total
  }
  gyroBias = sum / samples;       // the average while still = the zero offset
  return true;                    // gyro ready
}

// Adds the rotation since the last call to yawDeg, using the gyro rate and the elapsed time.
bool updateYaw() {                // adds the rotation since the last call to yawDeg
  unsigned long now = micros();   // current time in microseconds
  unsigned long elapsed = now - lastGyroUs;  // time since the last update
  if (elapsed < GYRO_PERIOD_US) return true; // too soon: skip this time, not an error

  float rate;                     // rotation speed in degrees per second
  if (!readGyroZ(rate)) return false;        // read the gyro, fail if it does not answer
  yawDeg += (rate - gyroBias) * (elapsed / 1000000.0f); // speed minus offset, times time in seconds = angle
  lastGyroUs = now;               // remember when this update happened
  return true;                    // success
}

// ---------- VL53L1X ----------
// Keeps one ToF sensor switched off (XSHUT LOW) during start-up, so the other sensors can be addressed first.
void holdInReset(byte pin) {      // keeps a ToF sensor switched off during startup
  digitalWrite(pin, LOW);         // pull the XSHUT pin low = sensor off
  pinMode(pin, OUTPUT);           // make it an output so it stays low
}

// Starts one VL53L1X and gives it its own I2C address.
bool initTof(VL53L1X &tof, byte xshut, byte address) {  // starts one ToF sensor and gives it a new address
  pinMode(xshut, INPUT);          // release the sensor: it turns on
  delay(10);                      // give it time to wake up
  tof.setTimeout(350);            // a read that takes longer than 350 ms counts as failed
  if (!tof.init()) return false;  // start the sensor at the default address 0x29
  tof.setAddress(address);        // move it to its own address so both sensors can be on the bus

  return tof.last_status == 0     // no error from the last command
      && tof.setDistanceMode(VL53L1X::Long)          // long mode: good range
      && tof.setMeasurementTimingBudget(140000UL)    // 140 ms per measurement: more accurate
      && !Wire.getWireTimeoutFlag();                 // no bus timeout happened
}

// Reads one distance from a ToF sensor in mm, prints it with the label, and returns it.
int measure(VL53L1X &tof, const __FlashStringHelper *label,  // reads one distance and prints it
            bool shortMode) {                                 // shortMode: short or long range this time
  Wire.clearWireTimeoutFlag();    // forget old bus timeouts
  Serial.print(label);            // print "Left" or "Right"
  Serial.print(shortMode ? F(" short: ") : F(" long: "));  // print which mode is used

  if (!tof.setDistanceMode(shortMode ? VL53L1X::Short   // change the mode for this reading
                                     : VL53L1X::Long)) {
    Serial.println(F("mode setup failed"));  // could not change the mode
    return 0;                     // 0 means "no valid reading"
  }

  uint16_t mm = tof.readSingle(); // take one distance reading, in millimeters
  if (tof.timeoutOccurred() || tof.last_status ||
      Wire.getWireTimeoutFlag()) {               // any kind of communication error?
    Serial.println(F("I2C error or timeout"));   // report it
    return 0;                     // no valid reading
  }

  auto status = tof.ranging_data.range_status;   // quality code of this reading
  Serial.print(mm);               // print the distance
  Serial.print(F(" mm, "));       // separator
  Serial.println(VL53L1X::rangeStatusToString(status));  // print the quality as text

  if (status != VL53L1X::RangeValid &&
      status != VL53L1X::RangeValidMinRangeClipped) return 0;  // only good readings are used

  return mm <= 4000 ? mm : 0;     // accept distances up to 4 m, otherwise treat as no reading
}

// Measures the left and right distances and starts the first turn toward the side with more free space.
void chooseDirection() {          // measures both sides and starts the first turn toward the open side
  static int left = 0, right = 0; // keep the last valid readings between calls (0 = not read yet)
  static bool shortMode = false;  // alternates between short and long mode on each call

  if (!left) left = measure(leftTof, F("Left"), shortMode);     // read the left side if still missing
  if (!right) right = measure(rightTof, F("Right"), shortMode); // read the right side if still missing
  shortMode = !shortMode;         // switch the mode for the next call

  if (!left || !right) return;    // one side is still unknown: try again next loop
  if (left == right) {            // both sides are exactly equal: no decision possible
    left = right = 0;             // clear both and measure again
    return;
  }

  bool turnLeft = left > right;   // more space on the left = turn left (out of the parking)
  returnSteering = turnLeft ? RIGHT_LOCK : LEFT_LOCK;  // countersteer in the opposite direction later
  steering.write(turnLeft ? LEFT_LOCK : RIGHT_LOCK);   // turn the wheels toward the open side
  Serial.println(turnLeft ? F("First turn LEFT") : F("First turn RIGHT"));  // report the choice
  delay(75);                      // let the servo move before the car starts to roll

  yawDeg = 0.0f;                  // start counting the rotation from zero
  lastGyroUs = micros();          // start the gyro clock now
  stageStartMs = millis();        // start the timeout clock for the turn
  stage = FIRST_TURN;             // next step: the first turn
  analogWrite(PWMA, DRIVE_PWM);   // start the rear motor
}

// ---------- Reverse maneuver ----------
// Switches the motor to reverse at the reverse speed and starts counting encoder ticks for the back-up.
void startReverse() {             // sets the motor direction to backward and starts counting
  // Motor PWM has been zero throughout the pause.
  digitalWrite(AIN2, LOW);        // direction: AIN2 low ...
  digitalWrite(AIN1, HIGH);       // ... AIN1 high = reverse

  // Exclude all ticks from the forward maneuver and stopping pause.
  reverseStartTicks = readTicks();      // remember the count now, so only reverse ticks are counted
  lastReverseCount = 0;           // nothing counted yet
  lastReversePulseMs = millis();  // start the stall timer now
  stage = REVERSING;              // next step: reverse
  analogWrite(PWMA, REVERSE_PWM); // start the motor at reverse speed
  Serial.println(F("Reversing straight."));  // report
}

// Runs every loop while reversing: slows the motor down near the target, and stops when the target
// tick count is reached, when the motor has been reversing too long, or when the encoder stalls.
void updateReverse() {            // runs every loop while reversing
  unsigned long traveled = readTicks() - reverseStartTicks;  // ticks counted since reverse started
  unsigned long now = millis();   // current time

  if (traveled >= REVERSE_TARGET_TICKS) {   // reached the measured distance
    stopCar(F("Five-turn encoder target reached. Motor stopped."));  // stop and report
    Serial.print(F("Reverse ticks at cutoff: "));   // print the final count
    Serial.println(traveled);
    return;                       // nothing more to do
  }

  if (traveled != lastReverseCount) {       // a new pulse arrived since the last loop
    lastReverseCount = traveled;  // remember the new count
    lastReversePulseMs = now;     // reset the stall timer
  } else if (now - lastReversePulseMs >= ENCODER_STALL_MS) {  // no pulse for too long
    stopCar(F("Reverse stopped: no encoder pulses for 1.5 seconds."));  // the wheel is stuck or the encoder is broken
    Serial.print(F("Reverse ticks counted: "));  // print how far it got
    Serial.println(traveled);
    return;                       // nothing more to do
  }

  unsigned long remaining = REVERSE_TARGET_TICKS - traveled;  // ticks still to go
  if (remaining <= REVERSE_TARGET_TICKS / 5) {  // in the last 20 % of the distance
    analogWrite(PWMA, REVERSE_SLOW_PWM);        // slow down for a gentler stop
  }
}

// Arduino setup(): configures the pins, the encoder interrupt and the servo, starts the gyro and the
// side sensors, and prints the start-up status.
void setup() {                    // runs once at power on or reset
  pinMode(PWMA, OUTPUT);          // motor speed pin is an output
  pinMode(AIN1, OUTPUT);          // direction pin 1 is an output
  pinMode(AIN2, OUTPUT);          // direction pin 2 is an output
  analogWrite(PWMA, 0);           // motor off
  digitalWrite(AIN1, LOW);        // set a direction (forward) with the motor off
  digitalWrite(AIN2, HIGH);

  steering.attach(SERVO_PIN);     // connect the servo to its pin
  steering.write(CENTER);         // wheels straight at start
  Serial.begin(115200);           // serial monitor speed

  pinMode(ENCODER_PIN, INPUT_PULLUP);   // encoder pin with internal pull-up resistor
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), encoderRise, RISING);  // count every rising edge

  if (CALIBRATE_ENCODER) {        // calibration mode
    Serial.println(F("Encoder calibration. Motor stays off."));
    Serial.println(F("Send z, turn wheel five revolutions, then send p."));
    return;                       // skip the maneuver
  }

  if (REVERSE_TARGET_TICKS == 0) {  // the distance was never measured
    stopCar(F("Enter the measured five-turn count in REVERSE_TARGET_TICKS."));
    return;                       // do not run the maneuver
  }

  holdInReset(REAR_XSHUT);        // keep all four ToF sensors off while starting
  holdInReset(FRONT_XSHUT);
  holdInReset(RIGHT_XSHUT);
  holdInReset(LEFT_XSHUT);
  delay(10);                      // short wait

  Wire.begin();                   // start the I2C bus
  Wire.setWireTimeout(5000, true);  // a bus that hangs for 5 ms is reset automatically

  if (!initGyro()) {              // start the gyro and calibrate it
    stopCar(F("BMI160 init or calibration failed."));  // stop if it fails
    return;
  }
  if (!initTof(leftTof, LEFT_XSHUT, 0x2A)) {       // start the left sensor at address 0x2A
    stopCar(F("Left ToF init failed."));
    return;
  }
  if (!initTof(rightTof, RIGHT_XSHUT, 0x2B)) {     // start the right sensor at address 0x2B
    stopCar(F("Right ToF init failed."));
    return;
  }

  stage = WAIT_FOR_SIDES;         // everything is ready: next step is measuring the sides
  Serial.println(F("Waiting for both side distances."));
}

// Arduino loop(), repeated forever. The parking maneuver is a stage machine (enum Stage). setup() leaves it in
// WAIT_FOR_SIDES, so it starts on power-up with no START button (see the README). The stages run in this order:
// WAIT_FOR_SIDES -> FIRST_TURN -> DRIVE_STRAIGHT -> RETURN_TURN -> PAUSE_BEFORE_REVERSE -> REVERSING -> FINISHED.
// REVERSING and PAUSE_BEFORE_REVERSE are handled before the gyro read; the rest are handled in the switch below.
void loop() {                     // runs over and over after setup()
  if (CALIBRATE_ENCODER) {        // calibration mode
    runCalibration();             // handle the keys z and p
    return;                       // nothing else
  }

  if (stage == FINISHED) return;  // stopped or finished: do nothing

  if (stage == WAIT_FOR_SIDES) {  // measuring the sides
    chooseDirection();            // measure and start the first turn when both sides are known
    return;
  }

  if (stage == PAUSE_BEFORE_REVERSE) {  // stopped, waiting
    if (millis() - stageStartMs >= PAUSE_BEFORE_REVERSE_MS) {  // the pause is over
      startReverse();             // start reversing
    }
    return;
  }

  if (stage == REVERSING) {       // reverse on the encoder count
    updateReverse();
    return;
  }

  if (!updateYaw()) {             // update the heading from the gyro
    stopCar(F("Gyro read failed."));  // the gyro did not answer: stop
    return;
  }

  unsigned long now = millis();   // current time for the timeouts

  switch (stage) {                // run the step that is active
    case FIRST_TURN:              // turning toward the open side
      if (fabs(yawDeg) >= FIRST_ANGLE_DEG) {      // turned far enough
        outboundSign = yawDeg >= 0.0f ? 1.0f : -1.0f;  // remember which way it turned
        steering.write(CENTER);   // straighten the wheels
        straightStartTicks = readTicks();  // remember the tick count to measure a short straight
        stageStartMs = now;       // start the timer of the next step
        stage = DRIVE_STRAIGHT;   // next step
        Serial.println(F("45 deg reached. Wheels centered."));
      } else if (now - stageStartMs >= TURN_TIMEOUT_MS) {  // the turn takes too long
        stopCar(F("First turn timed out."));
      }
      break;

    case DRIVE_STRAIGHT:          // rolling straight for a tiny distance
      if (readTicks() - straightStartTicks >= STRAIGHT_TICKS) {  // enough ticks counted
        steering.write(returnSteering);  // turn the wheels back the other way (countersteer)
        stageStartMs = now;       // start the timer of the next step
        stage = RETURN_TURN;      // next step
        Serial.println(F("Straight tick reached. Countersteering."));
      } else if (now - stageStartMs >= ENCODER_STALL_MS) {  // no tick for too long
        stopCar(F("No forward encoder tick. Check D3."));  // the encoder is probably not connected to D3
      }
      break;

    case RETURN_TURN:             // countersteering until the car is almost parallel to the wall
      if (outboundSign * yawDeg <= RETURN_ANGLE_DEG) {  // turned back to about 10 degrees
        analogWrite(PWMA, 0);     // stop the motor
        steering.write(CENTER);   // wheels straight
        stageStartMs = now;       // start the pause timer
        stage = PAUSE_BEFORE_REVERSE;  // next step: wait before reversing
        Serial.println(F("10 deg reached. Stopped before reversing."));
      } else if (now - stageStartMs >= TURN_TIMEOUT_MS) {  // the countersteer takes too long
        stopCar(F("Return turn timed out."));
      }
      break;

    default:                      // any other stage is handled in loop() above
      break;
  }
}
