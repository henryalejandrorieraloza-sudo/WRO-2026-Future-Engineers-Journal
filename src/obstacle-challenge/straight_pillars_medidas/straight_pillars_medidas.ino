// =====================================================================
// STRAIGHT WITH PILLARS - Arduino Nano (medidas del carro nuevo, 19 nov;
//                                        angulo real de llanta medido 21 nov;
//                                        servo 85/170/10 (centro real) y TICKS_PER_CM
//                                        medido con el motor corriendo, 21 nov)
// Camara (ESP32S3-CAM) + giroscopio (BMI160) + encoder + motor trasero + servo de direccion.
//
// Idea: el carro guarda un MAPA de la recta (odometria con encoder + giroscopio).
//   - Punto de odometria = centro del eje de las llantas con encoder (eje trasero).
//   - Punto de la camara = odometria + CAM_FWD_CM en la direccion del carro (vector).
//   - Cada mensaje de la camara ubica el pilar en el mapa: P = C_cam + R(h)*(adelante, izquierda).
//   - Rojo  -> el pilar queda a la IZQUIERDA del carro, a >= TARGET_CM (el carro pasa por su derecha)
//     Verde -> el pilar queda a la DERECHA del carro, a >= TARGET_CM (el carro pasa por su izquierda)
//   - Direccion: posicion lateral deseada -> rumbo = atan(error lateral / distancia que falta),
//     limitado a +-MAX_AIM_DEG -> el PD del giroscopio lo convierte en angulo del servo.
//   - Pasado el pilar (RETURN_AFTER_CM) el carro vuelve a la linea de inicio (y = 0).
//
// Medidas (19-21 nov):
//   ancho del carro 9 cm -> mitad 4.5 cm
//   distancia entre ejes 9 cm
//   largo total 17.5 cm (incluye protuberancia frontal ~4 cm)
//   eje de encoder -> lente de la camara: 11.7 cm (CAM_FWD_CM)
//   servo: centro REAL 85 (confirmado yendo recto), izquierda 170, derecha 10
//          (170/10 = 40 grados de llanta reales medidos con transportador; con centro en 85 el
//           recorrido ya no es simetrico: 75 hacia la derecha, 85 hacia la izquierda en unidades de servo)
//   STEER_LEFT_IS_BIGGER = false (confirmado con el probe: en este carro, numero de servo mas alto = derecha)
//   TICKS_PER_CM = 9.27, medido con el MOTOR CORRIENDO el 21 nov (190 ticks / 20.5 cm reales).
//     (el valor viejo, 12.67, estaba medido empujando a mano -> quedaba mal escalado y hacia que el
//      carro calculara su propia posicion equivocada apenas perdia de vista un pilar.)
//
// PENDIENTE (todavia sin medir en este chasis): YAW_REVERSED / YAW_SCALE del giroscopio
// (prueba: giro de 90 grados a mano, ver cuanto marca el yaw impreso).
//
// Conexiones: camara GPIO43 (TX) -> Nano D0 (RX) | encoder D3 | boton START D10
//   I2C: SDA=A4, SCL=A5 | servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8
// SUBIR: manten presionado el RESET de la CAMARA durante toda la subida (la camara esta en D0).
// Sketch de la camara: obstacle_camera v3.1b (mensaje de 14 campos; el Nano lee los 10 primeros).
// =====================================================================

#include <Wire.h>                         // I2C bus library (used by the BMI160 gyroscope)
#include <Servo.h>                        // servo library (generates the steering PWM signal)

// Defined at the top so the Arduino IDE knows it before any function
struct MapPillar {                        // one pillar stored on the odometry map
  bool  used;                             // true if this slot holds a real pillar
  float x, y;        // cm on the map (pillar center)
  int   color;       // 1 red, 2 green
  int   hits;        // how many camera messages confirmed this pillar (used to refine its position)
};  // end of the MapPillar structure

// ================= SETTINGS (change these) =================
// --- Test ---
const float RUN_CM = 2000.0;          // distance to drive (in cm of encoder travel), then stop

