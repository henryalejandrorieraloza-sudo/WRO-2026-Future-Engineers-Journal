# Test log

This file records how each test was run and what was measured. One row per run. Keep failed runs: the rubric rewards iteration, not only the final result.

Rows are added after each run, using the final code of the commit shown.

## Three-lap run (Open Challenge)

Procedure: place the car at the start of the Open Challenge track, start the round, and record each run.

Pass criterion: three laps, then a stop inside the starting section.

Target (team plan, not a rubric requirement): at least 20 runs with the final code, so the success rate can be compared between sessions. The rubric asks for testing and iteration evidence with metrics, not a specific number of runs.

| Run | Date | Commit | Battery (V) | Laps completed | Success (yes / no) | Time per lap (s) | Direction | Stop position | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 22 Nov 2026 | `3010306` or `c3441cd` (both on 22 Nov, 21:35 and 21:39; which one ran is not recorded) | not recorded | 3 (12 corners) | yes | not recorded | not recorded | Starting section, close to the start wall (not the exact spot) | First attempt, no hand intervention. Corridors narrow. Source: journal §19.2 |

## Summary

Only one run is recorded so far. The 20-run series is still to be done; the fields marked "not recorded" were not written down at the time.

| Test | Runs | Successes | Success rate | Median time per lap (s) |
|---|---:|---:|---:|---:|
| Open Challenge, 3 laps | 1 | 1 | 100 % (1 run so far) | not recorded |

## Obstacle Challenge (driving program `obstacle_challenge`, without parking)

Procedure: place the car at the start of the Obstacle track with the pillars in place, press START, and record each run. The car was run on the track on 23 Nov (team report), but no run has been written down with its data yet.

Columns to fill for each run: lap times, direction of the first corner (blue = left, orange = right), stop position (from the final report line `>>> FINAL`), pillars passed on the correct side (yes / no for each), and notes.

| Run | Date | Commit | Battery (V) | Laps completed | Success (yes / no) | Time per lap (s) | Direction | Stop position | Pillars on correct side | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — | — | — | — | No run recorded yet |

Edge cases the team checked (23 Nov, team report) are not listed in the repository. For each one, add a row with the case, the commit tested and the result (pass / fail, with what happened).

| Edge case | Commit | Result | Notes |
|---|---|---|---|
| Not listed yet | — | not recorded | — |

## Motor: gear ratio, speed and stall (drive)

Procedure: see `schemes/datasheets/N20_12V_1000RPM_specs.md`, "Measurements to take". Record every value with the battery voltage and the commit of the code used.

| Test | Date | Commit | Battery (V) | Method | Value | Repetitions | Notes |
|---|---|---|---|---|---|---:|---|
| Encoder pulses per wheel turn (10 turns) | — | — | — | hand turn, motor off | — | 10 | Ratio = pulses per turn ÷ (7 × edges counted) |
| Stall current (peak, < 0.5 s) | — | — | — | multimeter in series | — | — | Do not hold the wheel longer |
| Speed over 1 m at `RUN_PWM 150` | — | — | — | stopwatch | — | 3 | |
| Speed over 1 m at `RUN_PWM 255` | — | — | — | stopwatch | — | 3 | |
| Torque check (ramp or spring scale) | — | — | — | — | — | — | |

## Steering: centre, limits and symmetry

Procedure: run `servo_test` with the chassis fully assembled, then measure the real wheel angle at each stop with a protractor or a top-view photo.

| Test | Date | Commit | Servo value | Left wheel angle (°) | Right wheel angle (°) | Symmetric? | Notes |
|---|---|---|---|---:|---:|---|---|
| Centre (wheels straight) | — | — | 85 | — | — | — | |
| Left stop | — | — | 10 | — | — | — | Listen for buzzing or forcing |
| Right stop | — | — | 170 | — | — | — | Listen for buzzing or forcing |
| Turning radius at full lock | — | — | — | — | — | — | Need wheelbase and wheel angle |

## Car geometry and mass

