# Straight with pillars (`straight_pillars.ino`)

Arduino Nano sketch for the **straight section of the Obstacle Challenge**: the car drives forward along a straight with red and green pillars, passing each pillar on the correct side, and **brakes as soon as the camera confirms the blue or the orange floor line** (added 21 November 2026 — these lines mark the end of the straight / the start of a corner). It works together with the camera sketch (`obstacle_camera.ino`), which runs on the ESP32-S3-CAM and sends the pillar and floor-line data over a serial link.

The main idea is that the car keeps a small **map** of the straight. The camera sees a pillar only while it is close, so the Nano remembers where each pillar is and keeps steering correctly even when the camera can no longer see it.

**Status (21 Nov 2026):** validated on the track — gyro calibrated with a real 90° hand turn, the "clips the pillar right after dodging it" crash fixed, and the new line-stop tested. See `PARAMETERS.md` in this folder for the full calibration history and the car's measured dimensions.

## Hardware

| Part | Pin / connection |
|---|---|
| Camera (ESP32-S3-CAM) | GPIO43 (TX) to Nano **D0 (RX)**, 115200 baud |
| Encoder | **D3** (interrupt, rising edge, internal pull-up) |
| Start button | **D10** to GND (internal pull-up) |
| Steering servo | **D9** |
| Motor driver TB6612FNG | PWMA **D6**, AIN1 **D7**, AIN2 **D8** |
| Gyroscope BMI160 | I2C: SDA **A4**, SCL **A5**, address 0x68 or 0x69 |
| Front TOF sensor (VL53L1X), used only after the floor line | XSHUT **A1** (same wiring as `open_challenge.ino`); left/right/rear TOF XSHUT (**A3**/**A2**/**A0**) are held low to keep them off the I2C bus |

**Upload note:** the camera is connected to D0 (RX), which blocks the upload. Hold the camera **RESET** button during the whole upload.

The camera must run `obstacle_camera.ino` with `FOCAL_PX = 100`. This sketch reads the first **12** of the camera's 14 message fields (through `orangeFound`), so it can both dodge pillars and brake on either floor line.

## How it works

### 1. Odometry (the map)

- **Distance:** every encoder tick adds `1 / TICKS_PER_CM` cm (15.1 ticks per cm, measured: 980 ticks = 65 cm).
- **Heading:** the gyroscope gives the turning speed around Z. The code subtracts a zero offset measured at start and adds the rotation to `yaw`.
- **Position:** each distance step is added to `(carX, carY)` along the average heading of that step. `x` is forward from the start and `y` is to the left (positive).

### 2. Pillars on the map

Each camera message gives the pillar colour, its distance ahead, and its side offset. The code converts that into map coordinates using the current heading and stores the pillar in one of two slots:

- `current`: the pillar the car is dodging now.
- `nextOne`: a farther pillar that has already been seen.

If a new message is close to a stored pillar of the same colour (within `SAME_PILLAR_CM`), the code only moves the stored position a little (`BLEND`) instead of creating a new pillar.

### 3. Where to go (`headingGoal`)

- **Red pillar:** the car must pass on its **right**, so it stays to the right of the pillar by `TARGET_CM`.
- **Green pillar:** the car must pass on its **left**, so it stays to the left of the pillar by `TARGET_CM`.
- **Cleared pillar:** when the pillar is more than `RETURN_AFTER_CM` behind the car, it is removed and `nextOne` becomes `current`.
- **Beside a pillar:** while the car is level with a pillar, it never moves sideways toward that pillar. It can straighten up or move away.
- **No pillar:** the car returns to the home lane (`y = 0`).

The wanted lateral position is turned into a heading with `atan2`, using a look-ahead distance that depends on how far the car is from the pillar. The heading is limited to `MAX_AIM_DEG`.

### 4. Steering and motor

- A **PD controller** turns the heading error into a steering angle: `KP_HEADING` times the heading error, minus `KD_HEADING` times the turning speed (this damps the swing).
- The steering is limited to `MAX_STEER` and the servo moves at most `SERVO_SPEED_DPS` degrees per second, so the wheels do not jump.
- The motor runs at `DRIVE_SPEED` forward. When the run is finished, the motor stops (coast).

### 5. After the floor line: drive to the wall, then stop (updated, 21 Nov)

The camera already reports `blueFound` and `orangeFound` in every message (fields 5 and 11). `readCamera()` parses both into persistent variables (`camBlueFound`, `camOrangeFound`), and the first check in `loop()` — before the pillar logic runs — is: if `STOP_ON_LINE` is true and either flag is 1 (or the car is already in this mode), hand control to `handleLineStop()`.

