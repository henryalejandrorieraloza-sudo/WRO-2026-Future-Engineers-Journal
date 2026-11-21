// =====================================================================
// STRAIGHT WITH PILLARS - Arduino Nano (classic)
// Camera (ESP32S3-CAM) + gyro (BMI160) + encoder + rear motor + steering servo.
//
// Idea: the car keeps a small MAP of the straight (odometry: encoder + gyro).
//   - x = cm forward from the start, y = cm to the LEFT of the start line (the "home lane").
//   - Every camera message puts the pillar on that map (ahead + side -> x, y).
//     So when the camera loses the pillar (it leaves the picture 15 cm ahead), the car
//     still knows where it is: there is no separate "blind" state, the map IS the blind zone.
//   - Red  -> the pillar must stay on the car's LEFT  at >= TARGET_CM (car passes on its right)
//     Green-> the pillar must stay on the car's RIGHT at >= TARGET_CM (car passes on its left)
//   - Steering: wanted lateral position -> heading = atan(lateral error / distance left),
//     limited to +-MAX_AIM_DEG -> the gyro PD turns that heading into wheel angle.
//   - As soon as the camera passes the pillar, the car already aims at the NEXT pillar
//     (or straightens), but it may not move sideways TOWARD the pillar it has beside it.
//   - RETURN_AFTER_CM past the pillar center the pillar is "cleared": free to go back to the
//     home lane (y = 0). The rear does not swing out (front steering), checked in the simulation.
//   - Stops (coast) after RUN_CM, or if the camera stops talking.
// Values tested in a simulation: speed 20-40 cm/s, camera 6-10 Hz with 0.2 s delay,
// distance error +-10 %, noise 3 cm, pillars at y = +-10 cm, two pillars 100 cm apart.
// Validated on the track (27 Oct 2026): every straight-with-pillars case passes.
//
// Wiring (team PCB): camera GPIO43 (TX) -> Nano D0 (RX) | encoder C1 = D3
//         start button D10 | I2C: SDA=A4, SCL=A5 | Servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8
// UPLOADING: hold the CAMERA RESET button during the whole upload (the camera on D0 blocks it).
// Camera sketch: obstacle_camera.ino with FOCAL_PX = 100 (reads the first 10 fields of its message).
// =====================================================================

#include <Wire.h>                     // I2C bus library (used by the gyro)
#include <Servo.h>                    // library for the steering servo

// Defined at the top so the Arduino IDE knows it before any function
struct MapPillar {                    // one pillar stored on the map
  bool  used;                         // true if this slot holds a pillar
  float x, y;        // cm on the map (pillar center)
  int   color;       // 1 red, 2 green
  int   hits;        // how many camera messages confirmed this pillar
};

// ================= SETTINGS (change these) =================
// --- Test ---
const float RUN_CM = 150.0;          // distance to drive, then stop (1 m test without pillars: 100)

// --- Steering (same as the validated codes) ---
const int  CENTER_ANGLE         = 120;   // servo angle for wheels straight ahead
const int  LEFT_LIMIT           = 180;   // servo angle at full left lock (mechanical limit)
const int  RIGHT_LIMIT          = 50;    // servo angle at full right lock (mechanical limit)
const bool STEER_LEFT_IS_BIGGER = false;  // your validated codes: false (left = smaller servo angle)
const int  SERVO_SPEED_DPS      = 300;   // maximum servo speed in degrees per second (smooths the steering)

// --- Motor ---
const int  DRIVE_SPEED    = 70;      // motor speed, 0-255
const bool MOTOR_REVERSED = true;    // true if the motor wires are swapped

// --- Heading PD (validated values) ---
const float KP_HEADING   = 2.0;      // steering degrees per degree of heading error
const float KD_HEADING   = 0.10;     // steering degrees per (deg/s) of turning speed (damps the swing)
const float MAX_STEER    = 60.0;     // max steering away from center (servo degrees)