// --- Car geometry (measured 19 nov) ---
const float WHEELBASE_CM = 9.0;      // distancia entre ejes (informativo: radio minimo ~10.7 cm con 40 grados reales)
const float CAM_FWD_CM   = 11.7;     // medido 20 nov: eje de las llantas con encoder -> lente de la camara, a lo largo del carro.

// --- Steering (centro real confirmado 85; topes mecanicos 170/10 = 40 grados de llanta) ---
const int  CENTER_ANGLE         = 85;     // servo angle that drives straight (confirmed by test)
const int  LEFT_LIMIT           = 170;    // mechanical limit of the servo on the left side
const int  RIGHT_LIMIT          = 10;     // mechanical limit of the servo on the right side
const bool STEER_LEFT_IS_BIGGER = false;  // confirmado con el probe en este carro (higher servo number = right)
const int  SERVO_SPEED_DPS      = 300;    // max servo speed in degrees per second (software limit, protects the rack)

// --- Motor ---
const int  DRIVE_SPEED    = 70;      // 0-255 PWM value for the drive motor
const bool MOTOR_REVERSED = true;    // true = flip the motor direction pins for this wiring

// --- Heading PD (validated values; check them in the first test) ---
const float KP_HEADING   = 2.0;      // steering degrees per degree of heading error (proportional gain)
const float KD_HEADING   = 0.03;     // bajado de 0.10: con MAX_STEER mas alto el yawRate es mayor y
                                      // 0.10 frenaba el giro casi de inmediato (se veia "esquiva poquito")
const float MAX_STEER    = 70.0;     // rango mecanico real con centro 85: 75 a la derecha (85-10),
                                      // 85 a la izquierda (170-85); 70 deja margen sin sub-usar el rango

// --- Pillars ---
const float TARGET_CM       = 17.0;  // pillar center <-> camera (side): 4.5 (half car) + 10 gap + 2.5 (half pillar)
                                      // (vuelto a 17, el valor que sale de las medidas reales; sube esto
                                      //  si quieres mas separacion, pero ya no es un numero arbitrario)
const float MAX_AIM_DEG     = 60.0;  // max heading while dodging (degrees)
const float LOOK_MIN_CM     = 12.0;  // shortest distance used in atan (avoids a huge angle when very close)
const float ARRIVE_BEFORE_CM = 55.0; // be at the safe side this many cm BEFORE the pillar's front face
const float LOOK_HOME_CM    = 20.0;  // how fast it goes back to the home lane after a pillar
const float RETURN_AFTER_CM = 45.0;  // camera this far past the pillar center -> free to go back
                                      // (minimo geometrico: CAM_FWD_CM 11.7 + medio pilar 2.5 = 14.2;
                                      //  30 deja el doble de margen mientras se confirma el giroscopio)
const float SAME_PILLAR_CM  = 15.0;  // a detection this close to a stored pillar is the same pillar
const float BLEND           = 0.3;   // how much each new message moves the stored pillar (0-1, 0.3 = 30 % of the way)
const float MAX_SIDE_CM     = 60.0;  // detections farther to the side are ignored (outside the corridor)

// --- Pillar size ---
const float PILLAR_CM       = 5.0;   // real pillar width in cm (rules: 50 x 50 x 100 mm)
const float TICKS_PER_CM    = 9.27;  // medido con el motor corriendo el 21 nov: 190 ticks / 20.5 cm reales (encoder pulses per cm of travel)

// --- Camera link ---
const long  CAMERA_BAUD       = 115200;     // speed of the UART link from the camera (must match the camera)
const unsigned long CAMERA_TIMEOUT_MS = 500;  // if no camera message arrives for this long, the car stops
const bool  DEBUG_PRINTS      = true;       // true = print debug text to the Serial Monitor

// --- Gyro (check both in the first test: turn the car left by hand, yaw must go UP) ---
const bool  YAW_REVERSED = false;    // true if yaw goes DOWN when you turn the car LEFT by hand
const float YAW_SCALE    = 1.06;     // 90 / (yaw printed after turning exactly 90 degrees by hand) -- correction factor of the gyro
// ===========================================================

