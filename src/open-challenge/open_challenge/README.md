# open_challenge

The **Open Challenge program** of the car. It drives the lap(s) with gyro heading control, turns 90° at every corner, and after the last turn drives back to the **start position** and stops.

It runs on the Arduino Nano and uses only the two side ToF sensors (left and right), the gyroscope (BMI160), the wheel encoder, the drive motor and the steering servo. The front and rear ToF sensors are soldered but kept in reset (`XSHUT` low), so they never talk on the I²C bus.

## Settings (top of the sketch)
The sketch has three blocks of settings. Change only what is listed here unless the car misbehaves.

**1. Run settings** (change freely)
| Constant | Default | Meaning |
|---|---|---|
| `LAPS` | 3 | 1 = one lap (4 turns), 3 = the full challenge (12 turns) |
| `RUN_PWM` | 150 | Speed on the straights (0–255). Tested with 110; raise in steps of about 20 |
| `TURN_PWM` | 90 | Speed in the corners. Lower values were too weak to turn the car, so 90 is the value in use |
| `STOP_OFFSET_CM` | 0 | Final stop: + stops later, − stops earlier. Only affects the exact `LAPS ≥ 2` method (`LSTART − D1 + STOP_OFFSET_CM`); the `LAPS = 1` WALL estimate and the `FALLBACK_CM` stop ignore it |
| `LANE_MODE` | 1 | 0 = center of the corridor, 1 = fixed distance to the inner wall |
| `INNER_TARGET_MM` | 100 | Distance to keep from the inner wall (mode 1). 100 is below the 200 mm safety margin: the car may touch the inner corner in a turn. If it does, set 200 |
| `CENTER_SHIFT_MM` | 200 | Shift of the center toward the inner wall (mode 0) |
| `JUMP_DELTA_MM` / `JUMP_ABS_MM` | 250 / 1200 | How much a reading must jump to count as a corner |
| `TURN_PREDICT_S` | 0.12 | Ends the turn early by turn rate × this. Raise it if the car overshoots 90° |

**2. Tuning** (change only if the car zig-zags, drifts or misses corners)
`KP_HEAD` (2.0), `KI_HEAD` (4.0), `I_MAX` (25), `K_LAT` (0.03), `KI_LAT` (0.02), `LAT_MAX_DEG` (12), `ARM_WALL_MM` (900), `ARM_READINGS` (5), `MAX_TURN_MS` (4000), `MAX_STRAIGHT_MS` (8000), `STALL_MS` (500).

**3. Car constants** (measured on this car)
`MOTOR_REVERSED = true`, `ENCODER_REVERSED = true`, `GYRO_SCALE = 1.007` (measured in a 3-lap test), `STEER_CENTER = 85`, `STEER_LEFT_MAX = 10`, `STEER_RIGHT_MAX = 170`, `TICKS_PER_CM = 9.27`, and the pins.

