// tof_left_right_print.ino
// Prints what the LEFT and RIGHT TOF sensors see (nothing else), with the status of every reading.
// Used on 24 Oct 2026 to discover that, with the sensors angled 30 deg and ROI 1x6, every reading
// on the black wall was "signal fail" (journal Phase 10).
// Board: Arduino Nano | I2C: SDA=A4, SCL=A5
// LEFT  TOF400C (VL53L1X): XSHUT A3 -> address 0x30
// RIGHT TOF400C (VL53L1X): XSHUT A2 -> address 0x31
// FRONT (A1) and BACK (A0) are kept OFF so they do not use address 0x29.
// Serial Monitor: 115200 baud. Library: VL53L1X by Pololu.

#include <Wire.h>
#include <VL53L1X.h>

const int XSHUT_LEFT  = A3;
const int XSHUT_RIGHT = A2;
const int XSHUT_FRONT = A1;
const int XSHUT_BACK  = A0;

// Robot settings at the time of the test. New recommended values: ROI 16 x 8, 50000 us / 50 ms.
const uint8_t  ROI_WIDTH        = 1;
const uint8_t  ROI_HEIGHT       = 6;
const uint32_t TIMING_BUDGET_US = 33000;
const int      PERIOD_MS        = 33;

VL53L1X tofLeft;
VL53L1X tofRight;

bool startTof(VL53L1X &sensor, int xshutPin, uint8_t address) {
  digitalWrite(xshutPin, HIGH);
  delay(20);
  sensor.setTimeout(500);
  if (!sensor.init()) return false;
  sensor.setAddress(address);
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(TIMING_BUDGET_US);
  sensor.setROISize(ROI_WIDTH, ROI_HEIGHT);
  sensor.startContinuous(PERIOD_MS);
  return true;
}

void printSensor(const __FlashStringHelper *name, VL53L1X &sensor) {
  sensor.read();
  Serial.print(name);
  if (sensor.timeoutOccurred()) {
    Serial.print(F("TIMEOUT"));
    return;
  }
  Serial.print(sensor.ranging_data.range_mm / 10.0, 1);
  Serial.print(F(" cm ("));
  Serial.print(VL53L1X::rangeStatusToString(sensor.ranging_data.range_status));
  Serial.print(F(")"));
}

void setup() {
  Serial.begin(115200);

  pinMode(XSHUT_LEFT,  OUTPUT); digitalWrite(XSHUT_LEFT,  LOW);
  pinMode(XSHUT_RIGHT, OUTPUT); digitalWrite(XSHUT_RIGHT, LOW);
  pinMode(XSHUT_FRONT, OUTPUT); digitalWrite(XSHUT_FRONT, LOW);
  pinMode(XSHUT_BACK,  OUTPUT); digitalWrite(XSHUT_BACK,  LOW);
  delay(20);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  if (!startTof(tofLeft, XSHUT_LEFT, 0x30))   { Serial.println(F("ERROR: LEFT TOF not found"));  while (true); }
  if (!startTof(tofRight, XSHUT_RIGHT, 0x31)) { Serial.println(F("ERROR: RIGHT TOF not found")); while (true); }

  Serial.println(F("LEFT and RIGHT TOF ready"));
}

void loop() {
  printSensor(F("L: "), tofLeft);
  Serial.print(F("  |  "));
  printSensor(F("R: "), tofRight);
  Serial.println();
}
