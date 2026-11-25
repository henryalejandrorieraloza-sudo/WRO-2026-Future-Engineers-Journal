# Source code

All firmware for the car. Each Arduino sketch lives in a folder with the same name as its `.ino` file, as the Arduino IDE requires: open it with **File → Open** and choose the `.ino` file.

## 1. System overview

The car runs two programmable boards that communicate over a wired UART link. **Every sketch that drives the car (`open_challenge`, `obstacle_challenge` and every hardware test) targets the classic Arduino Nano.** The only sketches that do not are the two that run on the camera board (`obstacle_camera_v3_2`, `camera_test`): an ATmega328P has 2 KB of RAM, while one 320 × 240 picture is 150 KB, so the vision has to stay on its own board.

| Board | Role |
|---|---|
| **Arduino Nano** (classic, ATmega328P) | Main controller: four TOF distance sensors, BMI160 gyroscope, motor encoder, drive motor (TB6612FNG) and steering servo |
| **ESP32-S3-CAM** (emakefun, OV2640) | Vision: detects the red and green pillars (position in cm), the magenta parking walls and the orange and blue floor lines, and sends the result to the Nano |

### Pin map of the Arduino Nano

| Function | Pin |
|---|---|
| Steering servo | D9 |
| Drive motor driver (TB6612FNG) | PWMA D6 · AIN1 D7 · AIN2 D8 · STBY tied to +5 V on the PCB (always enabled; `turn_measure_moving` also drives D5 high, harmless) |
| Motor encoder | D2 · D3 |
| Start button | D10 |
| I²C bus (BMI160 and the four TOF sensors) | SDA A4 · SCL A5 |
| TOF `XSHUT` lines | FRONT A1 · BACK A0 · RIGHT A2 · LEFT A3 |
| UART from the camera board (one way) | D0 (RX) |

The full wiring, power budget and schematic are in [`../schemes`](../schemes/README.md).

## 2. Directory layout

```
src/
├── hardware-tests/       Component tests (motor + encoder, steering servo, four TOF + gyroscope, camera)
├── open-challenge/       open_challenge: the Open Challenge program
├── obstacle-challenge/   Obstacle Challenge: the driving program (with the parking exit) and the camera vision
└── tools/                Browser tools that run on a PC (camera viewer, scenario roulette)
```

Older hardware tests, earlier Open Challenge sketches and the earlier Obstacle sketches are not in `main` any more; they remain in the git history.

## 3. Hardware tests (`hardware-tests/`)

Each sketch validates one component on its own before it is combined with the others.

| Sketch | Board | Component | Purpose |
|---|---|---|---|
| [`motor_encoder_test`](hardware-tests/motor_encoder_test) | Nano | Drive motor (TB6612FNG) and encoder | Press the start button (D10): slow forward, fast forward, brake, reverse, stopped. Prints pulses, wheel turns and RPM every 200 ms, to check the motor direction (`MOTOR_REVERSED`), the encoder direction and the pulses per turn (`PULSES_PER_REV` must be measured for the real motor) |
| [`servo_test`](hardware-tests/servo_test) | Nano | Steering servo (D9) | After the start button, sweeps the servo between `MIN_ANGLE` (10°) and `MAX_ANGLE` (170°) in steps of 2°, printing the angle and pausing at the centre and at both limits, to find the real centre and the mechanical limits without pushing the rack. `CENTER` is 85, the value of the driving programs |
| [`tof_and_gyro_test`](hardware-tests/tof_and_gyro_test) | Nano | 4 × TOF400C (VL53L1X) and BMI160 | Gives each TOF its own address through `XSHUT` (LEFT A3 = 0x30, RIGHT A2 = 0x31, REAR A0 = 0x32, FRONT A1 = 0x33; the driving programs use 0x2A–0x2D, the test uses its own set), ranges in Short mode, reads the BMI160 by registers (chip ID 0xD1 at 0x69), calibrates the gyro offset (200 samples) and integrates the yaw. Prints `FRONT | LEFT | RIGHT | REAR | YAW`. Library: *VL53L1X* by Pololu |
| [`camera_test`](hardware-tests/camera_test) | ESP32-S3-CAM | Camera (OV2640) and vision | The v3.1 vision code at 320 × 240 (the competition camera `obstacle_camera_v3_2` runs the same pipeline at 160 × 120), kept to test the camera on its own. With `STREAM_TO_PC = true` it sends every picture, the pillar mask and the line mask over USB to [`camera_viewer.html`](tools/camera_viewer/camera_viewer.html); it also sends the 14-field message to the Nano (TX only, GPIO43). Upload it with *PSRAM: OPI PSRAM*, close the Serial Monitor, open the viewer in Chrome or Edge and press *Connect camera* |
| [`gyro_90_test`](hardware-tests/gyro_90_test) | Nano | BMI160 (same register-level driver idea as the driving programs) | Stand-alone hand-turn calibration: press START (D10) with the car still to zero the integrated yaw, turn the car exactly 90° left by hand, read the printed yaw value Y, then set `YAW_SCALE = 90/abs(Y)` (and `YAW_REVERSED = true` if Y came out negative) in the earlier `straight_pillars.ino` sketch (now only in the git history; the current programs use `GYRO_SCALE = 1.007`). Measured 21 Nov on this chassis: Y = 85 → `YAW_SCALE = 1.06`, `YAW_REVERSED = false` |
| [`turn_measure_moving`](hardware-tests/turn_measure_moving) | Nano | Turn of 90° while moving (BMI160 + encoder + steering) | Drives 40 cm, turns 90° with the gyro PD without stopping, brakes, and saves each run in EEPROM (up to 10). Prints the saved runs over USB. Produced the six in-motion turn results of `docs/TESTS.md`; its constants are those of 28 Oct (see its README) |

