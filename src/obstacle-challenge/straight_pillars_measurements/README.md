# Straight with pillars, measurement version (`straight_pillars_measurements.ino`)

Arduino Nano sketch that drives one straight with pillars using the camera messages, the odometry map and the gyroscope. It is the **measurement snapshot** of 19–21 November 2026: the car measurements, the real steering centre (85) and the wheel angle (40°) were taken in this version.

It is **not the current program**. The validated version, used on the track on 21 November, is [`straight_pillars`](../straight_pillars) (it also stops on the floor line). This folder keeps the earlier code so the development process stays in the repository. Both sketches use the same map-and-heading idea, and most of their settings are the same.

## What is different from `straight_pillars`

| | `straight_pillars_measurements` (this folder) | `straight_pillars` (validated 21 Nov) |
|---|---|---|
| Measurements | Car geometry from 19 Nov, wheel angle 21 Nov | Later measurements and fixes (see its PARAMETERS.md) |
| Floor lines (`blueFound` / `orangeFound`) | Read by the camera message, **not used** | Stops on the floor line and drives to the wall |
| Stop at the end | Stops after `RUN_CM` (2000 cm) or when the camera goes silent | Also stops at the wall (`FRONT_STOP_MM`) |
| Documentation | This README | [README](../straight_pillars/README.md) and [PARAMETERS.md](../straight_pillars/PARAMETERS.md) |

## Idea

The car keeps a **map** of the straight. It knows where it is from the encoder (distance) and the gyroscope (heading). Every camera message gives a pillar's distance ahead and side, which is placed on the map.

- **Red pillar:** the car passes on its right, so the goal is a lateral position at least `TARGET_CM` (17 cm) to the right of the pillar.
- **Green pillar:** the car passes on its left, so the goal is at least `TARGET_CM` to the left.
- The **target heading** is `atan(lateral error / distance still to go)`, limited to `MAX_AIM_DEG` (60°).
- A **PD controller on the gyroscope** turns that heading into a steering angle for the servo.
- After the camera is `RETURN_AFTER_CM` (45 cm) past a pillar's centre, the car is free to return to the home lane (y = 0).

## Hardware and pins (Arduino Nano)

| Function | Pin |
|---|---|
| Camera messages (UART RX from the ESP32-S3-CAM TX, GPIO43) | D0 |
| Encoder | D3 |
| Start button (`INPUT_PULLUP`, pressed = LOW) | D10 |
| Steering servo | D9 |
| Drive motor driver TB6612FNG: PWMA / AIN1 / AIN2 | D6 / D7 / D8 |
| BMI160 gyroscope (I²C) | SDA A4, SCL A5 |

The camera sketch it expects is `obstacle_camera` v3.1b (14 fields; this sketch reads the first 10).

## Settings

| Constant | Value | Meaning |
|---|---|---|
| `RUN_CM` | 2000 | distance to drive before stopping (a test value, cm of encoder travel) |
| `CAM_FWD_CM` | 11.7 | distance from the drive axle (encoder) to the camera lens, along the car |
| `CENTER_ANGLE` | 85 | servo angle that drives straight (measured) |
| `LEFT_LIMIT` / `RIGHT_LIMIT` | 170 / 10 | mechanical limits of the servo, 40° of wheel angle |
| `STEER_LEFT_IS_BIGGER` | false | in this car a higher servo number steers right |
| `DRIVE_SPEED` | 70 | motor PWM (0–255) |
| `KP_HEADING` / `KD_HEADING` | 2.0 / 0.03 | heading PD gains |
| `MAX_STEER` | 70 | largest steering request in servo degrees |
| `TARGET_CM` | 17 | 4.5 (half car) + 10 (gap) + 2.5 (half pillar) |
| `ARRIVE_BEFORE_CM` | 55 | be at the safe side this far before the pillar's front face |
| `RETURN_AFTER_CM` | 45 | camera past the pillar centre before the next pillar is aimed at |
| `SAME_PILLAR_CM` | 15 | a detection this close to a stored pillar is the same pillar |
| `BLEND` | 0.3 | how far each new detection moves a stored pillar |
| `TICKS_PER_CM` | 9.27 | encoder pulses per cm, measured with the motor running (190 ticks / 20.5 cm) |
| `YAW_SCALE` | 1.06 | gyroscope correction, from the hand turn of 90° (`gyro_90_test`) |
| `YAW_REVERSED` | false | true only if the yaw goes down when the car turns left |
| `CAMERA_TIMEOUT_MS` | 500 | no camera message for this long → motor stops |

## Startup sequence

1. The Nano starts the serial port and the motor pins (motor stopped).
2. It waits for **START** (D10). While waiting it prints `camera: OK` (or `no messages`) once per second, so you can check the camera link.
3. It calibrates the gyroscope for about 3 s. **Do not move the car.**
4. It sets yaw, the map and the distance to zero, and starts driving.

## Serial output

- `NEW PILLAR RED` / `NEW PILLAR GREEN`: a pillar was added to the map.
- `PILLAR CLEARED`: the car passed a pillar and the next one became current.
- `drive axle x … y … | yaw … goal … | goalY … | pillar … | cam …`: every 150 ms when `DEBUG_PRINTS` is on.
- `WAITING FOR CAMERA`: no valid message for 500 ms; the motor is stopped.
- `DONE | x … | y … | yaw …`: the run distance was reached.

## How to use

1. Upload to the Nano with the battery off. Hold the **camera's RESET** during the whole upload, because the camera's serial line shares D0.
2. Power the camera with its own code running (`obstacle_camera` v3.1b).
3. Power the car, press START, and wait for the gyroscope calibration.
4. For a test without wheels on the floor, watch `NEW PILLAR` and `pillar … cam …` in the Serial Monitor.

## Known limits

- The map depends on the encoder scale (`TICKS_PER_CM`) and the gyroscope (`YAW_SCALE`). Recheck both if the car drifts.
- A pillar seen only once may be a false detection. The time filter is in the camera, not here.
- The sketch does not look at the floor lines, so it cannot stop at the wall. Use `straight_pillars` for that.
- The gyroscope drifts over time. The calibration is only at start-up.