// ---------------- Pins ----------------
const int SERVO_PIN   = 9;           // Nano D9: steering servo signal
const int PWMA_PIN    = 6;           // Nano D6: TB6612FNG speed (PWM) input
const int AIN1_PIN    = 7;           // Nano D7: TB6612FNG direction input 1
const int AIN2_PIN    = 8;           // Nano D8: TB6612FNG direction input 2
const int BUTTON_PIN  = 10;          // Nano D10: START button (INPUT_PULLUP, pressed = LOW)
const int ENCODER_PIN = 3;           // Nano D3: motor encoder (interrupt input)

Servo steering;                      // the steering servo object (created once, attached in setup)

// ---------------- Car state (odometry point = drive axle) ----------------
float yaw = 0, yawRate = 0;          // degrees, + = car turned LEFT; yawRate = current turn speed in deg/s
float carX = 0, carY = 0;            // cm, drive axle on the map (y + = left)
float servoAngle = CENTER_ANGLE;     // the servo angle actually sent (moves slowly toward the target)
unsigned long prevMicros = 0;        // time of the previous gyro update (microseconds)
long  prevTicks = 0;                 // encoder count at the previous odometry update
float traveledCm = 0;                // total distance driven since START (cm)
bool  finished = false;              // true after the run distance is reached

volatile long encoderTicks = 0;      // encoder pulses counted by the interrupt (volatile: changed inside the ISR)
void onEncoder() { encoderTicks++; } // interrupt function: called on every rising edge of the encoder pin

long readTicks() {                   // read the encoder count safely
  noInterrupts();                    // stop interrupts so the 32-bit value cannot change while being read
  long t = encoderTicks;             // copy the counter
  interrupts();                      // turn the interrupts back on
  return t;                          // return the copy
}

// Camera point on the map: the odometry point moved CAM_FWD_CM forward along the heading (vector)
float camPosX() { return carX + CAM_FWD_CM * cos(radians(yaw)); }   // camera x on the map
float camPosY() { return carY + CAM_FWD_CM * sin(radians(yaw)); }   // camera y on the map

// ---------------- Pillars on the map ----------------
MapPillar current = {false, 0, 0, 0, 0};   // the pillar being dodged (not used at the start)
MapPillar nextOne = {false, 0, 0, 0, 0};   // a farther pillar already seen (waits until the current one is cleared)

// ---------------- Last camera message ----------------
// $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm (first 10 fields)
int camColor = 0, camDistCm = -1, camLatCm = 0;   // last received values: color, distance ahead, side (lateral)
unsigned long lastCamMs = 0;         // time of the last valid camera message (millis)
bool newCamData = false;             // true when a new message arrived and was not used yet

float goalY = 0, goalHeading = 0;    // for the debug prints (target lateral position and target heading)

// ---------------- Motor ----------------
void motorForward(int speed) {       // drive forward at the given PWM speed
  digitalWrite(AIN1_PIN, MOTOR_REVERSED ? LOW : HIGH);   // direction pin 1 (flipped if the motor is wired backwards)
  digitalWrite(AIN2_PIN, MOTOR_REVERSED ? HIGH : LOW);   // direction pin 2 (the opposite of pin 1)
  analogWrite(PWMA_PIN, speed);      // PWM speed on the enable/speed pin
}

void motorStop() {                   // free stop (coast): gentle for the gearbox
  digitalWrite(AIN1_PIN, LOW);       // both direction pins LOW = the driver lets the motor coast
  digitalWrite(AIN2_PIN, LOW);  // second direction pin LOW: with AIN1 also LOW the driver lets the motor coast
  analogWrite(PWMA_PIN, 0);          // no speed
}

