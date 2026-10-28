// =====================================================================
// LINE TEST - Arduino Nano (classic)
// Motor OFF, wheels straight. Move the car by hand.
// Prints what the camera says about the floor lines (orange / blue) and the pillar.
// Needs obstacle_camera.ino v3 (14-field message):
//   $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,
//    orangeFound,orangeRow,blueCenterRow,orangeCenterRow
// Used on 27 Oct 2026 to measure the row -> distance table of the floor lines.
// Wiring: camera GPIO43 (TX) -> Nano D0 (RX) | Servo D9 | TB6612FNG: PWMA D6, AIN1 D7, AIN2 D8
// UPLOADING: hold the CAMERA RESET button during the whole upload (the camera on D0 blocks it).
// =====================================================================

#include <Servo.h>

// ================= SETTINGS =================
const long CAMERA_BAUD          = 115200;
const int  CENTER_ANGLE         = 120;
const unsigned long PRINT_EVERY_MS    = 200;   // one line of text 5 times a second
const unsigned long CAMERA_TIMEOUT_MS = 500;
// ============================================

// ---------------- Pins ----------------
const int SERVO_PIN = 9;
const int PWMA_PIN  = 6;
const int AIN1_PIN  = 7;
const int AIN2_PIN  = 8;

Servo steering;

// ---------------- Last camera message ----------------
int camColor = 0, camDistCm = -1, camLatCm = 0;
int blueFound = 0, blueRow = -1, orangeFound = 0, orangeRow = -1;
int blueCenter = -1, orangeCenter = -1;
int fieldsRead = 0;
unsigned long lastCamMs = 0;

// Counters for the last print period (how stable the detection is)
int msgCount = 0, orangeCount = 0, blueCount = 0;

// ---------------- Camera messages ----------------
char lineBuf[80];                      // the 14-field message is longer than the old one
uint8_t lineLen = 0;

void readCamera() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '$') { lineLen = 0; continue; }
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      int col, x, b, ok, bf, br, w, d, lat, of, orw, bc, oc;
      unsigned long seq;
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%d",
                     &col, &x, &b, &ok, &bf, &br, &seq, &w, &d, &lat, &of, &orw, &bc, &oc);
      if (n >= 10) {                   // 10 = old camera code, 14 = new one
        fieldsRead = n;
        camColor = col; camDistCm = d; camLatCm = lat;
        blueFound = bf; blueRow = br;
        if (n == 14) { orangeFound = of; orangeRow = orw; blueCenter = bc; orangeCenter = oc; }
        lastCamMs = millis();
        msgCount++;
        if (blueFound) blueCount++;
        if (n == 14 && orangeFound) orangeCount++;
      }
      lineLen = 0;
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;
  }
}

// ---------------- Printing ----------------
void printLine(const __FlashStringHelper *name, int found, int lowRow, int centerRow, int count) {
  Serial.print(name);
  if (found) {
    Serial.print(F(" YES low ")); Serial.print(lowRow);
    Serial.print(F(" center "));
    if (centerRow >= 0) Serial.print(centerRow); else Serial.print(F("-"));
  } else {
    Serial.print(F(" no"));
  }
  Serial.print(F(" (")); Serial.print(count); Serial.print(F("/")); Serial.print(msgCount); Serial.print(F(")"));
}

void printStatus() {
  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {
    Serial.println(F("NO CAMERA (check the wire / wait for the camera to start)"));
    return;
  }
  if (fieldsRead < 14) {
    Serial.println(F("Camera sends the OLD message (10 fields): upload obstacle_camera v3 to the camera"));
    return;
  }
  printLine(F("ORANGE"), orangeFound, orangeRow, orangeCenter, orangeCount);
  Serial.print(F("  |  "));
  printLine(F("BLUE"), blueFound, blueRow, blueCenter, blueCount);
  Serial.print(F("  |  pillar "));
  if (camColor == 1)      Serial.print(F("RED "));
  else if (camColor == 2) Serial.print(F("GREEN "));
  else                    Serial.print(F("-"));
  if (camColor != 0) { Serial.print(camDistCm); Serial.print(F(" cm")); }
  Serial.println();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(CAMERA_BAUD);           // RX = camera, TX = Serial Monitor

  pinMode(PWMA_PIN, OUTPUT);           // motor OFF the whole time
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  digitalWrite(AIN1_PIN, LOW);
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);

  steering.attach(SERVO_PIN, 500, 2500);
  steering.write(CENTER_ANGLE);

  Serial.println(F("LINE TEST - motor off. Move the car by hand over the lines."));
}

// ---------------- LOOP ----------------
void loop() {
  readCamera();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= PRINT_EVERY_MS) {
    lastPrint = millis();
    printStatus();
    msgCount = orangeCount = blueCount = 0;   // new period
  }
}
