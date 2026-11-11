/*
  test_completo_nano.ino
  ----------------------
  Arduino Nano (ATmega328P). Prueba todo junto:
    - Motor trasero (TB6612FNG): adelante y atras, alternando
    - Servo de direccion en D9: izquierda -> centro -> derecha -> centro
    - 2 sensores VL53L1X laterales, cada uno con su propia direccion I2C
    - Giroscopio BMI160: yaw (gira el robot a mano 90 grados -> ~90)

  Conexiones:
    I2C (sensores + gyro):  SDA -> A4,  SCL -> A5
    XSHUT sensor IZQ:       A3
    XSHUT sensor DER:       A2
    Motor TB6612FNG:        PWMA -> D6, AIN1 -> D7, AIN2 -> D8, STBY -> D5
    Servo:                  D9

  Librerias: "VL53L1X" (Pololu). Wire y Servo ya vienen con el IDE.

  IMPORTANTE:
    - En la primera prueba levanta el carro para que las ruedas no toquen el piso.
    - Mantén el robot QUIETO durante los primeros 2 segundos (calibracion del gyro).
*/

#include <Wire.h>
#include <Servo.h>
#include <VL53L1X.h>

// ---------- Pines ----------
const int PIN_STBY    = 5;
const int PIN_PWMA    = 6;
const int PIN_AIN1    = 7;
const int PIN_AIN2    = 8;
const int PIN_SERVO   = 9;
const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;

// ---------- Ajustes ----------
const int MOTOR_SPEED        = 100;    // 0-255, empieza bajo
const unsigned long MOTOR_CHANGE_MS = 1500;  // cambia de sentido cada 1.5 s

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

uint8_t bmiAddr = 0;
bool gyroOk = false;
const float GYRO_LSB_PER_DPS = 65.6;   // rango +-500 dps
float gyroBias = 0;
float yawDeg = 0;
unsigned long lastUs = 0;

bool motorForward = true;
unsigned long lastMotorMs = 0;

const int SERVO_SEQ[4]        = {SERVO_LEFT, SERVO_CENTER, SERVO_RIGHT, SERVO_CENTER};
const char *SERVO_NAME[4]     = {"IZQ", "CENTRO", "DER", "CENTRO"};
int servoStep = 0;
unsigned long lastServoMs = 0;

unsigned long lastPrintMs = 0;

// ---------- Motor ----------
void motorSet(bool forward) {
  digitalWrite(PIN_AIN1, forward ? HIGH : LOW);
  digitalWrite(PIN_AIN2, forward ? LOW : HIGH);
  analogWrite(PIN_PWMA, MOTOR_SPEED);
}

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

// ---------- Giroscopio BMI160 ----------
void gyroWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(bmiAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t gyroReadReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

int16_t gyroZRaw() {
  Wire.beginTransmission(bmiAddr);
  Wire.write(0x10);                 // GYR_Z_L
  Wire.endTransmission(false);
  Wire.requestFrom(bmiAddr, (uint8_t)2);
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

bool initGyro() {
  if (gyroReadReg(0x68, 0x00) == 0xD1)      bmiAddr = 0x68;  // chip ID = 0xD1
  else if (gyroReadReg(0x69, 0x00) == 0xD1) bmiAddr = 0x69;
  else {
    Serial.println(F("BMI160 NO ENCONTRADO"));
    return false;
  }
  gyroWrite(0x7E, 0xB6); delay(100);   // soft reset
  gyroWrite(0x7E, 0x15); delay(100);   // gyro modo normal
  gyroWrite(0x42, 0x28);               // 100 Hz
  gyroWrite(0x43, 0x02);               // +-500 dps
  delay(50);

  long sum = 0;
  for (int i = 0; i < 400; i++) {
    sum += gyroZRaw();
    delay(5);
  }
  gyroBias = (sum / 400.0) / GYRO_LSB_PER_DPS;
  Serial.println(F("BMI160 OK"));
  return true;
}

// ---------- Impresion ----------
void printLine(float l, float r) {
  Serial.print(F("IZQ: "));
  if (l < 0) Serial.print(F("--")); else Serial.print(l, 1);
  Serial.print(F(" cm | DER: "));
  if (r < 0) Serial.print(F("--")); else Serial.print(r, 1);
  Serial.print(F(" cm | YAW: "));
  Serial.print(yawDeg, 1);
  Serial.print(F(" | MOTOR: "));
  Serial.print(motorForward ? F("ADELANTE") : F("ATRAS"));
  Serial.print(F(" | SERVO: "));
  Serial.println(SERVO_NAME[servoStep]);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  // Motor
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);        // saca el TB6612 del modo standby
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  analogWrite(PIN_PWMA, 0);            // motor parado mientras calibra

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

  // Gyro
  Serial.println(F("Calibrando gyro: NO MUEVAS EL ROBOT"));
  gyroOk = initGyro();

  Serial.println(F("Listo"));

  // Arranca motor y temporizadores
  motorSet(true);
  unsigned long now = millis();
  lastMotorMs = now;
  lastServoMs = now;
  lastPrintMs = now;
  lastUs = micros();
}

// ---------- Loop ----------
void loop() {
  unsigned long nowMs = millis();

  // 1) Gyro: integra el yaw en cada vuelta
  if (gyroOk) {
    unsigned long nowUs = micros();
    float dt = (nowUs - lastUs) / 1000000.0;
    lastUs = nowUs;
    float rateDps = gyroZRaw() / GYRO_LSB_PER_DPS - gyroBias;
    yawDeg += rateDps * dt;
  }

  // 2) Motor: alterna adelante / atras
  if (nowMs - lastMotorMs >= MOTOR_CHANGE_MS) {
    lastMotorMs = nowMs;
    motorForward = !motorForward;
    motorSet(motorForward);
  }

  // 3) Servo: izquierda -> centro -> derecha -> centro
  if (nowMs - lastServoMs >= SERVO_STEP_MS) {
    lastServoMs = nowMs;
    servoStep = (servoStep + 1) % 4;
    steering.write(SERVO_SEQ[servoStep]);
  }

  // 4) Sensores laterales + impresion
  if (nowMs - lastPrintMs >= PRINT_MS) {
    lastPrintMs = nowMs;
    float l = readCm(tofLeft,  leftOk);
    float r = readCm(tofRight, rightOk);
    printLine(l, r);
  }
}