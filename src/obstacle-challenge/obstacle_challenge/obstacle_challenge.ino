// =====================================================================
// OPEN CHALLENGE + ESQUIVAR PILARES + CENTRADO ENTRE PAREDES (v1)
// Combina:
//   - open_challenge_linea.ino: vueltas por la PRIMERA LINEA de la camara
//     (azul = izquierda, naranja = derecha), giro de 90 con gyro, centrado
//     con los dos ToF. Tras cada linea avanza hasta que el ToF frontal marque
//     FRONT_TURN_CM y luego gira EN REVERSA. Despues del giro retrocede hasta
//     que el ToF trasero marque REAR_STOP_CM.
//     Tras una linea ignora otras IGNORE_LINE_S.
//     Tras la ultima linea avanza FINAL_EXTRA_S y para.
//   - straight_pillars (mapa de pilares): si la camara ve un bloque rojo o
//     verde en la recta, lo esquiva (rojo por la derecha, verde por la izquierda).
//     Cuando lo ha pasado (PASS_CM), vuelve a centrarse entre las paredes.
//
// Prioridad en cada tramo recto:
//   1) BLOQUE: hay un pilar en el mapa -> rumbo para esquivarlo.
//   2) PASO: acaba de pasar el pilar -> mantiene el rumbo PASS_CM cm.
//   3) CENTRADO: sin pilares -> ambos ToF a la misma distancia.
// Las lineas de la camara siguen siendo las esquinas en todo momento.
//
// Velocidad: RUN_PWM = 70 en las rectas, TURN_PWM = 70 en los giros (en reversa),
// REVERSE_PWM = 70 al retroceder. Si con 70 no gira bien, sube TURN_PWM.
//
// Mapa de pilares: se reinicia tras cada retroceso (cada tramo recto tiene su
// propio sistema de coordenadas, con x hacia delante y y hacia la izquierda).
//
// Camara: TX -> RX (D0) del Nano, 115200 baud. Mensajes con '$'.
// Monitor serie: g + Enter = empezar, s + Enter = parada de emergencia.
// Placa: Arduino Nano | TB6612: PWMA D6, AIN1 D7, AIN2 D8 | Encoder C1 D3, C2 D2
// Servo D9 | START D10 | LED D13 | BMI160 0x69 | ToF izq A3 -> 0x2A | ToF der A2 -> 0x2B
// ToF trasero A0 -> 0x2C | ToF frontal A1 -> 0x2D
// Librerias: VL53L1X (Pololu), Servo
// =====================================================================

#include <Wire.h>
#include <VL53L1X.h>
#include <Servo.h>

// #####################################################################
// ##  1. AJUSTES DE CARRERA                                            ##
// #####################################################################
const int   LAPS      = 3;            // 1 = una vuelta (4 esquinas), 3 = Open completo (12)
const int   RUN_PWM   = 75;           // velocidad en las rectas (0-255)
const int   TURN_PWM  = 85;           // velocidad al girar (el giro se hace en reversa)
const int   REVERSE_PWM = 75;         // velocidad al retroceder tras cada giro
const float FRONT_TURN_CM = 50.0;     // tras la linea avanza hasta que el ToF frontal marque esto (cm) o menos
const unsigned long MAX_PRE_TURN_MS = 4000;  // seguridad: si el frontal nunca llega a FRONT_TURN_CM, parar
const float REAR_STOP_CM = 40.0;      // retrocede hasta que el ToF trasero marque esto (cm) o menos
const float REVERSE_CM  = 30.0;       // respaldo: si el ToF trasero no responde, retrocede esta distancia (cm)
const unsigned long MAX_REVERSE_MS = 3000;   // seguridad: si no termina de retroceder en este tiempo, parar
const float STOP_OFFSET_CM = 0;       // (ya no se usa para parar; se conserva)
const bool  ONLY_LOCKED_COLOR = true; // true: solo cuenta lineas del color de la primera
const float LINE_WAIT_S = 0.75;       // respaldo: si NO hay ToF frontal, avanza este tiempo antes de girar
const float FINAL_EXTRA_S = 0.45;     // tras la ULTIMA linea: avanza este tiempo y para

// #####################################################################
// ##  2. PILARES                                                       ##
// #####################################################################
const float TARGET_CM        = 17.0;  // distancia centro pilar <-> centro coche al pasarlo
const float MAX_AIM_DEG      = 60.0;  // angulo maximo para esquivar
const float LOOK_MIN_CM      = 12.0;  // distancia de mira minima
const float ARRIVE_BEFORE_CM = 55.0;  // estar ya en el lado seguro esta distancia antes del pilar
const float RETURN_AFTER_CM  = 10.0;  // pilar "pasado" cuando el coche lo supera en esta distancia
const float PASS_CM          = 25.0;  // tras perder el pilar, mantener el rumbo esta distancia (> RETURN_AFTER)
const float SAME_PILLAR_CM   = 30.0;  // deteccion a menos de esto de un pilar guardado = mismo pilar
const float BLEND            = 0.3;   // cuanto corrige cada mensaje la posicion guardada
const float MAX_SIDE_CM      = 60.0;  // pilares mas laterales que esto = ruido
const float PILLAR_CM        = 5.0;   // ancho del pilar
const float CAM_FWD_CM       = 11.7;  // eje trasero (encoder) -> lente de la camara

