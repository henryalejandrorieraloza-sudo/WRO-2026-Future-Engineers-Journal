// =====================================================================
// STRAIGHT WITH PILLARS - Arduino Nano
// Obstacle Challenge: drive down a straight lane, dodge red/green pillars on the
// correct side, and brake as soon as the camera confirms the blue or the orange
// floor line (these lines mark the end of this straight / the start of a corner).
//
// Hardware: Arduino Nano (classic, ATmega328P) + ESP32-S3-CAM (vision) +
//           BMI160 gyroscope (heading) + quadrature-less encoder (distance) +
//           TB6612FNG motor driver (rear drive motor) + MG90-class steering servo.
//
// -------------------------------------------------------------------
// BIG PICTURE / ALGORITHM
// -------------------------------------------------------------------
// The car does NOT simply "see a pillar, turn away from it". A camera-only
// reaction like that is noisy (the camera loses the pillar once it is beside
// the car, right when steering still matters) and it has no memory of a pillar
// once it leaves the frame. Instead this sketch builds a small 2D MAP of the
// straight using odometry, and places every pillar the camera has ever reported
// onto that map. Steering is then computed purely from the map, which is stable
// even while the camera cannot currently see the pillar.
//
//   - Odometry point = the centre of the rear (encoder) axle. This is the point
//     (carX, carY) that is integrated every loop from encoder ticks + gyro heading.
//   - Camera point = odometry point advanced CAM_FWD_CM forward along the current
//     heading vector (the camera lens is physically ahead of the rear axle).
//   - Every camera message gives a pillar's distance ahead of the camera and its
//     sideways offset. That is converted into an (x, y) point on the map:
//         P = cameraPoint + R(heading) * (forwardDistance, leftOffset)
//     so the pillar's position on the map does not change even after the camera
//     stops seeing it (it is now beside or behind the car).
//   - Rule of the challenge: RED pillar -> pass on its RIGHT (car's y stays to the
//     right of the pillar, i.e. <= pillarY - TARGET_CM). GREEN pillar -> pass on
//     its LEFT (car's y stays >= pillarY + TARGET_CM).
//   - Steering target: a desired lateral position "goalY" on the map is turned
//     into a desired heading with atan2(goalY - cameraY, lookAheadDistance),
//     clamped to +-MAX_AIM_DEG. A PD controller (gyro heading vs. that target,
//     plus a damping term on yaw rate) turns the heading error into a servo angle.
//   - Once the camera's position is RETURN_AFTER_CM behind the pillar's centre,
//     the pillar is considered "cleared": the car is free to steer back toward
//     the home lane (y = 0) or toward the next pillar already seen.
//
// -------------------------------------------------------------------
// NEW IN THIS VERSION (21 November 2026): stop on the floor line
// -------------------------------------------------------------------
// The camera sketch (obstacle_camera.ino, v3.1) already detects the blue and the
// orange floor lines and reports them in EVERY message, in fields 5 (blueFound)
// and 11 (orangeFound) of the 14-field message. Until now this sketch only read
// the first 10 fields (through latCm) with sscanf, so blueFound was parsed into a
// throwaway local variable and orangeFound was not parsed at all - neither one was
// ever acted on.
//
// This version:
//   1. Extends the sscanf format string in readCamera() from 10 fields to 12, so
//      it also reads field 11 (orangeFound). Field 5 (blueFound) was already being
//      read; the only change there is that it is now SAVED into a persistent
//      global variable instead of being discarded after the line is parsed.
//   2. Adds two new global variables, camBlueFound and camOrangeFound, which hold
//      the most recent value of each flag (0 or 1) and persist between camera
//      messages (they do not reset to 0 automatically - they only change when a
//      new, successfully-parsed message arrives).
//   3. Adds a new check at the very top of loop(), right after reading the camera
//      and before any driving logic: if STOP_ON_LINE is true and either flag is 1,
//      the car immediately commands zero speed (motorStop), centres the steering
//      (setSteering(0, dt)), sets finished = true (the same flag used when RUN_CM
//      is reached) and returns - so the rest of loop() (the pillar-dodging logic)
//      never runs for the remainder of the program. The car simply stays stopped,
//      matching the "finished" behaviour already used at the end of a run.
//
// Important design note: camBlueFound/camOrangeFound are confirmed as soon as the
// camera sees ANY stretch of that line in the picture - per the camera sketch's
// own comments, that can be from as far as ~16 cm (orange) or ~9 cm (blue) away,
// and from anywhere in the frame, not only straight ahead. STOP_ON_LINE is kept as
// its own named constant (not baked into the if-check) specifically so it can be
// switched off with a single edit (set it to false) without touching or deleting
// any of the stop logic, in case testing shows the car needs to be closer to /
// more centred on the line before stopping (that would mean switching this check
// to use blueCenterRow/orangeCenterRow - also sent by the camera - instead of the
// plain found flags).
//
// -------------------------------------------------------------------
// CALIBRATION HISTORY ON THIS CHASSIS (19-21 November 2026)
// -------------------------------------------------------------------
// Car geometry (measured 19 Nov):
//   - Car width: 9 cm -> half width 4.5 cm.
//   - Wheelbase (axle to axle): 9 cm.
//   - Full length (including the front protrusion, ~4 cm): 17.5 cm.
//   - Rear (encoder) axle to camera lens, along the car body: 11.7 cm -> CAM_FWD_CM.
//
// Steering servo (measured 19-21 Nov):
//   - CENTER_ANGLE = 85 is the servo value that was actually confirmed to drive
//     the car straight (not a theoretical midpoint).
//   - Mechanical limits are LEFT_LIMIT = 170 and RIGHT_LIMIT = 10. With a
//     protractor this is 40 degrees of real wheel angle in each direction
//     (170 <-> 10 spans 160 servo degrees total, which the steering linkage turns
//     into +-40 degrees of wheel angle).
//   - Because CENTER_ANGLE (85) sits closer to RIGHT_LIMIT (10) than to
//     LEFT_LIMIT (170), the usable SERVO-degree range is NOT symmetric even
//     though the real wheel-angle range is: 85 - 10 = 75 servo degrees available
//     to the right, 170 - 85 = 85 servo degrees available to the left.
//   - STEER_LEFT_IS_BIGGER = false was confirmed with a steering probe test on
//     this specific chassis: a LARGER servo number steers the wheels to the
//     RIGHT on this car (the opposite wiring would need "true" here).
//
// Drive encoder (measured 21 Nov, with the motor actually running - not pushed
// by hand, which gives different slip and is therefore unreliable):
//   - TICKS_PER_CM = 9.27, from 190 encoder ticks over a measured 20.5 cm of real
//     travel (190 / 20.5 = 9.27).
//
// Gyroscope (BMI160) calibration (measured 21 Nov with a dedicated hand-turn
// test sketch, gyro_90_test.ino, that reuses this file's own register-level
// BMI160 driver so the reading method is identical):
//   - Procedure: hold the car still, press the calibration button (zeroes the
//     integrated yaw and removes the sensor's resting bias), then turn the car
//     EXACTLY 90 degrees to the LEFT by hand and read the yaw value the test
//     sketch prints.
//   - Result: the sketch printed 85 degrees for an actual 90-degree turn, and the
//     sign was positive (yaw increased when turning left, which is the correct
//     direction - so YAW_REVERSED stays false).
//   - YAW_SCALE = 90 / 85 = 1.06 corrects the ~6% under-reading so that an
//     integrated 90-degree turn in code now matches a real 90-degree turn.
//
// -------------------------------------------------------------------
// THE "CRASHES RIGHT NEXT TO THE PILLAR IT JUST DODGED" BUG AND ITS FIX
// -------------------------------------------------------------------
// Symptom reported on the track: the car steers correctly to dodge a pillar, but
// then swings hard back toward that same pillar's side and clips it, right after
// passing it.
//
// Root cause: in headingGoal(), the moment the camera position passes
// RETURN_AFTER_CM behind the pillar's centre, the function declares the pillar
// "cleared" and the protective clamp ("while beside this pillar, never move
// sideways TOWARD it") is dropped on that exact same loop iteration. goalY then
// jumps instantly from "hugging the safe side of the pillar" to a very different
// target (the home lane y = 0, or the next pillar's offset), with no transition
// at all. The PD controller sees a large, sudden heading error and commands a
// hard, immediate steering correction while the car's camera point is still only
// just past the pillar - which is exactly when there is the least room to swing
// back without touching it.
//
// Two different fixes were explored for this:
//   1. A smoothing fix: rate-limit how fast the commanded goalY can move toward
//      the "ideal" goalY, so the return swing is gradual instead of instant.
//   2. A single-variable fix: simply increase RETURN_AFTER_CM, so the pillar is
//      declared "cleared" only once the car has put more real distance between
//      itself and the pillar before the (still instant) return swing fires. This
//      keeps the return SHARP (which is actually wanted here, so the car reaches
//      its target lane quickly) but DELAYS it until there is enough clearance.
//
// RETURN_AFTER_CM was raised from 30 to 45 cm (fix #2) and this alone was enough
// to resolve the crash on this chassis - validated on the track together with the
// gyro fix above. The smoothing approach (#1) was prototyped but is NOT used in
// this file; headingGoal() here is intentionally the simple, un-smoothed version.
// =====================================================================

