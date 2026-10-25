# Changelog

Versions of the car and of this repository. Dates are in 2026.

## v1.3 — 25 October · Rules compliance review
- Re-read the full *WRO 2026 Future Engineers General Rules* (15 Jan 2026) and Appendix C, and checked the repository against them.
- README: corrected the challenge description (corridors 1000 / 600 mm ± 100 mm, 3-minute rounds, no touching the outer wall in the Open Challenge, parking lot rules); new **§3.1 Rules compliance** table (size, mass, drive, steering, no wireless, start procedure, no data entry, reverse driving, stop in the starting section); new **§14.1 Submission checklist** with the rule 7 deadlines counted from the International Final (8 Dec 2026).
- Open Challenge: the competition number of turns is **12** (a 13th turn would leave the starting section, rule 9.24.3); edge cases for the 600 mm corridor and the outer wall added.
- `video/video.md`: one video per challenge (≥ 30 s autonomous) listed as required; Open and Obstacle videos pending.
- `camera_webserver_emakefun` marked as bench test only (Wi-Fi not allowed in rounds, rule 11.10).

## v1.2 — 25 October · The side sensors could not see the black wall → simpler, safer algorithm
- **One complete corner validated** (22–23 Oct, "works excellent"): turn side by opening or by comparing **valid** readings only, 90° gyro turn, FRONT/BACK check that undoes a wrong first turn, then `sideWallRun()` (find the inner wall → deviate to the target → precise P+I+D) and brake when the inner wall ends.
- **Open Challenge loop** (24 Oct): 13 turns with absolute headings, final 1 s straight and active brake. The car got closer to the inner wall every lap (gyro error ~1.5° per turn > the ±8° the wall PID can correct after ~6 turns). Fixed by using **the wall as the heading reference every straight** (`REALIGN_WITH_WALL`), plus an integral zone, asymmetric gains, a danger zone and a filtered D term.
- **Root cause of the wrong-side decisions found with data** (24–25 Oct, `tof_roi_budget_test`): with the side TOF angled 30° and ROI 1 × 6, the black wall at 30, 40, 50 and 60 cm and **no wall** all read ~2 m ("signal fail"). Control test: a white sheet on the wall at the same angle read 34–36 cm, all valid. Sensor **straight**: black wall at 30 cm → 32.0 cm, 30/30 valid. ROI **16 × 8**: 38.1 cm at 40 cm (30/30), 8/30 valid at 60 cm → reliable range ~45 cm.
- **Decision:** side sensors straight (0°) at ~5 cm from the floor, ROI 16 × 8, only "range valid" readings. The algorithm becomes simpler: WALL / NO WALL / NOT SURE from a window of 8 readings replaces the 2600 mm opening, 165 cm close/far and 500 mm jump thresholds.
- **Safety first:** NOT SURE → stop and measure again; decisions with the car still; FRONT safety in every phase; timeouts; lower speed during decisions. Goal: finish the 3 laps safely before going faster.
- Added `tof_left_right_print` and `tof_roi_budget_test`. Journal: Phases 9 and 10 (sections 11–12), Problems 40–46; journal §8.2 marked as overturned. README §6.3, §8, §10 and §13 updated.

## v1.1 — 21 October (night) · "Éxito rotundo": timed corner, no front sensor
- **Validated on the track:** `exito_rotundo`.
- **Straighter driving:** an integral was added to the gyro heading loop (`KI_HEADING = 2.5`, max 10° of steering). It cancels a constant steering offset (servo centre not exact), which made the car drift slowly to the left.
- **Smooth wall switch:** the car keeps the last `REF_HISTORY = 5` wall readings of each side. When the opening is detected (side > 2400 mm, 3 valid readings in a row) the PID follows the opposite wall at the **average of its last 5 readings before the opening**, instead of the distance at that instant, so it no longer drifts when it switches wall.
- **Corner by time instead of the front sensor:** after `RUN_MS = 1800 ms` the car brakes. If an opening was seen, it turns 90° toward it (the opening always wins, it is never re-evaluated). If not, it averages `COMPARE_SAMPLES = 10` readings per side with the car still and turns toward the farther side (invalid reading = no wall).
- Why: comparing the walls with a single reading sometimes chose the wrong side; averaging with the car stopped and trusting the detected opening first fixed it.
- Settings: `MOTOR_SPEED = 100`, `MAX_HEADING_DEG = 5`, turn at `TURN_SPEED = 60` / `SLOW_SPEED = 35`.