// ---------------- BMI160 (registers, same as the validated turn) ----------------
const float GYRO_SCALE = 65.6;       // LSB per deg/s at +-500 deg/s (sensor datasheet value)
uint8_t bmiAddr = 0x68;              // I2C address of the BMI160 (0x68 or 0x69, found at startup)
float gyroBias = 0;                  // average gyro reading while the car is still (zero offset)

void bmiWrite(uint8_t reg, uint8_t val) {   // write one value into one BMI160 register
  Wire.beginTransmission(bmiAddr);   // start talking to the sensor
  Wire.write(reg);                   // first byte: which register
  Wire.write(val);                   // second byte: the value to write
  Wire.endTransmission();            // send it
}

bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t len) {   // read 'len' bytes starting at register 'reg'
  Wire.beginTransmission(bmiAddr);   // start talking to the sensor
  Wire.write(reg);                   // which register to start from
  if (Wire.endTransmission(false) != 0) return false;   // send the register address, keep the bus (repeated start); fail if no answer
  if (Wire.requestFrom(bmiAddr, len) != len) return false;   // ask for 'len' bytes; fail if fewer came back
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();   // copy the received bytes into the buffer
  return true;                       // all bytes read
}

bool bmiFound(uint8_t addr) {        // check if the BMI160 answers at this address
  bmiAddr = addr;                    // try this address
  uint8_t id = 0;                    // the chip ID register value goes here
  return bmiRead(0x00, &id, 1) && id == 0xD1;   // register 0x00 is the chip ID; the BMI160 always returns 0xD1
}

bool startGyro() {                   // set up the gyroscope; return false if it is not found
  if (!bmiFound(0x68) && !bmiFound(0x69)) return false;   // look at both possible addresses
  bmiWrite(0x7E, 0x11); delay(50);   // command register: accelerometer into normal mode (wait 50 ms)
  bmiWrite(0x7E, 0x15); delay(100);  // command register: gyroscope into normal mode (wait 100 ms)
  bmiWrite(0x42, 0x2A);              // gyro configuration: 400 Hz output data rate
  bmiWrite(0x43, 0x02);              // gyro range register: +-500 deg/s
  delay(50);                         // let the settings take effect
  return true;                       // gyro ready
}

float readGyroZ() {                  // read the turn rate around the vertical axis, in deg/s
  uint8_t b[2];                      // two bytes: low and high part of the 16-bit value
  if (!bmiRead(0x10, b, 2)) return NAN;   // register 0x10 = gyro Z low byte; return NaN (not a number) on a read error
  int16_t raw = (int16_t)((b[1] << 8) | b[0]);   // join the two bytes into a signed 16-bit number
  float dps = raw / GYRO_SCALE;      // convert raw counts to degrees per second
  return YAW_REVERSED ? -dps : dps;  // flip the sign if the sensor is mounted the other way
}

void calibrateGyro() {               // measure the gyro zero offset while the car stands still
  float sum = 0;                     // sum of all readings
  int count = 0;                     // number of good readings
  for (int i = 0; i < 1000; i++) {   // take 1000 samples
    float z = readGyroZ();           // one reading
    if (!isnan(z)) { sum += z; count++; }   // keep only valid readings
    delay(3);                        // about 3 ms between samples (about 3 s in total)
  }
  gyroBias = (count > 0) ? sum / count : 0;   // average = the offset to subtract later (0 if nothing valid was read)
}

float updateGyro() {                 // read the gyro, update yawRate and yaw, return the time step dt
  unsigned long nowUs = micros();    // current time in microseconds
  float dt = (nowUs - prevMicros) / 1000000.0;   // time since the last call, in seconds
  prevMicros = nowUs;                // remember this time for the next call
  float z = readGyroZ();             // current turn rate in deg/s
  if (!isnan(z)) {                   // only if the reading is valid
    yawRate = (z - gyroBias) * YAW_SCALE;   // remove the offset, apply the correction factor
    yaw += yawRate * dt;             // integrate: angle = angle + speed * time
  }
  return dt;                         // give the time step back to the caller
}