#include <Wire.h>
#include <Servo.h>
#include <VL53L1X.h>          // Pololu library, for the front distance sensor used in the "after the
                               // line" block near the end of this file (see handleLineStop())

// Defined at the top so the Arduino IDE knows the type before any function uses it.
struct MapPillar {
  bool  used;
  float x, y;        // cm on the map (pillar center)
  int   color;       // 1 red, 2 green
  int   hits;
};

// ================= SETTINGS (change these) =================
// --- Test ---
const float RUN_CM = 2000.0;          // distance to drive before the normal "end of run" stop

// --- Stop on floor line (new, 21 Nov) ---
// Set this to false to disable the stop-on-line feature entirely, without deleting
// any of the logic below - useful if a test shows it needs to trigger differently
// (e.g. only on blueCenterRow/orangeCenterRow, once the line is centred and close,
// instead of on any detection anywhere in the frame).
const bool STOP_ON_LINE = true;       // true = brake as soon as EITHER the blue line OR the orange line is seen

// --- Car geometry (measured 19 Nov) ---
const float WHEELBASE_CM = 9.0;      // axle-to-axle distance (informative only: with +-40 real degrees of
                                      // wheel angle the minimum turning radius works out to ~10.7 cm)
const float CAM_FWD_CM   = 11.7;     // measured 20 Nov: rear (encoder) axle -> camera lens, along the car body.

