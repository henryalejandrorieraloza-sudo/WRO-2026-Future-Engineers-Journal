#include <Wire.h>                     // I2C bus library (used by the BMI160 and the ToF sensors)
#include <VL53L1X.h>                  // library for the VL53L1X time-of-flight sensors (TOF400C)

// TOF400C XSHUT control pins
#define PIN_SHUT_LEFT  A3             // A3: XSHUT of the LEFT sensor (LOW = off, HIGH = on)
#define PIN_SHUT_RIGHT A2             // A2: XSHUT of the RIGHT sensor
#define PIN_SHUT_REAR  A0             // A0: XSHUT of the REAR sensor
#define PIN_SHUT_FRONT A1             // A1: XSHUT of the FRONT sensor

// Start button pin
#define PIN_BUTTON     10             // D10: start button (internal pull-up, other leg to GND)

// Assigned unique I2C addresses
#define ADDR_TOF_LEFT  0x30           // new address given to the LEFT sensor
#define ADDR_TOF_RIGHT 0x31           // new address given to the RIGHT sensor
#define ADDR_TOF_REAR  0x32           // new address given to the REAR sensor
#define ADDR_TOF_FRONT 0x33           // new address given to the FRONT sensor
#define ADDR_BMI160    0x69           // I2C address of the BMI160 gyroscope on this board (SDO high)

// BMI160 internal registers
#define BMI160_CHIP_ID_REG    0x00    // chip ID register, must read 0xD1
#define BMI160_GYRO_Z_L_REG   0x10    // gyro Z, low byte (the high byte is at 0x11)
#define BMI160_GYRO_RANGE_REG 0x43    // gyro range setting
#define BMI160_CMD_REG        0x7E    // command register (soft reset, power modes)

VL53L1X sensorLeft;                   // object for the LEFT ToF sensor
VL53L1X sensorRight;                  // object for the RIGHT ToF sensor
VL53L1X sensorRear;                   // object for the REAR ToF sensor
VL53L1X sensorFront;                  // object for the FRONT ToF sensor

// Variables for Yaw calculation
float yaw = 0.0;                      // heading in degrees, integrated from the gyro
float gz_offset = 0.0;                // zero offset of the gyro (deg/s), measured while the car is still
unsigned long previousTime = 0;       // time of the last loop, in ms (to get dt)

// --- Native I2C functions for BMI160 ---
void writeBMIRegister(uint8_t reg, uint8_t value) {   // writes one byte to a BMI160 register
  Wire.beginTransmission(ADDR_BMI160); // start talking to the gyro
  Wire.write(reg);                    // first byte: which register
  Wire.write(value);                  // second byte: the value to write
  Wire.endTransmission();             // send it and stop
}

uint8_t readBMIByte(uint8_t reg) {    // reads one byte from a BMI160 register
  Wire.beginTransmission(ADDR_BMI160); // start talking to the gyro
  Wire.write(reg);                    // say which register to read
  Wire.endTransmission(false);        // repeated start: keep the bus for the read
  Wire.requestFrom(ADDR_BMI160, (uint8_t)1);   // ask for 1 byte
  if (Wire.available()) return Wire.read();    // return the byte if it arrived
  return 0;                           // no answer: return 0
}

int16_t readBMIGyroZ() {              // reads the raw 16-bit gyro Z value
  Wire.beginTransmission(ADDR_BMI160); // start talking to the gyro
  Wire.write(BMI160_GYRO_Z_L_REG);    // start at the low byte of gyro Z
  Wire.endTransmission(false);        // repeated start: keep the bus for the read
  Wire.requestFrom(ADDR_BMI160, (uint8_t)2);   // ask for 2 bytes (low and high)

  if (Wire.available() >= 2) {        // both bytes arrived?
    uint8_t lowByte = Wire.read();    // first byte received = low byte
    uint8_t highByte = Wire.read();   // second byte received = high byte
    return (int16_t)((highByte << 8) | lowByte);   // join them into one signed 16-bit number
  }
  return 0;                           // no data: return 0
}

bool initBMI160() {                   // resets and starts the gyro, returns false if the chip is not found
  writeBMIRegister(BMI160_CMD_REG, 0xB6); // Soft-reset: restart the chip to a known state
  delay(100);                         // wait for the reset to finish

  if (readBMIByte(BMI160_CHIP_ID_REG) != 0xD1) {   // read the chip ID; BMI160 must answer 0xD1
    return false;                     // wrong or no answer: the gyro is not working
  }

  writeBMIRegister(BMI160_CMD_REG, 0x15); // Power up gyroscope in normal mode
  delay(100);                         // wait for the gyro to start

  writeBMIRegister(BMI160_GYRO_RANGE_REG, 0x03); // Set range to ±250 deg/s (sensitivity 131.2 LSB per deg/s)
  delay(10);                          // short wait

  return true;                        // gyro ready
}

