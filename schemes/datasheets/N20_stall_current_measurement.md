# N20 motor: stall current from the measured winding resistance

Motor on the car: N20 12 V, 1000 rpm, with encoder ([N20_12V_1000RPM_specs.md](N20_12V_1000RPM_specs.md)).
Measured on 24 Nov 2026. Original document (Spanish): [N20_stall_current_measurement_es.pdf](N20_stall_current_measurement_es.pdf).

## Goal

Estimate the stall current of the motor with an indirect, non-destructive method. Holding the rotor against its limit with the motor powered can overheat the copper winding and wear the brushes.

## Motor

| Parameter | Value |
|---|---|
| Motor | Brushed DC micromotor with N20 gearbox |
| Rated voltage | 12 V DC |
| No-load speed | 1000 RPM |

## Instrument

Digital multimeter on the resistance (Ω) scale.

## Procedure

1. **Isolate the motor:** disconnected from the battery, the driver and any circuit, to avoid wrong readings or damage to the instrument.
2. **Keep the shaft still:** the rotor stays completely stopped. A turning rotor acts as a generator and induces a back-EMF that corrupts the ohmmeter reading.
3. **Measure R<sub>internal</sub>:** probes directly on the metal terminals of the motor, at room temperature, waiting until the reading settles.
4. **Record:** winding resistance **R<sub>internal</sub> = 11.7 Ω**.

## Calculation

With the rotor blocked (0 RPM) the back-EMF is zero (E = 0 V), so the current is limited only by the winding resistance and the brush contact (Ohm's law):

```
I_stall = V_nominal / R_internal = 12 V / 11.7 Ω ≈ 1.03 A
```

## Result and design consequences

- **Estimated stall current: 1.03 A (1,030 mA).**
- **Tolerance:** because of the angular position of the three-segment commutator and the resistance of the probes, the real stall current lies in a range of **0.95 A to 1.20 A**.
- **Sizing rule:** the power stage (H-bridge or driver) and the supply must handle peaks of at least **1.2 A per motor** without entering thermal protection or dropping the voltage.

## Limits of this measurement

- It is a **calculation from a resistance reading**, not a current read in series with the battery. The resistance depends on the rotor position and on temperature.
- At the battery's 11.4 V the same calculation gives about 0.97 A; the repository budget keeps 1.03 A (the 12 V value) as the conservative case.
- The value is higher than the 0.7 A of the datasheet of the same family used before, which is one more sign that our motor is not exactly that one (see the gear-ratio check in [N20_12V_1000RPM_specs.md](N20_12V_1000RPM_specs.md)).

## Where it is used

`schemes/README.md` §4.2 (power budget), README §5.2, §6.1 and §10.3, `docs/TESTS.md` (motor table).