// --- Steering (CENTER_ANGLE is the value confirmed to drive straight; LEFT/RIGHT_LIMIT
//     are the mechanical stops, measured as +-40 degrees of real wheel angle with a protractor) ---
const int  CENTER_ANGLE         = 85;
const int  LEFT_LIMIT           = 170;
const int  RIGHT_LIMIT          = 10;
const bool STEER_LEFT_IS_BIGGER = false;  // confirmed with the steering probe test on this chassis
const int  SERVO_SPEED_DPS      = 300;    // how fast the servo is allowed to move, in degrees per second
                                           // (limits how abruptly the physical steering can snap to a new angle)

// --- Motor ---
const int  DRIVE_SPEED    = 70;      // 0-255 PWM duty cycle while driving
const bool MOTOR_REVERSED = true;    // true if the two motor driver input wires are swapped for this wiring

// --- Heading PD controller (validated values; re-check if the chassis, wheels, or battery change) ---
const float KP_HEADING   = 2.0;      // steering degrees commanded per degree of heading error
const float KD_HEADING   = 0.03;     // lowered from 0.10: with MAX_STEER raised, the resulting yaw rate is
                                      // higher, and 0.10 damped (killed) the turn almost immediately, which
                                      // looked on the track like "barely dodges the pillar"
const float MAX_STEER    = 70.0;     // maximum steering command, in servo degrees away from CENTER_ANGLE.
                                      // The real mechanical range with CENTER_ANGLE = 85 is NOT symmetric
                                      // (75 degrees available to the right: 85-10; 85 degrees available to
                                      // the left: 170-85); 70 leaves a safety margin on both sides without
                                      // under-using the smaller (right) side of the range.

