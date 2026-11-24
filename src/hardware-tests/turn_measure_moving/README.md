# turn_measure_moving: how the car turns 90° while moving

This sketch produced the six "heading error after one 90° turn, in motion" rows of [docs/TESTS.md](../../../docs/TESTS.md) (gyroscope section). It was added to the repository on 23 November 2026 because the test log cited it without the code.

## What it does

1. Drives STRAIGHT_CM (40 cm) straight, with a gyro heading correction.
2. Turns 90° without stopping, with a gyro PD controller (the same method as `turn_left_90_pid_brake`).
3. Straightens the wheels and brakes at the same time, then waits until the car stops.
4. Saves the run in the Nano's EEPROM (up to 10 runs) and prints it.

Runs 1–3 turn left, runs 4–6 turn right. Each run prints the encoder distances, the angle reached when the wheels straightened and the final angle, and the camera point from odometry.

## How to use it

- Upload to the Arduino Nano with the camera RESET button held during the upload (the camera is on D0), as the comment at the top of the sketch says.
- Put the car on the mark. The LED blinks once before a left run and twice before a right run. Press START.
- Do not touch the car while the LED is on (gyro calibration).
- Connect the USB afterwards and open the Serial Monitor (115200 baud) to read all saved runs.
- To erase the memory, hold START while powering on (or pressing RESET) for 2 s.

## Constants used for the 6 runs of 28 October

The file in this folder is the version kept in the project file created on 28 October 2026. Its constants are **not** the ones of the current programs:

| Constant | This sketch (28 Oct) | Current programs |
|---|---|---|
| `YAW_SCALE` | 1.00 | `GYRO_SCALE` 1.007 |
| `TICKS_PER_CM` | 15.1 | 9.27 |
| `CENTER_ANGLE` (straight wheels) | 120 | `STEER_CENTER` 85 |
| Steering limits | 180 / 50 | 10 / 170 |

So the 89.3–90.3° results show how the turn method behaves on the car as it was on 28 October. They do not show the error of the current code. Use them as a reference for the method, and repeat the test with the current constants before quoting them as the error of the final car.

## Open items

- `CAM_FROM_REAR_AXLE_CM` is set to 9.0 with the comment "MEASURE". The measured value (11.7 cm) is used in the current programs.
- The current programs do not use this sketch; it is a test tool only.
