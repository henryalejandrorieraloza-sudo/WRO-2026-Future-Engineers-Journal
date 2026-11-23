# N20 12 V / 1000 RPM encoder gear motor: specifications

Drive motor used on the vehicle. Listing bought on Amazon (brand Anreak, "Motor codificador N20 DC 12V 1000RPM", style "12 V, 1000 RPM", 2 pcs, order of 20 Aug 2026).

This file replaces the earlier `N20_motors.pdf`, which describes a different motor (6 V, 33 RPM).

**Status:** the values below come from third-party sellers and datasheets for the same 12 V / 1000 RPM N20 class. They are **not yet confirmed against the Anreak listing or an Anreak datasheet**. Confirm them before the final documentation is submitted.

## Values reported by sources

| Parameter | Value | Source |
|---|---|---|
| Nominal voltage | 12 V DC (range 6–12 V) | Electronilab; Sigma Electrónica |
| No-load speed at 12 V | 1000 RPM | Vistronica; Sigma Electrónica |
| Gear ratio | 1:30 (typical for this class) | Naylamp; Zbotic |
| Rated (nominal) load | 0.3 kg·cm torque, 0.28 A current | Vistronica; Sigma Electrónica |
| Stall torque at 12 V | 2.4 kg·cm | Vistronica; Sigma Electrónica |
| Stall current at 12 V | 0.6 A | Vistronica; Sigma Electrónica; Electronilab |
| Stall current (peak) | 600 mA | Naylamp (encoder version) |
| No-load current | 190 mA (Vistronica) / 60 mA (Naylamp, encoder version) | conflicting sources |
| Shaft | 3 mm D-shaft, 9–10 mm long | Zbotic; Electronilab |

## Conflicting data

- One seller (Zbotic, "GA12-N20") lists a stall current of **1 A**, a rated current of 40 mA, and a stall torque of 2 kg·cm. These values do not match the other sources.
- The no-load current differs by a factor of three between sources (60 mA vs. 190 mA), which suggests different gearbox or encoder variants.

## Design values used until the Anreak datasheet is confirmed

- Worst-case (stall) current per motor for the power budget: **1.0 A**, taken from the highest reported value so that the budget holds even with the outlier data.
- Typical stall torque for the steering and drive calculations: **2.4 kg·cm** at 12 V.

## Sources

- Electronilab, "Motorreductor de metal 12V 1000RPM N20": https://electronilab.co/tienda/motorreductor-de-metal-12v-1000rpm-n20/
- Vistronica, "Motorreductor N20-20 12V 1000 RPM 0.3 Kg.Cm": https://www.vistronica.com/robotica/motores/motorreductores/motorreductor-n20-20-12v-1000-rpm-0-3-kg-cm-detail.html
- Sigma Electrónica, "N20 DC 12V/1000RPM": https://www.sigmaelectronica.net/producto/n20-dc-12v-1000rpm/
- Naylamp Mechatronics, "Micromotor DC N20 12V/1000RPM con encoder": https://naylampmechatronics.com/motores-dc/811-micromotor-dc-n20-con-encoder-12v-1000rpm.html
- Zbotic, "Buy Ga12 Motor 12v N20 (1000RPM Micro)": https://zbotic.in/product/ga12-n20-12v-1000rpm-micro-dc-reducer-motor-encoder-with-wire/
- Amazon listing used for purchase (Anreak): add the product URL from the browser address bar here.

## To do

1. Copy the Anreak product URL and its specifications section into this file.
2. Replace the 1.0 A design value with the confirmed stall current.
3. Update `schemes/README.md` §4.2 and §5.2 to use these values.