// --- Pillars ---
const float TARGET_CM       = 17.0;  // pillar center <-> car center clearance while passing:
                                      // 4.5 (half car width) + 10 (safety gap) + 2.5 (half pillar width)
const float MAX_AIM_DEG     = 60.0;  // maximum heading the car will aim for while dodging a pillar
const float LOOK_MIN_CM     = 12.0;  // shortest look-ahead distance used in the aim-angle calculation
                                      // (prevents an unrealistically extreme angle when very close to a pillar)
const float ARRIVE_BEFORE_CM = 55.0; // be established on the safe side this many cm BEFORE the pillar's front face
const float LOOK_HOME_CM    = 20.0;  // look-ahead distance used while steering back to the home lane
const float RETURN_AFTER_CM = 45.0;  // camera point this far PAST the pillar's centre before the pillar is
                                      // considered cleared and the car is free to swing back. Raised from 30
                                      // to 45 on 21 Nov specifically to fix the "clips the pillar right after
                                      // dodging it" crash described above - this alone resolved it on the track.
const float SAME_PILLAR_CM  = 15.0;  // a new detection within this distance of a stored pillar is treated as
                                      // a refinement of that same pillar, not a brand-new one
const float BLEND           = 0.3;   // how much each new camera message is allowed to nudge a stored pillar's
                                      // position (0 = ignore new data completely, 1 = replace it outright)
const float MAX_SIDE_CM     = 60.0;  // detections farther to the side than this are treated as noise/ignored

// --- Pillar size ---
const float PILLAR_CM       = 5.0;   // physical pillar width (competition rule: 50 x 50 x 100 mm)
const float TICKS_PER_CM    = 9.27;  // measured 21 Nov WITH THE MOTOR RUNNING (not pushed by hand):
                                      // 190 encoder ticks over 20.5 cm of real travel = 190 / 20.5

// --- Camera link ---
const long  CAMERA_BAUD       = 115200;
const unsigned long CAMERA_TIMEOUT_MS = 500;   // if no valid camera line arrives for this long, stop driving blind
const bool  DEBUG_PRINTS      = true;

// --- Gyro (measured 21 Nov with a 90-degree hand-turn test, see the big header comment above) ---
const bool  YAW_REVERSED = false;    // false: yaw correctly increases when the car is turned LEFT by hand
const float YAW_SCALE    = 1.06;     // 90 / (yaw value printed after an actual 90-degree hand turn) = 90 / 85
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN   = 9;
const int PWMA_PIN    = 6;
const int AIN1_PIN    = 7;
const int AIN2_PIN    = 8;
const int BUTTON_PIN  = 10;
const int ENCODER_PIN = 3;

Servo steering;

// ---------------- Car state (odometry point = drive axle) ----------------
float yaw = 0, yawRate = 0;          // degrees, and degrees/second; positive = car turned LEFT
float carX = 0, carY = 0;            // cm, drive (rear) axle position on the map (y positive = to the left)
float servoAngle = CENTER_ANGLE;
unsigned long prevMicros = 0;
long  prevTicks = 0;
float traveledCm = 0;
bool  finished = false;              // true once the car has stopped for good (end of run, or a floor line)

volatile long encoderTicks = 0;
void onEncoder() { encoderTicks++; }

long readTicks() {
  noInterrupts();
  long t = encoderTicks;
  interrupts();
  return t;
}

// Camera point on the map: the odometry point advanced CAM_FWD_CM forward along the current heading vector.
float camPosX() { return carX + CAM_FWD_CM * cos(radians(yaw)); }
float camPosY() { return carY + CAM_FWD_CM * sin(radians(yaw)); }

// ---------------- Pillars on the map ----------------
MapPillar current = {false, 0, 0, 0, 0};   // the pillar currently being dodged
MapPillar nextOne = {false, 0, 0, 0, 0};   // a farther pillar that has already been seen, waiting its turn

