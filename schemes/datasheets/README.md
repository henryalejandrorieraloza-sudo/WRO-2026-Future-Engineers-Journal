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