## v1.0 — 21 October · "Éxito glorioso": first complete straight + corner
- **Validated on the track:** `exito_glorioso` never hit a wall and always chose the correct turn side.
- Flow: gyro wall following on the nearest wall → a side TOF above `SIDE_OPEN_MM = 2400` (3 valid readings) marks the opening and switches the PID to the opposite wall without braking → the front TOF below `FRONT_STOP_MM = 1400` starts the validated 90° turn (`turn_left_90_pid_brake`, now left or right) toward the opening → active brake.
- Fixes from today's tests: the front TOF sometimes missed the wall because readings with `SigmaFail` and similar statuses were rejected → the front sensor now accepts **any** reading with a distance (40–4000 mm). If no opening was detected, the turn side is chosen by comparing both side walls (the farther one, invalid = no wall).
- Safety: after `MAX_RUN_MS = 40 s` it compares both sides and turns anyway; LED D13 shows when the front TOF is not giving a usable reading (diagnosis without a USB cable).
- Settings: `MOTOR_SPEED = 50`, `MAX_HEADING_DEG = 5`, `KI_DIST = 0.04`, turn at `TURN_SPEED = 40` / `SLOW_SPEED = 35`, `TURN_STEER_DEG = 60`.

## v0.9 — 20 October · Corner detection with the front sensor (in testing)
- Added `wall_follow_gyro_corner_front`: the side TOF sensors only **detect** the corner (reading > 3000 mm, 3 valid readings in a row → `cornerDetected = true`, turn side saved); the **front** TOF decides **when** to brake (front < `FRONT_STOP_MM`, 2 valid readings in a row). After the corner is detected the car stops following the wall and drives straight with the gyroscope.
- Why: the side sensors are angled about 30° forward, so they see the opening L × tan 30° ≈ 0.58 × L before the wall really ends (L = distance to the wall). Braking or turning on the side reading alone would happen at a different place each time.
- Setbacks found while building it: (1) treating invalid TOF readings (-1) as "open" stopped the car at the start even with an impossible threshold → invalid readings are now ignored; (2) a counter set to 100000 overflowed the Nano's 16-bit `int` (max 32 767) and became negative, so the car stopped immediately; (3) side readings above the opening threshold no longer enter the distance PID.
- Current test values: `OPEN_THRESHOLD_MM = 3000`, `FRONT_STOP_MM = 4000`, `MAX_VALID_MM = 4000`. Tested sensor: `front_tof_test`-style raw readout (prints the exact `range_mm`).

## v0.8 — 20 October · Smooth wall following with the gyroscope
- **Validated on the track:** `wall_follow_nearest_gyro` keeps the distance to the nearest wall straight and smooth at `MOTOR_SPEED = 60`.
- Two-loop controller: the side TOF gives a small target heading (max 8°), the BMI160 gyroscope steers to it about 200 times per second.
- Iterations documented: zigzag (KP too high) → too slow (too much smoothing) → S-shaped waves (a distance sensor cannot measure heading) → late correction (delay) → constant offset (KI) → gyroscope.
- TOF timing budget 33 ms and non-blocking reads (`dataReady()`); Serial output limited to 10 Hz.
- Journal: Phase 8 (section 10), Problems 35–39.