// ---------------- Odometry: encoder distance + gyro heading -> drive axle on the map ----------------
void updateOdometry(float yawBefore) {   // yawBefore = heading at the start of this loop step
  long t = readTicks();              // current encoder count
  long dTicks = t - prevTicks;       // new pulses since the last update
  prevTicks = t;                     // remember the count for the next update
  if (dTicks <= 0) return;           // no movement (or backwards, ignored): nothing to add
  float ds = dTicks / TICKS_PER_CM;  // distance driven in cm
  float h = radians((yawBefore + yaw) * 0.5);   // average heading during this step (in radians)
  carX += ds * cos(h);               // move the drive axle along the average heading (x part)
  carY += ds * sin(h);               // (y part)
  traveledCm += ds;                  // total distance driven
}

// Pillar position seen from the CAMERA: forward (+ ahead) and left (+ left) in cm
void relativeToCar(float px, float py, float &fwd, float &left) {  // express a map point in the car frame: forward and left distances from the camera
  float h = radians(yaw);            // car heading in radians
  float dx = px - camPosX(), dy = py - camPosY();   // vector from the camera to the pillar (map coordinates)
  fwd  =  dx * cos(h) + dy * sin(h);   // project the vector on the car's forward direction
  left = -dx * sin(h) + dy * cos(h);   // project it on the car's left direction
}

// ---------------- Steering ----------------
float maxLeftSteer()  { return STEER_LEFT_IS_BIGGER ? (LEFT_LIMIT - CENTER_ANGLE) : (CENTER_ANGLE - RIGHT_LIMIT); }   // how many degrees the wheels can turn left
float maxRightSteer() { return STEER_LEFT_IS_BIGGER ? (CENTER_ANGLE - RIGHT_LIMIT) : (LEFT_LIMIT - CENTER_ANGLE); }   // how many degrees the wheels can turn right

// steerLeft: + = left, - = right (servo degrees away from center)
void setSteering(float steerLeft, float dt) {   // dt = time since the last call, used to limit the servo speed
  steerLeft = constrain(steerLeft, -maxRightSteer(), maxLeftSteer());   // keep the request inside the mechanical range
  float target = CENTER_ANGLE + (STEER_LEFT_IS_BIGGER ? steerLeft : -steerLeft);   // convert "left/right" into a servo angle
  target = constrain(target, RIGHT_LIMIT, LEFT_LIMIT);   // never send an angle beyond the servo limits
  float maxStep = SERVO_SPEED_DPS * dt;   // largest move allowed in this time step (speed limit)
  if (target > servoAngle + maxStep)      servoAngle += maxStep;   // move up, but only by the allowed step
  else if (target < servoAngle - maxStep) servoAngle -= maxStep;   // move down, but only by the allowed step
  else                                    servoAngle = target;     // close enough: go straight to the target
  steering.write((int)(servoAngle + 0.5));   // send the angle to the servo (rounded to a whole degree)
}

// ---------------- Camera messages ----------------
char lineBuf[64];                    // buffer that collects one incoming text line from the camera
uint8_t lineLen = 0;                 // how many characters are in lineBuf now

void readCamera() {                  // read all waiting bytes from the camera and decode complete lines
  while (Serial.available()) {       // while there are bytes waiting in the UART buffer
    char c = Serial.read();          // take one character
    if (c == '$') { lineLen = 0; continue; }   // '$' starts a new message: throw away anything half-received
    if (c == '\n') {                 // end of line: the message is complete
      lineBuf[lineLen] = '\0';       // close the text so sscanf can read it as a string
      int col, x, b, ok, bf, br, w, d, lat;   // variables for the fields we need (x, b, ok, bf, br, w are read but not used)
      unsigned long seq;             // message counter (read, not used here)
      // the camera sends 14 fields; we read the first 10 (same as before)
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,  // read the message fields into the variables (first part of the call)
                     &w, &d, &lat);  // parse the comma-separated numbers; n = how many were read
      if (n == 10) {                 // only accept the message if all 10 fields were read
        camColor = col; camDistCm = d; camLatCm = lat;   // store color, distance ahead and side
        lastCamMs = millis();        // remember when the last valid message arrived (for the timeout)
        newCamData = true;           // tell loop() that there is a new message to use
      }
      lineLen = 0;                   // empty the buffer for the next line
      continue;                      // go on with the next character
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;   // store the character (ignore it if the buffer is full)
  }
}

