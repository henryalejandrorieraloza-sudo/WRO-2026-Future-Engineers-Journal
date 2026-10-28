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

#include <Wire.h>
#include <Servo.h>

// Defined at the top so the Arduino IDE knows it before any function
struct MapPillar {
  bool  used;
  float x, y;        // cm on the map (pillar center)
  int   color;       // 1 red, 2 green
  int   hits;
};

// ================= SETTINGS (change these) =================
// --- Test ---
const float RUN_CM = 150.0;          // distance to drive, then stop (1 m test without pillars: 100)

// --- Steering (same as the validated codes) ---
const int  CENTER_ANGLE         = 120;
const int  LEFT_LIMIT           = 180;
const int  RIGHT_LIMIT          = 50;
const bool STEER_LEFT_IS_BIGGER = false;  // your validated codes: false
const int  SERVO_SPEED_DPS      = 300;

// --- Motor ---
const int  DRIVE_SPEED    = 70;      // 0-255
const bool MOTOR_REVERSED = true;

// --- Heading PD (validated values) ---
const float KP_HEADING   = 2.0;      // steering degrees per degree of heading error
const float KD_HEADING   = 0.10;     // steering degrees per (deg/s) of turning speed
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
const float PILLAR_CM       = 5.0;
const float TICKS_PER_CM    = 15.1;  // encoder (measured: 980 ticks = 65 cm)

// --- Camera link ---
const long  CAMERA_BAUD       = 115200;
const unsigned long CAMERA_TIMEOUT_MS = 500;
const bool  DEBUG_PRINTS      = true;

// --- Gyro ---
const bool  YAW_REVERSED = false;    // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.00;
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN   = 9;
const int PWMA_PIN    = 6;
const int AIN1_PIN    = 7;
const int AIN2_PIN    = 8;
const int BUTTON_PIN  = 10;
const int ENCODER_PIN = 3;

Servo steering;

// ---------------- Car state ----------------
float yaw = 0, yawRate = 0;          // degrees, + = car turned LEFT
float carX = 0, carY = 0;            // cm, camera point on the map (y + = left)
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;
long  prevTicks = 0;
float traveledCm = 0;
bool  finished = false;

volatile long encoderTicks = 0;
void onEncoder() { encoderTicks++; }

long readTicks() {
  noInterrupts();
  long t = encoderTicks;
  interrupts();
  return t;
}

// ---------------- Pillars on the map ----------------
MapPillar current = {false, 0, 0, 0, 0};   // the pillar being dodged
MapPillar nextOne = {false, 0, 0, 0, 0};   // a farther pillar already seen

// ---------------- Last camera message ----------------
// $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm[, line fields of camera v3]
int camColor = 0, camDistCm = -1, camLatCm = 0;
unsigned long lastCamMs = 0;
bool newCamData = false;

float goalY = 0, goalHeading = 0;    // for the debug prints

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {                   // free stop (coast): gentle for the gearbox
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

// ---------------- BMI160 (registers, same as the validated turn) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at +-500 deg/s
uint8_t bmiAddr = 0x68;
float gyroBias = 0;

void bmiWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(bmiAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(bmiAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(bmiAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool bmiFound(uint8_t addr) {
  bmiAddr = addr;
  uint8_t id = 0;
  return bmiRead(0x00, &id, 1) && id == 0xD1;
}

bool startGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;
  bmiWrite(0x7E, 0x11); delay(50);   // accelerometer: normal mode
  bmiWrite(0x7E, 0x15); delay(100);  // gyroscope: normal mode
  bmiWrite(0x42, 0x2A);              // gyro 400 Hz
  bmiWrite(0x43, 0x02);              // gyro range +-500 deg/s
  delay(50);
  return true;
}

float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_SCALE;
  return YAW_REVERSED ? -dps : dps;
}

void calibrateGyro() {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < 1000; i++) {
    float z = readGyroZ();
    if (!isnan(z)) { sum += z; count++; }
    delay(3);
  }
  gyroBias = (count > 0) ? sum / count : 0;
}

float updateGyro() {
  unsigned long nowUs = micros();
  float dt = (nowUs - prevMicros) / 1000000.0;
  prevMicros = nowUs;
  float z = readGyroZ();
  if (!isnan(z)) {
    yawRate = (z - gyroBias) * YAW_SCALE;
    yaw += yawRate * dt;
  }
  return dt;
}

// ---------------- Odometry: encoder distance + gyro heading -> position on the map ----------------
void updateOdometry(float yawBefore) {
  long t = readTicks();
  long dTicks = t - prevTicks;
  prevTicks = t;
  if (dTicks <= 0) return;
  float ds = dTicks / TICKS_PER_CM;
  float h = radians((yawBefore + yaw) * 0.5);   // average heading during this step
  carX += ds * cos(h);
  carY += ds * sin(h);
  traveledCm += ds;
}

// Pillar position seen from the car: forward (+ ahead) and left (+ left) in cm
void relativeToCar(float px, float py, float &fwd, float &left) {
  float h = radians(yaw);
  float dx = px - carX, dy = py - carY;
  fwd  =  dx * cos(h) + dy * sin(h);
  left = -dx * sin(h) + dy * cos(h);
}

// ---------------- Steering ----------------
float maxLeftSteer()  { return STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT); }
float maxRightSteer() { return STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE); }