## v0.7 — 18 October · Chassis v3, 3D models and tyres
- **3D models published:** 25 STL files of every printed part in `models/stl/` with a render of each part, the STEP assembly link, the CAD of chassis v1, v2 and v3, printing notes and the assembly order (`models/README.md`).
- **Steering iteration documented:** v1 Ackermann → v2 Ackermann with a steering rack → v3 parallel **rack and pinion** (12-tooth pinion, module 1 mm). Reason: the Ackermann linkage damaged three servos (journal Problem 33).
- **Custom silicone tyres:** casting process with C25 silicone in printed moulds, formula B = ¾ A (30 g → 23 drops), 5–6 h cure (`models/README.md §4`, `other/tyres/`).
- **Six official vehicle views** of the final car plus all parts laid out (`v-photos/`). Size 13 × 9.5 × 6 cm, mass 270 g.
- **Power:** TOF sensors now powered from the Nano's 3.3 V pin; power budget recalculated (≈ 2.5 W typical, ≈ 11.0 W peak, ≈ 1.4 h per charge) and new failure points (3.3 V pin current, 5 V UART into the 3.3 V camera).
- **Corrections:** side TOF sensors raised 1.5 cm (not ~2 cm); main controller is the classic Arduino Nano everywhere in the docs; mechanical section rewritten with torque reasoning, 1 : 1 differential, gear geometry and the Ackermann vs parallel trade-off.
- Team and car names explained (Team X67, *Enner Valencia*). Build-process photos, soldered PCB and retired components added (`other/`, `schemes/`).
- Journal: Phase 7 (section 9), Problems 33–34; Problems 17 and 29 closed.

## v0.6 — 15 October · Pinion-and-rack steering on the classic Nano
- Steering changed from Ackermann to a parallel pinion-and-rack mechanism driven by the MG90 servo. Provisional values: centre 120°, left limit 180°, right limit 50° (left is now the larger angle).
- Main board is now a classic Arduino Nano (ATmega328P) after the Nano ESP32 was damaged.
- Added `servo_only_test` (steering sweep), `wall_follow_nearest_pid` (keep the distance to the nearest wall, 10-reading reference) and `wall_keep_both_pid` (keep both start distances).
- Tested settings on the new chassis: `MOTOR_REVERSED = true`, `INVERT_STEER = true`, `MAX_VALID_MM = 2000`.

## v0.5 — 7 October · Final strategy
- Open Challenge: distance references measured once at the start and kept with a PD controller (outer wall after the first corner); corners by sudden jump with the TOF ROI 1 × 6; 90° turns by gyroscope; after the 12th corner a short final straight and active brake.
- Obstacle Challenge: steer until the closest pillar is in the correct third of the image; corners detected by the floor lines with a 90° gyro turn and a corner counter. Parking still being designed.
- Journal §8.5 records what changed and why.

## v0.4 — 7 October · Complete documentation
- Merged the electrical documentation of the team into this repository: PCB schematic, wiring diagram, PCB photos, PCB manufacturing process, datasheets and component pictures (`schemes/`, `other/`).
- Added a power budget with run-time and discharge-rate analysis, and a failure-point table.
- Corrected the pin map: TB6612FNG STBY is tied to +5 V on the PCB; encoder C1/C2 on D3/D2; camera UART on D0/D1.
- Added team photos, vehicle photos, competition photos and the video link (`t-photos/`, `v-photos/`, `video/`).
- Rewrote the README following the five criteria of the documentation rubric: mobility, power and sensors, software, systems thinking and reproducibility. Added a state-machine diagram, a subsystem diagram, a "why X instead of Y" table, a risk table and testing metrics.

## v0.3 — 5 October · Chassis 2
- First tests on the second chassis: TOF ROI chosen by experiment (ROI 1 × 6), side sensors raised ~2 cm.
- Added `servo_calibration` and the first driving test `drive_until_left_open`.
- Added the WRO 2026 Scenario Roulette web tool.
- Journal: Problems 29–32.

## v0.2 — 4 October · Vision
- Pillar detection finished on the ESP32-S3-CAM (HSV + BFS blobs, closest pillar, correct-side check).
- Pillar Vision Lab web tool for live calibration.
- Repository restructured following the WRO Future Engineers template.

## v0.1 — 1–3 October · TOF sensors and gyroscope
- Sharp infrared sensors replaced by 4 × TOF400C (VL53L1X) with unique I²C addresses.
- Own BMI160 register-level driver with yaw integration.
- New Open Challenge state machine (WAITING → STRAIGHT → TURNING → FINISHED).

## v0.0 — 15–22 September · First algorithm
- Turning-direction logic with Sharp sensors, developed with hand tests while the chassis was being built.
- Motor driver and steering servo tests.
- First version of the custom PCB designed in EasyEDA and manufactured by JLCPCB (schematic rev 1.0, 8 July).
