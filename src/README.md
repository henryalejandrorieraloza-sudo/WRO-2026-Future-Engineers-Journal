# Source code

All firmware for the car. Each Arduino sketch lives in a folder with the same name as its `.ino` file, as the Arduino IDE requires: open it with **File → Open** and choose the `.ino` file.

## 1. System overview

The car runs two programmable boards that communicate over a wired UART link. **Every sketch that drives the car (`open_challenge`, `straight_pillars`, `line_test` and every hardware test) targets the classic Arduino Nano.** The only sketches that do not are the two that run on the camera board (`obstacle_camera`, `camera_test`): an ATmega328P has 2 KB of RAM, while one 320 × 240 picture is 150 KB, so the vision has to stay on its own board.

| Board | Role |
|---|---|
| **Arduino Nano** (classic, ATmega328P) | Main controller: four TOF distance sensors, BMI160 gyroscope, motor encoder, drive motor (TB6612FNG) and steering servo |
| **ESP32-S3-CAM** (emakefun, OV2640) | Vision: detects the red and green pillars (position in cm), the magenta parking walls and the orange and blue floor lines, and sends the result to the Nano |

### Pin map of the Arduino Nano

| Function | Pin |
|---|---|
| Steering servo | D9 |
| Drive motor driver (TB6612FNG) | PWMA D6 · AIN1 D7 · AIN2 D8 · STBY tied to +5 V on the PCB (always enabled; `rear_motor_loop_test` also drives D5 high, harmless) |
| Motor encoder | D2 · D3 |
| Start button | D10 |
| I²C bus (BMI160 and the four TOF sensors) | SDA A4 · SCL A5 |
| TOF `XSHUT` lines | FRONT A1 · BACK A0 · RIGHT A2 · LEFT A3 |
| UART to the camera board | D0 · D1 |

The full wiring, power budget and schematic are in [`../schemes`](../schemes/README.md).

## 2. Directory layout

```
src/
├── hardware-tests/       Component tests (motor + encoder, steering servo, four TOF + gyroscope, camera)
├── open-challenge/       open_challenge: the Open Challenge program
├── obstacle-challenge/   Camera vision, pillar avoidance, floor-line tests
└── tools/                Browser tools that run on a PC (camera viewer, scenario roulette)
```

