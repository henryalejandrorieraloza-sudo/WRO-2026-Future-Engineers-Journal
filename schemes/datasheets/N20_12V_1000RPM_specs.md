# N20 12 V / 1000 RPM encoder gear motor: specifications

Drive motor used on the vehicle.

- **Product:** Anreak "Motor codificador N20 DC 12V 1000RPM para Arduino 3-12V 7.5-3000RPM Motor de alta precisión con retroalimentación (12-1000)"
- **Amazon ASIN:** B0DB28BC6S (style "12 V, 1000 RPM", pack of 2)
- **Model number:** GA12-N20-S0310D-E-12-1000-2Pcs
- **Product URL:** https://www.amazon.com/dp/B0DB28BC6S
- **Purchase:** last order 20 Aug 2026, 2 pieces

This file replaces the earlier `N20_motors.pdf`, which described a different motor (6 V, 33 RPM). That PDF was removed from the repository on 23 Nov 2026; it stays in the git history.

**Update (24 Nov 2026):** the winding resistance of the motor on the car was measured (**11.7 Ω**) and the stall current was calculated from it: **≈ 1.03 A** (range 0.95 to 1.20 A). See [N20_stall_current_measurement.md](N20_stall_current_measurement.md). **Counted on 24 Nov 2026:** motor output-shaft turns against wheel turns = **1 : 1** (no reduction in the differential). The internal gearbox ratio (≈ 17 : 1 from the encoder count) still needs the hand-turn check below.

**Status (23 Nov 2026):** the gear ratio and the stall values of the motor on the car are **not yet confirmed for this ASIN**. The values below come from a datasheet of the same family, which is probably but not certainly the same motor. The team has to measure the ratio on the car (see "Measurements to take").

## Values from the Amazon listing (confirmed for this ASIN)

| Parameter | Value |
|---|---|
| Motor type | Brushed DC gear motor, metal gears, high precision |
| Rated voltage | 12 V DC |
| No-load speed | 1000 RPM |
| Output power | 1 W |
| Encoder | Hall sensor, 7 PPR (pulses per motor revolution) |
| Shaft | Plain D-shaft, 10 mm (0.394 in) |
| Material | Metal |
| Package | 2 motors, 2 encoder cables (about 15 cm / 5.906 in) |

The listing also states:

- The motor works at other voltages (3 V, 6 V, 12 V variants exist). Lower voltage reduces speed, current and torque proportionally. Higher voltage wears the motor.
- Reversing polarity reverses the rotation direction.
- Operating above the rated load causes excess current, overheating, and possible burnout.

The listing does **not** state the gear ratio, the stall current, the stall torque or the rated torque.

## Datasheet of the same family (probable match, not confirmed)

Source: SparkFun, "28633 datasheet N20 Motor With Encoder and Cable pair", for the model **BJ-N20-12-1000 encoder motor**. Its encoder is 7 PPR AB phase, its shaft is 10 mm and its weight is 15 g, which match the ASIN. The sheet does not say whether the counting is 4x.

| Reduction ratio | Rated voltage | No-load current | No-load speed | Rated load current | Rated load speed | Rated load torque | Stall current | Stall torque |
|---|---|---|---|---|---|---|---|---|
| 1:30 | 12 V | 0.06 A | 1000 RPM | 0.3 A | 681 RPM | 0.16 kg·cm | 0.7 A | 0.43 kg·cm |

Two checks say that the 1:30 ratio in this table is **not yet proven** for our motor:

1. For 1000 RPM at the output shaft with 1:30, the motor would have to turn about 30 000 RPM. That is above what an N20 usually reaches at 12 V (estimate from common N20 listings, not from a source in this file).
2. The encoder constant measured on the car is **9.27 ticks per cm** (`TICKS_PER_CM` in `open_challenge`). With the wheel at 41 mm (≈ 12.9 cm per turn), that is about **119 ticks per wheel turn**. At 1:30 with 7 PPR the count would be 210 (one edge), 420 (two edges) or 840 (four edges). None matches 119. Either the ratio is different, or the wheel diameter or the counting in the code is different. This has to be measured.

## Values reported for the same 12 V / 1000 RPM N20 class (unverified for this ASIN)

These come from other sellers of the class. They are kept only for comparison.

| Parameter | Value reported | Source |
|---|---|---|
| Gear ratio | 1:30 | Naylamp; Zbotic |
| Rated load | 0.3 kg·cm torque, 0.28 A current | Vistronica; Sigma Electrónica |
| Stall torque at 12 V | 2.4 kg·cm | Vistronica; Sigma Electrónica |
| Stall current at 12 V | 0.6 A | Vistronica; Sigma Electrónica; Electronilab |
| Stall current (peak) | 600 mA | Naylamp (encoder version) |
| No-load current | 190 mA (Vistronica) or 60 mA (Naylamp) | conflicting sources |

