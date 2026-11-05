# Source code

All firmware for the car. Each Arduino sketch lives in a folder with the same name as its `.ino` file, as the Arduino IDE requires: open it with **File → Open** and choose the `.ino` file.

## 1. System overview

The car runs two programmable boards that communicate over a wired UART link.

| Board | Role |
|---|---|
| **Arduino Nano** (classic, ATmega328P) | Main controller: four TOF distance sensors, BMI160 gyroscope, motor encoder, drive motor (TB6612FNG) and steering servo |
| **ESP32-S3-CAM** (emakefun, OV2640) | Vision: detects the red and green pillars (position in cm), the magenta parking walls and the orange and blue floor lines, and sends the result to the Nano |

### Pin map of the Arduino Nano

| Function | Pin |
|---|---|
| Steering servo | D9 |
| Drive motor driver (TB6612FNG) | PWMA D6 · AIN1 D7 · AIN2 D8 · STBY D5 |
| Motor encoder | D2 · D3 |
| Start button | D10 |
| I²C bus (BMI160 and the four TOF sensors) | SDA A4 · SCL A5 |
| TOF `XSHUT` lines | FRONT A1 · BACK A0 · RIGHT A2 · LEFT A3 |
| UART to the camera board | D0 · D1 |

The full wiring, power budget and schematic are in [`../schemes`](../schemes/README.md).

## 2. Directory layout

```
src/
├── hardware-tests/       Component tests and measurement tools (one sketch per part)
├── open-challenge/       Driving algorithms for the Open Challenge, in order of development
├── obstacle-challenge/   Camera vision, pillar avoidance, floor-line tests, Pillar Vision Lab
└── tools/                Browser tools that run on a PC
```

## 3. Hardware tests (`hardware-tests/`)

Each sketch validates one component on its own before it is combined with the others.

| Sketch | Board | Component | Purpose |
|---|---|---|---|
| [`tof_gyro_sensors_test`](hardware-tests/tof_gyro_sensors_test) | Nano | 4 × TOF400C and BMI160 | Reads all sensors at once: assigns a unique I²C address to each TOF through `XSHUT`, ranges in Short mode (cm), checks the BMI160 chip ID at `0x69`, calibrates the gyro offset and integrates yaw. Also scans the I²C bus. Output: `LEFT:… RIGHT:… BACK:… FRONT:… GZ:… YAW:…` (compatible with the Serial Plotter). Library: *VL53L1X* by Pololu |
| [`tof_left_right_print`](hardware-tests/tof_left_right_print) | Nano | Side TOF sensors | Prints the LEFT and RIGHT readings together with the status of each reading (`range valid`, `signal fail`, …), about 30 times per second. Diagnostic tool (journal Phase 10) |
| [`tof_roi_budget_test`](hardware-tests/tof_roi_budget_test) | Nano | TOF sensor | Experiment. With the car in front of a real wall, the operator types the measured distance and the sketch tries every region-of-interest size and timing budget, reporting the number of valid, signal-fail and other readings plus median, minimum, maximum and spread. Result: with the sensor mounted straight, ROI 16 × 8 measures the black wall to within 1–2 cm up to about 45 cm |
| [`rear_motor_loop_test`](hardware-tests/rear_motor_loop_test) | Nano | Drive motor and TB6612FNG | Cycle: forward, stop, backward, stop |
| [`servo_simple`](hardware-tests/servo_simple) | Nano | Steering servo | Sweep left, centre, right with the pulse generated in software (no library) |
| [`servo_only_test`](hardware-tests/servo_only_test) | Nano | Steering (rack and pinion) | Slow sweep centre, left, centre, right using `Servo.h`, to check that the servo never pushes against the mechanical limits |
| [`servo_calibration`](hardware-tests/servo_calibration) | Nano | Steering servo | Interactive calibration: type an angle in the Serial Monitor (or `a` / `d` for ±1°) to find the real steering centre and limits safely |
| [`drive_until_left_open`](hardware-tests/drive_until_left_open) | Nano | Drive motor and left TOF | First driving test: drives straight and brakes when the LEFT TOF reads more than 170 cm (the left wall has ended) |
| [`camera_webserver_emakefun`](hardware-tests/camera_webserver_emakefun) | ESP32-S3-CAM | Camera | Manufacturer's camera web server, used only to check the camera on the bench. Needs a 2.4 GHz Wi-Fi network and the other files of the manufacturer's example. It is never used in a round: Wi-Fi is not allowed during competition rounds (rule 11.10) |