// Put the pillar of the last message on the map (from the camera point)
void onCameraMessage() {  // store the pillar of the last camera message on the map
  if (camColor == 0 || camDistCm <= 0) return;     // no pillar in this message (color 0 or no distance): nothing to do
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return; // pillar too far to the side: outside the corridor, ignore it

  float fwd  = camDistCm + PILLAR_CM / 2;        // camera measures to the front face -> add half the width to get the center
  float left = -camLatCm;                        // camera: + = right; map: + = left (so the sign is flipped)
  float h = radians(yaw);                        // car heading in radians
  float cx = camPosX(), cy = camPosY();          // camera position on the map
  float px = cx + fwd * cos(h) - left * sin(h);  // pillar x on the map: camera + forward part - left part (rotated by the heading)
  float py = cy + fwd * sin(h) + left * cos(h);  // pillar y on the map (same rotation)

  // Same pillar as one we already have? -> refine its position
  MapPillar *slots[2] = { &current, &nextOne };  // check both stored pillars
  for (int i = 0; i < 2; i++) {                  // loop over the 2 slots
    MapPillar *p = slots[i];                     // the slot being checked
    if (p->used && p->color == camColor && hypot(p->x - px, p->y - py) < SAME_PILLAR_CM) {   // same color and close enough = same pillar
      p->x += BLEND * (px - p->x);               // move the stored x a little toward the new estimate (smoothing)
      p->y += BLEND * (py - p->y);               // same for y
      p->hits++;                                 // count one more confirmation
      return;                                    // done: it was an update, not a new pillar
    }
  }
  // A new pillar
  MapPillar fresh = {true, px, py, camColor, 1}; // a new pillar with 1 hit
  if (!current.used) {                           // no current pillar yet: this one becomes the current one
    current = fresh;  // the new pillar becomes the current one
    if (DEBUG_PRINTS) { Serial.print(F(">> NEW PILLAR ")); Serial.println(camColor == 1 ? F("RED") : F("GREEN")); }   // print "NEW PILLAR RED/GREEN"
  } else if (px > current.x + SAME_PILLAR_CM) {  // a different pillar farther ahead: keep it for later
    nextOne = fresh;                             // store it as the next pillar
  }
}

// ---------------- The control: where to go -> which heading ----------------
// Wanted lateral position (camera point) next to one pillar (never pulls the car back toward the pillar)
float goalFor(int color, float pillarY) {        // color: 1 red, 2 green; pillarY = the pillar's y on the map
  float cy = camPosY();                          // current lateral position of the camera
  if (color == 1) return min(cy, pillarY - TARGET_CM);   // red: car to its right (stay at least TARGET_CM to the right of the pillar, never move left of where we are)
  return max(cy, pillarY + TARGET_CM);                    // green: car to its left (stay at least TARGET_CM to the left, never move right of where we are)
}

