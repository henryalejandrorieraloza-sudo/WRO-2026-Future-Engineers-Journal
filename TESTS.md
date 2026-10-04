# Testing workflow and results

This file records **how** each part of the car is tested and **what we measured**. Every test has a procedure, a pass criterion and a results table with the date.

Fill one row per test session. Keep failed results too: the rubric rewards iteration, not only the final result.

---

## T1 — Drive motor (`rear_motor_loop_test`)

**Procedure:** wheels off the ground, upload the sketch, open the Serial Monitor at 115200.
**Pass:** the motor follows forward 2 s → stop 1 s → backward 2 s → stop 1 s, in a loop.

| Date | Chassis | MOTOR_SPEED | Result | Notes |
|---|---|---|---|---|
| 2026-10-02 | v2 | 150 | ✅ Pass | STBY on D5 (to confirm) |
| | | | | |

## T2 — Steering servo (`servo_simple`)

**Procedure:** sweep the servo. Find the angle where the wheels are parallel to a straight edge (centre), and the angles where the steering reaches its mechanical limit without forcing.
**Pass:** wheels straight at `CENTER_ANGLE`; no buzzing or forcing at the limits.

| Date | Chassis / servo | Centre | Left limit | Right limit | Max wheel angle (°) | Notes |
|---|---|---|---|---|---|---|
| 2026-09-22 | v1 / SG90 | 120° | 90° (provisional) | 150° (provisional) | — | Servo moved inside its mount |
| | v3 / new servo | | | | | |

## T3 — Gyroscope (`gyro_yaw_test`)

**Procedure:**
1. Leave the car still during calibration and for 60 s more → write down the yaw after 60 s (drift).
2. Rotate the car by hand exactly 90° against a square, 4 times in the same direction (should end near 360°).

**Pass:** drift < 1° per minute; each 90° rotation reads 90 ± 3°.

| Date | Drift after 60 s (°) | Turn 1 | Turn 2 | Turn 3 | Turn 4 (total) | Notes |
|---|---|---|---|---|---|---|
| | | | | | | |

## T4 — TOF sensors (`tof_sensors_test`)

**Procedure:** place a piece of black wall (10 cm high, same material as the track) in front of each sensor at 20, 40, 60 and 80 cm. Record about 20 readings per point and note the mean and the min–max spread. Repeat for each mounting height / ROI combination.

**Pass:** mean within ±2 cm of the real distance at all four distances, with no jumps above 5 cm.

| Date | Sensor | Height / tilt | ROI (W×H) | 20 cm | 40 cm | 60 cm | 80 cm | Notes |
|---|---|---|---|---|---|---|---|---|
| 2026-10-03 | sides | 0.8 cm / 8° up | 4×13 | | | | ~300 (beam over wall) | Fixed when an object was placed on top of the wall |
| | | ≈ 4 cm / 0° | 4×13 | | | | | |
| | | ≈ 4 cm / 0° | 4×8 | | | | | |
| | | ≈ 4 cm / 0° | 4×4 | | | | | |

Then set `OFFSET_CM` for each sensor = real distance − measured mean.

## T5 — Speed

**Procedure:** mark 1 m on the floor. Time the car over that 1 m (after a short run-up), 3 times per setting.

| Date | PWM | Time 1 (s) | Time 2 (s) | Time 3 (s) | Average speed (m/s) |
|---|---|---|---|---|---|
| | 150 | | | | |
| | 255 | | | | |

## T6 — Open Challenge runs (`open_challenge_v2`)

**Procedure:** 10 runs. For each run, randomise the direction (coin), the corridor widths and the starting zone, following rule 8. Use the same parameters for all 10 runs, then change one parameter at a time.

| Run | Date | Direction | KP / KD / JUMP | Laps | Correct direction at corner 1? | Stopped inside finish section? | Wall touches | Time (s) | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 1 | | | | | | | | | |
| 2 | | | | | | | | | |
| … | | | | | | | | | |

**Summary per parameter set:** M1 = runs with 3 laps / 10 · M2 = correct first corner / 10 · M3 = correct stops / 10 · M4 = average wall touches.

## T7 — Pillar colours (`color_detection_test`)

**Procedure:** place each object at the centre of the image and read the `CENTER H/S/V` line. Do this at 3 distances and in 2 lighting conditions.

| Date | Object | Distance | Light | H | S | V | Detected as | Notes |
|---|---|---|---|---|---|---|---|---|
| | Red pillar | | | | | | | |
| | Green pillar | | | | | | | |
| | White mat | | | | | | | |
| | Black wall | | | | | | | |
| | Magenta block | | | | | | | |

**M8:** 20 images per colour → % correctly classified.
