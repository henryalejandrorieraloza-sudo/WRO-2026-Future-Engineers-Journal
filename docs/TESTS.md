# Test log

This file records how each test was run and what was measured. One row per run. Keep failed runs: the rubric rewards iteration, not only the final result.

Rows are added after each run, using the final code of the commit shown.

## Open Challenge: 5 attempts (rule 10.2, maximum 30 points)

One row per attempt, with the points the judges give for each criterion of the scoring table (rule 10.2, items 1.1 to 1.3). Fill it in right after each run. Pass criterion: three laps, then a stop inside the starting section. Write the battery voltage, the direction and the stop position in Notes.

Earlier run (22 Nov 2026, commit `3010306` or `c3441cd`, which one ran is not recorded; journal §19.2): three laps (12 corners) completed on the first attempt with no hand intervention, stopped in the starting section close to the start wall. Its points, lap times, direction and battery voltage were not written down, so it is not a row in the table.

How the points are counted:

- **1.1** 1 point for each section driven in the round direction (8 sections per lap, maximum 24 in three laps). Not counted for the finish section or the section after it.
- **1.2** 1 point for each full lap (maximum 3).
- **1.3** 3 points if, after three laps, the car stops by itself with its whole projection inside the finish section (it must not move again for at least 15 s).
- **Touching the outer wall** is not allowed in the Open Challenge (rule 9.18). Record it.
- **Repair** (taking the car off the track): the round points are divided by 2 (item 2).
- **Time** is the tie-breaker between equal scores (rules 10.3 and 10.5).

| Attempt | Date | Commit | 1.1 Sections driven (max 24) | 1.2 Full laps (max 3) | 1.3 Stopped in finish section (3 / 0) | Outer wall touched (yes / no) | Repair taken (yes / no) | Round score (max 30) | Round time (s) | Notes |
|---|---|---|---:|---:|---:|---|---|---:|---:|---|
| 1 | | | | | | | | | | |
| 2 | | | | | | | | | | |
| 3 | | | | | | | | | | |
| 4 | | | | | | | | | | |
| 5 | | | | | | | | | | |

## Obstacle Challenge: 5 attempts (rule 10.2, maximum 62 points; driving program `obstacle_challenge`, without parking)

Procedure: place the car at the start of the Obstacle track with the pillars set (Scenario Roulette), press START, and record each run. One row per attempt, with the points of each criterion of rule 10.2. The `>>> FINAL` line of the Serial Monitor gives the stop position; copy it to Notes. The car does not park yet, so the parking columns stay at 0 until `obstacle_go_out_of_parking` and the parking routine are tested on the track.

How the points are counted:

- **1.1 / 1.2 / 1.3** as in the Open Challenge (24 + 3 + 3).
- **Traffic signs, three laps not finished:** 1.5 = 4 points if no sign was moved, 1.4 = 2 points if one or more were moved (at least one full lap needed).
- **Traffic signs, three laps finished:** 1.7 = 10 points if no sign was moved, 1.6 = 8 points if one or more were moved. Use these when the car completes the three laps.
- **1.8.1** 7 points for starting inside the parking lot and completing at least one full lap.
- **1.8.2 / 1.8.3** 15 points for parking completely inside the lot and parallel (wheels on one side within 2 cm of each other from the wall), 7 points for parking partly or not parallel. The lot limits must not be touched.
- **A red pillar passed on the left or a green pillar passed on the right** (wrong side) stops the round when the car completely crosses the pillar's radius line (rule 9.24.5).
- **Repair:** the round points are divided by 2.
- **Time** decides between equal scores (rule 10.7).

| Attempt | Date | Commit | Scenario (Scenario Roulette) | Direction | 1.1 Sections (max 24) | 1.2 Laps (max 3) | 1.3 Stop in finish section (3 / 0) | Pillars on the correct side (yes / no) | Pillars moved (none / one or more) | Signs points (1.4 / 1.5 / 1.6 / 1.7) | 1.8.1 Started in lot (7 / 0) | 1.8.2 / 1.8.3 Parking (15 / 7 / 0) | Repair taken (yes / no) | Round score (max 62) | Round time (s) | Notes |
|---|---|---|---|---|---:|---:|---:|---|---|---:|---:|---:|---|---:|---:|---|
| 1 | | | | | | | | | | | | | | | | |
| 2 | | | | | | | | | | | | | | | | |
| 3 | | | | | | | | | | | | | | | | |
| 4 | | | | | | | | | | | | | | | | |
| 5 | | | | | | | | | | | | | | | | |