## 4. Open Challenge algorithms (`open-challenge/`)

Listed in the order they were developed. Each version builds on the previous one.

| Sketch | Status | Description |
|---|---|---|
| [`wall_follow_nearest_pid`](open-challenge/wall_follow_nearest_pid) | Superseded | Averages 10 readings of each side TOF, selects the **nearest** wall and keeps that distance with a PID while driving forward |
| [`wall_keep_both_pid`](open-challenge/wall_keep_both_pid) | Superseded | Keeps the left and right distances measured at start-up (does not centre the car): drift = (errorL − errorR) / 2, corrected with a PID |
| [`wall_follow_nearest_gyro`](open-challenge/wall_follow_nearest_gyro) | Validated, 20 Oct | Keeps the distance to the nearest wall with a cascade controller: the distance error sets a small target heading (maximum 8°) and the BMI160 gyroscope drives the steering. Removes the zig-zag and S-shaped oscillation of the distance-only PID |
| [`wall_follow_gyro_corner_front`](open-challenge/wall_follow_gyro_corner_front) | In testing, 20 Oct | Adds corner handling to the gyro wall following. When a side TOF reads more than `OPEN_THRESHOLD_MM` (3000 mm) three times in a row, `cornerDetected` is set and the turn side is stored. The car then drives straight using the gyroscope and brakes (AIN1 = AIN2 = HIGH) when the FRONT TOF reads less than `FRONT_STOP_MM`. This separates *detecting* the corner (side sensors, which are angled about 30° forward and see the opening early) from *deciding when to turn* (front sensor) |
| [`turn_left_90_pid_brake`](open-challenge/turn_left_90_pid_brake) | Validated, 21 Oct | 90° left turn using the gyroscope: wheels at full left, then a PD controller on the yaw straightens them as the car approaches 90°. Slows down over the last 25°, brakes a few degrees early to absorb inertia (active brake) and stays still. Gyro at 400 Hz, 1000-sample calibration. Tuned with `STOP_LEAD_DEG` and `YAW_SCALE` using the printed final error |
| [`corner_turn_front_trigger`](open-challenge/corner_turn_front_trigger) | Validated, 21 Oct | First complete straight and corner. Gyro wall following on the nearest wall (`MAX_HEADING_DEG = 5`, `MOTOR_SPEED = 50`). When a side TOF reads more than 2400 mm three times in a row, the opening side is stored and the PID switches to the opposite wall without braking. A FRONT reading below 1400 mm (any sensor status accepted) starts the validated 90° turn toward the opening, or toward the farther side if no opening was seen, and the car stays braked. Safety: after 40 s of driving it compares both sides and turns anyway. LED D13 on means the front TOF is not giving a usable reading |
| [`corner_turn_timed`](open-challenge/corner_turn_timed) | Validated, 21 Oct | Same wall following without the front sensor, plus an **integral term on the heading loop** (`KI_HEADING = 2.5`) so the car no longer drifts to one side. It keeps the last 5 readings of each wall; when an opening is detected it switches to the opposite wall at the average of those readings (no drift when switching). After `RUN_MS` (1.8 s) it brakes: if an opening was seen it turns toward it, otherwise it averages 10 readings per side and turns toward the farther one |

Later iterations (one-corner version, 12-turn loop, wall re-alignment) are documented in the [engineering journal](../docs/ENGINEERING_JOURNAL.md), section 11.

## 5. Obstacle Challenge (`obstacle-challenge/`)