// steerLeft: + = left, - = right (servo degrees away from center)
void setSteering(float steerLeft, float dt) {
  steerLeft = constrain(steerLeft, -maxRightSteer(), maxLeftSteer());
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);
  float maxStep = SERVO_SPEED_DPS * dt;
  if (target > servoAngle + maxStep)      servoAngle += maxStep;
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;
  else                                    servoAngle = target;
  steering.write((int)(servoAngle + 0.5));
}

// ---------------- Camera messages ----------------
char lineBuf[80];                    // camera v3 sends 14 fields; only the first 10 are used here
uint8_t lineLen = 0;

void readCamera() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { lineLen = 0; continue; }
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      int col, x, b, ok, bf, br, w, d, lat;
      unsigned long seq;
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                     &w, &d, &lat);
      if (n == 10) {
        camColor = col; camDistCm = d; camLatCm = lat;
        lastCamMs = millis();
        newCamData = true;
      }
      lineLen = 0;
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
}

// Put the pillar of the last message on the map
void onCameraMessage() {
  if (camColor == 0 || camDistCm <= 0) return;
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;

  float fwd  = camDistCm + PILLAR_CM / 2;        // camera measures to the front face -> center
  float left = -camLatCm;                        // camera: + = right; map: + = left
  float h = radians(yaw);
  float px = carX + fwd * cos(h) - left * sin(h);
  float py = carY + fwd * sin(h) + left * cos(h);

  // Same pillar as one we already have? -> refine its position
  MapPillar *slots[2] = { &current, &nextOne };
  for (int i = 0; i < 2; i++) {
    MapPillar *p = slots[i];
    if (p->used && p->color == camColor && hypot(p->x - px, p->y - py) < SAME_PILLAR_CM) {
      p->x += BLEND * (px - p->x);
      p->y += BLEND * (py - p->y);
      p->hits++;
      return;
    }
  }
  // A new pillar
  MapPillar fresh = {true, px, py, camColor, 1};
  if (!current.used) {
    current = fresh;
    if (DEBUG_PRINTS) { Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN")); }
  } else if (px > current.x + SAME_PILLAR_CM) {
    nextOne = fresh;                             // farther than the current one: keep it for later
  }
}

// ---------------- The control: where to go -> which heading ----------------
// Wanted lateral position next to one pillar (never pulls the car back toward the pillar)
// (plain numbers, no MapPillar in the parameters: the Arduino IDE breaks on that)
float goalFor(int color, float pillarY) {
  if (color == 1) return min(carY, pillarY - TARGET_CM);   // red: car to its right
  return max(carY, pillarY + TARGET_CM);                    // green: car to its left
}

float headingGoal() {
  float fwd = 0, left = 0;

  // 1) Current pillar far enough behind the camera? -> cleared, the next one becomes current
  if (current.used) {
    relativeToCar(current.x, current.y, fwd, left);
    if (fwd < -RETURN_AFTER_CM) {
      if (DEBUG_PRINTS) Serial.println(F(">> PILLAR CLEARED"));
      current = nextOne;
      nextOne.used = false;
      if (current.used) relativeToCar(current.x, current.y, fwd, left);
    }
  }

  // 2) Which pillar do we aim at?
  //    camera still before the current pillar -> that one
  //    camera already past its center (beside it) -> the next one (or the home lane)
  bool beside = current.used && fwd < 0;
  const MapPillar *target = current.used ? (beside ? (nextOne.used ? &nextOne : NULL) : &current) : NULL;

  goalY = 0;                                     // home lane
  float look = LOOK_HOME_CM;
  if (target != NULL) {
    goalY = goalFor(target->color, target->y);
    float toFace = (target->x - PILLAR_CM / 2) - carX - ARRIVE_BEFORE_CM;
    look = max(LOOK_MIN_CM, toFace);
  }

  // 3) Beside a pillar: never move sideways TOWARD it (straightening or moving away is fine)
  if (beside) {
    if (current.color == 1) goalY = min(goalY, carY);      // red is on our left: no moving left
    else                    goalY = max(goalY, carY);      // green is on our right: no moving right
  }

  // 4) Heading that leads there
  float h = degrees(atan2(goalY - carY, look));
  return constrain(h, -MAX_AIM_DEG, MAX_AIM_DEG);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(CAMERA_BAUD);                     // RX = camera, TX = Serial Monitor

  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  motorStop();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoder, RISING);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);
  if (!startGyro()) {
    Serial.println(F("ERROR: BMI160 not found"));
    while (true) delay(1000);
  }

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  Serial.println(F("STRAIGHT WITH PILLARS - press START"));
  while (digitalRead(BUTTON_PIN) == HIGH) {      // waiting; also shows if the camera talks
    readCamera();
    static unsigned long t = 0;
    if (millis() - t > 1000) {
      t = millis();
      Serial.print(F("camera: "));
      Serial.println(millis() - lastCamMs < 500 ? F("OK") : F("no messages (10-field camera code?)"));
    }
  }
  Serial.println(F("Calibrating gyro... do not move the car"));
  calibrateGyro();
  yaw = 0;
  carX = carY = 0;
  traveledCm = 0;
  prevTicks = readTicks();
  prevMicros = micros();
  lastCamMs = millis();
  newCamData = false;
}