// ---------------- Last camera message ----------------
// Full 14-field message sent by obstacle_camera.ino v3.1:
//   $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
// This sketch only needs a subset of those fields (it does not use x, bottom, correct,
// blueRow, seq, width, orangeRow, blueCenterRow or orangeCenterRow), so readCamera()
// below only parses as many fields as it needs, in order, from the start of the message.
int camColor = 0, camDistCm = -1, camLatCm = 0;
int camBlueFound = 0, camOrangeFound = 0;   // NEW (21 Nov): persistent flags used to trigger the line-stop
unsigned long lastCamMs = 0;
bool newCamData = false;

float goalY = 0, goalHeading = 0;    // kept around just for the debug prints

// ---------------- Motor ----------------
void motorForward(int speed) {
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);
  analogWrite(PWMA_PIN, speed);
}

void motorStop() {                   // free stop (coast, both inputs LOW): gentle on the gearbox
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);
}

// ---------------- BMI160 (register-level driver, no external library) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at the +-500 deg/s range this sketch configures
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
  return bmiRead(0x00, &id, 1) && id == 0xD1;   // BMI160's chip-ID register must read back 0xD1
}

bool startGyro() {
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;   // try both possible I2C addresses
  bmiWrite(0x7E, 0x11); delay(50);   // accelerometer: normal power mode
  bmiWrite(0x7E, 0x15); delay(100);  // gyroscope: normal power mode
  bmiWrite(0x42, 0x2A);              // gyro output data rate: 400 Hz
  bmiWrite(0x43, 0x02);              // gyro full-scale range: +-500 deg/s
  delay(50);
  return true;
}

float readGyroZ() {
  uint8_t b[2];
  if (!bmiRead(0x10, b, 2)) return NAN;    // 0x10/0x11 = gyro Z axis, low byte then high byte
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);
  float dps = raw / GYRO_SCALE;
  return YAW_REVERSED ? -dps : dps;        // flip the sign here if the gyro is mounted the other way up
}

void calibrateGyro() {
  // Averages 1000 samples (about 3 seconds at ~3 ms per sample) while the car is held
  // still, to measure and remove the sensor's resting bias (its "zero reading" is
  // never exactly 0 deg/s). Must be run with the car completely still, or the bias
  // measurement is wrong and yaw will slowly drift during the run.
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
    yawRate = (z - gyroBias) * YAW_SCALE;   // remove the resting bias, then apply the 21-Nov correction factor
    yaw += yawRate * dt;                    // integrate angular rate into an absolute heading
  }
  return dt;
}

// ---------------- Odometry: encoder distance + gyro heading -> drive axle position on the map ----------------
void updateOdometry(float yawBefore) {
  long t = readTicks();
  long dTicks = t - prevTicks;
  prevTicks = t;
  if (dTicks <= 0) return;                         // ignore noise ticks / no motion this loop
  float ds = dTicks / TICKS_PER_CM;                 // distance travelled this loop, in cm
  float h = radians((yawBefore + yaw) * 0.5);       // average heading DURING this step (not before or after it)
  carX += ds * cos(h);
  carY += ds * sin(h);
  traveledCm += ds;
}

// Pillar position seen from the CAMERA point: forward (+ ahead) and left (+ left), in cm,
// rotated into the car's current reference frame.
void relativeToCar(float px, float py, float &fwd, float &left) {
  float h = radians(yaw);
  float dx = px - camPosX(), dy = py - camPosY();
  fwd  =  dx * cos(h) + dy * sin(h);
  left = -dx * sin(h) + dy * cos(h);
}

// ---------------- Steering ----------------
float maxLeftSteer()  { return STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT); }
float maxRightSteer() { return STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE); }

// steerLeft: positive = steer left, negative = steer right (servo degrees away from CENTER_ANGLE).
// The servo is also rate-limited here (SERVO_SPEED_DPS) so a big commanded change does not make
// the physical wheels snap instantly - they ramp toward the target at a bounded angular speed.
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
char lineBuf[64];
uint8_t lineLen = 0;