// #####################################################################
// ##  3. AJUSTES FINOS                                                 ##
// #####################################################################
const float KP_HEAD = 2.0;            // grados de servo por grado de error de rumbo
const float KD_HEAD = 0.03;           // amortiguacion con la velocidad de giro
const float KI_HEAD = 4.0;            // aprende el angulo recto real del servo (solo en el centrado)
const float I_MAX   = 25;
const float MAX_STEER = 70.0;         // comando maximo de direccion (grados de servo)
const float K_LAT       = 0.03;       // grados de rumbo por mm de descentrado
const float KI_LAT      = 0.02;
const float LAT_I_MAX   = 8;
const float LAT_MAX_DEG = 12;
const int   SIDE_MAX_MM = 1200;       // lectura lateral mayor = no hay pared usable
const int   SAFE_WALL_MM = 70;        // mas cerca que esto de una pared: alejarse a tope
const float TURN_PREDICT_S = 0.12;    // termina el giro antes. Si pasa de 90, subelo
const float IGNORE_LINE_S = 4.0;      // tras detectar una linea, ignora TODAS las lineas durante este tiempo (s)
const float MIN_DRIVE_CM = 10;        // no detecta lineas en los primeros cm tras START
const float FALLBACK_CM = 50;         // (ya no se usa; se conserva)
const float MAX_FINAL_CM = 250;       // seguridad: si no aparece la ultima linea tras el ultimo giro, parar
const unsigned long MAX_TURN_MS = 4000;
const unsigned long MAX_STRAIGHT_MS = 8000;   // sin esquina en 8 s -> parar
const unsigned long STALL_MS = 500;
const unsigned long CAMERA_TIMEOUT_MS = 500;  // sin camara -> no avanzar

// #####################################################################
// ##  4. CONSTANTES DEL COCHE (igual que open_challenge)              ##
// #####################################################################
const bool  MOTOR_REVERSED   = true;
const bool  ENCODER_REVERSED = true;
const int   YAW_SIGN         = 1;     // +1: girar a la izquierda sube el yaw
const float GYRO_SCALE       = 1.007;
const int   STEER_CENTER = 85, STEER_LEFT_MAX = 10, STEER_RIGHT_MAX = 170;   // izquierda = angulo menor
const int   RAMP_STEP = 5;
const float TICKS_PER_CM = 9.27;
const uint8_t BMI = 0x69;
const float GYRO_LSB_PER_DPS = 65.6;
const uint32_t TOF_BUDGET_US = 50000;
const int PWMA_PIN = 6, AIN1_PIN = 7, AIN2_PIN = 8;
const int ENC_C1_PIN = 3, ENC_C2_PIN = 2;
const int SERVO_PIN = 9, START_BTN_PIN = 10, LED_PIN = 13;
const int XSHUT_LEFT = A3, XSHUT_RIGHT = A2;
const uint8_t ADDR_LEFT = 0x2A, ADDR_RIGHT = 0x2B;
const int XSHUT_FRONT = A1, XSHUT_REAR = A0;
const uint8_t ADDR_REAR = 0x2C, ADDR_FRONT = 0x2D;

// #####################################################################
// ##  5. VARIABLES                                                     ##
// #####################################################################
struct Side {
  VL53L1X dev; int xshut; uint8_t addr; const char *name;
  int mm = -1; bool fresh = false; unsigned long lastData = 0;
};
struct MapPillar { bool used; float x, y; int color; int hits; };   // color 1 rojo, 2 verde
enum State { WAIT, STRAIGHT, PRE_TURN, TURN, REVERSE, DONE };
const char *STATE_NAME[] = { "WAIT", "STRAIGHT", "PRE_TURN", "TURN", "REVERSE", "DONE" };
enum LineLock { LOCK_NONE, LOCK_BLUE, LOCK_ORANGE };
enum Mode { MODE_LANE, MODE_BLOCK, MODE_PASS };
const char *MODE_NAME[] = { "CENTRADO", "PILAR", "PASO" };

