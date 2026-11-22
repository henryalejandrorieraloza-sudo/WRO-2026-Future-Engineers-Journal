# Straight with pillars: odometry and control parameters

Parameters of `straight_pillars.ino` (car measured 19-21 November 2026). The sketch keeps a map of the straight (odometry from the encoder and the gyroscope) and places each pillar seen by the camera on that map, and now also brakes on the blue/orange floor line (section 9).

**Status:** *measured* = taken with a ruler, a protractor or a counted test; *calibrate* = still to be checked on the car; *fixed* = value of the sensor or the design; *validated* = measured AND confirmed working on the track.

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
| `CENTER_ANGLE` | 85 | servo value confirmed on the track to drive the wheels straight | validated |
| `LEFT_LIMIT` | 170 | mechanical stop, left | measured |
| `RIGHT_LIMIT` | 10 | mechanical stop, right | measured |
| Real wheel angle, each side | 40° | `LEFT_LIMIT`↔`RIGHT_LIMIT` (160 servo degrees) measured with a protractor as ±40° of real wheel angle | measured |
| `STEER_LEFT_IS_BIGGER` | false | on this chassis a bigger servo number steers right (opposite of the October chassis) | confirmed with the steering-probe test |
| `SERVO_SPEED_DPS` | 300 | maximum servo speed, degrees per second | set |
| `MAX_STEER` | 70 | maximum correction on each side; **not symmetric in servo degrees** because `CENTER_ANGLE` (85) sits off-centre between the limits: 75 available to the right (85−10), 85 available to the left (170−85) | calculated, with margin |
| Minimum turning radius | ≈ 10.7 | cm, rear axle centre: 9 / tan 40° | calculated |

## 3. Drive motor and encoder

| Parameter | Value | Unit | Status | How it was obtained |
|---|---|---|---|---|
| `TICKS_PER_CM` | 9.27 | ticks/cm | validated | **measured with the motor running** (not pushed by hand): 190 ticks = 20.5 cm, 21 Nov |
| `DRIVE_SPEED` | 70 | PWM, 0–255 | set | |
| `MOTOR_REVERSED` | true | | set | wiring of the TB6612FNG |
| `RUN_CM` | 2000.0 | cm | set | test distance before the normal end-of-run stop; in practice the line stop (§9) fires first |

## 4. Gyroscope (BMI160)

| Parameter | Value | Status | Check |
|---|---|---|---|
| `GYRO_SCALE` | 65.6 LSB per °/s | fixed (±500 °/s range) | sensor datasheet |
| `YAW_REVERSED` | false | validated | confirmed with a standalone test sketch (`gyro_90_test.ino`): yaw increased when the car was turned left by hand, as expected |
| `YAW_SCALE` | 1.06 | validated | same test: turned the car exactly 90° by hand, the sketch printed yaw = 85° → `YAW_SCALE = 90 / 85 = 1.06` |

`gyro_90_test.ino` reuses this sketch's own register-level BMI160 driver (`bmiWrite`/`bmiRead`/`bmiFound`/`startGyro`/`readGyroZ`/`calibrateGyro`) with `YAW_SCALE = 1.00` hardcoded as neutral, so the printed value is the *raw* yaw before any correction — exactly what the `YAW_SCALE = 90 / printed` formula above needs.

## 5. Heading control (PD)

| Parameter | Value | Meaning | Status |
|---|---|---|---|
| `KP_HEADING` | 2.0 | steering degrees per degree of heading error | validated |
| `KD_HEADING` | 0.03 | steering degrees per degree/s of turning speed; lowered from 0.10 because at the current `MAX_STEER` the resulting yaw rate was high enough that 0.10 damped (killed) the dodge almost immediately | validated |
| `MAX_AIM_DEG` | 60.0 | maximum heading while dodging a pillar | validated |

## 6. Pillar logic

| Parameter | Value | Unit | Meaning |
|---|---|---|---|
| `TARGET_CM` | 17.0 | cm | distance from the camera to the pillar centre when passing it: 4.5 (half car) + 10 (gap) + 2.5 (half pillar) |
| `ARRIVE_BEFORE_CM` | 55.0 | cm | be on the safe side this far before the front face of the pillar |
| `LOOK_MIN_CM` | 12.0 | cm | shortest distance used in the aim angle |
| `LOOK_HOME_CM` | 20.0 | cm | distance used to return to the home lane after a pillar |
| `RETURN_AFTER_CM` | 45.0 | cm | camera this far past the pillar centre: the pillar is cleared. Raised from 30 on 21 Nov — see §10 |
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

## 9. After the floor line: ignore pillars, drive to the wall, stop (updated 21 Nov)

