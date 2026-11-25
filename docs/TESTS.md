# Test log

This file records how each test was run and what was measured. One row per run. Keep failed runs: the rubric rewards iteration, not only the final result.

Rows are added after each run, using the final code of the commit shown.

## Open Challenge: attempts (rule 10.2, maximum 30 points; 6 runs logged on 24 Nov)

One row per attempt, with the points the judges give for each criterion of the scoring table (rule 10.2, items 1.1 to 1.3). Fill it in right after each run. Pass criterion: three laps, then a stop inside the starting section. Write the battery voltage, the direction and the stop position in Notes.

Earlier run (22 Nov 2026, journal §19.2): three laps (12 corners) completed on the first attempt with no hand intervention, stopped in the starting section close to the start wall. Its commit is `3010306` or `c3441cd`; which one ran is not recorded. Both have `STEER_CENTER` 85 and `TICKS_PER_CM` 9.27, while their parent `30f5ca9` has 90 and 15.1. Its points, lap times, direction and battery voltage were not written down, so it is not a row in the table.

How the points are counted:

- **1.1** 1 point for each section driven in the round direction (8 sections per lap, maximum 24 in three laps). Not counted for the finish section or the section after it.
- **1.2** 1 point for each full lap (maximum 3).
- **1.3** 3 points if, after three laps, the car stops by itself with its whole projection inside the finish section (it must not move again for at least 15 s).
- **Touching the outer wall** is not allowed in the Open Challenge (rule 9.18). Record it.
- **Repair** (taking the car off the track): the round points are divided by 2 (item 2).
- **Time** is the tie-breaker between equal scores (rules 10.3 and 10.5).

| Attempt | Date | Commit | 1.1 Sections driven (max 24) | 1.2 Full laps (max 3) | 1.3 Stopped in finish section (3 / 0) | Outer wall touched (yes / no) | Repair taken (yes / no) | Round score (max 30) | Round time (s) | Notes |
|---|---|---|---:|---:|---:|---|---|---:|---:|---|
| 1 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 24.21 | Direction **ccw**. Reported as success, final stop good. Started in a central section. Time confirmed by the team as the full three-lap round |
| 2 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 24.36 | Direction **ccw**. Reported as success, final stop good. Started in one of the sections farthest from the inner wall. Time confirmed by the team as the full three-lap round |
| 3 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 24.41 | Direction **ccw**. Reported as success, final stop good. Started in one of the sections closest to the inner wall. Time confirmed by the team as the full three-lap round |
| 4 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 24.79 | Direction **cw**. Reported as success, final stop good. Started in a central section. Time confirmed by the team as the full three-lap round |
| 5 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 24.97 | Direction **cw**. Reported as success, final stop good. Started in one of the sections farthest from the inner wall. Time confirmed by the team as the full three-lap round |
| 6 | 24 Nov | f0b94b7 | 24 | 3 | 3 | no | no | 30 | 25.19 | Direction **cw**. Reported as success, final stop good. Started in one of the sections closest to the inner wall. Time confirmed by the team as the full three-lap round |

**Runs of 24 Nov 2026 (team report).** Six Open Challenge runs, three counter-clockwise (ccw) and three clockwise (cw), each from a different starting section (central, farthest from and closest to the inner wall). All six: success yes, final stop good. Round times: ccw 24.21, 24.36, 24.41 s; cw 24.79, 24.97, 25.19 s (range 24.21–25.19 s; the times rise from run to run, and the cw runs are on average about 0.65 s slower than the ccw runs). **Confirmed by the team afterwards:** no repair and no outer-wall touch in any run, three laps completed in all six, and the final stop inside the required section each time, so each round is entered as 24 + 3 + 3 = **30 points**. The runs used `STOP_OFFSET_CM = 10`, the value now in `open_challenge.ino`. **Battery:** the first run started with a fully charged pack, and the team did not measure the voltage before each run; a fully charged pack measured 13.05 V on 24 Nov (schemes §4.4), but the voltage of these runs is **not measured**. The slow rise in time from run 1 to run 6 (24.21 to 25.19 s) could go with the pack discharging, but it cannot be confirmed without measurements. **Not recorded:** the commit. The team confirmed that each time is the **full round of three laps**, so it is entered in the "Round time (s)" column.

## Obstacle Challenge: 3 runs logged (rule 10.2, maximum 62 points; driving program `obstacle_challenge`, with the parking exit as its first stage)

