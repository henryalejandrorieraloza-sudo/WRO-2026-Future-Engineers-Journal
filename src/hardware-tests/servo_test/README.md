# servo_test

Hardware test for the **steering servo** (MG90) on the Arduino Nano. Use it before any driving code, to find the real center and the mechanical limits of the rack-and-pinion steering.

## What it does
1. Starts, sets the servo to `CENTER` (85, the value measured on chassis v4 on 21 Nov), and waits for the start button on **D10**.
2. Sweeps the servo from `MIN_ANGLE` (10°) to `MAX_ANGLE` (170°) and back, in steps of 2° (`STEP`), every 30 ms (`DELAY_MS`).
3. Pauses for 600 ms at the center and at both limits so you can check the wheels.
4. Prints every angle to the Serial Monitor (115200 baud), with a label at the center and at the limits.

## Wiring
| Part | Nano pin |
|---|---|
| Servo signal | D9 |
| Start button | D10 (other leg to GND, internal pull-up) |

## Settings
| Constant | Default | Meaning |
|---|---|---|
| `MIN_ANGLE` | 10 | Left limit. Lower it only if the linkage does not hit the chassis |
| `MAX_ANGLE` | 170 | Right limit. Same rule |
| `CENTER` | 85 | Angle for wheels straight ahead (measured 21 Nov; same as `STEER_CENTER` in the driving programs). Change it to the real center you measure |
| `STEP` | 2 | Degrees per step |
| `DELAY_MS` | 30 | Speed of the sweep (smaller = faster) |

## How to use
1. Upload the sketch with the car on a stand (wheels in the air), or with the rack free to move.
2. Open the Serial Monitor at 115200 baud.
3. Press the button on D10. Watch the wheels at each `[CENTER]`, `[LEFT LIMIT]` and `[RIGHT LIMIT]`.
4. If the wheels are not straight at `CENTER`, change it and upload again, then copy the value to `STEER_CENTER` in `open_challenge` and `obstacle_challenge`.
5. Measure the wheel angle at each limit with a protractor (on chassis v4: 40° each way) and write it in `docs/TESTS.md`.
6. If the linkage touches the chassis at a limit, reduce `MIN_ANGLE` or raise `MAX_ANGLE` (the values were chosen so the rack never hits its end stops).

## Notes
- The servo is driven with the `Servo.h` library, which is what the rest of the car uses.
- This sketch does not read any sensor and does not move the car.
- The same test in software PWM (no library) and a LEDC version for the ESP32 are in the git history (see `src/README.md`, section 3).