// --- Pillars (from the measurements and the simulation) ---
const float TARGET_CM       = 17.0;  // pillar center <-> car center: 4.25 + 10 cm gap + 2.5
const float MAX_AIM_DEG     = 45.0;  // max heading while dodging (35 hit the 2nd pillar in the sim)
const float LOOK_MIN_CM     = 12.0;  // shortest distance used in atan (avoids crazy angles near the pillar)
const float ARRIVE_BEFORE_CM = 10.0; // be at the safe side this many cm BEFORE the pillar's front face
const float LOOK_HOME_CM    = 20.0;  // how fast it goes back to the home lane after a pillar
const float RETURN_AFTER_CM = 5.0;   // camera this far past the pillar center -> free to go back (was ~16.5)
const float SAME_PILLAR_CM  = 15.0;  // a detection this close to a stored pillar is the same pillar
const float BLEND           = 0.3;   // how much each new message moves the stored pillar (0-1)
const float MAX_SIDE_CM     = 60.0;  // detections farther to the side are ignored

// --- Pillar size ---
const float PILLAR_CM       = 5.0;   // pillar width in cm
const float TICKS_PER_CM    = 15.1;  // encoder (measured: 980 ticks = 65 cm)

// --- Camera link ---
const long  CAMERA_BAUD       = 115200;  // speed of the link from the camera
const unsigned long CAMERA_TIMEOUT_MS = 500;   // no camera message for this long = do not drive blind
const bool  DEBUG_PRINTS      = true;    // true = print the state every 150 ms

// --- Gyro ---
const bool  YAW_REVERSED = false;    // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;     // correction factor for the gyro rate
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN   = 9;           // D9: steering servo
const int PWMA_PIN    = 6;           // D6: motor speed
const int AIN1_PIN    = 7;           // D7: motor direction 1
const int AIN2_PIN    = 8;           // D8: motor direction 2
const int BUTTON_PIN  = 10;          // D10: start button
const int ENCODER_PIN = 3;           // D3: encoder channel (interrupt)

Servo steering;                      // the steering servo object

// ---------------- Car state ----------------
float yaw = 0, yawRate = 0;          // degrees, + = car turned LEFT; yawRate in deg/s
float carX = 0, carY = 0;            // cm, camera point on the map (y + = left)
float servoAngle = CENTER_ANGLE;     // servo angle now (moves slowly toward the target)
unsigned long prevMicros = 0;        // time of the last gyro update, in microseconds
long  prevTicks = 0;                 // encoder count at the last odometry update
float traveledCm = 0;                // total distance driven, from the encoder
bool  finished = false;              // true when RUN_CM is reached

volatile long encoderTicks = 0;      // encoder count, changed by the interrupt
void onEncoder() { encoderTicks++; } // interrupt routine: one more tick

long readTicks() {                   // safe read of the encoder count
  noInterrupts();                    // stop interrupts while copying the value
  long t = encoderTicks;             // copy it
  interrupts();                      // allow interrupts again
  return t;                          // return the copy
}

// ---------------- Pillars on the map ----------------
MapPillar current = {false, 0, 0, 0, 0};   // the pillar being dodged
MapPillar nextOne = {false, 0, 0, 0, 0};   // a farther pillar already seen

// ---------------- Last camera message ----------------
// $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm[, line fields of camera v3]
int camColor = 0, camDistCm = -1, camLatCm = 0;   // pillar color (0 none, 1 red, 2 green), distance and side from the camera
unsigned long lastCamMs = 0;         // time of the last valid camera message
bool newCamData = false;             // true when a new message is waiting to be put on the map

float goalY = 0, goalHeading = 0;    // for the debug prints

// ---------------- Motor ----------------
void motorForward(int speed) {       // drive forward at this speed
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);   // direction pin 1 (swapped if the motor is reversed)
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);   // direction pin 2
  analogWrite(PWMA_PIN, speed);      // set the speed
}

void motorStop() {                   // free stop (coast): gentle for the gearbox
  digitalWrite(AIN1_PIN, LOW);       // both direction pins low = the motor is free to turn
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);          // no speed
}