Edge cases of the Obstacle program: see the last section of this file.

## Motor: gear ratio, speed and stall (drive)

Procedure: see `schemes/datasheets/N20_12V_1000RPM_specs.md`, "Measurements to take". Record every value with the battery voltage and the commit of the code used.

| Test | Battery (V) | Method | Value | Notes |
| --- | --- | --- | --- | --- |
| Encoder ticks per cm, motor running | not recorded | 190 ticks over 20.5 cm of real travel | 9.27 ticks/cm | `TICKS_PER_CM` in `open_challenge.ino`, `obstacle_challenge.ino`, `straight_pillars.ino`. Hand-pushed values (12.67, 16.1) are superseded |
| Encoder pulses per wheel turn (10 turns) | — | calculated: π × 41 mm × 9.27 ticks/cm | 119 ticks; ratio 17.1:1 | Ratio = 119.4 ÷ 7 (one edge: the firmware counts only the rising edge of C1). Hand-turn count still to do |
| Stall current (peak, < 0.5 s) | — | datasheet, same family (not measured) | 0.7 A | Do not hold the wheel longer. Multimeter reading still to do |
| Speed over 1 m at `RUN_PWM 150` | 12 V (reference) | calculated upper bound, no load: 12.6 cm/s × 150/255 | 13.5 s | Not measured. Stopwatch test still to do |
| Speed over 1 m at `RUN_PWM 255` | 12 V (reference) | calculated upper bound, no load: 12.6 cm/s | 7.9 s | Not measured. Stopwatch test still to do |
| Torque check (ramp or spring scale) | — | datasheet, same family (not measured) | 0.43 kg·cm stall (≈ 42 mN·m) | Ramp or spring scale still to do |
| Reverse ticks to leave the parking space (five wheel turns) | — | calculated: 5 × π × 41 mm × 9.27 ticks/cm | 597 ticks | Set in `obstacle_go_out_of_parking.ino`. Check with calibration mode |

## Steering: centre, limits and symmetry

Procedure: run `servo_test` with the chassis fully assembled, then measure the real wheel angle at each stop with a protractor or a top-view photo.

| Test | Servo value | Left wheel angle (°) | Right wheel angle (°) | Symmetric? | Notes |
| --- | --- | ---: | ---: | --- | --- |
| Centre (wheels straight) | 85 | — | — | Confirmed | Value confirmed as driving straight. `STEER_CENTER` in the code |
| Left stop | 10 | 40 | — | Yes (±40° each side, measured with a protractor) | Listen for buzzing or forcing. Smaller servo value steers left |
| Right stop | 170 | — | 40 | Yes (±40° each side, measured with a protractor) | Listen for buzzing or forcing |
| Turning radius at full lock | — | wheelbase ÷ tan(40°) | 10.7 cm | — | Wheelbase 9 cm, wheel angle 40° |

## Car geometry and mass

