# Straight with pillars: odometry and control parameters

Parameters of `straight_pillars_medidas.ino` (car measured on 20 November 2026). The sketch keeps a map of the straight (odometry from the encoder and the gyroscope) and places each pillar seen by the camera on that map.

**Status:** *measured* = taken with a ruler, a protractor or a counted test; *calibrate* = still to be checked on the car; *fixed* = value of the sensor or the design.

## 1. Car geometry

| Parameter | Value | Unit | Status | How it was obtained |
|---|---|---|---|---|
| Car width | 9 | cm | measured | ruler |
| Full length (with front protrusion) | 17.5 | cm | measured | ruler |
| Front protrusion | ≈ 4 | cm | measured | part of the full length |
| Wheelbase `WHEELBASE_CM` | 9.0 | cm | measured | axle to axle |
| Encoder axle to camera lens `CAM_FWD_CM` | 11.7 | cm | measured | encoder axle to lens, along the car |
| Pillar size `PILLAR_CM` | 5.0 | cm | fixed | competition rules |

## 2. Steering servo

| Parameter | Value | Meaning | Status |
|---|---|---|---|
| `CENTER_ANGLE` | 120 | wheels straight (90° on the protractor) | measured |
| `LEFT_LIMIT` | 180 | 30° of wheel angle to the left (60° on the protractor) | measured |
| `RIGHT_LIMIT` | 60 | 30° of wheel angle to the right (120° on the protractor) | measured |
| `STEER_LEFT_IS_BIGGER` | true | 180 is the left side | confirmed |
| `SERVO_SPEED_DPS` | 300 | maximum servo speed, degrees per second | set |
| `MAX_STEER` | 60 | maximum correction on each side, symmetric | calculated (120 − 60 = 180 − 120) |
| Minimum turning radius | ≈ 15.6 | cm, rear axle centre: 9 / tan 30° | calculated |

## 3. Drive motor and encoder

| Parameter | Value | Unit | Status | How it was obtained |
|---|---|---|---|---|
| `TICKS_PER_CM` | 12.67 | ticks/cm | measured | 100 ticks = 7.9 cm |
| `DRIVE_SPEED` | 70 | PWM, 0–255 | set | |
| `MOTOR_REVERSED` | true | | set | wiring of the TB6612FNG |
| `RUN_CM` | 150.0 | cm | set | test distance, then the car stops |

## 4. Gyroscope (BMI160)

| Parameter | Value | Status | Check |
|---|---|---|---|
| `GYRO_SCALE` | 65.6 LSB per °/s | fixed (±500 °/s range) | sensor datasheet |
| `YAW_REVERSED` | false | **calibrate** | turn the car to the left by hand: yaw must increase. If it decreases, set `true` |
| `YAW_SCALE` | 1.00 | **calibrate** | turn the car exactly 90° by hand and read the yaw: `YAW_SCALE = 90 / yaw printed` |

## 5. Heading control (PD)

| Parameter | Value | Meaning | Status |
|---|---|---|---|
| `KP_HEADING` | 2.0 | steering degrees per degree of heading error | initial value, tune on track |
| `KD_HEADING` | 0.10 | steering degrees per degree/s of turning speed | initial value, tune on track |
| `MAX_AIM_DEG` | 45.0 | maximum heading while dodging a pillar | initial value |

## 6. Pillar logic

| Parameter | Value | Unit | Meaning |
|---|---|---|---|
| `TARGET_CM` | 17.0 | cm | distance from the camera to the pillar centre when passing it: 4.5 (half car) + 10 (gap) + 2.5 (half pillar) |
| `ARRIVE_BEFORE_CM` | 10.0 | cm | be on the safe side this far before the front face of the pillar |
| `LOOK_MIN_CM` | 12.0 | cm | shortest distance used in the aim angle |
| `LOOK_HOME_CM` | 20.0 | cm | distance used to return to the home lane after a pillar |
| `RETURN_AFTER_CM` | 5.0 | cm | camera this far past the pillar centre: the pillar is cleared |
| `SAME_PILLAR_CM` | 15.0 | cm | detections closer than this to a stored pillar are the same pillar |
| `BLEND` | 0.3 | 0–1 | how much each new message corrects the stored pillar position |
| `MAX_SIDE_CM` | 60.0 | cm | detections further to the side are ignored |

## 7. Camera link

| Parameter | Value | Meaning |
|---|---|---|
| `CAMERA_BAUD` | 115200 | ESP32-S3-CAM to Nano serial link (camera TX on GPIO 43 to Nano D0) |
| `CAMERA_TIMEOUT_MS` | 500 | without a camera message for this long, the car stops and waits |

## 8. Formulas

Odometry, every loop:

- `ds = ticks / TICKS_PER_CM`
- `h` = average yaw during the step
- `x += ds · cos h`, `y += ds · sin h`

Camera point (the lens, not the axle):

- `C = (x, y) + CAM_FWD_CM · (cos yaw, sin yaw)`

Pillar position on the map, from a camera message with distance `d` and lateral offset `lat`:

- `P = C + R(yaw) · (d + PILLAR_CM/2, −lat)`

Lateral goal next to the pillar:

- red (pillar on the left): `goalY = min(C_y, P_y − TARGET_CM)`
- green (pillar on the right): `goalY = max(C_y, P_y + TARGET_CM)`

Heading and steering:

- `goalHeading = atan2(goalY − C_y, look)`, limited to ±`MAX_AIM_DEG`
- `steer = KP_HEADING · (goalHeading − yaw) − KD_HEADING · yawRate`, limited to ±`MAX_STEER`

## 9. Changes from the October version (`straight_pillars.ino`)

| Parameter | October | November |
|---|---|---|
| `TICKS_PER_CM` | 15.1 | 12.67 |
| `RIGHT_LIMIT` | 50 | 60 |
| `STEER_LEFT_IS_BIGGER` | false | true |
| `CAM_FWD_CM` | not used | 11.7 |
| `WHEELBASE_CM` | not used | 9.0 |

## 10. Still to do before trusting the map

1. Check the direction of the gyroscope (`YAW_REVERSED`).
2. Calibrate `YAW_SCALE` with a hand-turned 90°.
3. Tune `KP_HEADING` and `KD_HEADING` on track.