// ---------------- BMI160 (registers, same as the validated turn) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;              // I2C address in use (0x68 or 0x69, found at startup)
float gyroBias = 0;                  // zero offset of the gyro, measured while still

void bmiWrite(uint8_t reg, uint8_t val) {   // write one byte into a gyro register
  Wire.beginTransmission(bmiAddr);   // address the gyro
  Wire.write(reg);                   // which register
  Wire.write(val);                   // which value
  Wire.endTransmission();            // send it
}

bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t len) {   // read len bytes starting at reg
  Wire.beginTransmission(bmiAddr);   // address the gyro
  Wire.write(reg);                   // which register to start at
  if (Wire.endTransmission(false) != 0) return false;     // no answer: fail
  if (Wire.requestFrom(bmiAddr, len) != len) return false;   // not all bytes came: fail
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();   // store the bytes
  return true;                       // success
}

bool bmiFound(uint8_t addr) {        // checks whether a BMI160 answers at this address
  bmiAddr = addr;                    // try this address
  uint8_t id = 0;                    // chip ID buffer
  return bmiRead(0x00, &id, 1) && id == 0xD1;   // chip ID must be 0xD1
}

bool startGyro() {                   // finds the gyro and starts it
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;   // try both addresses; fail if neither works
  bmiWrite(0x7E, 0x11); delay(50);   // accelerometer: normal mode
  bmiWrite(0x7E, 0x15); delay(100);  // gyroscope: normal mode
  bmiWrite(0x42, 0x2A);              // gyro 400 Hz
  bmiWrite(0x43, 0x02);              // gyro range +-500 deg/s
  delay(50);                         // let it settle
  return true;                       // gyro ready
}

float readGyroZ() {                  // reads the rotation speed around Z, in deg/s
  uint8_t b[2];                      // two bytes: low and high
  if (!bmiRead(0x10, b, 2)) return NAN;   // no answer: NaN means "no value"
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);   // join the bytes into one signed number
  float dps = raw / GYRO_SCALE;      // convert to deg/s
  return YAW_REVERSED ? -dps : dps;  // flip the sign if the gyro is mounted the other way
}

void calibrateGyro() {               // measures the zero offset while the car is still
  float sum = 0;                     // total of the readings
  int count = 0;                     // how many good readings
  for (int i = 0; i < 1000; i++) {   // 1000 readings, about 3 s
    float z = readGyroZ();           // one reading
    if (!isnan(z)) { sum += z; count++; }   // keep the good ones
    delay(3);                        // 3 ms between readings
  }
  gyroBias = (count > 0) ? sum / count : 0;   // average = the offset (0 if nothing was read)
}

float updateGyro() {                 // updates the heading and returns the time step
  unsigned long nowUs = micros();    // current time in microseconds
  float dt = (nowUs - prevMicros) / 1000000.0;   // time since the last update, in seconds
  prevMicros = nowUs;                // remember this time
  float z = readGyroZ();             // read the rotation speed
  if (!isnan(z)) {                   // a valid reading
    yawRate = (z - gyroBias) * YAW_SCALE;   // remove the offset, apply the scale
    yaw += yawRate * dt;             // add the rotation of this step to the heading
  }
  return dt;                         // give back the time step for the other functions
}

// ---------------- Odometry: encoder distance + gyro heading -> position on the map ----------------
void updateOdometry(float yawBefore) {   // moves the car point on the map
  long t = readTicks();              // current encoder count
  long dTicks = t - prevTicks;       // ticks since the last update
  prevTicks = t;                     // remember the count
  if (dTicks <= 0) return;           // no movement (or backwards): nothing to add
  float ds = dTicks / TICKS_PER_CM;  // distance travelled in cm
  float h = radians((yawBefore + yaw) * 0.5);   // average heading during this step
  carX += ds * cos(h);               // move forward along the average heading (x)
  carY += ds * sin(h);               // and sideways (y, + = left)
  traveledCm += ds;                  // total distance so far
}