Side tofs[2];                         // [0] = IZQUIERDO, [1] = DERECHO
Side rearTof;                         // ToF trasero (para el retroceso)
bool rearOk = false;
Side frontTof;                        // ToF frontal (para decidir cuando girar tras la linea)
bool frontOk = false;
Servo steering;
State state = WAIT;
Mode  mode = MODE_LANE;
volatile long encRaw = 0;
int  pwmNow = 0, pwmTarget = 0, servoNow = STEER_CENTER;
bool tofOk = false, finalPrinted = true;
bool driveBack = false;               // true = motor en sentido de retroceso
float biasDps = 0, yawDeg = 0, rateDps = 0;
float heading = 0;                    // rumbo objetivo del tramo (multiplo de 90)
float turnEndCm = 0;                  // odometria al terminar el ultimo retroceso
float revStartOdo = 0;                // odometria al empezar a retroceder
float d1Cm = -1, lStartCm = -1, stopAtCm = -1;
float iTerm = 0, stallOdo = 0;
float latDeg = 0, latI = 0;           // correccion de centrado (+ = izquierda)
float goalLocal = 0;                  // rumbo deseado relativo al tramo (+ = izquierda)
float passLeftCm = 0;                 // cm que faltan para terminar de pasar el pilar
int   dir = 0;                        // +1 = izquierda (azul), -1 = derecha (naranja)
int   corners = 0;
bool  finalLeg = false;
bool  finalLineSeen = false;          // ya se vio la ultima linea: avanzar FINAL_EXTRA_S y parar
unsigned long finalLineMs = 0;        // momento en que se vio la ultima linea
unsigned long lineIgnoreUntil = 0;    // hasta este momento (millis) se ignoran las lineas
LineLock lineLock = LOCK_NONE;
bool  sameLinePrev = false;
int   camBlue = 0, camOrange = 0;     // lineas: lo que ve la camara ahora
int   camColor = 0, camDistCm = -1, camLatCm = 0;   // pilar en el ultimo mensaje
bool  newCamData = false;
unsigned long lastCamMs = 0;
char  camBuf[64]; uint8_t camLen = 0; bool inCam = false;
unsigned long lastMicros = 0, lastRamp = 0, lastPrint = 0, phaseStart = 0, doneAt = 0;
unsigned long lastCtrl = 0, stallSince = 0, runStart = 0;
char  cmd = 0;

// mapa del tramo actual
float carX = 0, carY = 0;             // eje trasero en el tramo (cm)
float lastMapOdo = 0;                 // odometria la ultima vez que se actualizo el mapa
float segCm = 0, lastSegCm = 0;       // distancia recorrida en este tramo
MapPillar current = {false, 0, 0, 0, 0};
MapPillar nextOne = {false, 0, 0, 0, 0};

// ---------------- motor / encoder / servo ----------------
void encoderISR() { if (digitalRead(ENC_C2_PIN)) encRaw++; else encRaw--; }
float odoCm() { noInterrupts(); long c = encRaw; interrupts(); return (ENCODER_REVERSED ? -c : c) / TICKS_PER_CM; }
void motorOut(int pwm) {
  pwm = constrain(pwm, 0, 255);
  bool a = pwm > 0, b = false;
  if (MOTOR_REVERSED) { b = a; a = false; }
  if (driveBack) { bool t = a; a = b; b = t; }     // invertir sentido para retroceder
  digitalWrite(AIN1_PIN, a ? HIGH : LOW); digitalWrite(AIN2_PIN, b ? HIGH : LOW);
  analogWrite(PWMA_PIN, pwm); pwmNow = pwm;
}
void brake() { digitalWrite(AIN1_PIN, HIGH); digitalWrite(AIN2_PIN, HIGH); analogWrite(PWMA_PIN, 255); pwmNow = 0; pwmTarget = 0; }
void servoTo(int a) { a = constrain(a, STEER_LEFT_MAX, STEER_RIGHT_MAX); if (a != servoNow) { steering.write(a); servoNow = a; } }

// ---------------- BMI160 (gyro) ----------------
bool i2cTimedOut() { if (Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); return true; } return false; }
bool bmiWrite(uint8_t reg, uint8_t val) { Wire.beginTransmission(BMI); Wire.write(reg); Wire.write(val); return Wire.endTransmission() == 0 && !i2cTimedOut(); }
bool bmiRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(BMI); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) { i2cTimedOut(); return false; }
  if (Wire.requestFrom(BMI, n) != n) { i2cTimedOut(); return false; }
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return !i2cTimedOut();
}
bool readGyroZ(int16_t &raw) { uint8_t b[2]; if (!bmiRead(0x10, b, 2)) return false; raw = (int16_t)((b[1] << 8) | b[0]); return true; }
bool bmiStart() {
  for (int i = 0; i < 5; i++) {
    bmiWrite(0x7E, 0xB6); delay(100);
    bmiWrite(0x7E, 0x15); delay(100);
    bmiWrite(0x43, 0x02); delay(50);
    uint8_t id = 0, st = 0;
    if (bmiRead(0x00, &id, 1) && id == 0xD1 && bmiRead(0x03, &st, 1) && ((st >> 2) & 3) == 1) return true;
  }
  return false;
}
void calibrate() {
  long sum = 0; int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 700) { int16_t r; readGyroZ(r); delay(5); }
  for (int i = 0; i < 300; i++) {
    int16_t r;
    if (readGyroZ(r)) { sum += r; n++; }
    delay(5);
    digitalWrite(LED_PIN, (millis() / 80) % 2);
  }
  if (n >= 250) biasDps = (sum / (float)n) / GYRO_LSB_PER_DPS;
  yawDeg = 0; lastMicros = micros();
  Serial.print(F("Gyro bias ")); Serial.print(biasDps, 3); Serial.print(F(" (")); Serial.print(n); Serial.println(F("/300)"));
  digitalWrite(LED_PIN, HIGH);
}
void updateGyro() {
  unsigned long now = micros(); float dt = (now - lastMicros) / 1000000.0; lastMicros = now;
  int16_t raw;
  if (readGyroZ(raw)) { rateDps = YAW_SIGN * GYRO_SCALE * (raw / GYRO_LSB_PER_DPS - biasDps); yawDeg += rateDps * dt; }
}