## 4. Open Challenge code

### Current program

| Sketch | Board | Status | Description |
|---|---|---|---|
| [`open_challenge`](open-challenge/open_challenge) | Nano | Added 20 Nov; three-lap run recorded 22 Nov (journal §19.2); constants `TICKS_PER_CM` 9.27 and `STEER_CENTER` 85 measured on 21 Nov | The Open Challenge program: gyro heading hold (P + I) with a fixed distance to the inner wall or the corridor centre, corner detection by a jump of the inner-side TOF, 90° turns by predicted yaw, `LAPS × 4` turns, and a stop at the **start position** measured with the encoder on lap 2 (`LSTART − D1`). Uses only the left and right TOF (front and rear held in reset). Press START (D10) or send `g` + Enter; `s` + Enter is an emergency stop; battery on first, then USB. Settings in three blocks at the top (run, tuning, car constants). Design in [README §8.0](../README.md#80-the-open-challenge-program-open_challenge-20-november) |

Later iterations (one-corner version, 12-turn loop, wall re-alignment) are documented in the [engineering journal](../docs/ENGINEERING_JOURNAL.md), section 11, and ended in `open_challenge`.

## 5. Obstacle Challenge (`obstacle-challenge/`)

The folder holds the two driving programs of the round, one per challenge. Each has its own README with the constants, the flow and the measurements. Earlier Obstacle sketches are in the git history, not in `main`.

| Folder | Board | Status | Description |
|---|---|---|---|
| [`obstacle_challenge`](obstacle-challenge/obstacle_challenge) | Nano | Uploaded 24 Nov (open challenge laps, pillar dodging and wall centring, v1). It replaces the 23 Nov version, which the team reported working on the car. Validated on the car in the runs logged in `docs/TESTS.md` | **The driving program.** Follows the floor lines for three laps: the first line sets the direction (blue = left, orange = right), each corner is a 90° turn in reverse (gyro, front and rear ToF). Dodges red pillars on the right and green pillars on the left, using an odometry map of the straight. Centres between the side walls on straights without pillars. Stops inside the starting section after the last line. README: [obstacle_challenge/README.md](obstacle-challenge/obstacle_challenge/README.md) |
| [`obstacle_camera_v3_2`](obstacle-challenge/obstacle_camera_v3_2) | ESP32-S3-CAM | Validated on the car in the runs logged in `docs/TESTS.md`. `FOCAL_PX = 50` is not yet re-measured at 160 × 120. The 160 × 120 frame rate is still to be logged | **The vision.** 160 × 120 picture, camera settings locked after 5 s. Finds the closest red or green pillar and its distance ahead and to the side in cm (pinhole model), the magenta parking walls, and the orange and blue floor lines (long-and-thin test, white floor above the line). Sends one text line per picture to the Nano at 115200 baud. The Nano reads its first 12 fields. README: [obstacle_camera_v3_2/README.md](obstacle-challenge/obstacle_camera_v3_2/README.md) |

Parking at the end of the round (after the third lap) is not in the code yet; see [root README §9.7](../README.md#97-parking).

## 6. Tools (`tools/`)

| File | Description |
|---|---|
| [`camera_viewer/camera_viewer.html`](tools/camera_viewer/camera_viewer.html) | **Camera viewer**, the page we use to test the camera board. Shows the picture exactly as the ESP32-S3-CAM sends it over USB, the pillar mask, the orange / blue line mask, the rows that are ignored, the status of each floor line (found, why rejected, rows) and the RGB / HSV of the pixel under the mouse. Upload `camera_test` with `STREAM_TO_PC = true`, close the Serial Monitor, open the file in Chrome or Edge, press *Connect camera*. The page is drawn for 320 × 240 pictures (`camera_test`); the 160 × 120 competition camera is checked with its Serial Monitor status lines instead. Reads the 37-byte packet of the camera sketch, draws the accepted orange / blue lines and the pillar mask, and has an *IGNORED top* slider (default 115, drawing only) |
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