// Pillar position seen from the car: forward (+ ahead) and left (+ left) in cm
void relativeToCar(float px, float py, float &fwd, float &left) {   // turns map coordinates into car coordinates
  float h = radians(yaw);            // the car's heading in radians
  float dx = px - carX, dy = py - carY;   // vector from the car to the pillar, on the map
  fwd  =  dx * cos(h) + dy * sin(h);      // how far ahead of the car (rotated by the heading)
  left = -dx * sin(h) + dy * cos(h);      // how far to the left of the car
}

// ---------------- Steering ----------------
float maxLeftSteer()  { return STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT); }   // how far the wheels can turn left, in servo degrees
float maxRightSteer() { return STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE); }   // how far they can turn right

// steerLeft: + = left, - = right (servo degrees away from center)
void setSteering(float steerLeft, float dt) {   // moves the wheels toward the wanted angle, smoothly
  steerLeft = constrain(steerLeft, -maxRightSteer(), maxLeftSteer());   // keep the wanted angle inside the limits
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);   // convert to a servo angle
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);   // never past the mechanical limits
  float maxStep = SERVO_SPEED_DPS * dt;   // the most the servo may move in this time
  if (target > servoAngle + maxStep)      servoAngle += maxStep;   // move up by the limit
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;   // move down by the limit
  else                                    servoAngle = target;     // close enough: go straight there
  steering.write((int)(servoAngle + 0.5));   // send the angle, rounded to a whole degree
}

// ---------------- Camera messages ----------------
char lineBuf[80];                    // buffer for one message; camera v3 sends 14 fields, only the first 10 are used here
uint8_t lineLen = 0;                 // how many characters are in the buffer

void readCamera() {                  // reads the bytes from the camera and builds messages
  while (Serial.available()) {       // while bytes are waiting
    char c = Serial.read();          // read one byte
    if (c == '$') { lineLen = 0; continue; }   // '$' starts a new message
    if (c == '\n') {                 // end of the message
      lineBuf[lineLen] = '\0';       // close the text
      int col, x, b, ok, bf, br, w, d, lat;   // one variable per field used
      unsigned long seq;             // frame number (not used here)
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                     &w, &d, &lat);  // split the text into numbers
      if (n == 10) {                 // a complete message
        camColor = col; camDistCm = d; camLatCm = lat;   // keep the pillar data
        lastCamMs = millis();        // remember when it arrived
        newCamData = true;           // tell the loop there is new data
      }
      lineLen = 0;                   // get ready for the next message
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;   // store the character if there is room
  }
}

