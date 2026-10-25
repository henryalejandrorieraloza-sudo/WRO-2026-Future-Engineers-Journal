// tof_roi_budget_test.ino
// TOF ROI / timing budget test (car STILL, in front of a REAL track wall)
// For each configuration (ROI size x timing budget) it takes SAMPLES readings and prints:
//   counts of "range valid", "signal fail", other statuses and timeouts,
//   and the MEDIAN, MIN, MAX and SPREAD (p90 - p10) of the readings that have a distance.
// This test found that, on the black wall with the sensor angled 30 deg, ROI 1x6 read ~2 m at
// every distance, and that a STRAIGHT sensor with ROI 16x8 measures the wall within 1-2 cm
// (journal Phase 10).
// HOW TO USE:
//   1) Put the car STILL, the tested sensor facing the wall, at a known distance (tape measure).
//   2) In the Serial Monitor (115200, "New Line") type the distance in cm and press Enter
//      (type 0 = NO wall in front of the sensor, to see if it reads the floor).
//   3) It runs all configurations (about 15 s) and prints one line per configuration.
//   4) Move the car to the next distance and repeat (e.g. 30, 45, 60, 0).
//   Only "range valid" readings should be trusted: "signal fail" can give believable wrong numbers.
// Board: Arduino Nano | I2C: SDA=A4, SCL=A5
// Tested sensor: TEST_XSHUT (default LEFT, A3). The other TOFs are kept OFF.
// Library: VL53L1X by Pololu.

#include <Wire.h>
#include <VL53L1X.h>

// ================= SETTINGS =================
const int TEST_XSHUT = A3;              // A3 = LEFT, A2 = RIGHT, A1 = FRONT, A0 = BACK
const int SAMPLES    = 30;              // readings per configuration
const int DISCARD    = 5;               // first readings after a change are thrown away

struct Cfg { uint8_t w; uint8_t h; uint32_t budgetUs; };

// Test 1 (24 Oct): first comparison
const Cfg CFGS[] = {
  { 1,  6, 33000}, { 1,  6, 50000},     // old robot setting (1x6, 33 ms) + 50 ms
  { 4,  4, 33000}, { 4,  4, 50000},     // ST minimum ROI
  { 8,  8, 33000}, { 8,  8, 50000},
  {16, 16, 33000}, {16, 16, 50000},     // full sensor
};
// Test 2 (25 Oct): wide vs tall cone, sensor straight
//   {16,16,50000}, {16,4,50000}, {4,16,50000}, {16,6,50000}, {6,16,50000}, {16,8,50000}, {8,16,50000}
// Test 3 (next, after remounting the sensors straight at ~5 cm): more time = more light
//   {16,8,50000}, {16,8,100000}, {16,8,140000}, {16,10,100000}, {16,12,100000}
const int NCFG = sizeof(CFGS) / sizeof(CFGS[0]);
// ============================================

const int ALL_XSHUT[] = {A3, A2, A1, A0};

VL53L1X tof;

void sortInts(int *a, int n) {
  for (int i = 1; i < n; i++) {
    int x = a[i], j = i - 1;
    while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; j--; }
    a[j + 1] = x;
  }
}

void runConfig(const Cfg &c, int distCm) {
  tof.stopContinuous();
  delay(20);
  tof.setMeasurementTimingBudget(c.budgetUs);
  tof.setROISize(c.w, c.h);
  tof.startContinuous(c.budgetUs / 1000);

  for (int i = 0; i < DISCARD; i++) tof.read();     // let it settle

  int vals[SAMPLES];
  int nv = 0, nRV = 0, nSF = 0, nOther = 0, nTO = 0;
  for (int i = 0; i < SAMPLES; i++) {
    tof.read();                                     // waits for a new measurement
    if (tof.timeoutOccurred()) { nTO++; continue; }
    VL53L1X::RangeStatus st = tof.ranging_data.range_status;
    int mm = tof.ranging_data.range_mm;
    if (st == VL53L1X::RangeValid)      nRV++;
    else if (st == VL53L1X::SignalFail) nSF++;
    else                                nOther++;
    if ((st == VL53L1X::RangeValid || st == VL53L1X::SignalFail) && mm > 0) vals[nv++] = mm;
  }

  Serial.print(distCm);
  Serial.print(F("\t"));
  Serial.print(c.w); Serial.print(F("x")); Serial.print(c.h);
  Serial.print(F("\t"));
  Serial.print(c.budgetUs / 1000);
  Serial.print(F("\t"));
  Serial.print(nRV);
  Serial.print(F("\t"));
  Serial.print(nSF);
  Serial.print(F("\t"));
  Serial.print(nOther);
  Serial.print(F("\t"));
  Serial.print(nTO);
  Serial.print(F("\t"));

  if (nv == 0) {
    Serial.println(F("---\t---\t---\t---"));
    return;
  }
  sortInts(vals, nv);
  int med    = vals[nv / 2];
  int spread = vals[(nv * 9) / 10 < nv ? (nv * 9) / 10 : nv - 1] - vals[nv / 10];
  Serial.print(med / 10.0, 1);
  Serial.print(F("\t"));
  Serial.print(vals[0] / 10.0, 1);
  Serial.print(F("\t"));
  Serial.print(vals[nv - 1] / 10.0, 1);
  Serial.print(F("\t"));
  Serial.println(spread / 10.0, 1);
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) { pinMode(ALL_XSHUT[i], OUTPUT); digitalWrite(ALL_XSHUT[i], LOW); }
  delay(20);
  digitalWrite(TEST_XSHUT, HIGH);                   // only the tested sensor ON (address 0x29)
  delay(20);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(25000, true);

  tof.setTimeout(500);
  if (!tof.init()) {
    Serial.println(F("ERROR: TOF not found (check TEST_XSHUT and wiring)"));
    while (true);
  }
  tof.setDistanceMode(VL53L1X::Long);

  Serial.println(F("TOF ROI / budget test ready."));
  Serial.println(F("Car STILL, sensor facing the wall."));
  Serial.println(F("Type the distance in cm and press Enter (0 = no wall)."));
}

void loop() {
  if (!Serial.available()) return;
  int distCm = Serial.parseInt();
  while (Serial.available()) Serial.read();         // clear the rest of the line

  Serial.println();
  Serial.print(F("=== Distance: "));
  if (distCm == 0) Serial.println(F("NO WALL ===")); else { Serial.print(distCm); Serial.println(F(" cm ===")); }
  Serial.println(F("dist\tROI\tms\tvalid\tsigFail\tother\ttimeout\tmedian\tmin\tmax\tspread(cm)"));

  for (int i = 0; i < NCFG; i++) runConfig(CFGS[i], distCm);

  Serial.println(F("Done. Move the car and type the next distance."));
}
