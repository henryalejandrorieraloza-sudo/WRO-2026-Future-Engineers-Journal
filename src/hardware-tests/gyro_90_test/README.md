# gyro_90_test

Standalone hand-turn calibration test for the **BMI160 gyroscope**, used to measure `YAW_SCALE` and confirm `YAW_REVERSED` (it was written for the earlier `straight_pillars.ino` sketch, now only in the git history; the current programs use `GYRO_SCALE = 1.007` and `YAW_SIGN`) (Obstacle Challenge). It reuses that sketch's exact register-level BMI160 driver (no external gyro library), so a value measured here applies directly to the main sketch with no conversion.

## Why this is needed

The gyroscope measures angular **rate** (degrees per second), not angle. `straight_pillars.ino` integrates that rate over time into an absolute heading (`yaw`). A real sensor never reads exactly the true rate: there is a small gain error (needs `YAW_SCALE`), and the sign depends on which way the chip sits on the board (needs `YAW_REVERSED`). Both have to be measured on the actual car.

## How to use

1. Upload this sketch to the Nano. Open the Serial Monitor at 115200 baud.
2. Put the car still on the floor. Wait for `GYRO 90 TEST ready.`
3. Press the button on **D10** and immediately let go of the car (it must stay still). This calibrates the gyro's resting bias (~1 s) and zeroes `yaw`.
4. Turn the car **by hand exactly 90° to the left** (use a set square, a book corner, or floor marks), in one smooth motion, no overshoot-and-correct.
5. Leave the car still and read the last `yaw = ..` value printed.

## Reading the result (call it Y)

- If **Y is negative** (yaw went down instead of up when turning left): set `YAW_REVERSED = true` in `straight_pillars.ino` (it currently is `false`).
- **New `YAW_SCALE` = 90.0 / abs(Y)**
  - Example: Y = 73.5 → `YAW_SCALE = 90/73.5 = 1.224`
  - Example: Y = 108.2 → `YAW_SCALE = 90/108.2 = 0.832`
- Put that `YAW_SCALE` (and the correct `YAW_REVERSED`) into `straight_pillars.ino` and re-upload.
- Repeat 2-3 times; results within ±5° of each other are fine.

## Result on this chassis (21 November 2026)

Y = 85° for an intended 90° turn, and the sign was positive (so `YAW_REVERSED` stays `false`). `YAW_SCALE = 90 / 85 = 1.06`. Both values are already applied in `straight_pillars.ino`.

## Wiring

Same as `straight_pillars.ino`: I²C SDA = A4, SCL = A5 (BMI160 at 0x68 or 0x69), START button on D10 (internal pull-up).

## Notes

- `YAW_REVERSED` and `YAW_SCALE` in this test sketch are hardcoded to their neutral values (`false` / `1.00`) on purpose, so the printed number is the *raw* integrated yaw — exactly what the `YAW_SCALE = 90 / Y` formula above needs. Do not change them here; change them in `straight_pillars.ino` once Y is known.
- If `ERROR: BMI160 not found` prints, check the SDA/SCL wiring before anything else — the test halts there, because no yaw measurement is possible without the sensor.