| Measure | Date | Method | Value | Notes |
|---|---|---|---|---|
| Wheel diameter, four wheels, mounted | — | calliper | — | Design value 41 mm |
| Wheelbase (front to rear axle) | 19 Nov 2026 | ruler (team notes, to confirm with a tape measure) | 9 cm | Source: measurement notes of the new car, 19 Nov. Not yet re-measured on the final chassis |
| Track width (between wheels) | — | ruler | — | |
| Mass, ready to run, with battery | — | kitchen scale, ±1 g | — | Design value 248 g. Note what is mounted |
| Turning sweep at full lock, left (from the lens) | 28 Oct 2026 | tape measure, 6 runs | 12 cm forward, 40 cm sideways | Includes about 2.4 cm of braking. Source: notes of 28 Oct |
| Turning sweep at full lock, right (from the lens) | 28 Oct 2026 | tape measure, 6 runs | 15 cm forward, 39 cm sideways | Includes about 2.2 cm of braking. Less repeatable than the left turn in the tape readings. Source: notes of 28 Oct |
| Odometry against tape (lens path, straight part) | 28 Oct 2026 | tape measure | Y within ±1–2 cm; X 2–3.5 cm more than the odometry | Possible small slip in the curve. Source: notes of 28 Oct |

## ToF sensors (distance, on the black and the white wall)

Procedure: place the car at 5, 20, 40 and 60 cm from the wall, read 30 samples per distance with `src/hardware-tests/tof_and_gyro_test`, and compare with a tape measure.

| Side | Wall | Distance (cm, tape) | Valid readings / 30 | Mean (cm) | Spread p90 − p10 (cm) | Notes |
|---|---|---:|---:|---:|---:|---|
| LEFT | white | — | — | — | — | |
| LEFT | black | — | — | — | — | Check the black-wall case (Problem 46) |
| RIGHT | white | — | — | — | — | |
| RIGHT | black | — | — | — | — | |
| FRONT | white | — | — | — | — | |
| BACK | white | — | — | — | — | |

## Gyroscope: drift and turn accuracy

| Test | Date | Commit | Method | Value | Notes |
|---|---|---|---|---|---|
| Drift at rest, 60 s | — | — | `src/hardware-tests/tof_and_gyro_test` | — (°/min) | Car still on the floor |
| Heading error after one 90° turn, in motion | 28 Oct 2026 | not recorded | Turn with the PD gyro while driving (`turn_measure_moving`), 6 runs | 89.3 to 90.3 ° | Source: notes of 28 Oct. The commit of that sketch was not recorded. The 10 hand turns each way with `gyro_90_test` are still to be done |
| Heading error after 12 turns | — | — | run `open_challenge` and measure the final heading | — (°) | Also record the run in the three-lap table |

## Camera: frame rate and colour detection

| Test | Date | Commit | Resolution | Lighting | Value | Notes |
|---|---|---|---|---|---|---|
| Frames per second, colour detection | — | — | 160 × 120 | room / venue | — (fps) | The README asks for this value; it was measured only at 320 × 240 |
| Pillar detected at 20, 40, 60 cm | 27 Oct 2026 | not recorded | not recorded | not recorded | Detected up to about 50 cm (centred pillar); 60 cm not tested | Source: notes of 27 Oct. Red and green, each side, not recorded separately |
| Floor line detected at 9, 16 cm | 28 Oct 2026 | not recorded | not recorded | not recorded | Orange from 16 cm; blue from 9 cm | Source: camera v3.1 test, journal Phase 11. No false line on the empty floor or the black wall |

## Obstacle Challenge: edge cases to add

The cases below are suggestions, not results. Replace them with the cases the team checked on 23 Nov, and add one row per case with the commit tested and the result.

| Case | Commit | Result (pass / fail) | What happened |
|---|---|---|---|
| Two pillars of the same colour in a row | — | — | — |
| Pillar at the edge of the picture | — | — | — |
| Battery below the working level | — | — | — |
| Corridor 600 mm (instead of 1000 mm) | — | — | — |
| Start from inside the parking lot | — | — | — |