// Put the pillar of the last message on the map
void onCameraMessage() {             // converts the camera data into a pillar on the map
  if (camColor == 0 || camDistCm <= 0) return;   // no pillar in this message
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;   // too far to the side: not a pillar of this straight

  float fwd  = camDistCm + PILLAR_CM / 2;        // camera measures to the front face -> add half the width for the center
  float left = -camLatCm;                        // camera: + = right; map: + = left, so flip the sign
  float h = radians(yaw);            // heading of the car now
  float px = carX + fwd * cos(h) - left * sin(h);   // pillar x on the map
  float py = carY + fwd * sin(h) + left * cos(h);   // pillar y on the map

  // Same pillar as one we already have? -> refine its position
  MapPillar *slots[2] = { &current, &nextOne };   // the two stored pillars
  for (int i = 0; i < 2; i++) {      // check both
    MapPillar *p = slots[i];
    if (p->used && p->color == camColor && hypot(p->x - px, p->y - py) < SAME_PILLAR_CM) {   // same color and close: same pillar
      p->x += BLEND * (px - p->x);   // move the stored position a little toward the new one
      p->y += BLEND * (py - p->y);
      p->hits++;                     // one more confirmation
      return;                        // done, it was not a new pillar
    }
  }
  // A new pillar
  MapPillar fresh = {true, px, py, camColor, 1};   // a new pillar, seen once
  if (!current.used) {               // no pillar is being dodged yet
    current = fresh;                 // this one becomes the current pillar
    if (DEBUG_PRINTS) { Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN")); }
  } else if (px > current.x + SAME_PILLAR_CM) {   // it is farther ahead than the current one
    nextOne = fresh;                 // keep it for later
  }
}

// ---------------- The control: where to go -> which heading ----------------
// Wanted lateral position next to one pillar (never pulls the car back toward the pillar)
// (plain numbers, no MapPillar in the parameters: the Arduino IDE breaks on that)
float goalFor(int color, float pillarY) {   // the lateral position (y) the car should have for this pillar
  if (color == 1) return min(carY, pillarY - TARGET_CM);   // red: car to its right (y smaller than the pillar)
  return max(carY, pillarY + TARGET_CM);                    // green: car to its left (y bigger than the pillar)
}

float headingGoal() {                // returns the heading (degrees) the car should aim at now
  float fwd = 0, left = 0;           // pillar position relative to the car

  // 1) Current pillar far enough behind the camera? -> cleared, the next one becomes current
  if (current.used) {                // a pillar is being dodged
    relativeToCar(current.x, current.y, fwd, left);   // where is it relative to the car?
    if (fwd < -RETURN_AFTER_CM) {    // it is behind the car by more than the limit
      if (DEBUG_PRINTS) Serial.println(F(">> PILLAR CLEARED"));
      current = nextOne;             // the next pillar (if any) becomes the current one
      nextOne.used = false;          // the next slot is empty again
      if (current.used) relativeToCar(current.x, current.y, fwd, left);   // update the position for the new current
    }
  }

  // 2) Which pillar do we aim at?
  //    camera still before the current pillar -> that one
  //    camera already past its center (beside it) -> the next one (or the home lane)
  bool beside = current.used && fwd < 0;   // true when the car is already level with the pillar
  const MapPillar *target = current.used ? (beside ? (nextOne.used ? &nextOne : NULL) : &current) : NULL;   // which pillar to aim at, or none

  goalY = 0;                         // default: the home lane (y = 0)
  float look = LOOK_HOME_CM;         // distance used to aim: default for going home
  if (target != NULL) {              // there is a pillar to aim at
    goalY = goalFor(target->color, target->y);   // the lateral position we want next to it
    float toFace = (target->x - PILLAR_CM / 2) - carX - ARRIVE_BEFORE_CM;   // distance until we must be at the safe side
    look = max(LOOK_MIN_CM, toFace); // aim this far ahead, but not closer than LOOK_MIN_CM
  }

  // 3) Beside a pillar: never move sideways TOWARD it (straightening or moving away is fine)
  if (beside) {
    if (current.color == 1) goalY = min(goalY, carY);      // red is on our left: do not move left
    else                    goalY = max(goalY, carY);      // green is on our right: do not move right
  }

  // 4) Heading that leads there
  float h = degrees(atan2(goalY - carY, look));   // angle from the car to the goal point, in degrees
  return constrain(h, -MAX_AIM_DEG, MAX_AIM_DEG); // limit the angle to MAX_AIM_DEG
}

// ---------------- SETUP ----------------
void setup() {                       // runs once when the Nano starts
  Serial.begin(CAMERA_BAUD);         // RX = camera, TX = Serial Monitor

  pinMode(PWMA_PIN, OUTPUT);         // motor pins are outputs
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  motorStop();                       // motor off

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // start button with pull-up
  pinMode(ENCODER_PIN, INPUT_PULLUP); // encoder pin with pull-up
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoder, RISING);   // count every rising edge

  Wire.begin();                      // start the I2C bus
  Wire.setClock(400000);             // 400 kHz: fast bus for the gyro
  Wire.setWireTimeout(25000, true);  // reset the bus if it hangs for 25 ms
  if (!startGyro()) {                // find and start the gyro
    Serial.println(F("ERROR: BMI160 not found"));
    while (true) delay(1000);        // stop here: the heading cannot work without the gyro
  }

  steering.attach(SERVO_PIN, 500, 2500);   // connect the servo, with pulse limits of 0.5 to 2.5 ms
  steering.write(CENTER_ANGLE);      // wheels straight

  Serial.println(F("STRAIGHT WITH PILLARS - press START"));
  while (digitalRead(BUTTON_PIN) == HIGH) {      // waiting for START; also shows if the camera talks
    readCamera();                    // keep reading the camera while waiting
    static unsigned long t = 0;      // time of the last camera check
    if (millis() - t > 1000) {       // once a second
      t = millis();
      Serial.print(F("camera: "));
      Serial.println(millis() - lastCamMs < 500 ? F("OK") : F("no messages (10-field camera code?)"));   // is the camera talking?
    }
  }
  Serial.println(F("Calibrating gyro... do not move the car"));
  calibrateGyro();                   // measure the gyro offset while the car is still
  yaw = 0;                           // the heading starts at zero
  carX = carY = 0;                   // the car starts at the origin of the map
  traveledCm = 0;                    // no distance yet
  prevTicks = readTicks();           // start counting ticks from now
  prevMicros = micros();             // start the gyro clock now
  lastCamMs = millis();              // do not time out before the first message
  newCamData = false;                // no data waiting
}