void setup() {                        // runs once when the Nano starts
  Serial.begin(115200);               // serial port at 115200 baud
  while (!Serial && millis() < 3000); // wait up to 3 s for the serial port

  pinMode(PIN_BUTTON, INPUT_PULLUP);  // button with pull-up: HIGH = released, LOW = pressed

  Serial.println("\n=== TEST: 4x TOF400C (FRONT, LEFT, RIGHT, REAR) + NATIVE BMI160 ===");   // title

  // Set XSHUT pins as outputs and shut down all 4 ToF sensors
  pinMode(PIN_SHUT_LEFT, OUTPUT);     // XSHUT pins become outputs so we can control them
  pinMode(PIN_SHUT_RIGHT, OUTPUT);
  pinMode(PIN_SHUT_REAR, OUTPUT);
  pinMode(PIN_SHUT_FRONT, OUTPUT);

  digitalWrite(PIN_SHUT_LEFT, LOW);   // all four sensors off, so each one can be given its own address
  digitalWrite(PIN_SHUT_RIGHT, LOW);
  digitalWrite(PIN_SHUT_REAR, LOW);
  digitalWrite(PIN_SHUT_FRONT, LOW);
  delay(20);                          // let them power down

  // Initialize I2C bus (A4 = SDA, A5 = SCL)
  Wire.begin();                       // start the I2C bus
  Wire.setClock(100000);              // 100 kHz: slow and reliable for the long wires

  // 1. Configure LEFT ToF (A3 -> 0x30)
  digitalWrite(PIN_SHUT_LEFT, HIGH);  // turn the LEFT sensor on (it answers at the default address 0x29)
  delay(20);                          // wait for it to boot
  sensorLeft.setTimeout(500);         // a read that takes more than 500 ms counts as a timeout
  if (!sensorLeft.init()) {           // start the sensor; false = no sensor found
    Serial.println("❌ ERROR: Left ToF (A3) not detected.");
  } else {
    sensorLeft.setAddress(ADDR_TOF_LEFT);            // move it to 0x30 so the others can use 0x29
    sensorLeft.setDistanceMode(VL53L1X::Short);      // short range mode: less noise, up to about 1.3 m
    sensorLeft.setMeasurementTimingBudget(50000);    // 50 ms per measurement: faster readings
    sensorLeft.startContinuous(50);                  // measure continuously, one new value every 50 ms
    Serial.println("✅ Left ToF ready (0x30)");
  }

  // 2. Configure RIGHT ToF (A2 -> 0x31)
  digitalWrite(PIN_SHUT_RIGHT, HIGH); // turn the RIGHT sensor on; it still answers at 0x29
  delay(20);                          // wait for it to boot
  sensorRight.setTimeout(500);        // same timeout as the left sensor
  if (!sensorRight.init()) {          // start it at the default address
    Serial.println("❌ ERROR: Right ToF (A2) not detected.");
  } else {
    sensorRight.setAddress(ADDR_TOF_RIGHT);          // move it to 0x31
    sensorRight.setDistanceMode(VL53L1X::Short);     // short range mode
    sensorRight.setMeasurementTimingBudget(50000);   // 50 ms per measurement
    sensorRight.startContinuous(50);                 // continuous measurements every 50 ms
    Serial.println("✅ Right ToF ready (0x31)");
  }

  // 3. Configure REAR ToF (A0 -> 0x32)
  digitalWrite(PIN_SHUT_REAR, HIGH);  // turn the REAR sensor on
  delay(20);                          // wait for it to boot
  sensorRear.setTimeout(500);         // same timeout
  if (!sensorRear.init()) {           // start it at the default address
    Serial.println("❌ ERROR: Rear ToF (A0) not detected.");
  } else {
    sensorRear.setAddress(ADDR_TOF_REAR);            // move it to 0x32
    sensorRear.setDistanceMode(VL53L1X::Short);      // short range mode
    sensorRear.setMeasurementTimingBudget(50000);    // 50 ms per measurement
    sensorRear.startContinuous(50);                  // continuous measurements every 50 ms
    Serial.println("✅ Rear ToF ready (0x32)");
  }

  // 4. Configure FRONT ToF (A1 -> 0x33)
  digitalWrite(PIN_SHUT_FRONT, HIGH); // turn the FRONT sensor on
  delay(20);                          // wait for it to boot
  sensorFront.setTimeout(500);        // same timeout
  if (!sensorFront.init()) {          // start it at the default address
    Serial.println("❌ ERROR: Front ToF (A1) not detected.");
  } else {
    sensorFront.setAddress(ADDR_TOF_FRONT);          // move it to 0x33
    sensorFront.setDistanceMode(VL53L1X::Short);     // short range mode
    sensorFront.setMeasurementTimingBudget(50000);   // 50 ms per measurement
    sensorFront.startContinuous(50);                 // continuous measurements every 50 ms
    Serial.println("✅ Front ToF ready (0x33)");
  }

  // 5. Configure BMI160 directly via registers
  if (!initBMI160()) {                // reset and check the gyro
    Serial.println("❌ ERROR: BMI160 not responding or wrong ID (expected 0xD1).");
    while (1);                        // stop here forever: the yaw cannot work without the gyro
  }
  Serial.println("✅ BMI160 ready (ID 0xD1 verified)");   // gyro found

  // Static offset calibration
  Serial.println("Calibrating gyroscope... KEEP SENSOR STILL.");   // tell the user to keep still
  long gzSum = 0;                     // total of the raw gyro readings
  const int SAMPLES = 200;            // number of readings to average

  for (int i = 0; i < SAMPLES; i++) { // take all the readings
    gzSum += readBMIGyroZ();          // add one raw reading
    delay(10);                        // one reading every 10 ms (about 2 s in total)
  }

  gz_offset = (gzSum / (float)SAMPLES) / 131.2;   // average raw value divided by 131.2 = offset in deg/s

  Serial.print("✅ Calibration complete. Offset: ");   // result label
  Serial.print(gz_offset, 3);         // print the offset with 3 decimals
  Serial.println(" deg/s");
  Serial.println("\n>>> Press button on D10 to start reading sensors...");   // next instruction

  // Wait until the button on D10 is pressed (LOW)
  while (digitalRead(PIN_BUTTON) == HIGH) {   // while the button is not pressed
    delay(10);                        // check every 10 ms
  }
  delay(250);                         // simple debounce

  // Reset Yaw and timer right when the button is pressed
  yaw = 0.0;                          // the heading starts at zero when the test starts
  previousTime = millis();            // remember the time, to measure dt in the first loop
  Serial.println(">>> Starting sensor readings!\n");   // confirm the start
}