void readCamera() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { lineLen = 0; continue; }          // '$' marks the start of a new message: reset the buffer
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      int col, x, b, ok, bf, br, w, d, lat, of, orow;
      unsigned long seq;
      // Message layout (14 fields total, only the first 12 are parsed here):
      //   $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
      // Before 21 Nov this only read the first 10 fields (through latCm): blueFound (bf) was parsed
      // into a local variable and then thrown away at the end of this function, and orangeFound was
      // not read at all. It is now extended to 12 fields specifically to also capture orangeFound (of),
      // and both bf and of are saved into persistent globals below so the new stop-on-line check in
      // loop() can see them on every iteration, not just the instant the message arrives.
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                     &w, &d, &lat, &of, &orow);
      if (n == 12) {
        camColor = col; camDistCm = d; camLatCm = lat;
        camBlueFound = bf; camOrangeFound = of;   // NEW (21 Nov): persisted so loop() can check them
        lastCamMs = millis();
        newCamData = true;
      }
      lineLen = 0;
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
}

// Places the pillar reported in the last camera message onto the map (measured from the camera point).
void onCameraMessage() {
  if (camColor == 0 || camDistCm <= 0) return;             // no pillar in this message
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;         // too far to the side: treat as noise

  float fwd  = camDistCm + PILLAR_CM / 2;        // camera measures to the pillar's front face -> convert to its centre
  float left = -camLatCm;                        // camera convention: + = right; map convention: + = left
  float h = radians(yaw);
  float cx = camPosX(), cy = camPosY();
  float px = cx + fwd * cos(h) - left * sin(h);
  float py = cy + fwd * sin(h) + left * cos(h);

  // Is this the same pillar we already have stored (same colour, close position)? If so, just refine
  // its stored position a little instead of creating a duplicate pillar.
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
  // Otherwise this is a brand-new pillar.
  MapPillar fresh = {true, px, py, camColor, 1};
  if (!current.used) {
    current = fresh;
    if (DEBUG_PRINTS) { Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN")); }
  } else if (px > current.x + SAME_PILLAR_CM) {
    nextOne = fresh;                             // farther ahead than the current one: hold it for later
  }
}

// ---------------- The control: decide where to go -> turn that into a heading ----------------
// Desired lateral position (at the camera point) alongside one pillar. Written so it can never pull
// the car sideways BACK toward the pillar it is passing (only straightening or moving further away).
float goalFor(int color, float pillarY) {
  float cy = camPosY();
  if (color == 1) return min(cy, pillarY - TARGET_CM);   // red pillar: car passes on its right
  return max(cy, pillarY + TARGET_CM);                    // green pillar: car passes on its left
}

float headingGoal() {
  float fwd = 0, left = 0;

  // 1) Is the current pillar now far enough behind the camera point to call it cleared?
  //    If so, promote nextOne (if any) to current, and recompute fwd/left for the new current pillar.
  if (current.used) {
    relativeToCar(current.x, current.y, fwd, left);
    if (fwd < -RETURN_AFTER_CM) {
      if (DEBUG_PRINTS) Serial.println(F(">> PILLAR CLEARED"));
      current = nextOne;
      nextOne.used = false;
      if (current.used) relativeToCar(current.x, current.y, fwd, left);
    }
  }

  // 2) Which pillar (if any) should the car currently be aiming around?
  bool beside = current.used && fwd < 0;          // camera point is already alongside/behind the current pillar
  const MapPillar *target = current.used ? (beside ? (nextOne.used ? &nextOne : NULL) : &current) : NULL;

  goalY = 0;                                      // default: aim for the home lane
  float look = LOOK_HOME_CM;
  if (target != NULL) {
    goalY = goalFor(target->color, target->y);
    float toFace = (target->x - PILLAR_CM / 2) - camPosX() - ARRIVE_BEFORE_CM;
    look = max(LOOK_MIN_CM, toFace);
  }

  // 3) While still beside the current pillar: never let goalY move sideways TOWARD it.
  //    (Straightening out or moving further away is fine; moving closer is exactly how a crash happens.)
  if (beside) {
    if (current.color == 1) goalY = min(goalY, camPosY());   // red is on our left: never move further left
    else                    goalY = max(goalY, camPosY());   // green is on our right: never move further right
  }

  // 4) Convert the desired lateral position into a heading the PD loop can chase.
  float h = degrees(atan2(goalY - camPosY(), look));
  return constrain(h, -MAX_AIM_DEG, MAX_AIM_DEG);
}

// =====================================================================
// NEW (21 Nov), INDEPENDENT BLOCK: what to do AFTER the floor line.
// -------------------------------------------------------------------
// Everything in this block is new and self-contained: it does not read or change any pillar/map/
// camera code above, and nothing above reads any variable from this block except for the single
// trigger line in loop() (search for "handleLineStop(dt)") that was changed to call into it instead
// of stopping the car immediately. Delete this whole block and that one line reverts to the plain
// "stop as soon as the line is seen" behaviour.
//
// What it does: the first time a floor line is confirmed, it powers on the FRONT distance sensor
// (VL53L1X / TOF400C) - the same sensor wired for open_challenge.ino, shared on this Nano's I2C bus,
// normally held in reset by every sketch that does not need it. From that moment on the car ignores
// every pillar and every colour message completely (it ARMS itself with lineStopArmed = true, which
// never goes back to false), steers back to the home lane position (y = 0, not just a fixed compass
// heading) and keeps driving forward, until the front
// sensor reads less than FRONT_STOP_MM - only then does it brake and stop for good.
// =====================================================================

// Front TOF pins, shared wiring with open_challenge.ino: FRONT = A1 (used here), LEFT = A3, RIGHT = A2,
// REAR = A0 (all three held in reset here, so only the front sensor answers on the shared I2C bus).
const int XSHUT_FRONT = A1, XSHUT_LEFT = A3, XSHUT_RIGHT = A2, XSHUT_REAR = A0;
const long TOF_BUDGET_US = 50000;     // 50 ms per measurement (same setting as open_challenge.ino)
const int  FRONT_STOP_MM = 1200;      // stop once the front sensor reads less than this -> 120 cm

VL53L1X frontTof;
bool  frontTofOk    = false;   // true once the front sensor answered init() successfully
bool  lineStopArmed = false;   // becomes true on the first line detection and then never resets to false
int   frontMm       = -1;      // last distance read from the front sensor, in mm (-1 = no reading yet)

// Powers on only the front sensor (the other three stay in reset) and configures it exactly like the
// left/right sensors in open_challenge.ino: Long range mode, 50 ms timing budget, a 16 x 8 pixel
// region of interest (narrower than default, so it is less fooled by reflections from the sides).
bool startFrontTof() {
  pinMode(XSHUT_LEFT,  OUTPUT); digitalWrite(XSHUT_LEFT,  LOW);
  pinMode(XSHUT_RIGHT, OUTPUT); digitalWrite(XSHUT_RIGHT, LOW);
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);
  pinMode(XSHUT_FRONT, INPUT); delay(10);     // release the front sensor's reset: it wakes up at 0x29
  frontTof.setTimeout(200);
  if (!frontTof.init()) return false;         // it is the only TOF on the bus, so no address change is needed
  frontTof.setDistanceMode(VL53L1X::Long);
  frontTof.setMeasurementTimingBudget(TOF_BUDGET_US);
  frontTof.setROISize(16, 8);
  frontTof.startContinuous(TOF_BUDGET_US / 1000);
  return true;
}