// ---------------- ToF laterales, trasero y frontal ----------------
bool startOne(Side &t) {
  pinMode(t.xshut, INPUT); delay(10);
  t.dev.setAddress(0x29); t.dev.setTimeout(200);
  if (!t.dev.init()) { i2cTimedOut(); return false; }
  t.dev.setAddress(t.addr);
  t.dev.setDistanceMode(VL53L1X::Long);
  t.dev.setMeasurementTimingBudget(TOF_BUDGET_US);
  t.dev.setROISize(16, 8);
  t.dev.startContinuous(TOF_BUDGET_US / 1000);
  t.lastData = millis();
  return true;
}
void tofExtraOff() {
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_REAR,  OUTPUT); digitalWrite(XSHUT_REAR,  LOW);
}
bool tofStart() {
  tofExtraOff();                      // trasero y frontal apagados mientras se configuran izq y der
  for (int i = 0; i < 2; i++) { pinMode(tofs[i].xshut, OUTPUT); digitalWrite(tofs[i].xshut, LOW); }
  delay(10);
  bool a = startOne(tofs[0]); bool b = startOne(tofs[1]);
  rearOk  = startOne(rearTof);        // se enciende despues de izq y der
  frontOk = startOne(frontTof);       // despues del trasero
  if (!rearOk)  { i2cTimedOut(); }
  if (!frontOk) { i2cTimedOut(); }
  return a && b;
}
void updateTof() {
  bool restart = false;
  for (int i = 0; i < 2; i++) {
    Side &t = tofs[i]; t.fresh = false;
    if (tofOk && t.dev.dataReady()) {
      t.dev.read(false);
      if (!i2cTimedOut()) {
        uint16_t v = t.dev.ranging_data.range_mm;
        t.mm = (v > 0 && v < 4000) ? v : 4000;
        t.fresh = true; t.lastData = millis();
      }
    }
    if (millis() - t.lastData > 500) restart = true;
  }
  if (rearOk && rearTof.dev.dataReady()) {
    rearTof.dev.read(false);
    if (!i2cTimedOut()) {
      uint16_t v = rearTof.dev.ranging_data.range_mm;
      rearTof.mm = (v > 0 && v < 4000) ? v : 4000;
      rearTof.lastData = millis();
    }
  }
  if (frontOk && frontTof.dev.dataReady()) {
    frontTof.dev.read(false);
    if (!i2cTimedOut()) {
      uint16_t v = frontTof.dev.ranging_data.range_mm;
      frontTof.mm = (v > 0 && v < 4000) ? v : 4000;
      frontTof.lastData = millis();
    }
  }
  if (restart) {
    tofOk = tofStart();
    Serial.print(F("!!! TOF RESTART -> ")); Serial.println(tofOk ? F("OK") : F("FAILED"));
    tofs[0].lastData = tofs[1].lastData = millis();
  }
}

// ---------------- camara ----------------
void handleCameraLine() {
  camBuf[camLen] = '\0';
  int col, x, b, ok, bf, br, w, d, lat, of, orow;
  unsigned long seq;
  int n = sscanf(camBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d", &col, &x, &b, &ok, &bf, &br, &seq,
                 &w, &d, &lat, &of, &orow);
  if (n == 12) {
    camBlue = bf; camOrange = of;          // lineas (esquinas)
    camColor = col; camDistCm = d; camLatCm = lat;   // pilar
    lastCamMs = millis();
    newCamData = true;
  }
}
void readSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { inCam = true; camLen = 0; continue; }
    if (inCam) {
      if (c == '\n') { handleCameraLine(); inCam = false; camLen = 0; }
      else if (c != '\r' && camLen < sizeof(camBuf) - 1) camBuf[camLen++] = c;
      continue;
    }
    if (c == '\n' || c == '\r') {
      if ((cmd == 's' || cmd == 'S') && (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE)) finish(F("STOPPED (s)"));
      else if ((cmd == 'g' || cmd == 'G') && (state == WAIT || state == DONE)) startRun();
      cmd = 0;
    } else cmd = c;
  }
}
bool sameColorVisible() {
  if (lineLock == LOCK_BLUE)   return camBlue != 0;
  if (lineLock == LOCK_ORANGE) return camOrange != 0;
  return false;
}
bool lineVisibleForCount() {
  if (ONLY_LOCKED_COLOR) return sameColorVisible();
  return camBlue != 0 || camOrange != 0;
}