| File | Board | Status | Description |
|---|---|---|---|
| [`obstacle_camera`](obstacle-challenge/obstacle_camera) | ESP32-S3-CAM | Current vision code (v3.1), validated 28 Oct | 320 × 240 image, camera locked after 5 s, HSV classification, red / magenta majority vote. Pillars are detected on 4 × 4 blocks with a solid-shape and plausibility check and a temporal filter, and their distance ahead and to the side is computed in **cm** (pinhole model, README §9.3). **Orange and blue floor lines** are detected with a long-and-thin test and a floor-above check (white floor above the line, so the edge of the black wall is rejected); blue starts at hue 110° so the floor is never classified as blue (README §9.6). No false lines on the empty floor or the wall; orange is seen from 16 cm, blue from 9 cm. Sends `$color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow` to the Nano at 115200 baud. With `STREAM_TO_PC = true` it also streams pictures to Pillar Vision Lab; with `false` it prints a status every 0.5 s (why each line was accepted or rejected, and a **colour probe** of two fixed boxes to recalibrate under any light) |
| [`straight_pillars`](obstacle-challenge/straight_pillars) | Nano | Validated, 27 Oct (all cases pass) | One straight with pillars. Builds an odometry map of the straight (encoder 15.1 ticks per cm plus gyro), places every pillar on the map and sets a lateral goal 17 cm from the pillar on its correct side. The target heading is `atan2(error, distance)` (maximum 45°), followed by a gyro PD controller and the servo. Aims at the next pillar as soon as the camera passes the current one and clears it 5 cm after its centre. Waits for START (D10) and stops after `RUN_CM` or if the camera stops sending messages (README §9.4–9.5) |
| [`line_test`](obstacle-challenge/line_test) | Nano | Measurement, 27 Oct | Motor off. Prints five times per second whether the camera sees the orange and blue lines, their lowest row and their row straight ahead, how many messages contained them, and the pillar. Used to measure the row-to-distance table of README §9.6 |
| [`pillar_viewer/pillar_viewer.ino`](obstacle-challenge/pillar_viewer) | ESP32-S3-CAM | Development tool | Pillar detection (HSV classification, blob search, closest pillar, correct-side check). Sends every picture and its colour mask over USB |
| [`pillar_viewer/viewer.html`](obstacle-challenge/pillar_viewer/viewer.html) | PC (Chrome / Edge) | Development tool | **Pillar Vision Lab**: shows what the camera sees, runs the same algorithm in JavaScript and checks it against the board, and allows every parameter to be tuned live. Open it [online](https://henryalejandrorieraloza-sudo.github.io/WRO-2026-Future-Engineers-Journal/src/obstacle-challenge/pillar_viewer/viewer.html) or as a local file, press *Connect* and choose the camera port (close the Arduino Serial Monitor first) |

## 6. Tools (`tools/`)

| File | Description |
|---|---|
| [`scenario-roulette/index.html`](tools/scenario-roulette/index.html) | **WRO 2026 Scenario Roulette** ([open online](https://henryalejandrorieraloza-sudo.github.io/WRO-2026-Future-Engineers-Journal/src/tools/scenario-roulette/)). Draws valid Open and Obstacle track scenarios with the official procedure and draws the field to scale, so realistic tests can be set up. Also logs Open Challenge attempts with the official score |

## 7. Arduino IDE settings

### Arduino Nano (classic, ATmega328P)

| Option | Value |
|---|---|
| Board package | Arduino AVR Boards (built in) |
| Board | Arduino Nano |
| Processor | ATmega328P. If the upload fails with `not in sync` or `resp=0x00`, select **ATmega328P (Old Bootloader)** |
| Libraries | *VL53L1X* by Pololu. Steering uses the built-in `Servo.h` (500–2500 µs) |

Upload code with the battery switched off.

### ESP32-S3-CAM

| Option | Value |
|---|---|
| Board package | *esp32* by Espressif |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP / 1.5MB SPIFFS) |
| PSRAM | OPI PSRAM |

If the upload stays at `Connecting...`: hold **BOOT**, press and release **RESET**, then release **BOOT**.

## 8. Conventions

- All code, comments and identifiers are in English.
- Tunable values are grouped at the top of each sketch as named constants.
- Sketches that move the car must not depend on a live Serial Monitor connection while moving.
- The status labels (*Validated*, *In testing*, *Superseded*) refer to tests on the real track, recorded in the engineering journal.