Procedure: place the car at the start of the Obstacle track with the pillars set (Scenario Roulette), press START, and record each run. One row per attempt, with the points of each criterion of rule 10.2. The `>>> FINAL` line of the Serial Monitor gives the stop position; copy it to Notes. The driver leaves the parking space itself (parking exit is its first stage). The stop inside the parking lot (1.8.2 / 1.8.3) is not implemented yet, so those columns stay at 0.

How the points are counted:

- **1.1 / 1.2 / 1.3** as in the Open Challenge (24 + 3 + 3).
- **Traffic signs, three laps not finished:** 1.5 = 4 points if no sign was moved, 1.4 = 2 points if one or more were moved (at least one full lap needed).
- **Traffic signs, three laps finished:** 1.7 = 10 points if no sign was moved, 1.6 = 8 points if one or more were moved. Use these when the car completes the three laps.
- **1.8.1** 7 points for starting inside the parking lot and completing at least one full lap.
- **1.8.2 / 1.8.3** 15 points for parking completely inside the lot and parallel (wheels on one side within 2 cm of each other from the wall), 7 points for parking partly or not parallel. The lot limits must not be touched.
- **A red pillar passed on the left or a green pillar passed on the right** (wrong side) stops the round when the car completely crosses the pillar's radius line (rule 9.24.5).
- **Repair:** the round points are divided by 2.
- **Time** decides between equal scores (rule 10.7).

| Attempt | Date | Commit | Direction | 1.1 Sections (max 24) | 1.2 Laps (max 3) | 1.3 Stop in finish section (3 / 0) | Pillars on the correct side (yes / no) | Pillars moved (none / one or more) | Signs points (1.4 / 1.5 / 1.6 / 1.7) | 1.8.1 Started in lot (7 / 0) | 1.8.2 / 1.8.3 Parking (15 / 7 / 0) | Repair taken (yes / no) | Round score (max 62) | Round time (s) | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 24 Nov | 32d873d | right | 24 | 3 | 3 | yes | none | 10 | 7 | 0 | no | 47 | 1:40 | Stopped in the correct section, not in the parking lot. Started in the parking lot. |
| 2 | 24 Nov | 32d873d | right | 24 | 3 | 3 | yes | none | 10 | 7 | 0 | no | 47 | 1:41 | Stopped in the correct section, not in the parking lot. Started in the parking lot. |
| 3 | 23 Nov | 32d873d | left | 24 | 3 | 3 | yes | none | 10 | 7 | 0 | no | 47 | 2:00 | Stopped in the correct section, not in the parking lot. No pillars touched. |

Scenarios: random, picked by the Scenario Roulette page for each run.

Obstacle edge cases are not logged yet: there are no measured results for them. How the code handles them is in README §9.10.

## Motor: gear ratio, speed and stall (drive)

Procedure: see `schemes/datasheets/N20_12V_1000RPM_specs.md`, "Measurements to take".

| Test | Method | Value | Notes |
| --- | --- | --- | --- |
| Encoder ticks per cm, motor running | 190 ticks over 20.5 cm of real travel | 9.27 ticks/cm | `TICKS_PER_CM` in `open_challenge.ino` and `obstacle_challenge.ino` (also in the earlier `straight_pillars.ino`, git history). Earlier values (15.1 and 12.67 pushed by hand, 16.1 with the previous motor) are superseded |
| Motor output-shaft turns against wheel turns | counted by hand, 24 Nov 2026 | 1 : 1 | Motor gear and differential gear have 30 teeth each; one turn of one gives one turn of the other. The differential adds no reduction. Does not measure the gearbox inside the N20 |
| Encoder pulses per wheel turn (10 turns) | calculated: π × 41 mm × 9.27 ticks/cm | 119 ticks; internal gearbox ≈ 17.1 : 1 | Ratio = 119.4 ÷ 7 (one edge: the firmware counts only the rising edge of C1). This ratio is inside the N20; the 1000 rpm rating is already the output speed after it. Hand-turn count still to do |
| Winding resistance (rotor still, motor disconnected) | Multimeter on the Ω scale, probes on the motor terminals, room temperature, 24 Nov | 11.7 Ω | Measured. Full procedure: `schemes/datasheets/N20_stall_current_measurement.md` |
| Stall current (peak) | calculated: 12 V ÷ 11.7 Ω | 1.03 A (range 0.95 to 1.20 A) | Calculated from the measured resistance, not read in series. The 0.7 A of the datasheet of the same family is no longer used. A reading in series (wheel held < 0.5 s) is still optional |
| Speed over 1 m at `RUN_PWM 150` | calculated upper bound, no load: 1000 rpm × 12.9 cm × 150/255 ≈ 1.26 m/s | ≥ 0.8 s | Not measured. Stopwatch test still to do. **Correction (24 Nov):** the earlier 12.6 cm/s (13.5 s) divided by the gearbox ratio twice (README §5.2). The 22 Nov 3-lap run, with the 8 s no-corner stop active, already shows the car is much faster than 12.6 cm/s |
| Speed over 1 m at `RUN_PWM 255` | calculated upper bound, no load: 1000 rpm × 12.9 cm ≈ 2.15 m/s | ≥ 0.47 s | Not measured. Stopwatch test still to do (corrected 24 Nov, see the row above) |
| Torque check (ramp or spring scale) | datasheet, same family (not measured) | 0.43 kg·cm stall (≈ 42 mN·m) | Ramp or spring scale still to do |
| Reverse ticks to leave the parking space | confirmed by the team on the car, 24 Nov 2026 | 1200 ticks | `PARK_REVERSE_TICKS` in `obstacle_challenge.ino` = 1200. The 597 ticks calculated from five wheel turns (5 × π × 4.1 cm × 9.27 ticks/cm) is not used |