The car does **not** stop the instant the line is seen. Instead `handleLineStop()`:
1. The first time it runs, powers on the **front TOF sensor** (`startFrontTof()`) — the same VL53L1X wired for `open_challenge.ino`, held in reset by every sketch that does not need it — and sets `lineStopArmed = true` for good (it never resets to `false`).
2. From then on it **ignores every pillar and every colour message completely**, and steers back toward the **home lane position** (`camPosY() = 0`, the same `atan2`/look-ahead formula `headingGoal()` already uses once a pillar is cleared — not just a fixed compass heading, so the car actually recentres even if it was still mid-dodge when the line was seen) and keeps driving forward.
3. Once the front sensor reads less than `FRONT_STOP_MM` (120 cm), it is only allowed to actually stop if it is also pointing close to the same direction it started with — within `STOP_YAW_TOL_DEG` (2°) of yaw = 0. If it is close to the wall but still at an angle, it steers at full lock toward yaw = 0 (whichever side is closer) instead of easing into it, to straighten out in the least time possible, and only brakes, centres the steering and sets `finished = true` once both conditions hold together (close to the wall **and** close to yaw = 0).

This whole behaviour is one self-contained block (constants, globals, `startFrontTof()`, `handleLineStop()`) placed just above `setup()`. It does not read or change any pillar/map/camera code elsewhere in the file — the **only** line changed outside that block is the trigger in `loop()`, which now calls `handleLineStop(dt)` instead of stopping immediately. Deleting the whole block and that one line reverts to the plain "stop as soon as the line is seen" behaviour.

