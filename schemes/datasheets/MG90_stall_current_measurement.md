# MG90 servo: stall current from the measured winding resistance

Servo on the car: MG90 (metal gears), steering with rack and pinion ([README §5.3](../../README.md)).
Measured on 24 Nov 2026. Original document (Spanish): [MG90_stall_current_measurement_es.pdf](MG90_stall_current_measurement_es.pdf).

## Goal

Estimate the stall current of the servo's internal DC motor with an indirect, non-destructive method. Holding the servo against a mechanical stop with power on is the worst case for the 5 V rail, and it can overheat the winding and the driver.

## Instrument

Digital multimeter on the resistance (Ω) scale, probes on the terminals of the internal DC motor of the MG90, at room temperature, rotor still.

## Result of the measurement

**R<sub>internal</sub> = 3.3 Ω** (measured).

## Calculation

With the rotor blocked the back-EMF is zero, so only the resistance limits the current.

```
Ideal (no losses):  I = 5.0 V / 3.3 Ω ≈ 1.52 A

Real: the servo's H-bridge and the wiring drop about 0.8 V (assumed, typical 0.5–1.0 V)
      V_motor ≈ 5.0 − 0.8 = 4.2 V
      I_stall = 4.2 V / 3.3 Ω ≈ 1.27 A
```

| Assumed drop | Stall current |
|---|---|
| 0.5 V | 1.36 A |
| **0.8 V (used)** | **1.27 A** |
| 1.0 V | 1.21 A |

## Result and design consequences

- **Estimated stall current: 1.27 A.** Practical range **1.0 to 1.3 A**. Sustained stall consumption 0.8–1.2 A; the start-up peak can reach 1.3 A.
- **Heat at stall:** about 5.3 W in the motor (4.2 V × 1.27 A) and about 1.0 W in the bridge. A servo blocked for more than a few seconds can burn the bridge or the winding.
- **This is about twice the 650 mA** the power budget used before. The 5 V rail peak rises from 1.05 A to ≈ 1.67 A (56 % of the 3 A of the CN3903 buck), still with margin.

## Limits of this measurement

- The **resistance is measured; the current is calculated**, not read in series. Same method and same honesty as [N20_stall_current_measurement.md](N20_stall_current_measurement.md).
- The **0.8 V drop is an assumption**, not measured.
- The probe leads add a tare of about 0.2–0.5 Ω. The pure winding may be 2.8–3.0 Ω, which would give a slightly higher current than 1.27 A.
- Copper resistance rises about 0.39 % per °C, so a warm winding draws less.
- **Still open:** measure the 5 V rail with a multimeter while the servo stalls and the four ToF sensors are ranging (journal §9.4, §16).

## Consequence for the code

The steering limits in the driving programs are `STEER_CENTER ± 80` = **5 / 165** ([`open_challenge`](../../src/open-challenge/open_challenge), [`obstacle_challenge`](../../src/obstacle-challenge/obstacle_challenge)). The left stop measured with `servo_test` is about 10 ([docs/TESTS.md](../../docs/TESTS.md)). If 10 is a mechanical stop, a command to 5 pushes the servo against it and it stalls at about 1.27 A for as long as the command lasts. The servo code has no stall timeout. This is documented as a risk; the code is not changed in this update.

## Where it is used

`schemes/README.md` §4.1–§4.3 (power budget), README §6.1 and §10.3, `docs/TESTS.md` (servo table), journal §24.10.