float headingGoal() {                            // returns the heading (degrees) the car should aim for
  float fwd = 0, left = 0;                       // position of the current pillar relative to the camera

  // 1) Current pillar far enough behind the camera? -> cleared, the next one becomes current
  if (current.used) {                            // only if a pillar is stored
    relativeToCar(current.x, current.y, fwd, left);   // where is it relative to the camera?
    if (fwd < -RETURN_AFTER_CM) {                // the camera is well past its center
      if (DEBUG_PRINTS) Serial.println(F(">> PILLAR CLEARED"));  // debug: report that the car has passed the current pillar
      current = nextOne;                         // the next pillar (maybe unused) becomes the current one
      nextOne.used = false;                      // the next slot is empty again
      if (current.used) relativeToCar(current.x, current.y, fwd, left);   // recompute the position for the new current pillar
    }
  }

  // 2) Which pillar do we aim at?
  //    camera still before the current pillar -> that one
  //    camera already past its center (beside it) -> the next one (or the home lane)
  bool beside = current.used && fwd < 0;         // true when the camera has already passed the pillar's center
  const MapPillar *target = current.used ? (beside ? (nextOne.used ? &nextOne : NULL) : &current) : NULL;   // choose the target pillar (or NULL = none)

  goalY = 0;                                     // home lane: lateral goal 0 (the start line)
  float look = LOOK_HOME_CM;                     // distance used by atan (how far ahead to aim)
  if (target != NULL) {                          // a pillar is the target
    goalY = goalFor(target->color, target->y);   // lateral goal next to that pillar
    float toFace = (target->x - PILLAR_CM / 2) - camPosX() - ARRIVE_BEFORE_CM;   // distance until we must be at the safe side (before its front face)
    look = max(LOOK_MIN_CM, toFace);             // do not let it go below LOOK_MIN_CM (avoids a huge angle)
  }

  // 3) Beside a pillar: never move sideways TOWARD it (straightening or moving away is fine)
  if (beside) {                                  // the camera is already next to the pillar
    if (current.color == 1) goalY = min(goalY, camPosY());   // red is on our left: no moving left
    else                    goalY = max(goalY, camPosY());   // green is on our right: no moving right
  }

  // 4) Heading that leads there
  float h = degrees(atan2(goalY - camPosY(), look));   // angle to the goal point: sideways error / distance ahead
  return constrain(h, -MAX_AIM_DEG, MAX_AIM_DEG);      // limit the angle to MAX_AIM_DEG
}

// ---------------- SETUP ----------------
void setup() {                                   // runs once at power-on
  Serial.begin(CAMERA_BAUD);                     // RX = camera, TX = Serial Monitor (same UART used for both)

  pinMode(PWMA_PIN, OUTPUT);                     // motor speed pin is an output
  pinMode(AIN1_PIN, OUTPUT);                     // motor direction pin 1 is an output
  pinMode(AIN2_PIN, OUTPUT);                     // motor direction pin 2 is an output
  motorStop();                                   // make sure the motor is stopped at start

  pinMode(BUTTON_PIN, INPUT_PULLUP);             // START button: input with internal pull-up (idle HIGH)
  pinMode(ENCODER_PIN, INPUT_PULLUP);            // encoder input with pull-up
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoder, RISING);   // call onEncoder() on every rising edge of the encoder

  Wire.begin();                                  // start the I2C bus (gyro)
  Wire.setClock(400000);                         // fast I2C: 400 kHz
  Wire.setWireTimeout(25000, true);              // if the bus hangs for 25 ms, reset it instead of freezing the car
  if (!startGyro()) {                            // set up the gyro; if it is not found...
    Serial.println(F("ERROR: BMI160 not found"));  // debug: tell the user the gyroscope is missing, then stop forever
    while (true) delay(1000);                    // ...stop here forever and keep printing nothing (the car does not drive)
  }

  steering.attach(SERVO_PIN, 500, 2500);         // attach the servo on D9 with pulse width 500-2500 microseconds
  steering.write(CENTER_ANGLE);                  // put the wheels straight

  Serial.println(F("STRAIGHT WITH PILLARS (medidas 19-21 nov) - press START"));  // print the start message
  while (digitalRead(BUTTON_PIN) == HIGH) {      // waiting until START is pressed (LOW = pressed); also shows if the camera talks
    readCamera();                                // keep reading the camera so lastCamMs is updated
    static unsigned long t = 0;                  // time of the last status print (static: keeps its value between calls)
    if (millis() - t > 1000) {                   // once per second
      t = millis();  // remember the time of this print (once per second)
      Serial.print(F("camera: "));  // print the label first, then OK or the warning
      Serial.println(millis() - lastCamMs < 500 ? F("OK") : F("no messages (camera on, RX wired to D0?)"));   // OK if a message arrived in the last 500 ms
    }
  }
  Serial.println(F("Calibrating gyro... do not move the car"));  // tell the user to keep the car still during calibration
  calibrateGyro();                               // measure the gyro zero offset (car must stand still)
  yaw = 0;                                       // the heading starts at zero
  carX = carY = 0;                               // the map starts at the origin
  traveledCm = 0;                                // no distance driven yet
  prevTicks = readTicks();                       // start counting encoder pulses from now
  prevMicros = micros();                         // start the gyro time from now
  lastCamMs = millis();                          // avoid an immediate camera timeout
  newCamData = false;                            // no camera message waiting yet
}