| Measure | Method | Value | Notes |
| --- | --- | --- | --- |
| Wheel diameter, four wheels, mounted | calliper | — | Design value 41 mm |
| Wheelbase (front to rear axle) | ruler | 9 cm | Confirmed on 23 Nov 2026. Also in `straight_pillars.ino`, measured 19 Nov |
| Car width | ruler | 9 cm | Confirmed on 23 Nov 2026. Also in `straight_pillars.ino`, measured 19 Nov. Used everywhere: half width 4.5 cm in `SAFE_LAT_CM` (4.5 + 10 + 2.5 = 17 cm) |
| Full length, with front protrusion | ruler | 17.5 cm | Protrusion about 4 cm. Also in `straight_pillars.ino` |
| Rear (encoder) axle to camera lens | ruler | 11.7 cm | `CAM_FWD_CM` in `obstacle_challenge.ino` and `straight_pillars.ino` |
| Track width (between wheels) | ruler | 9 cm | Measured by the team on 23 Nov 2026 |
| Mass, ready to run, with battery | kitchen scale, ±1 g | 248 g | Weighed with the battery. Note what is mounted |
| Turning sweep at full lock, left (from the lens) | tape measure, 6 runs | 12 cm forward, 40 cm sideways | Includes about 2.4 cm of braking. Source: notes of 28 Oct |
| Turning sweep at full lock, right (from the lens) | tape measure, 6 runs | 15 cm forward, 39 cm sideways | Includes about 2.2 cm of braking. Less repeatable than the left turn in the tape readings. Source: notes of 28 Oct |
| Odometry against tape (lens path, straight part) | tape measure | Y within ±1–2 cm; X 2–3.5 cm more than the odometry | Possible small slip in the curve. Source: notes of 28 Oct |
| Turning sweep at full lock, calculated (from the lens) | calculated | 22.4 cm sideways, −1.0 cm forward | Turn centre 10.7 cm from the rear axle (wheelbase 9 cm ÷ tan 40°); lens 11.7 cm forward of the axle; 90° turn | Geometry only, no braking. Compare with the tape rows above |

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

| Test | Method | Value | Notes |
| --- | --- | --- | --- |
| Drift at rest, 60 s | `src/hardware-tests/tof_and_gyro_test` | — (°/min) | Car still on the floor |
| Heading error after one 90° turn, in motion | Turn with the PD gyro while driving (`turn_measure_moving`), 6 runs | 89.3 to 90.3 ° | Source: notes of 28 Oct. The commit of that sketch was not recorded. The 10 hand turns each way with `gyro_90_test` are still to be done |
| Heading error after 12 turns | 12 × per-turn error range (−0.7° to +0.3°) | −8.4° to +3.6° (worst case) | Not measured. Run `open_challenge` and measure the final heading. Also record the run in the Open Challenge table |
| Yaw of a hand-turned 90° (`gyro_90_test`) | Turn the car exactly 90° by hand | 85° printed, giving `YAW_SCALE` 1.06 | Old value, used only by the earlier `straight_pillars.ino`. Decision (23 Nov): `GYRO_SCALE` 1.007 is the value of `open_challenge` and `obstacle_challenge` |

## Camera: frame rate and colour detection

| Test | Resolution | Lighting | Value | Notes |
| --- | --- | --- | --- | --- |
| Frames per second, colour detection | 160 × 120 | room / venue | — (fps) | The README asks for this value; it was measured only at 320 × 240 |
| Pillar detected at 20, 40, 60 cm | not recorded | not recorded | Detected up to about 50 cm (centred pillar); 60 cm not tested | Source: notes of 27 Oct. Red and green, each side, not recorded separately |
| Pillar width in pixels at 20, 40, 60 cm | 160 × 120 | — | 12.5 px / 6.25 px / 4.2 px | Calculated, not measured: 50 px focal length × 5 cm pillar ÷ distance. `MAX_AHEAD_CM` = 40 in `obstacle_camera_v3_2` |
| Floor line detected at 9, 16 cm | not recorded | not recorded | Orange from 16 cm; blue from 9 cm | Source: camera v3.1 test, journal Phase 11. No false line on the empty floor or the black wall |
| Focal length at 160 × 120, `FOCAL_PX` | 160 × 120 | camera_viewer.html | — | 50 | Value in `obstacle_camera_v3_2.ino`: half of the 100 px measured at 320 × 240 in `camera_test.ino`. Confirmed by the team on 23 Nov 2026 as the correct value |

## Obstacle Challenge: edge cases to add

The cases below are suggestions, not results. Replace them with the cases the team checked on 23 Nov (team report, not yet written down), and add one row per case with the commit tested and the result.

| Case | Result (pass / fail) | What happened |
| --- | --- | --- |
| Two pillars of the same colour in a row | — | — |
| Pillar at the edge of the picture | — | — |
| Battery below the working level | — | — |
| Corridor 600 mm (instead of 1000 mm) | — | — |
| Start from inside the parking lot | — | — |