// Called every loop once a floor line has been confirmed, instead of stopping immediately. Starts the
// front sensor the first time it runs, then every loop does ONE continuous thing (not separate "return"
// and "drive" phases): it recomputes a heading that steers back toward the home lane position
// (camPosY() = 0) and immediately drives forward with that heading, so the car naturally straightens
// out as it gets closer to the centre lane while it keeps moving the whole time - all while completely
// ignoring the pillar map and every new colour message - until the front sensor reports a wall closer
// than FRONT_STOP_MM, at which point it brakes and stays stopped.
void handleLineStop(float dt) {
  if (!lineStopArmed) {
    lineStopArmed = true;
    frontTofOk = startFrontTof();
    if (DEBUG_PRINTS) Serial.println(frontTofOk ? F(">> FRONT TOF READY") : F(">> FRONT TOF NOT FOUND"));
  }

  if (frontTofOk && frontTof.dataReady()) {
    frontTof.read(false);                              // false = read the value already there, do not wait
    uint16_t v = frontTof.ranging_data.range_mm;
    frontMm = (v > 0 && v < 4000) ? v : 4000;           // 0 or >= 4000 = nothing in range = treat as far
  }

  if (frontTofOk && frontMm > 0 && frontMm < FRONT_STOP_MM) {
    finished = true;
    motorStop();
    setSteering(0, dt);
    Serial.print(F(">> WALL AHEAD: STOP (front ")); Serial.print(frontMm); Serial.println(F(" mm)"));
    return;
  }

  // Steer back toward the home lane POSITION (y = 0), the same way headingGoal() already does once a
  // pillar is cleared and there is no target left - NOT a fixed compass heading. If the car was still
  // offset to one side (mid-dodge) the instant the line was seen, a fixed heading = 0 would just drive
  // it straight in parallel, offset forever; aiming at y = 0 actually brings it back to the centre lane.
  float targetHeading = degrees(atan2(0 - camPosY(), LOOK_HOME_CM));
  targetHeading = constrain(targetHeading, -MAX_AIM_DEG, MAX_AIM_DEG);
  float steer = KP_HEADING * (targetHeading - yaw) - KD_HEADING * yawRate;
  setSteering(constrain(steer, -MAX_STEER, MAX_STEER), dt);
  motorForward(DRIVE_SPEED);

  if (DEBUG_PRINTS) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 150) {
      lastPrint = millis();
      Serial.print(F("LINE STOP MODE | yaw ")); Serial.print(yaw, 1);
      Serial.print(F(" | front ")); Serial.print(frontTofOk ? frontMm : -1); Serial.println(F(" mm"));
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(CAMERA_BAUD);                     // RX = camera link, TX = Serial Monitor (shared USB UART)

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
    while (true) delay(1000);                    // no gyro -> heading control is impossible, so stop here for good
  }

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  Serial.println(F("STRAIGHT WITH PILLARS - press START"));
  while (digitalRead(BUTTON_PIN) == HIGH) {      // wait for the start button; also shows if the camera is talking
    readCamera();
    static unsigned long t = 0;
    if (millis() - t > 1000) {
      t = millis();
      Serial.print(F("camera: "));
      Serial.println(millis() - lastCamMs < 500 ? F("OK") : F("no messages (camera on, RX wired to D0?)"));
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

  if (finished) { motorStop(); setSteering(0, dt); return; }   // already stopped for good: just hold position

  // NEW (21 Nov): as soon as the camera confirms the blue line OR the orange line, hand control to
  // the independent handleLineStop() block (defined above setup()) instead of stopping immediately:
  // it straightens the car out, ignores every pillar/colour message from then on, and only stops once
  // the front TOF sensor reports a wall closer than FRONT_STOP_MM. This is the ONLY line in loop() that
  // changed for that feature; everything else in this function is exactly as before. lineStopArmed is
  // set to true inside handleLineStop() and never reset, so this check keeps calling it on every later
  // loop even on a camera message where camBlueFound/camOrangeFound happen to read 0 again.
  if (STOP_ON_LINE && (camBlueFound || camOrangeFound || lineStopArmed)) {
    handleLineStop(dt);
    return;
  }

  if (traveledCm >= RUN_CM) {
    finished = true;
    motorStop();
    Serial.print(F(">> DONE | x ")); Serial.print(carX, 1);
    Serial.print(F(" | y ")); Serial.print(carY, 1);
    Serial.print(F(" | yaw ")); Serial.println(yaw, 1);
    return;
  }

  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {   // camera link silent for too long: do not drive blind
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
    Serial.print(F("eje x ")); Serial.print(carX, 0);
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
    Serial.print(F("/")); Serial.print(camLatCm);
    Serial.print(F(" | line blue ")); Serial.print(camBlueFound);
    Serial.print(F(" orange ")); Serial.println(camOrangeFound);
  }
}