// ---------------- rumbo local y mapa del tramo ----------------
float yawL() { return yawDeg - heading; }     // desviacion respecto al rumbo del tramo (+ = izquierda)
float camPosX() { return carX + CAM_FWD_CM * cos(radians(yawL())); }
float camPosY() { return carY + CAM_FWD_CM * sin(radians(yawL())); }

void updateMap() {
  float o = odoCm();
  float ds = o - lastMapOdo; lastMapOdo = o;
  float h = radians(yawL());
  carX += ds * cos(h);
  carY += ds * sin(h);
  segCm += ds;
}

void relativeToCar(float px, float py, float &fwd, float &left) {
  float h = radians(yawL());
  float dx = px - camPosX(), dy = py - camPosY();
  fwd  =  dx * cos(h) + dy * sin(h);
  left = -dx * sin(h) + dy * cos(h);
}

// Coloca el pilar del ultimo mensaje en el mapa del tramo
void onCameraPillar() {
  if (camColor == 0 || camDistCm <= 0) return;
  if (fabs((float)camLatCm) > MAX_SIDE_CM) return;

  float fwd  = camDistCm + PILLAR_CM / 2;        // hasta el centro del pilar
  float left = -camLatCm;                        // camara: + = derecha; mapa: + = izquierda
  float h = radians(yawL());
  float cx = camPosX(), cy = camPosY();
  float px = cx + fwd * cos(h) - left * sin(h);
  float py = cy + fwd * sin(h) + left * cos(h);

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
  MapPillar fresh = {true, px, py, camColor, 1};
  if (!current.used) {
    current = fresh;
    Serial.print(F(">> PILAR NUEVO ")); Serial.println(camColor == 1 ? F("ROJO") : F("VERDE"));
  } else if (px > current.x + SAME_PILLAR_CM) {
    nextOne = fresh;
  }
}

// Lado por el que hay que pasar el pilar: rojo por la derecha, verde por la izquierda
float goalFor(int color, float pillarY) {
  float cy = camPosY();
  if (color == 1) return min(cy, pillarY - TARGET_CM);
  return max(cy, pillarY + TARGET_CM);
}

// Devuelve true y el rumbo local si hay que esquivar un pilar ahora
bool pillarGoal(float &goalOut) {
  float fwd = 0, left = 0;
  if (current.used) {
    relativeToCar(current.x, current.y, fwd, left);
    if (fwd < -RETURN_AFTER_CM) {
      Serial.println(F(">> PILAR PASADO"));
      current = nextOne;
      nextOne.used = false;
      if (current.used) relativeToCar(current.x, current.y, fwd, left);
    }
  }
  if (!current.used) return false;

  bool beside = fwd < 0;                          // ya estoy a la altura del pilar
  const MapPillar *target = beside ? (nextOne.used ? &nextOne : NULL) : &current;
  if (target == NULL) return false;

  float goalY = goalFor(target->color, target->y);
  float toFace = (target->x - PILLAR_CM / 2) - camPosX() - ARRIVE_BEFORE_CM;
  float look = max(LOOK_MIN_CM, toFace);

  if (beside) {                                   // a la altura del pilar: nunca volver hacia el
    if (current.color == 1) goalY = min(goalY, camPosY());
    else                    goalY = max(goalY, camPosY());
  }
  goalOut = constrain(degrees(atan2(goalY - camPosY(), look)), -MAX_AIM_DEG, MAX_AIM_DEG);
  return true;
}

// Centrado entre paredes: devuelve la correccion de rumbo (+ = izquierda)
float laneGoal(float dt) {
  bool okL = tofs[0].mm > 0 && tofs[0].mm < SIDE_MAX_MM;
  bool okR = tofs[1].mm > 0 && tofs[1].mm < SIDE_MAX_MM;
  if (okL && okR) {
    float latErrMm = (tofs[0].mm - tofs[1].mm) / 2.0;   // + = estoy mas cerca del derecho -> ir a la izquierda
    latI = constrain(latI + KI_LAT * latErrMm * dt, -LAT_I_MAX, LAT_I_MAX);
    latDeg = constrain(K_LAT * latErrMm + latI, -LAT_MAX_DEG, LAT_MAX_DEG);
  } else {
    latDeg = latI;
  }
  if (tofs[0].mm > 0 && tofs[0].mm < SAFE_WALL_MM) latDeg = -LAT_MAX_DEG;
  if (tofs[1].mm > 0 && tofs[1].mm < SAFE_WALL_MM) latDeg =  LAT_MAX_DEG;
  return latDeg;
}