`obstacle_camera.ino` v3.1 reports `blueFound` (field 5) and `orangeFound` (field 11) in every message. `straight_pillars.ino` parses 12 of the 14 fields (up from 10) to read `orangeFound` as well, and saves both flags into persistent globals `camBlueFound`/`camOrangeFound`.

The first version of this feature stopped the car immediately on either flag. It now instead arms a self-contained block (`startFrontTof()` + `handleLineStop()`, placed just above `setup()`) that:

```cpp
if (STOP_ON_LINE && (camBlueFound || camOrangeFound || lineStopArmed)) {
  handleLineStop(dt);   // the ONLY other line in loop() that changed for this feature
  return;
}
```

1. Powers on the **front VL53L1X TOF sensor** the first time it runs (same wiring as `open_challenge.ino`: XSHUT `A1`; left/right/rear XSHUT `A3`/`A2`/`A0` are held low so only the front sensor answers on the shared I2C bus), and sets `lineStopArmed = true` permanently.
2. From then on **ignores the pillar map and every colour message**, and steers back toward the home lane **position** (`camPosY() = 0`, via `atan2(0 - camPosY(), LOOK_HOME_CM)` — the same formula and the same `KP_HEADING`/`KD_HEADING` gains `headingGoal()` already uses once a pillar is cleared, not a fixed compass heading) and keeps driving forward.
3. Once the front sensor reads less than `FRONT_STOP_MM` (1200 mm = 120 cm), only stops for good if also within `STOP_YAW_TOL_DEG` (2°) of yaw = 0 — the same direction the car faced at START, so it never ends the run sitting at an angle. If close to the wall but still diagonal, it steers at full lock (`±MAX_STEER`) toward yaw = 0 instead of easing into it, to fix the heading in the least time possible.

Both flags are confirmed as soon as the line appears anywhere in the frame (camera-side range: orange from ~16 cm, blue from ~9 cm), not only when centred ahead — this is considered acceptable now because the only effect of triggering is arming the "go find the wall" mode, not an immediate stop; if an early trigger mid-dodge turns out to be a problem on the track, switch to `blueCenterRow`/`orangeCenterRow` instead (already sent, −1 unless the line is in the straight-ahead strip).

## 10. History: the "crashes right after dodging" bug (21 Nov)

**Symptom:** the car dodged a pillar correctly, then swung hard back toward it and clipped it right after passing.

**Cause:** in `headingGoal()`, the instant the camera point passes `RETURN_AFTER_CM` behind the pillar, it is declared cleared and the protective clamp (never move toward the pillar while beside it) is dropped on that same loop — `goalY` jumps instantly to a different target and the PD controller fires a hard correction with almost no clearance.

**Fix:** `RETURN_AFTER_CM` raised from 30 to 45 cm, together with the `YAW_SCALE` fix above. Keeps the return sharp but delays it until there is real distance from the pillar. A smoothing alternative (rate-limiting `goalY`'s approach to its target instead of letting it jump) was prototyped but was not needed once `RETURN_AFTER_CM` alone fixed it on the track, so it is not in the committed sketch.

## 11. Changes from the October version (`straight_pillars.ino`)

| Parameter | October | 20 Nov | 21 Nov |
|---|---|---|---|
| `TICKS_PER_CM` | 15.1 | 12.67 | **9.27** (measured with the motor running, not pushed by hand) |
| `CENTER_ANGLE` | 120 | 120 | **85** (confirmed on track) |
| `LEFT_LIMIT` / `RIGHT_LIMIT` | 180 / 50 | 180 / 60 | **170 / 10** |
| `STEER_LEFT_IS_BIGGER` | false | true | **false** |
| `MAX_STEER` | 60 | 60 | **70** |
| `KD_HEADING` | 0.10 | 0.10 | **0.03** |
| `RETURN_AFTER_CM` | 5 | 5 | **45** |
| `YAW_SCALE` | 1.00 | 1.00 | **1.06** |
| `CAM_FWD_CM` | not used | 11.7 | 11.7 |
| `WHEELBASE_CM` | not used | 9.0 | 9.0 |
| Stop on floor line | no | no | **yes** (`STOP_ON_LINE`) |

## 12. Still to do before trusting the map fully

1. ~~Check the direction of the gyroscope (`YAW_REVERSED`).~~ Done 21 Nov: confirmed `false`.
2. ~~Calibrate `YAW_SCALE` with a hand-turned 90°.~~ Done 21 Nov: `1.06`.
3. ~~Fix the crash right after dodging a pillar.~~ Done 21 Nov: `RETURN_AFTER_CM` 30 → 45.
4. Validate the new after-line behaviour (`handleLineStop` / front TOF / `FRONT_STOP_MM`) over several real runs; switch to the centre-row fields for the trigger if ignoring pillars starts too early.
5. Keep tuning `KP_HEADING` / `KD_HEADING` if the dodge still feels too sharp or too soft on different battery levels.
