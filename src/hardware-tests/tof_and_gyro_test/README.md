# tof_and_gyro_test

Hardware test for the **four ToF distance sensors** (TOF400C, VL53L1X) and the **BMI160 gyroscope**, both on the Arduino Nano. It checks that every sensor answers at its own I²C address and that the heading (yaw) is calculated correctly.

## What it does
1. Shuts down the four ToF sensors through their `XSHUT` pins.
2. Turns them on one by one. Each sensor is moved to a unique address, so all four can share the I²C bus:

| Sensor | XSHUT | Address |
|---|---|---|
| LEFT | A3 | 0x30 |
| RIGHT | A2 | 0x31 |
| REAR | A0 | 0x32 |
| FRONT | A1 | 0x33 |

3. Starts the BMI160 (soft reset, chip ID check `0xD1`, normal mode, ±250 °/s).
4. Calibrates the gyro zero offset: 200 samples while the car is still (about 2 s).
5. Waits for the start button on **D10**. Then it prints the four distances and the yaw every 40 ms.

Yaw is calculated by integrating the gyro Z rate: `yaw += (rate − offset) × dt`. Rates under 0.25 °/s are ignored, so a still car does not drift. The angle wraps between −180° and +180°.

## Wiring
| Part | Nano pin |
|---|---|
| I²C SDA / SCL | A4 / A5 |
| ToF XSHUT | A3 (LEFT), A2 (RIGHT), A0 (REAR), A1 (FRONT) |
| Start button | D10 (internal pull-up) |

The BMI160 is read with its registers directly (no gyro library). The address used is `0x69`.

## Output
```
FRONT: 512mm | LEFT: 180mm | RIGHT: 1200mm | REAR: 300mm | YAW: 12.5°
```
`ERR` is printed in place of a distance if that sensor timed out.

## How to use
1. Upload, open the Serial Monitor at 115200 baud. Keep the car still during the calibration.
2. If a sensor is missing, the message `❌ ERROR: ... not detected` names it. Check its wire and the XSHUT pin.
3. If the BMI160 is missing, the test stops after the error (`while (1)`), because the heading cannot work without it. Reset the Nano after fixing the wire.
4. Press the button on D10, then turn the car by hand 90° and check that YAW shows about 90°.
5. Put a hand in front of each sensor to check that its distance changes.

## Settings
| Setting | Value | Meaning |
|---|---|---|
| ToF mode | Short | Less noise, shorter range |
| ToF timing budget | 50 ms | Faster readings |
| ToF period | 50 ms | One new reading every 50 ms |
| Gyro sensitivity | 131.2 LSB per °/s | Used to convert the raw value |
| Dead band | 0.25 °/s | Small rates are treated as zero |

## Notes
- The main driving programs use a different heading filter and the Long mode of the ToF sensors. This test is only for checking the hardware.
- Bus speed is 100 kHz.