// ---------------- esquina = linea ----------------
void onCorner(float odo) {
  lineIgnoreUntil = millis() + IGNORE_LINE_S * 1000.0;   // ignorar todas las lineas durante IGNORE_LINE_S
  int n = corners + 1;
  Serial.print(F(">>> ESQUINA ")); Serial.print(n);
  Serial.print(F(" (linea) a ")); Serial.print(odo, 1); Serial.println(F(" cm"));
  if (n == 1) { d1Cm = odo; Serial.print(F(">>> D1 = ")); Serial.print(d1Cm, 1); Serial.println(F(" cm")); }
  if (n == 5 && lStartCm < 0) {
    lStartCm = odo - turnEndCm;
    Serial.print(F(">>> LSTART = ")); Serial.print(lStartCm, 1); Serial.println(F(" cm (fin giro 4 -> esquina 5)"));
  }
  if (finalLeg) {                                 // ultima linea: seguir FINAL_EXTRA_S y parar (en runTest)
    finalLineSeen = true;
    finalLineMs = millis();
    Serial.println(F(">>> ULTIMA LINEA detectada: avanzo FINAL_EXTRA_S y paro"));
    return;
  }
  state = PRE_TURN; phaseStart = millis(); pwmTarget = RUN_PWM;   // avanza hasta FRONT_TURN_CM y luego gira
}

// ---------------- parada ----------------
void finish(const __FlashStringHelper *why) {
  brake(); servoTo(STEER_CENTER); state = DONE; doneAt = millis(); finalPrinted = false;
  Serial.print(F(">>> ")); Serial.print(why);
  Serial.print(F(" | yaw ")); Serial.print(yawDeg, 1);
  Serial.print(F(" | odo ")); Serial.print(odoCm(), 1); Serial.println(F(" cm"));
}

void startRun() {
  Serial.println(F("Calibrating gyro: do NOT touch the car..."));
  servoTo(STEER_CENTER); delay(300);
  calibrate();
  noInterrupts(); encRaw = 0; interrupts();
  while (Serial.available()) Serial.read();
  inCam = false; camLen = 0;
  state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
  driveBack = false;
  dir = 0; corners = 0; heading = 0; turnEndCm = 0; revStartOdo = 0;
  d1Cm = -1; lStartCm = -1; stopAtCm = -1; finalLeg = false;
  finalLineSeen = false; finalLineMs = 0; lineIgnoreUntil = 0;
  lineLock = LOCK_NONE; sameLinePrev = false;
  latDeg = 0; latI = 0; iTerm = 0; goalLocal = 0; passLeftCm = 0;
  mode = MODE_LANE;
  carX = carY = 0; segCm = 0; lastSegCm = 0; lastMapOdo = 0;
  current.used = false; nextOne.used = false; newCamData = false;
  stallOdo = 0; stallSince = millis(); runStart = millis();
  Serial.println(F(">>> GO"));
}