`blueFound`/`orangeFound` are confirmed the moment the line appears **anywhere** in the picture (per the camera's own comments, from as far as ~16 cm for orange and ~9 cm for blue). That is fine here precisely because the car no longer reacts to the line itself — it only uses it as the signal to arm the "ignore pillars, go find the wall" mode.

### 6. Other safety stops

- **Camera timeout:** if no valid camera message arrives for `CAMERA_TIMEOUT_MS` (500 ms), the car stops and keeps the wheels straight. It does not drive blind.
- **Distance reached:** when the encoder distance reaches `RUN_CM`, the car stops and prints the final position.
- **No gyro:** if the BMI160 is not found at startup, the program stops and prints an error, because the heading cannot work without it.

## Startup sequence

1. `setup()` starts the serial port, the motor pins, the encoder interrupt, the I2C bus and the gyro.
2. The servo is attached and set to the centre.
3. The sketch waits for the **START** button. While waiting, it prints `camera: OK` or a warning once per second, so you can check the camera link.
4. After START, the gyro is calibrated for about 3 seconds. **Do not move the car during this time.**
5. The map is reset to `(0, 0)` with heading 0, and the loop starts.

## Settings

| Constant | Default | Meaning |
|---|---|---|
| `RUN_CM` | 2000 | Distance to drive before the normal end-of-run stop (in practice the line stop fires first) |
| `STOP_ON_LINE` | true | Arm the after-line behaviour as soon as `blueFound` or `orangeFound` is 1. Set `false` to disable without deleting the logic |
| `FRONT_STOP_MM` | 1200 | After a floor line, stop once the front TOF reads less than this (120 cm) |
| `CENTER_ANGLE` | 85 | Servo angle confirmed (on the track) to drive the wheels straight |
| `LEFT_LIMIT` / `RIGHT_LIMIT` | 170 / 10 | Mechanical steering limits (servo angles); ±40° of real wheel angle measured with a protractor |
| `STEER_LEFT_IS_BIGGER` | false | Confirmed with a steering probe test: on this chassis a bigger servo number steers right |
| `SERVO_SPEED_DPS` | 300 | Maximum servo speed, degrees per second |
| `DRIVE_SPEED` | 70 | Motor speed, 0 to 255 |
| `MOTOR_REVERSED` | true | Set to `true` if the motor wires are swapped |
| `KP_HEADING` / `KD_HEADING` | 2.0 / 0.03 | Steering PD gains. `KD` lowered from 0.10 because at the current `MAX_STEER` it was damping the dodge almost instantly |
| `MAX_STEER` | 70 | Maximum steering away from the centre (servo degrees); the real range is asymmetric (75° right, 85° left) because `CENTER_ANGLE` sits off-centre between the limits |
| `TARGET_CM` | 17 | Distance from the pillar centre to the car centre while passing it |
| `MAX_AIM_DEG` | 60 | Maximum heading used to dodge a pillar |
| `LOOK_MIN_CM` | 12 | Shortest look-ahead distance (avoids extreme angles near a pillar) |
| `ARRIVE_BEFORE_CM` | 55 | Be on the safe side this far before the pillar face |
| `LOOK_HOME_CM` | 20 | Look-ahead distance when returning to the home lane |
| `RETURN_AFTER_CM` | 45 | Distance past the pillar centre before it counts as cleared. Raised from 30 on 21 Nov to fix a crash — see "Known limits / history" below |
| `SAME_PILLAR_CM` | 15 | Distance within which a detection is the same pillar |
| `BLEND` | 0.3 | How much each new detection moves a stored pillar (0 to 1) |
| `MAX_SIDE_CM` | 60 | Detections further to the side are ignored |
| `PILLAR_CM` | 5 | Pillar width |
| `TICKS_PER_CM` | 9.27 | Encoder ticks per cm, measured 21 Nov **with the motor running** (190 ticks = 20.5 cm) |
| `CAMERA_BAUD` | 115200 | Serial speed of the camera link |
| `CAMERA_TIMEOUT_MS` | 500 | Stop if the camera is silent this long |
| `DEBUG_PRINTS` | true | Print the state every 150 ms |
| `YAW_REVERSED` / `YAW_SCALE` | false / 1.06 | Gyro sign (confirmed correct) and correction factor (`90 / 85`, from a real 90° hand-turn test, see `PARAMETERS.md`) |

The values were measured on the car (ruler, protractor, counted ticks, a hand-turn gyro test) and validated on the track. Check them again if the car, the wheels or the battery change.

## Serial output

- `camera: OK` / `camera: no messages`: shown while waiting for START.
- `Calibrating gyro... do not move the car`: calibration in progress.
- `>> NEW PILLAR RED` / `GREEN`: a pillar was taken as the one to dodge.
- `>> PILLAR CLEARED`: the car has passed the pillar.
- `>> FRONT TOF READY` / `>> FRONT TOF NOT FOUND`: printed once, the moment the floor line is first seen.
- `LINE STOP MODE | yaw .. | front .. mm`: state line every 150 ms while driving straight after the line, looking for the wall.
- `>> WALL AHEAD: STOP (front .. mm)`: the car braked because the front sensor read less than `FRONT_STOP_MM`.
- `WAITING FOR CAMERA`: no camera message for more than 500 ms.
- `x .. y .. | yaw .. goal .. | goalY .. | pillar .. | cam color dist/lat | line blue .. orange ..`: normal state line every 150 ms, before any line is seen.
- `>> DONE | x .. | y .. | yaw ..`: final position after `RUN_CM` (only reached if `STOP_ON_LINE` is off or no line is seen).

## How to use

1. Connect the camera (RX on D0) and flash `obstacle_camera.ino` with `FOCAL_PX = 100` on the ESP32-S3-CAM.
2. Hold the camera RESET button and upload this sketch to the Nano. Release RESET.
3. Open the Serial Monitor at 115200 baud and check that `camera: OK` appears.
4. Place the car at the start line, keep it still, and press START.
5. Wait for the gyro calibration, then watch the state lines. The car stops at `RUN_CM` and prints `>> DONE`.

## Known limits

- The map depends on the encoder and the gyro. Wheel slip or a wrong `TICKS_PER_CM` moves the whole map.
- The camera only sees the pillar near the car, so the first detection sets the position of each pillar. A wrong first detection changes all later steering.
- The sketch has no obstacle-free fallback for a missing camera: it stops. This is intended.
- `camBlueFound`/`camOrangeFound` trigger on any detection anywhere in the frame, not only a centred one directly ahead. Since the only effect now is arming the "ignore pillars, drive straight, find the wall" mode, an early trigger mid-dodge would make the car straighten out and stop reacting to the pillar it was still beside — if that happens on the track, switch to `blueCenterRow`/`orangeCenterRow` instead (already sent by the camera, just not read by this sketch yet).
- The front TOF and the left/right/rear TOF share the same I2C bus and the same XSHUT pins as `open_challenge.ino`. Both sketches cannot run at the same time with all four sensors powered — this sketch only ever brings up the front one and holds the other three in reset.
- `PARAMETERS.md` in this folder describes the car measurements and how each value was obtained.

## History: the "crashes right after dodging" bug (21 Nov 2026)

**Symptom:** the car steered correctly around a pillar, then swung hard back toward that same side and clipped the pillar right after passing it.

**Cause:** in `headingGoal()`, the instant the camera point passes `RETURN_AFTER_CM` behind the pillar, the pillar is declared "cleared" and the protective clamp (never move sideways toward the pillar while beside it) is dropped on that same loop. `goalY` then jumps instantly to a very different target (the home lane, or the next pillar), and the PD controller commands a hard, immediate correction right next to the pillar — exactly where there is the least room to swing back.

**Fix used:** raising `RETURN_AFTER_CM` from 30 to 45 cm. This keeps the return swing sharp (which is wanted, so the car reaches its target lane quickly) but delays it until there is more real clearance from the pillar. Validated on the track together with the gyro fix (`YAW_SCALE`). A smoothing alternative (rate-limiting how fast the commanded `goalY` can move toward its target, instead of jumping instantly) was prototyped but was not needed once `RETURN_AFTER_CM` alone fixed it, so it is not in this file.