One seller (Zbotic, "GA12-N20") lists a stall current of 1 A, which did not match the others; it does agree with our calculated 1.03 A (24 Nov).

**Note:** the 2.4 kg·cm stall torque in this table is about 5 times the 0.43 kg·cm of the datasheet above. The team no longer uses it as a design value. It is kept only to show the disagreement between sources.

## Design values used until the motor is measured

| Value | Used in the repo | Basis |
|---|---|---|
| Stall current per motor (power budget) | **1.03 A** (calculated from the measured 11.7 Ω, 24 Nov) | `schemes/README.md` §4.2 uses this for the stall case. Range 0.95 to 1.20 A; the driver and the supply are sized for ≥ 1.2 A. The 0.7 A of the datasheet of the same family is no longer used (our motor differs from it) |
| Rated torque at the wheel | 0.16 kg·cm ≈ 16 mN·m | Datasheet of the same family; needed: ≈ 8 mN·m at the wheel (margin ≈ 2×) |
| Stall torque | 0.43 kg·cm ≈ 42 mN·m | Datasheet of the same family (margin ≈ 5× over the need) |

## Measurements to take

1. **Gear ratio:** mark the rear wheel, turn it exactly 10 full turns by hand with the motor off, and count the encoder pulses from one channel (one edge) and, if the firmware counts both edges, from all of them. Divide the pulses per wheel turn by 7 for one edge, by 14 for two edges or by 28 for four edges, to get the ratio.
2. **Stall current:** done indirectly on 24 Nov (winding resistance 11.7 Ω → 1.03 A, see [N20_stall_current_measurement.md](N20_stall_current_measurement.md)). Optional direct check: with a multimeter in series with the battery, hold the wheel for less than 0.5 s and read the peak. Do not hold it longer: the gears can break (the code stops the motor after `STALL_MS`).
3. **No-load speed:** cronometer the time for 1 m with `RUN_PWM 150` and then with `RUN_PWM 255`.
4. **Torque check:** push the car up a ramp of known slope with the motor at a known PWM, or measure the force with a spring scale on the wheel rim.
5. Record every value with the date, the battery voltage and the commit of the code used.

## Sources

- Amazon listing, ASIN B0DB28BC6S (Anreak): https://www.amazon.com/dp/B0DB28BC6S (not readable from the team's tools on 23 Nov; the listing photo shows the same ASIN)
- SparkFun, datasheet of the BJ-N20-12-1000 encoder motor: https://cdn.sparkfun.com/assets/a/3/9/9/9/28633_datasheet_N20_Motor_With_Encoder_and_Cable_pair.pdf
- Electronilab, "Motorreductor de metal 12V 1000RPM N20": https://electronilab.co/tienda/motorreductor-de-metal-12v-1000rpm-n20/
- Vistronica, "Motorreductor N20-20 12V 1000 RPM 0.3 Kg.Cm": https://www.vistronica.com/robotica/motores/motorreductores/motorreductor-n20-20-12v-1000-rpm-0-3-kg-cm-detail.html
- Sigma Electrónica, "N20 DC 12V/1000RPM": https://www.sigmaelectronica.net/producto/n20-dc-12v-1000rpm/
- Naylamp Mechatronics, "Micromotor DC N20 12V/1000RPM con encoder": https://naylampmechatronics.com/motores-dc/811-micromotor-dc-n20-con-encoder-12v-1000rpm.html
- Zbotic, "Buy Ga12 Motor 12v N20 (1000RPM Micro)": https://zbotic.in/product/ga12-n20-12v-1000rpm-micro-dc-reducer-motor-encoder-with-wire/

## To do

1. ~~Motor-to-wheel ratio~~ Counted 1 : 1 on 24 Nov (motor gear and differential gear, 30 teeth each; one turn of one gives one turn of the other). Still to do: the hand-turn count of encoder pulses per wheel turn to confirm the internal ≈ 17 : 1 (see "Measurements to take").
2. ~~Measure the stall current~~ Done 24 Nov (indirect, from the winding resistance: 1.03 A). Optional: confirm with a reading in series.
3. ~~Replace `N20_motors.pdf` (6 V, 33 RPM, a different motor)~~ Done 23 Nov: the 6 V PDF was removed. Optional: save a copy of the product listing next to this file.
4. Update `schemes/README.md` §4.2 and the encoder constants that depend on counts per output revolution, once the ratio is measured.