// ---------------- logica principal ----------------
void runTest() {
  if (state == STRAIGHT) {
    float dt = (millis() - lastCtrl) / 1000.0; lastCtrl = millis();
    float odo = odoCm();
    bool camOk = (millis() - lastCamMs) <= CAMERA_TIMEOUT_MS;
    pwmTarget = camOk ? RUN_PWM : 0;

    updateMap();
    float dSeg = segCm - lastSegCm; lastSegCm = segCm;

    // 0) ULTIMA LINEA vista hace FINAL_EXTRA_S: parar
    if (finalLineSeen && millis() - finalLineMs >= FINAL_EXTRA_S * 1000.0) {
      finish(F(">>> ULTIMA LINEA + avance -> STOP"));
      return;
    }

    // 1) PRIORIDAD MAXIMA: LINEA = ESQUINA -> girar (si la linea aparece, se gira aunque haya pilar)
    if (camOk && !finalLineSeen) {
      if (lineLock == LOCK_NONE) {
        if (odo >= MIN_DRIVE_CM && (camBlue || camOrange)) {
          lineLock = camOrange ? LOCK_ORANGE : LOCK_BLUE;
          dir = (lineLock == LOCK_BLUE) ? 1 : -1;
          sameLinePrev = true;
          Serial.println(lineLock == LOCK_BLUE ? F(">> PRIMERA LINEA AZUL: giro a la IZQUIERDA")
                                               : F(">> PRIMERA LINEA NARANJA: giro a la DERECHA"));
          onCorner(odo);
          return;
        }
      } else {
        bool now = lineVisibleForCount();
        bool rising = now && !sameLinePrev;
        sameLinePrev = now;                                    // se actualiza siempre, aunque se ignore
        if (rising && millis() >= lineIgnoreUntil) { onCorner(odo); return; }
      }
    }

    // 2) SEGUNDA PRIORIDAD: PILARES. Si hay que esquivar, mando; si acaba de pasar, mantengo el rumbo; si no, centrado
    if (newCamData) { newCamData = false; if (camOk) onCameraPillar(); }
    float g = 0;
    if (pillarGoal(g)) {
      goalLocal = g; passLeftCm = PASS_CM; mode = MODE_BLOCK;
    } else if (passLeftCm > 0) {
      passLeftCm -= dSeg; mode = MODE_PASS;           // goalLocal se queda igual
    } else {
      goalLocal = laneGoal(dt); mode = MODE_LANE;
    }

    float err = goalLocal - yawL();
    if (mode == MODE_LANE && pwmNow >= RUN_PWM) iTerm = constrain(iTerm + KI_HEAD * err * dt, -I_MAX, I_MAX);
    float steer = KP_HEAD * err - KD_HEAD * rateDps + iTerm;
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    // 3) TRAS EL ULTIMO GIRO: seguir hasta la ultima linea. Seguridad por distancia.
    if (finalLeg && !finalLineSeen) {
      if (!finalLeg && millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }
    }
    if (millis() - phaseStart > MAX_STRAIGHT_MS) { finish(F("NO CORNER in 8 s -> STOP")); return; }

  } else if (state == PRE_TURN) {
    // linea vista: sigue recto mantenido el rumbo, ignorando pilares y lineas,
    // hasta que el ToF frontal marque FRONT_TURN_CM (o, sin ToF frontal, tras LINE_WAIT_S)
    newCamData = false;
    pwmTarget = RUN_PWM;
    float steer = KP_HEAD * (-yawL()) - KD_HEAD * rateDps;
    servoTo(STEER_CENTER - (int)constrain(steer, -MAX_STEER, MAX_STEER));

    bool frontFresh = frontOk && frontTof.mm > 0 && (millis() - frontTof.lastData < 300);
    bool readyToTurn = frontOk
                       ? (frontFresh && frontTof.mm <= FRONT_TURN_CM * 10)
                       : (millis() - phaseStart >= LINE_WAIT_S * 1000.0);

    if (readyToTurn) {
      // cambio a reversa: frenar, luego girar retrocediendo
      brake();
      driveBack = true;
      state = TURN; phaseStart = millis(); pwmTarget = TURN_PWM;
    } else if (millis() - phaseStart > MAX_PRE_TURN_MS) {
      finish(F("FRONT never reached FRONT_TURN_CM -> STOP"));
    }

  } else if (state == TURN) {
    newCamData = false;                            // no se mapean pilares mientras gira
    // Al retroceder, el volante va al lado contrario: para girar a la izquierda, volante a la derecha
    servoTo(dir > 0 ? STEER_RIGHT_MAX : STEER_LEFT_MAX);
    float predicted = dir * (yawDeg + rateDps * TURN_PREDICT_S - heading);
    if (predicted >= 90) {
      heading += dir * 90.0;
      corners++;
      if (corners >= 4 * LAPS) {
        finalLeg = true;
        stopAtCm = -1;                             // ya no se para por distancia
        Serial.println(F(">>> FINAL: avanzar hasta la ultima linea, luego FINAL_EXTRA_S y parar"));
      }
      // tras el giro: retroceder hasta REAR_STOP_CM en el ToF trasero
      brake();
      servoTo(STEER_CENTER);
      revStartOdo = odoCm();
      driveBack = true;
      state = REVERSE; phaseStart = millis(); pwmTarget = REVERSE_PWM;
      goalLocal = 0; passLeftCm = 0; mode = MODE_LANE;
      current.used = false; nextOne.used = false;
      Serial.print(F(">>> GIRO ")); Serial.print(corners); Serial.print(F(" hecho | yaw ")); Serial.print(yawDeg, 1);
      Serial.print(F(" -> rumbo ")); Serial.println(heading, 0);
    } else if (millis() - phaseStart > MAX_TURN_MS) {
      finish(F("TURN TOO LONG -> STOP"));
    }

  } else if (state == REVERSE) {
    newCamData = false;                            // no se mapean pilares ni se cuentan lineas al retroceder
    servoTo(STEER_CENTER);
    // Parar con el ToF trasero (fresco, de menos de 300 ms); si no hay dato, usar la odometria
    bool rearFresh = rearOk && rearTof.mm > 0 && (millis() - rearTof.lastData < 300);
    bool reached = rearFresh
                   ? (rearTof.mm <= REAR_STOP_CM * 10)
                   : (odoCm() <= revStartOdo - REVERSE_CM);
    if (reached) {
      brake();
      driveBack = false;
      // nuevo tramo: mapa, pilares y cuenta de lineas empiezan aqui
      carX = carY = 0; segCm = 0; lastSegCm = 0; lastMapOdo = odoCm();
      turnEndCm = odoCm();
      goalLocal = 0; mode = MODE_LANE;
      sameLinePrev = lineVisibleForCount();
      state = STRAIGHT; phaseStart = millis(); pwmTarget = RUN_PWM; lastCtrl = millis();
      Serial.println(F(">>> RETROCESO hecho -> avanzo"));
    } else if (millis() - phaseStart > MAX_REVERSE_MS) {
      finish(F("REVERSE TOO LONG -> STOP"));
    }
  }


  if (state == DONE && !finalPrinted && millis() - doneAt > 1000) {
    finalPrinted = true; motorOut(0);
    Serial.print(F(">>> FINAL yaw: ")); Serial.print(yawDeg, 1);
    Serial.print(F(" | rumbo ")); Serial.print(heading, 0);
    Serial.print(F(" | esquinas ")); Serial.print(corners);
    Serial.print(F(" | D1 ")); Serial.print(d1Cm, 1);
    Serial.print(F(" | LSTART ")); Serial.print(lStartCm, 1);
    Serial.print(F(" | tiempo ")); Serial.print((doneAt - runStart) / 1000.0, 1); Serial.println(F(" s"));
  }
}

