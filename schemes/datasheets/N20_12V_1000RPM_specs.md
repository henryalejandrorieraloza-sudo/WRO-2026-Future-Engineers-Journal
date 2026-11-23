# N20 12 V / 1000 RPM encoder gear motor: specifications

Drive motor used on the vehicle.

- **Product:** Anreak "Motor codificador N20 DC 12V 1000RPM para Arduino 3-12V 7.5-3000RPM Motor de alta precisión con retroalimentación (12-1000)"
- **Amazon ASIN:** B0DB28BC6S (style "12 V, 1000 RPM", pack of 2)
- **Model number:** GA12-N20-S0310D-E-12-1000-2Pcs
- **Product URL:** https://www.amazon.com/dp/B0DB28BC6S
- **Purchase:** last order 20 Aug 2026, 2 pieces

This file replaces the earlier `N20_motors.pdf`, which describes a different motor (6 V, 33 RPM).

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

## Not yet confirmed for this ASIN

- **Gear ratio.** Not stated on the listing. The encoder count at the output shaft is 7 PPR × gear ratio, so this must be known before the odometry constants are used.
- **Stall current and stall torque.** Not stated on the listing.
- **Rated (nominal) torque and current.** Not stated on the listing.

## Values reported for the same 12 V / 1000 RPM N20 class (unverified for this ASIN)

These come from other sellers of the class. Use them only as a reference until the values above are confirmed.

| Parameter | Value reported | Source |
|---|---|---|
| Gear ratio | 1:30 | Naylamp; Zbotic |
| Rated load | 0.3 kg·cm torque, 0.28 A current | Vistronica; Sigma Electrónica |
| Stall torque at 12 V | 2.4 kg·cm | Vistronica; Sigma Electrónica |
| Stall current at 12 V | 0.6 A | Vistronica; Sigma Electrónica; Electronilab |
| Stall current (peak) | 600 mA | Naylamp (encoder version) |
| No-load current | 190 mA (Vistronica) or 60 mA (Naylamp) | conflicting sources |

One seller (Zbotic, "GA12-N20") lists a stall current of 1 A, which does not match the others.

## Design values used until the stall data is confirmed

- Worst-case (stall) current per motor for the power budget: **1.0 A**, the highest reported value, so the budget still holds.
- Stall torque for steering and drive calculations: **2.4 kg·cm** at 12 V.

## Sources

- Amazon listing, ASIN B0DB28BC6S (Anreak): https://www.amazon.com/dp/B0DB28BC6S
- Electronilab, "Motorreductor de metal 12V 1000RPM N20": https://electronilab.co/tienda/motorreductor-de-metal-12v-1000rpm-n20/
- Vistronica, "Motorreductor N20-20 12V 1000 RPM 0.3 Kg.Cm": https://www.vistronica.com/robotica/motores/motorreductores/motorreductor-n20-20-12v-1000-rpm-0-3-kg-cm-detail.html
- Sigma Electrónica, "N20 DC 12V/1000RPM": https://www.sigmaelectronica.net/producto/n20-dc-12v-1000rpm/
- Naylamp Mechatronics, "Micromotor DC N20 12V/1000RPM con encoder": https://naylampmechatronics.com/motores-dc/811-micromotor-dc-n20-con-encoder-12v-1000rpm.html
- Zbotic, "Buy Ga12 Motor 12v N20 (1000RPM Micro)": https://zbotic.in/product/ga12-n20-12v-1000rpm-micro-dc-reducer-motor-encoder-with-wire/

## To do

1. Find the gear ratio and stall current on the Anreak datasheet or the motor's label, and record them here.
2. Replace the 1.0 A design value with the confirmed stall current.
3. Replace `schemes/datasheets/N20_motors.pdf` with the datasheet or a saved copy of the listing.
4. Update `schemes/README.md` §4.2, §5.2 and the encoder constants that depend on counts per output revolution.
