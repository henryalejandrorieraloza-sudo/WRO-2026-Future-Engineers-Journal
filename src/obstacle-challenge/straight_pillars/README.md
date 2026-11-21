# Straight with pillars (`straight_pillars.ino`)

Arduino Nano sketch for the **straight section of the Obstacle Challenge**: the car drives forward along a straight with red and green pillars, passing each pillar on the correct side. It works together with the camera sketch (`obstacle_camera.ino`), which runs on the ESP32-S3-CAM and sends the pillar data over a serial link.

The main idea is that the car keeps a small **map** of the straight. The camera sees a pillar only while it is close, so the Nano remembers where each pillar is and keeps steering correctly even when the camera can no longer see it.

## Hardware

| Part | Pin / connection |
|---|---|
| Camera (ESP32-S3-CAM) | GPIO43 (TX) to Nano **D0 (RX)**, 115200 baud |
| Encoder | **D3** (interrupt, rising edge, internal pull-up) |
| Start button | **D10** to GND (internal pull-up) |
| Steering servo | **D9** |
| Motor driver TB6612FNG | PWMA **D6**, AIN1 **D7**, AIN2 **D8** |
| Gyroscope BMI160 | I2C: SDA **A4**, SCL **A5**, address 0x68 or 0x69 |

**Upload note:** the camera is connected to D0 (RX), which blocks the upload. Hold the camera **RESET** button during the whole upload.

The camera must run `obstacle_camera.ino` with `FOCAL_PX = 100`. This sketch reads only the first 10 fields of the camera message.

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

### 5. Safety stops

- **Camera timeout:** if no valid camera message arrives for `CAMERA_TIMEOUT_MS` (500 ms), the car stops and keeps the wheels straight. It does not drive blind.
- **Distance reached:** when the encoder distance reaches `RUN_CM` (150 cm by default), the car stops and prints the final position.
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
| `RUN_CM` | 150 | Distance to drive before stopping (use 100 for a test without pillars) |
| `CENTER_ANGLE` | 120 | Servo angle with the wheels straight |
| `LEFT_LIMIT` / `RIGHT_LIMIT` | 180 / 50 | Mechanical steering limits (servo angles) |
| `STEER_LEFT_IS_BIGGER` | false | Direction of the servo; `false` means a smaller servo angle is left |
| `SERVO_SPEED_DPS` | 300 | Maximum servo speed, degrees per second |
| `DRIVE_SPEED` | 70 | Motor speed, 0 to 255 |
| `MOTOR_REVERSED` | true | Set to `true` if the motor wires are swapped |
| `KP_HEADING` / `KD_HEADING` | 2.0 / 0.10 | Steering PD gains |
| `MAX_STEER` | 60 | Maximum steering away from the centre (servo degrees) |
| `TARGET_CM` | 17 | Distance from the pillar centre to the car centre |
| `MAX_AIM_DEG` | 45 | Maximum heading used to dodge a pillar |
| `LOOK_MIN_CM` | 12 | Shortest look-ahead distance (avoids extreme angles near a pillar) |
| `ARRIVE_BEFORE_CM` | 10 | Be on the safe side this far before the pillar face |
| `LOOK_HOME_CM` | 20 | Look-ahead distance when returning to the home lane |
| `RETURN_AFTER_CM` | 5 | Distance past the pillar centre before it counts as cleared |
| `SAME_PILLAR_CM` | 15 | Distance within which a detection is the same pillar |
| `BLEND` | 0.3 | How much each new detection moves a stored pillar (0 to 1) |
| `MAX_SIDE_CM` | 60 | Detections further to the side are ignored |
| `PILLAR_CM` | 5 | Pillar width |
| `TICKS_PER_CM` | 15.1 | Encoder ticks per cm |
| `CAMERA_BAUD` | 115200 | Serial speed of the camera link |
| `CAMERA_TIMEOUT_MS` | 500 | Stop if the camera is silent this long |
| `DEBUG_PRINTS` | true | Print the state every 150 ms |
| `YAW_REVERSED` / `YAW_SCALE` | false / 1.00 | Gyro sign and correction factor |

The values were measured on the car and checked in a simulation, as described in the header comment of the sketch. Check them again if the car, the wheels or the battery change.

## Serial output

- `camera: OK` / `camera: no messages`: shown while waiting for START.
- `Calibrating gyro... do not move the car`: calibration in progress.
- `>> NEW PILLAR RED` / `GREEN`: a pillar was taken as the one to dodge.
- `>> PILLAR CLEARED`: the car has passed the pillar.
- `WAITING FOR CAMERA`: no camera message for more than 500 ms.
- `x .. y .. | yaw .. goal .. | goalY .. | pillar .. | cam color dist/lat`: state line every 150 ms.
- `>> DONE | x .. | y .. | yaw ..`: final position after `RUN_CM`.

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
- `PARAMETERS.md` in this folder describes the car measurements and how each value was obtained.
