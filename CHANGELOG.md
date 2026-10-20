# Changelog

Versions of the car and of this repository. Dates are in 2026.

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