void loop() {                         // runs again and again after setup()
  // Read distances from all 4 ToF sensors
  uint16_t distFront = sensorFront.read();   // distance in mm from the front sensor
  uint16_t distLeft  = sensorLeft.read();    // distance in mm from the left sensor
  uint16_t distRight = sensorRight.read();   // distance in mm from the right sensor
  uint16_t distRear  = sensorRear.read();    // distance in mm from the rear sensor

  // Read BMI160 Z-axis directly
  int16_t gz_raw = readBMIGyroZ();    // raw gyro Z value
  float gz_dps = (gz_raw / 131.2) - gz_offset;   // convert to deg/s and remove the zero offset

  // Calculate delta time (dt)
  unsigned long currentTime = millis();   // current time in ms
  float dt = (currentTime - previousTime) / 1000.0;   // time since the last loop, in seconds
  previousTime = currentTime;         // remember this time for the next loop

  // Deadband filter against static drift
  if (abs(gz_dps) < 0.25) {           // very small rotation speed = noise or drift
    gz_dps = 0.0;                     // ignore it, so a still car does not slowly turn
  }

  // Integrate angular velocity to obtain Yaw angle
  yaw += gz_dps * dt;                 // add the rotation of this loop (speed x time) to the heading

  // Normalize angle between -180° and +180°
  if (yaw > 180.0) yaw -= 360.0;      // above +180: wrap around to the negative side
  if (yaw < -180.0) yaw += 360.0;     // below -180: wrap around to the positive side

  // Print results
  Serial.print("FRONT: ");            // label
  if (sensorFront.timeoutOccurred()) Serial.print("ERR");   // print ERR if the sensor timed out
  else { Serial.print(distFront); Serial.print("mm"); }    // otherwise print the distance in mm

  Serial.print(" | LEFT: ");          // label
  if (sensorLeft.timeoutOccurred()) Serial.print("ERR");   // ERR if the left sensor timed out
  else { Serial.print(distLeft); Serial.print("mm"); }    // otherwise the left distance

  Serial.print(" | RIGHT: ");         // label
  if (sensorRight.timeoutOccurred()) Serial.print("ERR");  // ERR if the right sensor timed out
  else { Serial.print(distRight); Serial.print("mm"); }   // otherwise the right distance

  Serial.print(" | REAR: ");          // label
  if (sensorRear.timeoutOccurred()) Serial.print("ERR");   // ERR if the rear sensor timed out
  else { Serial.print(distRear); Serial.print("mm"); }    // otherwise the rear distance

  Serial.print(" | YAW: ");           // label
  Serial.print(yaw, 1);               // heading with one decimal
  Serial.println("°");                // degree sign and end of line

  delay(40);                          // wait 40 ms before the next reading
}
