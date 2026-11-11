/*
  tof_servo_sensors_test.ino
  ---------------------------
  Arduino Nano (ATmega328P). Prueba solo sensores y servo:
    - Servo de direccion en D9: izquierda -> centro -> derecha -> centro
    - 2 sensores VL53L1X laterales, cada uno con su propia direccion I2C

  Conexiones:
    I2C (sensores):   SDA -> A4,  SCL -> A5
    XSHUT sensor IZQ: A3
    XSHUT sensor DER: A2
    Servo:            D9

  Librerias: "VL53L1X" (Pololu). Wire y Servo ya vienen con el IDE.
*/

#include <Wire.h>
#include <Servo.h>
#include <VL53L1X.h>

// ---------- Pines ----------
const int PIN_SERVO   = 9;
const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;

// ---------- Ajustes ----------
const int SERVO_LEFT   = 10;   // bajo = mas a la izquierda (ajusta a tu mecanica)
const int SERVO_CENTER = 90;
const int SERVO_RIGHT  = 170;  // alto = mas a la derecha
const unsigned long SERVO_STEP_MS = 1000;

const unsigned long PRINT_MS = 200;

// Direcciones I2C para cada VL53L1X (0x29 es la de fabrica)
const uint8_t ADDR_LEFT  = 0x30;
const uint8_t ADDR_RIGHT = 0x31;

// ---------- Objetos y estado ----------
VL53L1X tofLeft;
VL53L1X tofRight;
Servo steering;

bool leftOk = false;
bool rightOk = false;

const int SERVO_SEQ[4]    = {SERVO_LEFT, SERVO_CENTER, SERVO_RIGHT, SERVO_CENTER};
const char *SERVO_NAME[4] = {"IZQ", "CENTRO", "DER", "CENTRO"};
int servoStep = 0;
unsigned long lastServoMs = 0;
unsigned long lastPrintMs = 0;

// ---------- Sensores VL53L1X ----------
// Enciende un sensor por XSHUT, le asigna una direccion nueva y lo configura
bool initTof(VL53L1X &s, int xshut, uint8_t addr, const char *name) {
  digitalWrite(xshut, HIGH);   // solo este sensor esta encendido
  delay(20);
  s.setTimeout(500);
  if (!s.init()) {
    Serial.print(name);
    Serial.println(F(" NO ENCONTRADO"));
    return false;
  }
  s.setAddress(addr);          // desde aqui responde en la nueva direccion
  s.setDistanceMode(VL53L1X::Long);
  s.setMeasurementTimingBudget(100000);
  s.setROISize(4, 13);
  s.startContinuous(100);
  Serial.print(name);
  Serial.println(F(" OK"));
  return true;
}

// Devuelve distancia en cm, o -1 si la lectura no es valida
float readCm(VL53L1X &s, bool ok) {
  if (!ok) return -1;
  uint16_t mm = s.read();
  if (s.timeoutOccurred()) return -1;
  VL53L1X::RangeStatus st = s.ranging_data.range_status;
  if (st == VL53L1X::RangeValid || (st == VL53L1X::SignalFail && mm > 0)) {
    return mm / 10.0;
  }
  return -1;
}

// ---------- Impresion ----------
void printLine(float l, float r) {
  Serial.print(F("IZQ: "));
  if (l < 0) Serial.print(F("--")); else Serial.print(l, 1);
  Serial.print(F(" cm | DER: "));
  if (r < 0) Serial.print(F("--")); else Serial.print(r, 1);
  Serial.print(F(" cm | SERVO: "));
  Serial.println(SERVO_NAME[servoStep]);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Servo
  steering.attach(PIN_SERVO);
  steering.write(SERVO_CENTER);

  // I2C (en el Nano, A4/A5 son SDA/SCL automaticamente)
  Wire.begin();

  // Sensores: apagar ambos y encenderlos uno por uno
  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(20);
  leftOk  = initTof(tofLeft,  XSHUT_LEFT,  ADDR_LEFT,  "IZQ");
  rightOk = initTof(tofRight, XSHUT_RIGHT, ADDR_RIGHT, "DER");

  Serial.println(F("Listo"));

  unsigned long now = millis();
  lastServoMs = now;
  lastPrintMs = now;
}

// ---------- Loop ----------
void loop() {
  unsigned long nowMs = millis();

  // 1) Servo: izquierda -> centro -> derecha -> centro
  if (nowMs - lastServoMs >= SERVO_STEP_MS) {
    lastServoMs = nowMs;
    servoStep = (servoStep + 1) % 4;
    steering.write(SERVO_SEQ[servoStep]);
  }

  // 2) Sensores laterales + impresion
  if (nowMs - lastPrintMs >= PRINT_MS) {
    lastPrintMs = nowMs;
    float l = readCm(tofLeft,  leftOk);
    float r = readCm(tofRight, rightOk);
    printLine(l, r);
  }
}
