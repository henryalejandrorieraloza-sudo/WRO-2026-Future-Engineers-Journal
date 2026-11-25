# Obstacle Challenge driver (`obstacle_challenge.ino`)

Arduino Nano sketch that drives the car through the Obstacle Challenge of WRO 2026 Future Engineers. It is the **driving half** of the Obstacle Challenge. The **vision half** is [`obstacle_camera_v3_2`](../obstacle_camera_v3_2/README.md), an ESP32-S3-CAM that sends one short text line about 10 times per second to the Nano's RX pin (D0).

This sketch contains two stages in one program:

1. **Parking exit.** The car leaves its parking space before the run starts. This stage used to be a separate sketch (`obstacle_go_out_of_parking`), which has been removed.
2. **Open challenge (laps).** The car drives 3 laps (12 corners), using the first floor line seen by the camera to choose the direction of the round, and avoids the pillars.

## What changed from the previous version

- **No USB/serial command input.** The run starts only with the START button on D10. The sketch never reads `g` or `s` from the computer. This follows rules 11.6–11.7 (no wired control while the robot moves).
- **No start at power-on.** Power-on only calibrates the sensors and the gyro. The car does not move until START is pressed (rule 9.11: one start button).
- **No serial emergency stop.** The car stops by itself through the safety limits listed below. The battery switch is the physical stop.

## How a run goes

1. **Power on.** The four ToF sensors start, the gyro bias is measured (the LED blinks), and the encoder is set to 0. The car stays still.
2. **Press START (D10).** The parking exit runs (`PHASE_PARK`):
   - `WAIT_SIDES`: reads the left and right side sensors. The side with more space is the exit side.
   - `FIRST_TURN`: turns 45° with the gyro toward the open side.
   - `DRIVE_STRAIGHT`: one very short straight section.
   - `RETURN_TURN`: a PD counter-steer brings the heading back to 0° (parallel to the start).
   - `PAUSE`: stops for `PARK_PAUSE_MS`.
   - `REVERSING`: reverses `PARK_REVERSE_TICKS` encoder ticks, holding the heading at 0°.
3. **Open challenge (`PHASE_OBSTACLE`).** The run starts in state `STRAIGHT`:

| State | What the car does |
|---|---|
| `STRAIGHT` | Drives and steers. A floor line seen by the camera starts a corner. Otherwise it follows the pillar map or centers between the walls. |
| `PRE_TURN` | Keeps straight after a line until the front ToF reads `FRONT_TURN_CM`. |
| `BRAKE` | Brakes for `BRAKE_S` with the wheels straight. |
| `TURN` | Turns 90° **in reverse** (the steering is set to the opposite side). The gyro measures the angle. |
| `REVERSE` | Backs up until the rear ToF reads `REAR_STOP_CM` (or `REVERSE_CM` by odometry if the rear ToF has no data). |
| `DONE` | Stops. After the last corner (`LAPS × 4`) the car drives `FINAL_EXTRA_S` past the last line, then stops. |

### Steering priority on each straight section

1. **Corner:** a floor line seen by the camera starts a turn. Blue = turn left (the first line sets the direction). Orange = turn right. After a line is detected, all lines are ignored for `IGNORE_LINE_S` seconds.
2. **Pillar avoidance:** if a pillar is on the map, the car aims to pass it on the correct side. Red pillars are passed on the **right**; green pillars on the **left**.
3. **Pass hold:** right after a pillar is passed, the car keeps its heading for `PASS_CM` cm.
4. **Centering:** with no pillar, the car stays between the left and right side ToF sensors.

The pillar map is built from the camera messages and the odometry (encoder + gyro), so the car still remembers a pillar for a short time after the camera loses sight of it.

## Safety limits

The run ends in `DONE` (motors off) if any of these happens:

- The front ToF never reaches `FRONT_TURN_CM` within `MAX_PRE_TURN_MS` (4 s).
- A turn takes longer than `MAX_TURN_MS` (4 s).
- A reverse takes longer than `MAX_REVERSE_MS` (3 s).
- No corner is detected within `MAX_STRAIGHT_MS` (8 s).
- The motor is on but the wheels do not move for `STALL_MS` (0.5 s), for example when the car hits a wall.

