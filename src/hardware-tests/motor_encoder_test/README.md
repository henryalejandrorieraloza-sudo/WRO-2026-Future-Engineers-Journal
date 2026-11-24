# motor_encoder_test

Hardware test for the **drive motor** (N20 12 V with encoder, driven by a **TB6612FNG**) on the Arduino Nano. It checks the motor direction, the encoder direction and the number of pulses per wheel turn.

## What it does
1. Waits for the start button on **D10**.
2. Runs a fixed sequence, repeated forever:

| Phase | PWM | Time |
|---|---|---|
| 1. SLOW FORWARD | +110 | 2.5 s |
| 2. FAST FORWARD | +210 | 2.5 s |
| 3. BRAKE | 0 (both inputs high) | 1.5 s |
| 4. REVERSE | −140 | 2.5 s |
| 5. STOPPED | 0 (both inputs low/high, PWM 0) | 2 s |

3. Every 200 ms prints the total pulses, the number of wheel turns, and the speed in RPM.

The encoder is read by an interrupt on **D2** (channel A). Channel **B** on **D3** tells the direction (the driving programs do it the other way round: interrupt on **D3** = C1, direction on **D2** = C2; both pins are interrupt pins on the Nano, so either works): if B is high on a rising edge of A, the count goes up, otherwise down.

## Wiring
| Signal | Nano pin |
|---|---|
| TB6612FNG PWMA | D6 |
| TB6612FNG AIN1 | D7 |
| TB6612FNG AIN2 | D8 |
| Encoder A (INT0) | D2 |
| Encoder B | D3 |
| Start button | D10 (internal pull-up) |

STBY on the driver is tied to +5 V on the PCB, so the driver is always enabled.

## Settings
| Constant | Default | Meaning |
|---|---|---|
| `MOTOR_REVERSED` | `false` | Set to `true` if "forward" drives the car backward |
| `PULSES_PER_REV` | 374.0 | Pulses for one full wheel turn. **Measure it for the real motor**: turn the wheel by hand exactly one turn and read the count. 374 is only an example value; with the current motor the driving programs count about 119 per wheel turn (9.27 ticks/cm × 12.9 cm) |

## How to use
1. Put the car on a stand, wheels in the air, the first time.
2. Upload, open the Serial Monitor at 115200 baud, press the button on D10.
3. Check that phase 1 and 2 turn the wheels **forward**. If they go backward, set `MOTOR_REVERSED = true`.
4. Check that the pulse count goes **up** in forward phases and **down** in the reverse phase. If it does not, swap the encoder wires on D2 / D3 (or the logic of `readEncoder()`).
5. Turn the wheel by hand one full turn, read the total pulses, and write that number in `PULSES_PER_REV`. This value is used later by the odometry in the other programs.
6. Brake (phase 3) should stop the wheels quickly. If it does not, check AIN1 and AIN2.

## Notes
- The loop stops the motor between phases using `moveMotor(0)`, which is an active brake.
- The RPM is calculated from the pulses counted since the last print, so it is accurate even if the timing is not exact.
- Interrupts are switched off only while the counter is read, so the value is never half-updated.