**Car constants:** `STEER_CENTER` (85) and `TICKS_PER_CM` (9.27) are the same values as `straight_pillars`, the program that was last validated on the track. Until 22 Nov this program used 90 and 15.1. Those values were changed on 22 Nov without a new track test, so the first run after the change should be watched closely, especially the stop position (it is computed from the encoder). The full table is in [README §8.0](../../../README.md#80-the-open-challenge-program-open_challenge-20-november).

## Wiring
| Part | Nano pin |
|---|---|
| TB6612FNG PWMA / AIN1 / AIN2 | D6 / D7 / D8 |
| Encoder C1 (interrupt) / C2 (direction) | D3 / D2 |
| Steering servo | D9 |
| START button | D10 (internal pull-up) |
| Status LED | D13 |
| BMI160 (I²C) | A4 / A5, address 0x69 |
| Left ToF | XSHUT A3, I²C address 0x2A |
| Right ToF | XSHUT A2, I²C address 0x2B |
| Front / rear ToF | XSHUT A1 / A0, held LOW (off) |

## How it runs
### Start
1. Press **START** (D10) or send `g` and Enter in the Serial Monitor.
2. The gyro is calibrated for about 2 s (700 ms settling + 300 samples at ~5 ms each; the LED blinks, **do not touch the car**). The encoder count and the walls are reset.
3. The car drives straight with the wheels centered.

### Straight: heading hold and lane
- The heading is the integral of the gyro rate (`updateGyro()`), with the zero offset measured at each start.
- The steering is a PI controller on the heading error: `servo = 90 − (KP × error + integral)`. The integral learns the real straight angle of this car, which is not exactly 90°.
- In **mode 1** the car keeps `INNER_TARGET_MM` from the inner wall. In **mode 0** it keeps the middle of the corridor (shifted by `CENTER_SHIFT_MM`). The lateral error becomes a small heading change (`K_LAT`, limited to `LAT_MAX_DEG`).
- If a side reading is under `SAFE_WALL_MM` (70 mm), the car steers away at full correction.

### Corner detection
- Each side learns its **normal distance** after `ARM_READINGS` close readings in a row.
- A **jump** is a reading more than `JUMP_DELTA_MM` above the normal distance, or above `JUMP_ABS_MM`. When it happens `JUMP_CONFIRM` times in a row, that side shows a corner.
- **Corner 1** decides which side is the **inner** side: the side that opens. Left = counter-clockwise, right = clockwise.
- After that only the inner side is watched. Jumps are ignored for `IGNORE_AFTER_TURN_CM` (60 cm) after each turn, and the inner wall must be seen again (armed) before the next corner counts. This avoids false corners.

### Turn
- The steering goes fully to the inner side, and the speed is `TURN_PWM`.
- The turn ends when the **predicted** heading (`yaw + rate × TURN_PREDICT_S`) reaches the new target heading (old heading ± 90°). Then the car drives straight again on the new heading, without braking.
- Heading is always an exact multiple of 90°, so errors do not add up.

### The start position (no rear sensor)
The car must stop where it started. It uses distances measured with the encoder:
- **D1** = distance from the start to the first corner (the inner wall ends there).
- **LSTART** (only with `LAPS ≥ 2`, the exact method): on lap 2 the car measures the distance from the end of turn 4 to corner 5, which is the same start straight. After the last turn it drives `LSTART − D1` and stops.
- **WALL** (with `LAPS = 1`): the distance from "inner wall seen" to corner 3 on the opposite straight. After the last turn it drives `WALL − D1` more from the moment it sees the inner wall. It is less exact (about 36 cm off in the first test).
- If none of these exist, it stops `FALLBACK_CM` (50 cm) after the inner wall is seen.

### Safety stops
The car stops (brake, wheels straight, reason printed) when:
- the **stall** check sees no movement for 0.5 s with the motor on,
- a turn takes more than 4 s,
- there is no corner for 8 s on a straight,
- a corner appears in the last leg before the start position (wrong measurement),
- the final leg drives more than 250 cm,
- the **s** key (emergency stop) is sent.

If a ToF sensor stops answering for 0.5 s, both side sensors restart (`TOF RESTART`).

## Serial output
Press `g` + Enter or START to start. The Serial Monitor (115200 baud) shows:
- events: `CORNER n on LEFT/RIGHT`, `D1`, `WALL`, `LSTART`, `FINAL`, `TURN n done`,
- a status line every 100 ms with the state, corners, target and real heading, odometry, servo, the integral, the lateral correction, the side distances and their normal values, and the motor PWM,
- a final report one second after stopping: yaw, heading, error (yaw − heading), corners, D1, WALL, LSTART and total time.

Use the final error to tune the gyro scale (`GYRO_SCALE`). A large error after a full lap means the gyro or the wheel scale needs tuning.

## How to use
1. Battery **on first**, then USB. Otherwise the sensors may not start.
2. Put the car in the **middle** of the first straight, centered and parallel to the walls.
3. Keep the hands away while the LED blinks (gyro calibration).
4. Start with `LAPS = 1` and `RUN_PWM = 110`. When it works, raise `LAPS` and the speed.
5. If the car zig-zags, lower `KP_HEAD`. If it drifts to one side, check `GYRO_SCALE` and the servo center.
6. If it misses a corner, lower `JUMP_DELTA_MM` (more sensitive). If it turns too early, lower `TURN_PREDICT_S`; if it overshoots, raise it.

## Notes
- The three-lap results are not recorded yet (see `CHANGELOG.md`, v1.9). Use `LAPS = 1` for the first runs.
- The start-position method with `LAPS ≥ 2` is the exact one; `LAPS = 1` uses the less exact WALL estimate.
- It needs the libraries **VL53L1X** (Pololu) and **Servo**.
- The Nano uses `Wire.setWireTimeout(25000, true)`, so Arduino AVR Boards 1.8.3 or newer is required.