Earlier Open Challenge sketches and older hardware tests are not in `main` any more; they are listed in [section 3](#retired-hardware-tests) and [section 4](#4-open-challenge-code) with a link to each one in the git history.

## 3. Hardware tests (`hardware-tests/`)

Each sketch validates one component on its own before it is combined with the others.

| Sketch | Board | Component | Purpose |
|---|---|---|---|
| [`motor_encoder_test`](hardware-tests/motor_encoder_test) | Nano | Drive motor (TB6612FNG) and encoder | Press the start button (D10): slow forward, fast forward, brake, reverse, stopped. Prints pulses, wheel turns and RPM every 200 ms, to check the motor direction (`MOTOR_REVERSED`), the encoder direction and the pulses per turn (`PULSES_PER_REV` must be measured for the real motor) |
| [`servo_test`](hardware-tests/servo_test) | Nano | Steering servo (D9) | After the start button, sweeps the servo between `MIN_ANGLE` (10°) and `MAX_ANGLE` (170°) in steps of 2°, printing the angle and pausing at the centre and at both limits, to find the real centre and the mechanical limits without pushing the rack |
| [`tof_and_gyro_test`](hardware-tests/tof_and_gyro_test) | Nano | 4 × TOF400C (VL53L1X) and BMI160 | Gives each TOF its own address through `XSHUT` (LEFT A3 = 0x30, RIGHT A2 = 0x31, REAR A0 = 0x32, FRONT A1 = 0x33), ranges in Short mode, reads the BMI160 by registers (chip ID 0xD1 at 0x69), calibrates the gyro offset (200 samples) and integrates the yaw. Prints `FRONT | LEFT | RIGHT | REAR | YAW`. Library: *VL53L1X* by Pololu |
| [`camera_test`](hardware-tests/camera_test) | ESP32-S3-CAM | Camera (OV2640) and vision | The same vision code as `obstacle_camera` (v3.1), kept to test the camera on its own. With `STREAM_TO_PC = true` it sends every picture, the pillar mask and the line mask over USB to [`camera_viewer.html`](tools/camera_viewer/camera_viewer.html); it also sends the 14-field message to the Nano (TX only, GPIO43). Upload it with *PSRAM: OPI PSRAM*, close the Serial Monitor, open the viewer in Chrome or Edge and press *Connect camera* |
| [`gyro_90_test`](hardware-tests/gyro_90_test) | Nano | BMI160 (reuses `straight_pillars.ino`'s own register driver) | Stand-alone hand-turn calibration: press START (D10) with the car still to zero the integrated yaw, turn the car exactly 90° left by hand, read the printed yaw value Y, then set `YAW_SCALE = 90/abs(Y)` (and `YAW_REVERSED = true` if Y came out negative) in `straight_pillars.ino`. Measured 21 Nov on this chassis: Y = 85 → `YAW_SCALE = 1.06`, `YAW_REVERSED = false` |

### Retired hardware tests

Older component tests, replaced on 20 November by the three tests above. Each one is still available in the git history (the link opens the folder at the last commit that contained it). The first seven were already removed on 11 November.

| Sketch | Board | Component | Purpose |
|---|---|---|---|
| [`tof_gyro_sensors_test`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/tof_gyro_sensors_test) | Nano | 4 × TOF400C and BMI160 | Reads all sensors at once: assigns a unique I²C address to each TOF through `XSHUT`, ranges in Short mode (cm), checks the BMI160 chip ID at `0x69`, calibrates the gyro offset and integrates yaw. Also scans the I²C bus. Output: `LEFT:… RIGHT:… BACK:… FRONT:… GZ:… YAW:…` (compatible with the Serial Plotter). Library: *VL53L1X* by Pololu |
| [`tof_left_right_print`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/tof_left_right_print) | Nano | Side TOF sensors | Prints the LEFT and RIGHT readings together with the status of each reading (`range valid`, `signal fail`, …), about 30 times per second. Diagnostic tool (journal Phase 10) |
| [`tof_roi_budget_test`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/tof_roi_budget_test) | Nano | TOF sensor | Experiment. With the car in front of a real wall, the operator types the measured distance and the sketch tries every region-of-interest size and timing budget, reporting the number of valid, signal-fail and other readings plus median, minimum, maximum and spread. Result: with the sensor mounted straight, ROI 16 × 8 measures the black wall to within 1–2 cm up to about 45 cm |
| [`servo_simple`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/servo_simple) | Nano | Steering servo | Sweep left, centre, right with the pulse generated in software (no library) |
| [`servo_only_test`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/servo_only_test) | Nano | Steering (rack and pinion) | Slow sweep centre, left, centre, right using `Servo.h`, to check that the servo never pushes against the mechanical limits |
| [`servo_calibration`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/servo_calibration) | Nano | Steering servo | Interactive calibration: type an angle in the Serial Monitor (or `a` / `d` for ±1°) to find the real steering centre and limits safely |
| [`drive_until_left_open`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/06f5685ccd94ee9dd8e3f550c635fe16d0a5c412/src/hardware-tests/drive_until_left_open) | Nano | Drive motor and left TOF | First driving test: drives straight and brakes when the LEFT TOF reads more than 170 cm (the left wall has ended) |
| [`rear_motor_loop_test`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/685ccdc1dd669fb5e4a6d7ca3f2aee6515969397/src/hardware-tests/rear_motor_loop_test) | Nano | Drive motor and TB6612FNG | Cycle: forward, stop, backward, stop (replaced by `motor_encoder_test`) |
| [`tof_servo_sensors_test`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/685ccdc1dd669fb5e4a6d7ca3f2aee6515969397/src/hardware-tests/tof_servo_sensors_test) | Nano | Side TOF sensors and steering servo | Servo sweep ±25° around the rack centre while printing LEFT and RIGHT in cm (replaced by `servo_test` and `tof_and_gyro_test`) |
| [`camera_webserver_emakefun`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/685ccdc1dd669fb5e4a6d7ca3f2aee6515969397/src/hardware-tests/camera_webserver_emakefun) | ESP32-S3-CAM | Camera | Manufacturer's web server, bench check only; needs Wi-Fi, never used in a round (rule 11.10) |

## 4. Open Challenge code

### Current program

| Sketch | Board | Status | Description |
|---|---|---|---|
| [`open_challenge`](open-challenge/open_challenge) | Nano | Added 20 Nov; three-lap results still to record | The Open Challenge program: gyro heading hold (P + I) with a fixed distance to the inner wall or the corridor centre, corner detection by a jump of the inner-side TOF, 90° turns by predicted yaw, `LAPS × 4` turns, and a stop at the **start position** measured with the encoder on lap 2 (`LSTART − D1`). Uses only the left and right TOF (front and rear held in reset). Press START (D10) or send `g` + Enter; `s` + Enter is an emergency stop; battery on first, then USB. Settings in three blocks at the top (run, tuning, car constants). Design in [README §8.0](../README.md#80-the-open-challenge-program-open_challenge-20-november) |

### Earlier sketches (history)

Removed from `main` on 20 November. **They were not lost**: each link opens the sketch at commit `f5aa0e6`, and `git checkout f5aa0e6 -- src/open-challenge` restores the folder. They show the development process (the rubric asks for iterations), in the order they were developed; each version builds on the previous one. The strategy of those versions is in [README §8.1–8.7](../README.md#81-idea-design-of-the-earlier-versions).

| Sketch | Status | Description |
|---|---|---|
| [`wall_follow_nearest_pid`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/wall_follow_nearest_pid) | Superseded | Averages 10 readings of each side TOF, selects the **nearest** wall and keeps that distance with a PID while driving forward |
| [`wall_keep_both_pid`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/wall_keep_both_pid) | Superseded | Keeps the left and right distances measured at start-up (does not centre the car): drift = (errorL − errorR) / 2, corrected with a PID |
| [`wall_follow_nearest_gyro`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/wall_follow_nearest_gyro) | Validated, 20 Oct | Keeps the distance to the nearest wall with a cascade controller: the distance error sets a small target heading (maximum 8°) and the BMI160 gyroscope drives the steering. Removes the zig-zag and S-shaped oscillation of the distance-only PID |
| [`wall_follow_gyro_corner_front`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/wall_follow_gyro_corner_front) | In testing, 20 Oct | Adds corner handling to the gyro wall following. When a side TOF reads more than `OPEN_THRESHOLD_MM` (3000 mm) three times in a row, `cornerDetected` is set and the turn side is stored. The car then drives straight using the gyroscope and brakes (AIN1 = AIN2 = HIGH) when the FRONT TOF reads less than `FRONT_STOP_MM`. This separates *detecting* the corner (side sensors, which are angled about 30° forward and see the opening early) from *deciding when to turn* (front sensor) |
| [`turn_left_90_pid_brake`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/turn_left_90_pid_brake) | Validated, 21 Oct | 90° left turn using the gyroscope: wheels at full left, then a PD controller on the yaw straightens them as the car approaches 90°. Slows down over the last 25°, brakes a few degrees early to absorb inertia (active brake) and stays still. Gyro at 400 Hz, 1000-sample calibration. Tuned with `STOP_LEAD_DEG` and `YAW_SCALE` using the printed final error |
| [`corner_turn_front_trigger`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/corner_turn_front_trigger) | Validated, 21 Oct | First complete straight and corner. Gyro wall following on the nearest wall (`MAX_HEADING_DEG = 5`, `MOTOR_SPEED = 50`). When a side TOF reads more than 2400 mm three times in a row, the opening side is stored and the PID switches to the opposite wall without braking. A FRONT reading below 1400 mm (any sensor status accepted) starts the validated 90° turn toward the opening, or toward the farther side if no opening was seen, and the car stays braked. Safety: after 40 s of driving it compares both sides and turns anyway. LED D13 on means the front TOF is not giving a usable reading |
| [`corner_turn_timed`](https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal/tree/f5aa0e6825ab4aaec6edc42665a4e4d067a71765/src/open-challenge/corner_turn_timed) | Validated, 21 Oct | Same wall following without the front sensor, plus an **integral term on the heading loop** (`KI_HEADING = 2.5`) so the car no longer drifts to one side. It keeps the last 5 readings of each wall; when an opening is detected it switches to the opposite wall at the average of those readings (no drift when switching). After `RUN_MS` (1.8 s) it brakes: if an opening was seen it turns toward it, otherwise it averages 10 readings per side and turns toward the farther one |

Later iterations (one-corner version, 12-turn loop, wall re-alignment) are documented in the [engineering journal](../docs/ENGINEERING_JOURNAL.md), section 11, and ended in `open_challenge`.

## 5. Obstacle Challenge (`obstacle-challenge/`)

| File | Board | Status | Description |
|---|---|---|---|
| [`obstacle_camera`](obstacle-challenge/obstacle_camera) | ESP32-S3-CAM | Current vision code (v3.1), colour thresholds retuned 19 Nov (orange H 55–90, blue H 165–215; not yet validated on the track) | 320 × 240 image, camera locked after 5 s, HSV classification, red / magenta majority vote. Pillars are detected on 4 × 4 blocks with a solid-shape and plausibility check and a temporal filter, and their distance ahead and to the side is computed in **cm** (pinhole model, README §9.3). **Orange and blue floor lines** are detected with a long-and-thin test and a floor-above check (white floor above the line, so the edge of the black wall is rejected); blue starts at hue 165° (the floor sits at H 111–125, the blue line at 191–208). Earlier numbers (blue from 110°, orange H 29–52) were for the yellow light of October (README §9.6). Red pillars are accepted only up to V 165, so the bright orange line is not taken for a red pillar. Sends `$color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow` to the Nano at 115200 baud. With `STREAM_TO_PC = true` it also streams pictures to Pillar Vision Lab; with `false` it prints a status every 0.5 s (why each line was accepted or rejected, and a **colour probe** of two fixed boxes to recalibrate under any light). **`WIFI_STREAM` is on by default**: whenever the camera is powered, `http://<printed IP>/` shows a live MJPEG view with the same segmentation `camera_viewer.html` draws (pillar blocks blended red/green, accepted lines painted solid, a yellow box on the chosen pillar) painted straight onto the picture, no USB cable or separate viewer needed |
| [`straight_pillars`](obstacle-challenge/straight_pillars) | Nano | Validated on the track, 21 Nov | One straight with pillars, now also stopping on the floor line. Builds an odometry map of the straight (encoder 9.27 ticks per cm, measured with the motor running, plus a hand-turn-calibrated gyro: `YAW_SCALE = 1.06`), places every pillar on the map and sets a lateral goal 17 cm from the pillar on its correct side. The target heading is `atan2(error, distance)` (maximum 60°), followed by a gyro PD controller and the servo. Aims at the next pillar once the camera is 45 cm past the current one's centre (raised from 5 cm on 21 Nov to fix a crash right after dodging — see `PARAMETERS.md §10`). **On `blueFound`/`orangeFound`, ignores every pillar from then on, recenters to the home lane, and drives to the wall** using the front TOF sensor (same wiring as `open_challenge`). Only brakes once the front sensor reads under 120 cm **and** the car is within 2° of its starting yaw (`FRONT_STOP_MM`, `STOP_YAW_TOL_DEG`); if it's close to the wall but still diagonal, it steers at full lock instead of stopping crooked (`STOP_ON_LINE`, `handleLineStop`). Waits for START (D10) and stops after `RUN_CM` or if the camera stops sending messages (README §9.4–9.5) |
| [`obstacle_go_out_of_parking`](obstacle-challenge/obstacle_go_out_of_parking) | Nano | Added 20 Nov; not yet tested on the track | Leaves the parking space. Two ToF sensors (left and right) measure the free space; the car turns 45° by gyro toward the side with more room, straightens, counter-steers back to 10°, stops, waits 500 ms and reverses `REVERSE_TARGET_TICKS` encoder ticks (five wheel turns, to be measured with `CALIBRATE_ENCODER`). Stops with a printed reason on any sensor, gyro, encoder or timeout error |
| [`line_test`](obstacle-challenge/line_test) | Nano | Measurement, 27 Oct | Motor off. Prints five times per second whether the camera sees the orange and blue lines, their lowest row and their row straight ahead, how many messages contained them, and the pillar. Used to measure the row-to-distance table of README §9.6 |

## 6. Tools (`tools/`)

| File | Description |
|---|---|
| [`camera_viewer/camera_viewer.html`](tools/camera_viewer/camera_viewer.html) | **Camera viewer**, the page we use to test the camera board. Shows the picture exactly as the ESP32-S3-CAM sends it over USB, the pillar mask, the orange / blue line mask, the rows that are ignored, the status of each floor line (found, why rejected, rows) and the RGB / HSV of the pixel under the mouse. Upload `camera_test` (or `obstacle_camera`) with `STREAM_TO_PC = true`, close the Serial Monitor, open the file in Chrome or Edge, press *Connect camera*. Reads the 37-byte packet of the camera sketch, draws the accepted orange / blue lines and the pillar mask, and has an *IGNORED top* slider (default 115, drawing only) |
| [`scenario-roulette/index.html`](tools/scenario-roulette/index.html) | **WRO 2026 Scenario Roulette** ([open online](https://henryalejandrorieraloza-sudo.github.io/WRO-2026-Future-Engineers-Journal/src/tools/scenario-roulette/)). Draws valid Open and Obstacle track scenarios with the official procedure and draws the field to scale, so realistic tests can be set up. Also logs Open Challenge attempts with the official score |

## 7. Arduino IDE settings

### Arduino Nano (classic, ATmega328P)

| Option | Value |
|---|---|
| Board package | Arduino AVR Boards **1.8.3 or newer** (built in; the sketches call `Wire.setWireTimeout`, which older versions do not have) |
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
