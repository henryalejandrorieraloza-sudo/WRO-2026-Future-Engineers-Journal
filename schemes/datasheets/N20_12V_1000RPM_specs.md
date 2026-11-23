# N20 12 V / 1000 RPM encoder gear motor: specifications

Drive motor used on the vehicle: Anreak encoder motor, style "12 V, 1000 RPM" (the Amazon listing title reads "N20 DC 12V 1000RPM ... Encoder Motor for Arduino 3-12V 7.5-3000RPM ... (12-1000)"). Purchased 2 pieces, last order 20 Aug 2026.

This file replaces the earlier `N20_motors.pdf`, which describes a different motor (6 V, 33 RPM).

## Status

- **Anreak datasheet:** not found. The Amazon product page blocks automated access, so the values for this exact listing could not be read.
- **Values below:** taken from sellers of the same 12 V / 1000 RPM N20 class. They are **not confirmed for the Anreak unit**. Confirm them against the listing's specification section before the final documentation is submitted.

## Values reported for the 12 V / 1000 RPM N20 class

| Parameter | Value reported | Source |
|---|---|---|
| Nominal voltage | 12 V DC (range 6–12 V) | Electronilab; Sigma Electrónica |
| No-load speed at 12 V | 1000 RPM | Vistronica; Sigma Electrónica |
| Gear ratio | 1:30 | Naylamp; Zbotic |
| Rated (nominal) load | 0.3 kg·cm torque, 0.28 A current | Vistronica; Sigma Electrónica |
| Stall torque at 12 V | 2.4 kg·cm | Vistronica; Sigma Electrónica |
| Stall current at 12 V | 0.6 A | Vistronica; Sigma Electrónica; Electronilab |
| Stall current (peak) | 600 mA | Naylamp (encoder version) |
| No-load current | 190 mA (Vistronica) or 60 mA (Naylamp, encoder version) | conflicting sources |
| Shaft | 3 mm D-shaft, 9–10 mm long | Zbotic; Electronilab |

## Encoder

- Not confirmed for the Anreak unit. Encoder resolution (counts per motor revolution) must be measured or taken from the listing before the odometry constants are used.
- Other N20 encoder motors in the same class report a basic pulse count of 7 per motor revolution, multiplied by the gear ratio at the output shaft (SparkFun ROB-28633 datasheet; Waveshare DCGM-N20 page). This is a reference only.

## Conflicting data

- One seller (Zbotic, "GA12-N20") lists a stall current of **1 A**, a rated current of 40 mA, and a stall torque of 2 kg·cm. These do not match the other sources.
- The no-load current differs by a factor of three between sources (60 mA vs. 190 mA), which suggests different gearbox or encoder variants.

## Design values used until the Anreak values are confirmed

- Worst-case (stall) current per motor for the power budget: **1.0 A**, taken from the highest reported value so the budget still holds with the outlier data.
- Stall torque for steering and drive calculations: **2.4 kg·cm** at 12 V.

## Sources

- Electronilab, "Motorreductor de metal 12V 1000RPM N20": https://electronilab.co/tienda/motorreductor-de-metal-12v-1000rpm-n20/
- Vistronica, "Motorreductor N20-20 12V 1000 RPM 0.3 Kg.Cm": https://www.vistronica.com/robotica/motores/motorreductores/motorreductor-n20-20-12v-1000-rpm-0-3-kg-cm-detail.html
- Sigma Electrónica, "N20 DC 12V/1000RPM": https://www.sigmaelectronica.net/producto/n20-dc-12v-1000rpm/
- Naylamp Mechatronics, "Micromotor DC N20 12V/1000RPM con encoder": https://naylampmechatronics.com/motores-dc/811-micromotor-dc-n20-con-encoder-12v-1000rpm.html
- Zbotic, "Buy Ga12 Motor 12v N20 (1000RPM Micro)": https://zbotic.in/product/ga12-n20-12v-1000rpm-micro-dc-reducer-motor-encoder-with-wire/
- SparkFun ROB-28633 datasheet (BJ-N20-12-1000 encoder motor): https://cdn.sparkfun.com/assets/a/3/9/9/9/28633_datasheet_N20_Motor_With_Encoder_and_Cable_pair.pdf
- Waveshare DCGM-N20-12V-EN-200RPM wiki: https://waveshare.com/wiki/DCGM-N20-12V-EN-200RPM
- Amazon listing (Anreak, 12-1000): the product page could not be read automatically. Add the product URL from the browser address bar here.

## To do

1. Read the Anreak listing's specification section and copy the values (stall current, stall torque, gear ratio, encoder counts per revolution) into this file.
2. Replace the 1.0 A design value with the confirmed stall current.
3. Replace `schemes/datasheets/N20_motors.pdf` with the actual datasheet or a saved copy of the listing.
4. Update `schemes/README.md` §4.2, §5.2 and the encoder constants that depend on counts per revolution.