// ---------------- setup / loop ----------------
void setup() {
  pinMode(PWMA_PIN, OUTPUT); pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT); motorOut(0);
  tofExtraOff();
  pinMode(START_BTN_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_C1_PIN, INPUT_PULLUP); pinMode(ENC_C2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_C1_PIN), encoderISR, RISING);
  steering.attach(SERVO_PIN); steering.write(STEER_CENTER);
  Serial.begin(115200); delay(500);
  Serial.println(F("\nBOOT - OPEN CHALLENGE + PILARES"));
  Wire.begin(); Wire.setClock(100000); Wire.setWireTimeout(25000, true);
  tofs[0].xshut = XSHUT_LEFT;  tofs[0].addr = ADDR_LEFT;  tofs[0].name = "LEFT";
  tofs[1].xshut = XSHUT_RIGHT; tofs[1].addr = ADDR_RIGHT; tofs[1].name = "RIGHT";
  rearTof.xshut = XSHUT_REAR;  rearTof.addr = ADDR_REAR;  rearTof.name = "REAR";
  frontTof.xshut = XSHUT_FRONT; frontTof.addr = ADDR_FRONT; frontTof.name = "FRONT";
  for (int i = 0; i < 5 && !tofOk; i++) { tofOk = tofStart(); if (!tofOk) delay(300); }
  bool g = bmiStart();
  Serial.println(g ? F("BMI160 OK") : F("WARNING: BMI160 not ready (battery ON first?)"));
  Serial.println(tofOk ? F("Both TOF OK") : F("WARNING: a TOF is missing"));
  Serial.println(rearOk ? F("REAR TOF OK") : F("WARNING: REAR TOF missing (reverse by odometry)"));
  Serial.println(frontOk ? F("FRONT TOF OK") : F("WARNING: FRONT TOF missing (turn by time)"));
  calibrate();
  Serial.println(F("Ready. Press START or g + Enter. s + Enter = stop."));
}

void loop() {
  readSerialInput();

  if ((state == WAIT || state == DONE) && digitalRead(START_BTN_PIN) == LOW) {
    delay(30);
    if (digitalRead(START_BTN_PIN) == LOW) {
      while (digitalRead(START_BTN_PIN) == LOW) delay(5);
      delay(300);
      startRun();
    }
  }

  updateGyro();
  updateTof();
  runTest();

  unsigned long now = millis();
  if (now - lastRamp >= 20) {
    lastRamp = now;
    bool moving = (state == STRAIGHT || state == PRE_TURN || state == TURN || state == REVERSE);
    int target = moving ? pwmTarget : 0;
    driveBack = (state == TURN || state == REVERSE);   // giro y retroceso van en reversa
    if (state == DONE) { /* freno */ }
    else if (pwmNow < target) motorOut(min(pwmNow + RAMP_STEP, target));
    else if (pwmNow > target) motorOut(target);
  }

  if (now - lastPrint >= 100 && state != WAIT) {
    lastPrint = now;
    Serial.print(STATE_NAME[state]);
    Serial.print(F("\tc ")); Serial.print(corners);
    Serial.print(F("\tyawL ")); Serial.print(yawL(), 1);
    Serial.print(F("\tgoal ")); Serial.print(goalLocal, 1);
    Serial.print(F("\t")); Serial.print(MODE_NAME[mode]);
    Serial.print(F("\todo ")); Serial.print(odoCm(), 1);
    Serial.print(F("\tL ")); Serial.print(tofs[0].mm);
    Serial.print(F("\tR ")); Serial.print(tofs[1].mm);
    Serial.print(F("\tFR ")); Serial.print(frontTof.mm);
    Serial.print(F("\tRA ")); Serial.print(rearTof.mm);
    Serial.print(F("\tpilar ")); Serial.print(current.used ? (current.color == 1 ? F("R") : F("G")) : F("-"));
    Serial.print(F("\tlinea B/O ")); Serial.print(camBlue); Serial.print('/'); Serial.print(camOrange);
    Serial.print(F("\tlock ")); Serial.print((int)lineLock);
    Serial.print(F("\tpwm ")); Serial.println(pwmNow);
  }
}