// 4x TOF400C (VL53L1X) - most precise distance (in cm)
// Board: Arduino Nano ESP32 | I2C: SDA=A4, SCL=A5
// Library: "VL53L1X" by Pololu
// XSHUT: LOW = off, HIGH = on. Each sensor gets its own I2C address at startup.
//
// Precision tricks used:
//  1. Long measuring time (less noise per reading)
//  2. Only VALID readings are used (bad ones are thrown away)
//  3. Median filter: takes the middle value of the last readings (kills spikes)
//  4. Offset per sensor: corrects a fixed error you measure with a ruler

#include <Wire.h>
#include <VL53L1X.h>

const int NUM_SENSORS = 4;

// Same start-up order as the code that worked: LEFT, RIGHT, BACK, FRONT
const char   *NAMES[NUM_SENSORS]      = {"LEFT", "RIGHT", "BACK", "FRONT"};
const int     XSHUT_PINS[NUM_SENSORS] = {A3, A2, A0, A1};
const uint8_t ADDRESSES[NUM_SENSORS]  = {0x30, 0x31, 0x32, 0x33};

// Print order: FRONT, BACK, LEFT, RIGHT (index in the lists above)
const int PRINT_ORDER[NUM_SENSORS] = {3, 2, 0, 1};

// --- Precision settings ---
// Long = up to ~4 m. Medium = up to ~3 m, handles weak signals a bit better.
const VL53L1X::DistanceMode DISTANCE_MODE = VL53L1X::Long;
const uint32_t TIMING_BUDGET_US = 100000;  // 100 ms per measurement (more = less noise)

// Cone size (ROI): how many of the 16 x 16 receivers are used.
// Chosen by experiment on 5 Oct 2026 (journal section 8.2): for each height we
// measured the difference "wall at 60 cm" vs "no wall"; height 6 gave the
// largest difference (64), heights >= 10 almost none (the cone sees the floor).
// Note: ST's datasheet gives 4 x 4 as the minimum ROI; width 1 worked best in
// our tests but is outside the official range, so it will be re-checked with width 4.
// If readings get WORSE, the module is mounted rotated 90 deg: swap the two numbers.
const uint8_t ROI_WIDTH  = 1;   // horizontal
const uint8_t ROI_HEIGHT = 6;   // vertical

// "signal fail" = the light that came back was weak (angled wall, dark surface).
// The distance is usually still good, so we accept it. The median filter removes bad ones.
const bool ACCEPT_WEAK_SIGNAL = true;
const int      PERIOD_MS        = 100;
const int      FILTER_SIZE      = 5;       // median of the last 5 valid readings

// Fixed error of each sensor (cm), same order as NAMES: LEFT, RIGHT, BACK, FRONT
// If the sensor says 51.2 cm and the real distance is 50.0 cm -> offset = -1.2
float OFFSET_CM[NUM_SENSORS] = {0.0, 0.0, 0.0, 0.0};

VL53L1X sensors[NUM_SENSORS];
bool     found[NUM_SENSORS] = {false, false, false, false};
uint16_t history[NUM_SENSORS][FILTER_SIZE];
int      historyCount[NUM_SENSORS] = {0, 0, 0, 0};
int      historyIndex[NUM_SENSORS] = {0, 0, 0, 0};
const char *lastStatus[NUM_SENSORS] = {"", "", "", ""};

// Saves a new valid reading in the sensor's history (overwrites the oldest)
void addReading(int s, uint16_t value) {
  history[s][historyIndex[s]] = value;
  historyIndex[s] = (historyIndex[s] + 1) % FILTER_SIZE;
  if (historyCount[s] < FILTER_SIZE) historyCount[s]++;
}

// Returns the middle value of the saved readings in mm (-1 if there are none)
int median(int s) {
  int n = historyCount[s];
  if (n == 0) return -1;

  uint16_t sorted[FILTER_SIZE];
  for (int i = 0; i < n; i++) sorted[i] = history[s][i];

  for (int i = 1; i < n; i++) {        // simple sort (small list)
    uint16_t key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = key;
  }
  return sorted[n / 2];
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 5000) delay(10);

  // 1. Turn all sensors off
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(XSHUT_PINS[i], OUTPUT);
    digitalWrite(XSHUT_PINS[i], LOW);
  }
  delay(20);

  Wire.begin(A4, A5);

  // 2. Turn on one at a time and give each one its own address
  for (int i = 0; i < NUM_SENSORS; i++) {
    digitalWrite(XSHUT_PINS[i], HIGH);
    delay(20);

    sensors[i].setTimeout(500);
    if (!sensors[i].init()) {
      Serial.print("ERROR: ");
      Serial.print(NAMES[i]);
      Serial.println(" not found");
      continue;
    }

    sensors[i].setAddress(ADDRESSES[i]);
    sensors[i].setDistanceMode(DISTANCE_MODE);
    sensors[i].setMeasurementTimingBudget(TIMING_BUDGET_US);
    sensors[i].setROISize(ROI_WIDTH, ROI_HEIGHT);   // smaller cone
    sensors[i].startContinuous(PERIOD_MS);
    found[i] = true;

    Serial.print(NAMES[i]);
    Serial.println(" OK");
  }

  Serial.println("Measuring...");
}

void loop() {
  // Read all sensors that started correctly
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (!found[i]) continue;

    uint16_t raw = sensors[i].read();
    VL53L1X::RangeStatus status = sensors[i].ranging_data.range_status;
    bool weakButOk = ACCEPT_WEAK_SIGNAL && status == VL53L1X::SignalFail && raw > 0;

    if (sensors[i].timeoutOccurred()) {
      lastStatus[i] = "TIMEOUT";
    } else if (status == VL53L1X::RangeValid || weakButOk) {
      lastStatus[i] = "";
      addReading(i, raw);
    } else {
      lastStatus[i] = VL53L1X::rangeStatusToString(status);
    }
  }

  // Print in order FRONT, BACK, LEFT, RIGHT
  for (int k = 0; k < NUM_SENSORS; k++) {
    int i = PRINT_ORDER[k];
    Serial.print(NAMES[i]);
    Serial.print(": ");

    if (!found[i]) {
      Serial.print("NOT FOUND");
    } else {
      int filteredMm = median(i);
      if (filteredMm < 0) {
        Serial.print("--- (");
        Serial.print(lastStatus[i]);
        Serial.print(")");
      } else {
        Serial.print(filteredMm / 10.0 + OFFSET_CM[i], 1);
        Serial.print(" cm");
      }
    }
    if (k < NUM_SENSORS - 1) Serial.print(" | ");
  }
  Serial.println();
}
