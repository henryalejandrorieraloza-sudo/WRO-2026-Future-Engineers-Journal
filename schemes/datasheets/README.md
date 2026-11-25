# Datasheets

Which PDF matches which part on the car:

| File | Part | On the current car? |
|---|---|---|
| [`SG90.PDF`](SG90.PDF) | SG90 servo | No. It was the first steering servo; it moved inside its mount (journal Problem 8) and was replaced |
| MG90S (external) | MG90S metal-gear servo, steering | **Yes.** The local copy is not in this folder yet; the datasheet is linked from [`schemes/README.md`](../README.md) |
| [`MG90_stall_current_measurement.md`](MG90_stall_current_measurement.md) | MG90 stall current | Measurement notes for the steering servo |
| [`N20_12V_1000RPM_specs.md`](N20_12V_1000RPM_specs.md) | N20 drive motor | Yes |
| [`BMI160.pdf`](BMI160.pdf) | BMI160 gyroscope | Yes |
| [`DS_vl53l1x.pdf`](DS_vl53l1x.pdf) | VL53L1X distance sensors | Yes |
| [`OV2640DS.pdf`](OV2640DS.pdf) | OV2640 camera | Yes (camera board) |
| [`TB6612FNG.PDF`](TB6612FNG.PDF) | TB6612FNG motor driver | Yes |
| [`CN3903.PDF`](CN3903.PDF) | CN3903 buck converter, 11.4 V → 5 V, 3 A | Yes |

The steering servo on the car is the **MG90S**, not the SG90. Use the MG90S datasheet for the pulse range and the angle limits. The centre (85) and the limits (5 / 165) in the code were measured on the car, not taken from the datasheet.

## MG90S specifications (steering servo on the car)

Source: the MG90S datasheet published by Tinytronics (https://www.tinytronics.nl/product_files/000263_Data_Sheet_of_MG90S_Analog_Servo_Motor.pdf), read on 24 Nov 2026. Values as the datasheet states them; the car's own limits are in the code.

| Parameter | Value |
|---|---|
| Operating voltage | 4.8 V and 6.0 V |
| Stall torque (peak) | 1.8 kg·cm at 4.8 V; 2.0 kg·cm at 6.0 V |
| Speed, no load | 0.12 s/60° at 4.8 V; 0.09 s/60° at 6.0 V |
| Dead band | 5 µs |
| Rotation range | 180° ± 3° for pulses of 500–2500 µs; limit angle 200° ± 5° |
| Pulse width range | 500–2500 µs |
| Centre (neutral) pulse | 1500 µs |
| Gear | Metal |
| Weight | 13.5 g ± 5 % |
| Dimensions | 22.4 × 12.1 × 22.8 mm |
| Operating temperature | −25 °C to 70 °C |

The servo's own neutral pulse is 1500 µs; the code uses its own steering values (centre 85, limits 5 / 165), which were measured on the car.
