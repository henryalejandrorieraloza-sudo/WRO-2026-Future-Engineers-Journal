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

#include <Servo.h>                    // standard library for the steering servo

// ================= SETTINGS =================
const long CAMERA_BAUD          = 115200;   // speed of the link from the camera (must match the camera)
const int  CENTER_ANGLE         = 120;      // servo angle that keeps the wheels straight
const unsigned long PRINT_EVERY_MS    = 200;   // one line of text 5 times a second
const unsigned long CAMERA_TIMEOUT_MS = 500;   // no camera message for this long = camera is not sending
// ============================================

// ---------------- Pins ----------------
const int SERVO_PIN = 9;              // D9: steering servo signal
const int PWMA_PIN  = 6;              // D6: motor speed (TB6612FNG PWMA)
const int AIN1_PIN  = 7;              // D7: motor direction 1
const int AIN2_PIN  = 8;              // D8: motor direction 2

Servo steering;                       // the steering servo object

// ---------------- Last camera message ----------------
int camColor = 0, camDistCm = -1, camLatCm = 0;     // pillar: color (0 none, 1 red, 2 green), distance and side
int blueFound = 0, blueRow = -1, orangeFound = 0, orangeRow = -1;   // lines: found flag and lowest row
int blueCenter = -1, orangeCenter = -1;             // lines: row straight ahead (-1 = not in the center)
int fieldsRead = 0;                   // how many fields the last message had (10 = old, 14 = new)
unsigned long lastCamMs = 0;          // time of the last valid camera message

// Counters for the last print period (how stable the detection is)
int msgCount = 0, orangeCount = 0, blueCount = 0;   // messages, and messages where each line was found

// ---------------- Camera messages ----------------
char lineBuf[80];                     // buffer for one message; the 14-field message is longer than the old one
uint8_t lineLen = 0;                  // how many characters are in lineBuf right now

void readCamera() {                   // reads all the bytes waiting from the camera and builds messages
  while (Serial.available()) {        // while there are bytes waiting
    char c = Serial.read();           // read one byte
    if (c == '$') { lineLen = 0; continue; }   // '$' starts a new message: clear the buffer
    if (c == '\n') {                  // end of the message
      lineBuf[lineLen] = '\0';        // close the text so it can be read as a string
      int col, x, b, ok, bf, br, w, d, lat, of, orw, bc, oc;   // one variable per field
      unsigned long seq;              // frame number
      int n = sscanf(lineBuf, "%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%d",   // split the text into numbers
                     &col, &x, &b, &ok, &bf, &br, &seq, &w, &d, &lat, &of, &orw, &bc, &oc);
      if (n >= 10) {                  // 10 = old camera code, 14 = new one
        fieldsRead = n;               // remember how many fields arrived
        camColor = col; camDistCm = d; camLatCm = lat;   // pillar data
        blueFound = bf; blueRow = br; // blue line data
        if (n == 14) { orangeFound = of; orangeRow = orw; blueCenter = bc; orangeCenter = oc; }   // orange and center rows only in the new message
        lastCamMs = millis();         // remember when the last good message came
        msgCount++;                   // one more message in this period
        if (blueFound) blueCount++;   // count the messages where blue was found
        if (n == 14 && orangeFound) orangeCount++;   // count the messages where orange was found
      }
      lineLen = 0;                    // get ready for the next message
      continue;
    }
    if (lineLen < sizeof(lineBuf) - 1) lineBuf[lineLen++] = c;   // store the character if there is room
  }
}

// ---------------- Printing ----------------
void printLine(const __FlashStringHelper *name, int found, int lowRow, int centerRow, int count) {   // prints one line's status
  Serial.print(name);                 // "ORANGE" or "BLUE"
  if (found) {                        // the line was found in the last message
    Serial.print(F(" YES low ")); Serial.print(lowRow);      // lowest row of the line
    Serial.print(F(" center "));      // label for the center row
    if (centerRow >= 0) Serial.print(centerRow); else Serial.print(F("-"));   // center row, or "-" if not in the center
  } else {
    Serial.print(F(" no"));           // not found
  }
  Serial.print(F(" (")); Serial.print(count); Serial.print(F("/")); Serial.print(msgCount); Serial.print(F(")"));   // how many messages had it, out of all messages
}

void printStatus() {                  // prints one status line for the Serial Monitor
  if (millis() - lastCamMs > CAMERA_TIMEOUT_MS) {   // no message recently
    Serial.println(F("NO CAMERA (check the wire / wait for the camera to start)"));
    return;                           // nothing else to show
  }
  if (fieldsRead < 14) {              // the camera sends the old message
    Serial.println(F("Camera sends the OLD message (10 fields): upload obstacle_camera v3 to the camera"));
    return;
  }
  printLine(F("ORANGE"), orangeFound, orangeRow, orangeCenter, orangeCount);   // orange line status
  Serial.print(F("  |  "));          // separator
  printLine(F("BLUE"), blueFound, blueRow, blueCenter, blueCount);   // blue line status
  Serial.print(F("  |  pillar "));    // label for the pillar
  if (camColor == 1)      Serial.print(F("RED "));     // red pillar
  else if (camColor == 2) Serial.print(F("GREEN "));   // green pillar
  else                    Serial.print(F("-"));        // no pillar
  if (camColor != 0) { Serial.print(camDistCm); Serial.print(F(" cm")); }   // its distance, if there is one
  Serial.println();                   // end of the line
}

// ---------------- SETUP ----------------
void setup() {                        // runs once when the Nano starts
  Serial.begin(CAMERA_BAUD);          // RX = camera, TX = Serial Monitor

  pinMode(PWMA_PIN, OUTPUT);          // motor OFF the whole time
  pinMode(AIN1_PIN, OUTPUT);          // direction pins are outputs
  pinMode(AIN2_PIN, OUTPUT);
  digitalWrite(AIN1_PIN, LOW);        // both direction pins low
  digitalWrite(AIN2_PIN, LOW);
  analogWrite(PWMA_PIN, 0);           // speed zero: the motor does not turn

  steering.attach(SERVO_PIN, 500, 2500);   // connect the servo, with pulse limits of 0.5 ms to 2.5 ms
  steering.write(CENTER_ANGLE);       // wheels straight

  Serial.println(F("LINE TEST - motor off. Move the car by hand over the lines."));   // instruction
}

// ---------------- LOOP ----------------
void loop() {                         // runs again and again after setup()
  readCamera();                       // read the camera messages as they arrive

  static unsigned long lastPrint = 0; // time of the last status line (kept between loops)
  if (millis() - lastPrint >= PRINT_EVERY_MS) {   // 200 ms have passed
    lastPrint = millis();             // remember this print time
    printStatus();                    // print the status line
    msgCount = orangeCount = blueCount = 0;   // start a new counting period
  }
}