During the parking exit, the stage stops the run if a turn takes longer than 5 s, or if the encoder gives no pulses for `PARK_STALL_MS` (1.5 s).

If the camera messages stop, the car stops driving until they come back (`CAMERA_TIMEOUT_MS` = 0.5 s). It does not end the run.

## Main constants (top of the sketch)

| Constant | Value | Meaning |
|---|---|---|
| `LAPS` | 3 | 3 laps = 12 corners |
| `RUN_PWM`, `TURN_PWM`, `REVERSE_PWM` | 70 | Motor power (0–255) on straights, turns and reverses |
| `FRONT_TURN_CM` | 40 | Front distance at which the car brakes to turn after a line |
| `BRAKE_S` | 0.5 | Braking time before reversing into the turn |
| `REAR_STOP_CM` | 20 | Rear distance at which the reverse ends |
| `FINAL_EXTRA_S` | 0.45 | Drive time after the last line, then stop |
| `IGNORE_LINE_S` | 6.0 | Time after a line during which other lines are ignored |
| `TARGET_CM`, `PASS_CM`, `RETURN_AFTER_CM` | 17, 30, 25 | Pillar clearance, pass-hold distance and "passed" distance |
| `KP_HEAD`, `KD_HEAD`, `KI_HEAD` | 2.5, 0.03, 4.0 | Steering PID gains (heading) |
| `PARK_FIRST_ANGLE_DEG` | 45 | Parking exit turn angle |
| `PARK_REVERSE_TICKS` | 1200 | Parking reverse distance in encoder ticks (see the note below) |
| `TICKS_PER_CM` | 9.27 | Encoder ticks per cm |

**Open items to check on the car:**

- `PARK_REVERSE_TICKS` is 1200, confirmed by the team on the car (24 Nov 2026). The 597 ticks from five wheel turns (5 × π × 4.1 cm × 9.27 ticks/cm) are a calculation only and are not used.
- `TICKS_PER_CM` is 9.27, the value in the code, measured on 21 Nov with the motor running (190 ticks over 20.5 cm). The 16.1 value of 28 Oct (a 100 cm run that read 97 cm) is not used. Henry confirmed on 24 Nov that the two values differ by about 1.7×. Run a 100 cm check with the final code before the competition.
- `STEER_CENTER` is 85 and the limits are `STEER_CENTER` ± `STEER_RANGE` (80) = 5 / 165, the same as the Open program (aligned on 24 Nov 2026). The protractor stops of 10 and 170 are the record of the test in `docs/TESTS.md`.

## Pins

| Part | Pin |
|---|---|
| Motor driver TB6612 | PWMA D6, AIN1 D7, AIN2 D8 |
| Encoder | C1 D3, C2 D2 |
| Steering servo | D9 |
| START button | D10 (INPUT_PULLUP, active LOW) |
| LED | D13 |
| Camera data in | D0 (RX, from the camera TX on GPIO43) |
| ToF XSHUT pins | left A3, right A2, rear A0, front A1 |
| ToF I²C addresses | left `0x2A`, right `0x2B`, rear `0x2C`, front `0x2D` |
| Gyro (BMI160) | I²C address `0x69` |

## Camera messages

The camera sends one line per frame that starts with `$`. The driver reads 12 comma-separated values from it: color, x, bottom, correct, blueFound, blueRow, seq, width, distCm, latCm, orangeFound, orangeRow. The camera also sends two more fields (blueCenterRow and orangeCenterRow) that this driver does not read.

## Build and upload

1. Arduino IDE, board **Arduino Nano (ATmega328P)**.
2. Libraries: **VL53L1X** (Pololu) and **Servo**.
3. Upload with the battery on. D0 is also the USB receive line, so if the upload fails, unplug the camera wire from D0 while uploading.
4. Keep USB disconnected during runs. The serial output is for debugging after a run.

## Debug output

After power-on and during a run, the sketch prints one status line every 100 ms with the state, corner count, heading error, map mode, distances (left, right, front, rear), the pillar seen, and the line flags. The parking stage prints its own status line. These are for debugging only; no input is read from the computer.