// ---------------- LOOP ----------------
void loop() {
  float yawBefore = yaw;
  float dt = updateGyro();
  updateOdometry(yawBefore);
  readCamera();

  if (finished) { motorStop(); setSteering(0, dt); return; }

  if (traveledCm >= RUN_CM) {
    finished = true;
    motorStop();
    Serial.print(F(">> DONE | x ")); Serial.print(carX, 1);
    Serial.print(F(" | y ")); Serial.print(carY, 1);
    Serial.print(F(" | yaw ")); Serial.println(yaw, 1);
    return;
  }

  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {   // no camera -> do not drive blind
    motorStop();
    setSteering(0, dt);
    static unsigned long t = 0;
    if (millis() - t > 500) { t = millis(); Serial.println(F("WAITING FOR CAMERA")); }
    return;
  }

  if (newCamData) { newCamData = false; onCameraMessage(); }

  goalHeading = headingGoal();
  float steer = KP_HEADING * (goalHeading - yaw) - KD_HEADING * yawRate;
  setSteering(constrain(steer, -MAX_STEER, MAX_STEER), dt);
  motorForward(DRIVE_SPEED);

  static unsigned long lastPrint = 0;
  if (DEBUG_PRINTS && millis() - lastPrint >= 150) {
    lastPrint = millis();
    Serial.print(F("x ")); Serial.print(carX, 0);
    Serial.print(F(" y ")); Serial.print(carY, 1);
    Serial.print(F(" | yaw ")); Serial.print(yaw, 1);
    Serial.print(F(" goal ")); Serial.print(goalHeading, 1);
    Serial.print(F(" | goalY ")); Serial.print(goalY, 1);
    Serial.print(F(" | pillar "));
    if (current.used) {
      Serial.print(current.color == 1 ? F("RED (") : F("GREEN ("));
      Serial.print(current.x, 0); Serial.print(F(",")); Serial.print(current.y, 0); Serial.print(F(")"));
    } else Serial.print(F("-"));
    Serial.print(F(" | cam ")); Serial.print(camColor); Serial.print(F(" ")); Serial.print(camDistCm);
    Serial.print(F("/")); Serial.println(camLatCm);
  }
}
