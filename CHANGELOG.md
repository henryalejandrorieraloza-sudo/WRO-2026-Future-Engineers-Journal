# Changelog

Versions of the car and of this repository. Dates are in 2026.

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
