# Go out of parking (`obstacle_go_out_of_parking.ino`)

Arduino Nano sketch for the **start from inside the parking lot** of the Obstacle Challenge (+7 points, rules §10). The car starts parked nose-to-wall between two walls and has to leave the space without touching either wall. It is the only program in `src/` that is not yet tested on the track, so treat it as a draft until the steps in *Before the first run* are done.

**Status (22 Nov 2026):** written 20 Nov, not yet tested on the track. `REVERSE_TARGET_TICKS` is a placeholder, not a measured value (see below).

## How it works

The car measures the free space on both sides, then leaves the space in five steps:

| Step | What the car does | Measured with |
|---|---|---|
| 1. Measure the sides | Reads the left and right distance sensors and picks the side with more room | Left and right ToF (VL53L1X) |
| 2. First turn | Turns 45° toward the open side with the wheels at full lock | Gyroscope (BMI160) |
| 3. Straighten | Drives forward a very short distance with the wheels centred | Encoder (`STRAIGHT_TICKS = 1`) |
| 4. Counter-steer | Turns the wheels back to about 10° of heading | Gyroscope |
| 5. Reverse | Stops, waits 500 ms, then reverses for a fixed number of encoder ticks | Encoder (`REVERSE_TARGET_TICKS`) |

The motor is driven only through the TB6612FNG, on the rear wheels. The reverse slows down over the last 20 % of the target distance for a gentler stop.

## Hardware

| Part | Connection |
|---|---|
| TB6612FNG | PWMA D6, AIN1 D7, AIN2 D8 |
| Encoder | Pin D3 (interrupt pin) |
| Steering servo | D9 |
| Left ToF (VL53L1X) | XSHUT A3, I²C address 0x2A |
| Right ToF (VL53L1X) | XSHUT A2, I²C address 0x2B |
| Front and rear ToF | XSHUT A1 / A0, not used by this sketch |
| BMI160 gyroscope | I²C A4 / A5, address 0x68 or 0x69 (found at start-up) |

## Settings (top of the sketch)

| Constant | Value | Meaning |
|---|---|---|
| `CALIBRATE_ENCODER` | `false` | `true` = only count encoder ticks, the motor stays off |
| `REVERSE_TARGET_TICKS` | `1024` (placeholder) | Encoder ticks for five wheel turns. **Must be measured** |
| `LEFT_LOCK` / `CENTER` / `RIGHT_LOCK` | 10 / 85 / 170 | Servo angles. `CENTER = 85` is the same value as `obstacle_challenge` (measured 21 Nov) |
| `DRIVE_PWM` / `REVERSE_PWM` / `REVERSE_SLOW_PWM` | 90 / 90 / 75 | Motor speeds (0–255) |
| `FIRST_ANGLE_DEG` / `RETURN_ANGLE_DEG` | 45 / 10 | Size of the first turn and of the countersteer |
| `PAUSE_BEFORE_REVERSE_MS` | 500 | Motor off before reversing |
| `TURN_TIMEOUT_MS` / `ENCODER_STALL_MS` | 5000 / 1500 | Safety timeouts |

## Before the first run

1. **Measure the encoder for reverse.** Set `CALIBRATE_ENCODER = true`, upload, open the Serial Monitor at 115200 baud, send `z`, turn the wheel by hand exactly five revolutions, then send `p`. Put the printed count in `REVERSE_TARGET_TICKS` and set `CALIBRATE_ENCODER = false`.
2. **Check the steering.** Confirm with the servo test that 10 is the left lock and 170 the right lock on this chassis, and re-measure the centre (use the servo test in the git history, `src/hardware-tests/servo_test`).
3. **Test the wheels off the floor first**, then on the floor with the car parked against a wall.

## Stop messages

Every stop prints its reason to the Serial Monitor:

| Message | Meaning |
|---|---|
| `BMI160 init or calibration failed.` | Gyroscope not found or did not calibrate. Check the A4 / A5 wiring and the power to the gyroscope |
| `Left ToF init failed.` / `Right ToF init failed.` | Distance sensor not answering. Check its XSHUT pin and address |
| `Gyro read failed.` | Gyroscope stopped answering during the run |
| `First turn timed out.` / `Return turn timed out.` | A turn took longer than `TURN_TIMEOUT_MS` |
| `No forward encoder tick. Check D3.` | The encoder is not counting. Check the wire on D3 |
| `Reverse stopped: no encoder pulses for 1.5 seconds.` | The wheel is blocked or the encoder stopped |
| `Five-turn encoder target reached. Motor stopped.` | Normal end of the reverse |
| `Enter the measured five-turn count in REVERSE_TARGET_TICKS.` | The count is 0: the reverse was never measured. The maneuver is not run |

## Known gaps

- **No start button.** The sketch starts when it is powered. The rules require a start button before a round, so the button on D10 (as in `open_challenge`) must be added before a competition run.
- **`REVERSE_TARGET_TICKS` is not measured yet** (see step 1 above).
- **Steering centre and speeds are not calibrated** for this chassis yet.