## Steering: centre, limits and symmetry

Procedure: run `servo_test` with the chassis fully assembled, then measure the real wheel angle at each stop with a protractor or a top-view photo.

| Test | Servo value | Left wheel angle (°) | Right wheel angle (°) | Symmetric? | Notes |
| --- | --- | ---: | ---: | --- | --- |
| Centre (wheels straight) | 85 | — | — | Confirmed | Value confirmed as driving straight. `STEER_CENTER` in the code |
| Left stop | 10 | 40 | — | 40° measured with a protractor; centre check open (see below) | Listen for buzzing or forcing. Smaller servo value steers left |
| Right stop | 170 | — | 40 | 40° measured with a protractor; centre check open (see below) | Listen for buzzing or forcing |
| Turning radius at full lock | — | wheelbase ÷ tan(40°) | 10.7 cm | — | Wheelbase 9 cm, wheel angle 40° |

**Open check (confirmed by Henry, 24 Nov).** The design centre is 85 and the sides are ±80, so both programs use 5 / 165. The protractor stops are 10 and 170: for the same 40°, the left stop is 75 units from 85 and the right stop is 85 units from 85, so the stops average 90, not 85. They are kept as the record of the test. Open item: measure the wheel angle again with the wheels straight and record it here.

**What the code uses (aligned 24 Nov 2026).** `open_challenge` and `obstacle_challenge` both take `STEER_CENTER` 85 ± `STEER_RANGE` 80, so the limits are **5 (left) and 165 (right)**. The measured stops above (10 and 170) are the record of the protractor test. The code limit on the left (5) is 5 servo units beyond the measured left stop (10) and the right limit (165) is 5 units short of the right stop (170): listen for buzzing at full lock on the left and re-check it with `servo_test`. A servo pushed against a stop stalls at about 1.27 A (table above), and the servo code has no stall timeout.

## Steering servo MG90: winding resistance and stall current

Procedure and numbers: `schemes/datasheets/MG90_stall_current_measurement.md` (original PDF in Spanish next to it).

| Test | Method | Value | Notes |
| --- | --- | --- | --- |
| Winding resistance (internal DC motor, rotor still) | Multimeter on the Ω scale, 24 Nov | 3.3 Ω | Measured. Probe-lead tare 0.2–0.5 Ω, so the pure winding may be 2.8–3.0 Ω |
| Stall current | calculated: (5.0 V − 0.8 V) ÷ 3.3 Ω | 1.27 A (range 1.0 to 1.3 A; ideal 1.52 A) | Calculated, not read in series. The 0.8 V drop in the H-bridge and wiring is an assumption (0.5 V → 1.36 A, 1.0 V → 1.21 A). The 650 mA used before is superseded |
| Sustained stall consumption / start peak | from the same calculation | 0.8–1.2 A / up to 1.3 A | Heat at stall ≈ 5.3 W in the motor + 1.0 W in the bridge: do not hold a stop for more than a few seconds |
| 5 V rail under servo stall, four ToF ranging | Multimeter on the 5 V rail | — | Not measured yet; the rail peak is calculated at ≈ 1.67 A of 3 A |

## Car geometry and mass

