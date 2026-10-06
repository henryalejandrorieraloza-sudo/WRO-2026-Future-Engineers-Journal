# WRO 2026 Future Engineers — Self-Driving Car

Autonomous car for the **World Robot Olympiad 2026, Future Engineers** category. The car must complete three laps of a track whose inner walls change position every round (**Open Challenge**). In the **Obstacle Challenge** it must also pass red pillars on their right and green pillars on their left, and finish by parking in a magenta parking lot.

This repository holds all the code, the engineering journal and the supporting files of the car. All code and documentation are in English.

| | |
|---|---|
| **Main controller** | Arduino Nano ESP32 |
| **Vision** | ESP32-S3 CAM (OV2640) with our own colour-detection code |
| **Sensors** | 4 × TOF400C laser distance sensors (VL53L1X), BMI160 gyroscope |
| **Drive / steering** | One N20 DC motor with encoder on the rear axle (differential), Ackermann steering with an MG90 servo |
| **Engineering journal** | [docs/ENGINEERING_JOURNAL.md](docs/ENGINEERING_JOURNAL.md) |

---

## Contents

1. [Repository structure](#repository-structure)
2. [Current status](#current-status)
3. [Vehicle overview: mobility, power and sense](#vehicle-overview-mobility-power-and-sense)
4. [Wiring / pin map](#wiring--pin-map)
5. [Software architecture](#software-architecture)
6. [Open Challenge strategy](#open-challenge-strategy)
7. [Obstacle Challenge strategy](#obstacle-challenge-strategy)
8. [How to build, compile and upload the code](#how-to-build-compile-and-upload-the-code)
9. [Testing workflow](#testing-workflow)
10. [Team](#team)

---

## Repository structure

The repository follows the official [WRO Future Engineers template](https://github.com/World-Robot-Olympiad-Association/wro2022-fe-template).

```
.
├── README.md                  ← this file: overview, wiring, build instructions
├── docs/
│   ├── ENGINEERING_JOURNAL.md ← day-by-day engineering process, problems and decisions
│   └── images/                ← plots and pictures used in the journal
├── src/                       ← all code (see src/README.md)
│   ├── hardware-tests/        ← one sketch per component (TOF, gyro, motor, servo, camera)
│   └── obstacle-challenge/    ← pillar detection + Pillar Vision Lab web tool
├── models/                    ← 3D-printing / laser-cutting files of the chassis
├── schemes/                   ← wiring diagrams
├── t-photos/                  ← team photos
├── v-photos/                  ← vehicle photos (6 sides)
└── video/                     ← links to the driving videos
```

Each Arduino sketch lives in a folder with the same name as its `.ino` file, as the Arduino IDE requires. A full table of every sketch is in [src/README.md](src/README.md).

---

## Current status

| Part | Status |
|---|---|
| TOF sensors (×4), unique I²C addresses, filtering | ✅ Working — ROI tuned by experiment (journal §8.2) |
| Gyroscope heading (yaw) | ✅ Working — own register-level driver |
| Drive motor through TB6612FNG | ✅ Tested |
| Pillar detection with the camera | ✅ Finished and tested on the robot (journal §6) |
| Steering servo on the new chassis | 🔄 Calibration in progress (journal Problem 29) |
| Open Challenge program on the new chassis | 🔄 Integration in progress |
| Floor lines, parking detection, camera → Nano link | ⏳ Next steps |

The journal records every problem we found (32 so far), how we solved it and why. The summary table is in [section 10 of the journal](docs/ENGINEERING_JOURNAL.md#10-summary-of-problems-and-solutions).

---

## Vehicle overview: mobility, power and sense

### Mobility

- **Drive:** one N20 12 V DC motor with an encoder drives the rear wheels through a differential. A single motor on one axle follows rules 11.3 and 11.5 (the drive wheels must be mechanically connected). The **TB6612FNG** H-bridge controls speed (PWM) and direction, and its *short brake* mode is used to stop precisely.
- **Steering:** Ackermann geometry on the front wheels, moved by an **MG90** metal-gear servo (it replaced an SG90 that moved inside its mount; journal Problem 8). The steering centre and limits are measured on the real chassis, never assumed: the centre depends on how the servo arm is mounted (journal Problems 7 and 29).

### Power

- The car runs from its own on-board battery; the power budget and the wiring diagram will be added in [schemes/](schemes).
- The input/output pins of the Nano ESP32 work at **3.3 V** and are **not 5 V tolerant**, so no 5 V or battery voltage may reach them; the battery may only go to the board's power input.
- **Safety rule** (from journal Problem 31): code is uploaded with the battery **off**, powered only by USB; the battery is switched on only for driving tests, with the USB disconnected.

### Sense

| Sensor | What it gives | Why we chose it |
|---|---|---|
| 4 × **TOF400C** (VL53L1X) — front, back, left, right | Distance to the walls, up to several metres | They replaced Sharp infrared sensors, whose non-linear output became unreliable beyond 50–60 cm (journal Problem 3). The measurement area (ROI) was tuned by experiment so the beam does not see the floor or pass over the 10 cm wall |
| **BMI160** gyroscope | Turning rate → integrated heading (yaw) | Keeps the car straight and measures exactly 90° per corner, independent of speed |
| **ESP32-S3 CAM** (OV2640) | 160 × 120 colour pictures at about 25 fps | Detects the red and green pillars (and later the floor lines and the parking lot) |

---

## Wiring / pin map

**Arduino Nano ESP32** (pins are always written with the `D` prefix; journal Problem 9)

| Device | Signal | Nano ESP32 pin |
|---|---|---|
| TB6612FNG motor driver | PWMA / AIN1 / AIN2 | D6 / D7 / D8 |
| TB6612FNG motor driver | STBY | D5 (to confirm on the car) |
| Steering servo MG90 | Signal | D9 |
| Start button | Input | D10 |
| I²C bus (all TOF sensors + BMI160) | SDA / SCL | A4 / A5 |
| TOF FRONT / BACK / RIGHT / LEFT | XSHUT | A1 / A0 / A2 / A3 |

All four TOF sensors share one I²C bus with the gyroscope. At start-up every sensor is switched off with its XSHUT pin, then switched on one at a time and given its own address (LEFT 0x30, RIGHT 0x31, BACK 0x32, FRONT 0x33).

**ESP32-S3 CAM** (emakefun board, camera pins = ESP32S3_EYE layout): SDA 4, SCL 5, XCLK 15, VSYNC 6, HREF 7, PCLK 13, Y2–Y9 = 11, 9, 8, 10, 12, 18, 17, 16. It is a separate board with its own USB-C port.

---

## Software architecture

The work is split between two boards, each doing what it is best at:

```
 ESP32-S3 CAM                           Arduino Nano ESP32
 ─────────────                          ──────────────────
 camera 160×120 RGB565                  TOF ×4  ──► distances
   │                                    BMI160  ──► heading (yaw)
   ▼                                        │
 HSV colour classification                  ▼
   ▼                                    state machine (drive, turn, stop)
 blob search (BFS) + front-pillar split     │
   ▼                                        ▼
 closest pillar + side to pass ──UART──►  steering servo (D9) + motor (TB6612FNG)
```

- **Camera board:** all image processing. It only needs to send a few bytes per picture to the Nano (colour of the closest pillar, its position and whether it is already on the correct side), so the link stays fast.
- **Nano ESP32:** sensors, control loops and actuators. It never processes images, so its control loop stays fast and predictable.
- The camera → Nano UART link is the next integration step; until the servo is calibrated, the Nano prints the steering decision instead of applying it.

**Why our own vision code instead of OpenCV:** OpenCV does not run on a microcontroller without an operating system, and the neural-network libraries for the ESP32 are far more than colour detection needs. Our own pipeline runs at about 25 fps, was verified pixel by pixel against a JavaScript copy of itself, and every step can be explained. The full comparison of alternatives (OpenMV, Pixy2, HuskyLens, Raspberry Pi + OpenCV) is in [journal §6.2](docs/ENGINEERING_JOURNAL.md#62-why-we-wrote-the-vision-ourselves-instead-of-using-opencv).

---

## Open Challenge strategy

A state machine: `WAITING → STRAIGHT → TURNING → FINISHED` (journal §4.2).

1. **WAITING** — calibrate the gyroscope at rest (200 samples), wait for the start button.
2. **STRAIGHT** — keep the gyro heading, and keep the left/right distances measured at the start of each straight with a lateral PD controller. The car does not try to centre itself, because the inner walls move every round.
3. **Corner detection** — a sudden jump in a side distance means that wall has ended; the car turns toward that side. The turning direction of the round (clockwise or counter-clockwise) is therefore learned at the first corner. The front sensor is only an emergency backup.
4. **TURNING** — steer to the limit and turn until the gyroscope reads 90°, then take new distance references.
5. **FINISHED** — after 12 corners (3 laps × 4), stop in the starting section.

---

## Obstacle Challenge strategy

Pillar detection is finished and tested on the robot (journal §6):

1. Take a 160 × 120 RGB565 picture, about 25 times per second.
2. Classify each pixel in **HSV** as RED, GREEN or nothing (calibrated thresholds: red hue ≥ 340° or ≤ 15°, green hue 80–160°, with minimum saturation and brightness).
3. Group pixels into blobs with a **BFS** flood fill; blobs under 30 pixels are noise.
4. Split two touching pillars of the same colour and keep only the **front** one (lowest base in the picture).
5. The **closest pillar** is the one with the lowest base. Red → pass on its right; green → pass on its left. The pillar is on the correct side once it leaves the central zone (±28 px) on that side.

Only a horizontal band of the picture is searched (rows 53–78 of 120): the camera's wide view could see red and green objects outside the field, such as a flag behind the track, and read them as pillars.

**Pillar Vision Lab** ([src/obstacle-challenge/pillar_viewer](src/obstacle-challenge/pillar_viewer)) is our calibration tool: a web page that receives every picture over USB, shows the colour mask, runs the same algorithm in JavaScript, checks that it matches the camera board exactly, and lets us tune every parameter live.

Next: floor-line detection (orange/blue), parking-lot detection (magenta) and the camera → Nano link.

---

## How to build, compile and upload the code

### 1. Software

- **Arduino IDE 2.x**
- Board packages (*Tools → Board → Boards Manager*):
  - **Arduino ESP32 Boards** (by Arduino) — for the Nano ESP32
  - **esp32** (by Espressif) — for the ESP32-S3 CAM
- Library (*Sketch → Include Library → Manage Libraries*): **VL53L1X** by **Pololu** (for the TOF400C). The BMI160 gyroscope and the servo need **no library**: they are driven directly in our code.

### 2. Nano ESP32 sketches

1. Open the sketch (`File → Open`, choose the `.ino`).
2. *Tools → Board → Arduino ESP32 Boards → **Arduino Nano ESP32***, and choose its port.
3. **Battery off**, USB connected. Click *Upload*.
4. If the upload fails with `dfu-util` errors: double-press **RESET** (the LED "breathes" green), select the port again and upload.
5. Open the Serial Monitor at **115200** baud.

### 3. ESP32-S3 CAM sketches

| Tools option | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| PSRAM | **OPI PSRAM** (without it the camera fails with `frame buffer malloc failed`) |

If the upload stays at `Connecting...`: hold **BOOT**, press and release **RESET**, release **BOOT**. Only one program can use the port at a time: close the Serial Monitor or Pillar Vision Lab before uploading.

### 4. Pillar Vision Lab

Upload `pillar_viewer.ino` to the camera, open the local file `viewer.html` in **Chrome or Edge** (Web Serial is needed), press **Connect** and choose the camera port.

---

## Testing workflow

Every component is tested alone before it is joined to the rest:

1. **Hardware tests** in [src/hardware-tests](src/hardware-tests): TOF sensors, gyroscope, motor, servo, camera. Each prints its readings to the Serial Monitor.
2. **Hand tests:** the car (or its sensors) is moved by hand to check the decision logic before it drives on its own. Pillar detection was validated this way with the camera mounted on the robot.
3. **Measured experiments** for parameters, for example the TOF ROI test (journal §8.2) and the colour thresholds tuned live in Pillar Vision Lab.
4. **Driving tests** on the track (first one: `drive_until_left_open`).

Every result, failure and decision is written down in the [engineering journal](docs/ENGINEERING_JOURNAL.md).

---

## Team

- **Henry Riera Loza** — software and electronics (code, sensors, vision, this repository).
- Mechanical design and chassis — teammate; the chassis files go in [models/](models).