// ---------------- LOOP ----------------
void loop() {  // runs again and again: one control step per call
  float yawBefore = yaw;                         // heading before this step (needed for the odometry average)
  float dt = updateGyro();                       // read the gyro, update yaw; dt = time step
  updateOdometry(yawBefore);                     // move the car's map position using the encoder and the heading
  readCamera();                                  // read any new camera messages

  if (finished) { motorStop(); setSteering(0, dt); return; }   // run is over: stop the motor, keep the wheels straight, do nothing else

  if (traveledCm >= RUN_CM) {                    // the run distance was reached
    finished = true;                             // remember that the run is over
    motorStop();                                 // stop the motor
    Serial.print(F(">> DONE | x ")); Serial.print(carX, 1);     // print the final map position...
    Serial.print(F(" | y ")); Serial.print(carY, 1);  // print the y position with one decimal
    Serial.print(F(" | yaw ")); Serial.println(yaw, 1);        // ...and the final heading
    return;  // end this loop step after the final print
  }

  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {   // no camera message for too long
    motorStop();                                 // do not drive blind: stop the motor
    setSteering(0, dt);                          // keep the wheels straight
    static unsigned long t = 0;                  // time of the last warning (static: keeps its value)
    if (millis() - t > 500) { t = millis(); Serial.println(F("WAITING FOR CAMERA")); }   // print the warning at most every 500 ms
    return;                                      // skip the rest of this loop
  }

  if (newCamData) { newCamData = false; onCameraMessage(); }   // if a new message arrived, put its pillar on the map

  goalHeading = headingGoal();                   // compute the heading we want right now
  float steer = KP_HEADING * (goalHeading - yaw) - KD_HEADING * yawRate;   // PD controller: proportional to the heading error, minus the damping from the turn speed
  setSteering(constrain(steer, -MAX_STEER, MAX_STEER), dt);   // limit the steering request and send it to the servo
  motorForward(DRIVE_SPEED);                     // keep driving forward

  static unsigned long lastPrint = 0;            // time of the last debug print (static: keeps its value)
  if (DEBUG_PRINTS && millis() - lastPrint >= 150) {   // every 150 ms when debugging is on
    lastPrint = millis();  // remember the time of this print (150 ms interval)
    Serial.print(F("eje x ")); Serial.print(carX, 0);    // drive axle position x, no decimals
    Serial.print(F(" y ")); Serial.print(carY, 1);       // y with one decimal
    Serial.print(F(" | yaw ")); Serial.print(yaw, 1);    // heading
    Serial.print(F(" goal ")); Serial.print(goalHeading, 1);   // heading we want
    Serial.print(F(" | goalY ")); Serial.print(goalY, 1);     // lateral goal
    Serial.print(F(" | pillar "));  // start of the pillar part of the debug line
    if (current.used) {                          // print the current pillar
      Serial.print(current.color == 1 ? F("RED (") : F("GREEN ("));  // print the colour of the current pillar and an opening bracket
      Serial.print(current.x, 0); Serial.print(F(",")); Serial.print(current.y, 0); Serial.print(F(")"));  // print the map position (x, y) of the current pillar, then the bracket
    } else Serial.print(F("-"));                 // "-" = no pillar stored
    Serial.print(F(" | cam ")); Serial.print(camColor); Serial.print(F(" ")); Serial.print(camDistCm);   // last camera color and distance
    Serial.print(F("/")); Serial.println(camLatCm);   // and its side value, then end the line
  }
}