| Measure | Method | Value | Notes |
| --- | --- | --- | --- |
| Wheel diameter, four wheels, mounted | calliper | — | Design value 41 mm |
| Wheelbase (front to rear axle) | ruler | 9 cm | Confirmed on 23 Nov 2026 (first measured 19 Nov for the earlier `straight_pillars.ino`) |
| Car width, including the wheels | ruler | 9 cm | Confirmed on 23 Nov 2026 and again on 24 Nov 2026. Used everywhere: half width 4.5 cm in `SAFE_LAT_CM` and `TARGET_CM` (4.5 + 10 + 2.5 = 17 cm) |
| Full length, with front protrusion (total length of the car) | ruler | 17.5 cm | ≈ 13.5 cm without the protrusion (120 mm base plus the camera overhang) plus about 4 cm of protrusion. This 17.5 cm is the length declared for the parking-lot rule (lot = 1.5 × 17.5 ≈ 26.25 cm) |
| Rear (encoder) axle to camera lens | ruler | 11.7 cm | `CAM_FWD_CM` in `obstacle_challenge.ino` |
| Car body width, without the wheels | ruler | 7 cm | Measured on 24 Nov 2026 (the 9 cm above is this 7 cm plus the wheels) |
| Mass, ready to run, with battery | kitchen scale, ±1 g | 248 g | Weighed with the battery. Note what is mounted |
| Turning sweep at full lock, left (from the lens) | tape measure, 6 runs | 12 cm forward, 40 cm sideways | Includes about 2.4 cm of braking. Source: notes of 28 Oct |
| Turning sweep at full lock, right (from the lens) | tape measure, 6 runs | 15 cm forward, 39 cm sideways | Includes about 2.2 cm of braking. Less repeatable than the left turn in the tape readings. Source: notes of 28 Oct |
| Odometry against tape (lens path, straight part) | tape measure | Y within ±1–2 cm; X 2–3.5 cm more than the odometry | Possible small slip in the curve. Source: notes of 28 Oct |
| Turning sweep at full lock, calculated (from the lens) | calculated | 22.4 cm sideways, −1.0 cm forward | Turn centre 10.7 cm from the rear axle (wheelbase 9 cm ÷ tan 40°); lens 11.7 cm forward of the axle; 90° turn. Geometry only, no braking. Compare with the tape rows above |

## Gyroscope: drift and turn accuracy

| Test | Method | Value | Notes |
| --- | --- | --- | --- |
| Drift at rest, 60 s | `src/hardware-tests/tof_and_gyro_test` | — (°/min) | Car still on the floor |
| Heading error after one 90° turn, in motion | Turn with the PD gyro while driving (`turn_measure_moving`), 6 runs | 89.3 to 90.3 ° | Source: notes of 28 Oct. The sketch is in the repository: `src/hardware-tests/turn_measure_moving/` (version of 28 Oct). Its constants were `YAW_SCALE` 1.00, `TICKS_PER_CM` 15.1 and steering centre 120, so these runs are not the error of the current code (see its README). The 10 hand turns each way with `gyro_90_test` are still to be done |
| Heading error after 12 turns | 12 × per-turn error range (−0.7° to +0.3°) | −8.4° to +3.6° (worst case) | Not measured. Run `open_challenge` and measure the final heading. Also record the run in the Open Challenge table |
| Yaw of a hand-turned 90° (`gyro_90_test`) | Turn the car exactly 90° by hand | 85° printed, giving `YAW_SCALE` 1.06 | Old value, used only by the earlier `straight_pillars.ino`. Decision (23 Nov): `GYRO_SCALE` 1.007 is the value of `open_challenge` and `obstacle_challenge` |

## Camera: frame rate and colour detection

| Test | Resolution | Lighting | Value | Notes |
| --- | --- | --- | --- | --- |
| Frames per second, colour detection | 160 × 120 | room / venue | — (fps) | The README asks for this value; it was measured only at 320 × 240 |
| Pillar detected at 20, 40, 60 cm | not recorded | not recorded | Detected up to about 50 cm (centred pillar); 60 cm not tested | Source: notes of 27 Oct. Red and green, each side, not recorded separately |
| Pillar width in pixels at 20, 40, 60 cm | 160 × 120 | — | 12.5 px / 6.25 px / 4.2 px | Calculated, not measured: 50 px focal length × 5 cm pillar ÷ distance. `MAX_AHEAD_CM` = 40 in `obstacle_camera_v3_2` |
| Floor line detected at 9, 16 cm | not recorded | not recorded | Orange from 16 cm; blue from 9 cm | Source: camera v3.1 test, journal Phase 11. No false line on the empty floor or the black wall |
| Focal length at 160 × 120, `FOCAL_PX` | 160 × 120 | — | 50 | Value in `obstacle_camera_v3_2.ino`: half of the 100 px measured at 320 × 240 in `camera_test.ino`. Reported correct by the team on 23 Nov 2026; confirmed with the camera viewer at 160 × 120 (24 Nov 2026) |