// ---------------- LOOP ----------------
void loop() {                        // runs again and again after setup()
  float yawBefore = yaw;             // heading before this step
  float dt = updateGyro();           // update the heading
  updateOdometry(yawBefore);         // update the car position on the map
  readCamera();                      // read the camera messages

  if (finished) { motorStop(); setSteering(0, dt); return; }   // finished: stop and keep the wheels straight

  if (traveledCm >= RUN_CM) {        // driven the whole distance
    finished = true;                 // remember that we are done
    motorStop();                     // stop the motor
    Serial.print(F(">> DONE | x ")); Serial.print(carX, 1);   // final position
    Serial.print(F(" | y ")); Serial.print(carY, 1);
    Serial.print(F(" | yaw ")); Serial.println(yaw, 1);
    return;
  }

  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {   // no camera -> do not drive blind
    motorStop();                     // stop the motor
    setSteering(0, dt);              // wheels straight
    static unsigned long t = 0;      // time of the last message
    if (millis() - t > 500) { t = millis(); Serial.println(F("WAITING FOR CAMERA")); }   // print the warning every 0.5 s
    return;                          // do not drive
  }

  if (newCamData) { newCamData = false; onCameraMessage(); }   // put the new pillar on the map

  goalHeading = headingGoal();       // which heading we want now
  float steer = KP_HEADING * (goalHeading - yaw) - KD_HEADING * yawRate;   // PD control: error (P) and turning speed (D)
  setSteering(constrain(steer, -MAX_STEER, MAX_STEER), dt);   // steer, limited to MAX_STEER
  motorForward(DRIVE_SPEED);         // drive forward

  static unsigned long lastPrint = 0;   // time of the last debug line
  if (DEBUG_PRINTS && millis() - lastPrint >= 150) {   // every 150 ms
    lastPrint = millis();
    Serial.print(F("x ")); Serial.print(carX, 0);      // position along the straight
    Serial.print(F(" y ")); Serial.print(carY, 1);     // lateral position
    Serial.print(F(" | yaw ")); Serial.print(yaw, 1);  // heading
    Serial.print(F(" goal ")); Serial.print(goalHeading, 1);   // wanted heading
    Serial.print(F(" | goalY ")); Serial.print(goalY, 1);      // wanted lateral position
    Serial.print(F(" | pillar "));     // the pillar being dodged
    if (current.used) {
      Serial.print(current.color == 1 ? F("RED (") : F("GREEN ("));
      Serial.print(current.x, 0); Serial.print(F(",")); Serial.print(current.y, 0); Serial.print(F(")"));
    } else Serial.print(F("-"));       // no pillar
    Serial.print(F(" | cam ")); Serial.print(camColor); Serial.print(F(" ")); Serial.print(camDistCm);   // raw camera data
    Serial.print(F("/")); Serial.println(camLatCm);
  }
}
