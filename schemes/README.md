# Schemes — Electrical System, PCB and Power

This folder holds the complete electrical documentation of the Team X67 car: the PCB schematic, the wiring diagram, photos of the manufactured board, the power budget and the datasheets of every electronic component.

All the electronics are mounted on a **custom PCB that we designed in EasyEDA and had manufactured by JLCPCB**. The board is also part of the chassis: its custom outline is the base that the steering and drive supports are mounted to.

| File / folder | Content |
|---|---|
| [`schematic.png`](schematic.png) | Electrical schematic, rev 2 (24 Nov 2026); source in [`make_schematic.py`](make_schematic.py) |
| [`wiring_diagram.png`](wiring_diagram.png) | Every external connection between the PCB and the components |
| [`pcb_front.jpeg`](pcb_front.jpeg), [`pcb_back.jpeg`](pcb_back.jpeg) | Manufactured PCB, both sides |
| [`pcb_soldered_top.jpg`](pcb_soldered_top.jpg), [`pcb_soldered_back.jpg`](pcb_soldered_back.jpg) | PCB with all modules soldered — **photos of the previous chassis (v3)**, kept because they are the clearest view of every component soldered in place; the board itself has not changed |
| [`pcb-blocks/`](pcb-blocks) | Close-ups of each functional block of the PCB layout |
| [`datasheets/`](datasheets) | Datasheets of all electronic components |
| Online PCB project | [OSHWLab — Team X67 PCB](https://oshwlab.com/edu560/project_ehytvvog) (open it to order or modify the board) |

---

## 1. Bill of Materials (BOM)

| Component | Image | Qty | Role | Why we chose it |
|---|---|---|---|---|
| **Arduino Nano** (classic, ATmega328P) | <img src="../other/components/arduino_nano.jpg" width="140"> | 1 | Main controller: sensors, control loops, motor and servo | Same footprint and pinout as the board the PCB was originally designed for, so it drops into the same headers. 5 V logic, standard `Servo.h` and `Wire` libraries. It replaced our first main controller after that board was damaged (journal Problem 31) |
| **ESP32-S3-CAM** (emakefun) | <img src="../other/components/esp32s3-cam.jpg" width="140"> | 1 | Vision board | ESP32-S3 with 8 MB PSRAM (needed for camera frame buffers) and native USB. Keeps image processing off the main controller |
| **OV2640** camera | <img src="../other/components/ov2640.jpg" width="140"> | 1 | Colour images for pillar detection | 2 MP CMOS sensor, 68° field of view, supported natively by the ESP32 camera driver |
| **TOF400C** (VL53L1X) | <img src="../other/components/tof400c.jpg" width="140"> | 4 | Distance to the walls: front, back, left, right | Laser time-of-flight, up to 4 m, 27° field of view, programmable measurement area (ROI). Replaced Sharp infrared sensors (see section 5) |
| **BMI160** IMU | <img src="../other/components/bmi160.jpg" width="140"> | 1 | Heading (yaw) from the gyroscope | 6-axis IMU on the same I²C bus as the TOF sensors; low noise and low current |
| **TB6612FNG** driver | <img src="../other/components/tb6612fng.jpg" width="140"> | 1 | Speed and direction of the drive motor | MOSFET H-bridge (much lower losses than an L298N), PWM control and an active short-brake mode for precise stops |
| **CN3903** buck converter | <img src="../other/components/buck_converter.jpg" width="140"> | 1 | 11.4 V → 5 V, up to 3 A | Switching regulator: far less heat than a linear regulator when dropping ~6 V |
| **Steering servo** (MG90, metal gear) | <img src="../other/components/servo.jpg" width="140"> | 1 | Rack-and-pinion steering | The PCB was designed for an SG90; it was replaced by the pin-compatible MG90 (metal gears) because the SG90 moved inside its mount (journal Problem 8) |
| **N20 DC motor with Hall encoder**, 1000 rpm | <img src="../other/components/n20_motor_encoder.jpg" width="140"> | 1 | Rear-wheel drive through a differential | Very small and light, 12 V, built-in encoder for distance/speed feedback |
| **GNB 3S 380 mAh HV LiPo** (11.4 V nominal, **13.05 V fully charged**, 4.33 Wh, 90C) | <img src="../other/components/battery.jpg" width="140"> | 1 | Power source | Lightest pack that powers the 12 V motor directly with enough capacity for many rounds (see power budget) |
| **Start button** | <img src="../other/components/start_button.jpg" width="140"> | 1 | Starts the run (rules require a single start button) | — |
| **Power switch** | <img src="../other/components/switch.jpg" width="140"> | 1 | Main power on/off (required by the rules) | Between the battery and the PCB |
| **Custom tyres** | <img src="../other/components/custom_tire.jpeg" width="140"> | 4 | Grip | 41 mm diameter, C25 silicone rubber cast in printed moulds ([how we make them](../models/README.md#4-custom-silicone-tyres)) |

---

## 2. Schematic

<div align="center">
  <img src="schematic.png" alt="Electrical schematic, rev 2 (24 Nov 2026)" width="800">
  <br><em>Schematic rev 2, drawn on 24 Nov 2026 to match the car as built: classic Arduino Nano, MG90 steering servo, 4 × TOF400C, BMI160, ESP32-S3-CAM, TB6612FNG, CN3903 buck converter.</em>
</div>

The image is generated by [`make_schematic.py`](make_schematic.py), which writes [`schematic.svg`](schematic.svg) and `schematic.png`. Edit the script, not the image, to change it.

Conventions: coloured lines are real wires between the parts shown; pins with the same net name (+5 V, GND, VBAT, SDA, SCL) are connected through the net label instead of a long wire. The rev 1.0 EasyEDA drawing (8 July 2026) is superseded by this one for the labels; the PCB itself is unchanged.

| Block | Connections |
|---|---|
| **Arduino Nano** (classic, ATmega328P) | VIN from the battery · D0 = UART receive from the camera (camera GPIO43 → D0) · D2/D3 = encoder C2/C1 · D6/D7/D8 = PWMA/AIN1/AIN2 · D9 = servo signal · D10 = start button · A0/A1/A2/A3 = XSHUT of the BACK/FRONT/RIGHT/LEFT TOF sensors · A4/A5 = I²C SDA/SCL |
| **Power stage** | Battery (11.4 V nominal, 13.05 V fully charged) → power switch → CN3903 buck converter → +5 V rail |
| **Distance sensors** | 4 × TOF400C, each with SDA/SCL on the shared I²C bus, +5 V, GND and XSHUT (addresses 0x2A LEFT, 0x2B RIGHT, 0x2C BACK, 0x2D FRONT) |
| **BMI160** | Gyroscope on the same I²C bus, address 0x69 |
| **TB6612FNG** | VM = battery, VCC = +5 V, **STBY tied to +5 V** (no MCU pin needed), channel A only (single motor) |
| **Camera** | GPIO43 (TX) → Nano D0; the Nano sends nothing to the camera |
| **Motor + encoder** | A01/A02 → N20 motor (M1/M2); encoder channels C1/C2 → D3/D2 |
| **Servo** | Signal = D9, +5 V, GND |

---

## 3. Wiring diagram

<div align="center">
  <img src="wiring_diagram.png" alt="Wiring diagram" width="800">
  <br>
  <img src="../v-photos/top.jpg" alt="The real car, top view" width="600">
  <br>
  <em>Top: every external connection of the PCB. Bottom: the real car (top view) — our old physical-wiring photo was retired because it no longer matched the current parts.</em>
</div>

**Pin map summary (Arduino Nano)**

| Device | Signal | Pin |
|---|---|---|
| Camera board (ESP32-S3-CAM) | UART, one way | GPIO43 (TX) → Nano D0 (RX); GPIO44 not used |
| Encoder | C1 / C2 | D3 / D2 |
| TB6612FNG | PWMA / AIN1 / AIN2 | D6 / D7 / D8 |
| TB6612FNG | STBY | +5 V (hard-wired) |
| Servo | Signal | D9 |
| Start button | Input (to GND) | D10 |
| I²C bus (4 × TOF + BMI160) | SDA / SCL | A4 / A5 |
| TOF BACK / FRONT / RIGHT / LEFT | XSHUT | A0 / A1 / A2 / A3 |

Only the Nano's VIN pin receives the battery voltage. The classic Nano works with 5 V logic; the TOF400C modules are powered from the +5 V buck rail (section 4).

---

## 4. Power system

### 4.1 Architecture

```
 3S HV LiPo 11.4 V nominal · 13.05 V fully charged (380 mAh, 4.33 Wh)
        │
   [power switch]
        │
        ├────────────► TB6612FNG VM ─────► N20 drive motor
        ├────────────► Arduino Nano VIN ─► on-board 5 V regulator ─► Nano (5 V logic)
        │
        └─► CN3903 buck 5 V / 3 A ─► +5 V rail ─┬─► ESP32-S3-CAM (→ 3.3 V for OV2640)
                                                ├─► 4 × TOF400C
                                                ├─► BMI160
                                                ├─► TB6612FNG logic (VCC, STBY)
                                                ├─► Steering servo
                                                └─► Encoder
```

**Design reasoning**

- **The motor takes the battery voltage directly.** The N20 is rated for 12 V, so a second regulator would only add losses and weight. **Caution:** a fully charged pack is **13.05 V** (measured 24 Nov), above the motor's 12 V rating and the Nano's recommended 12 V on VIN; section 4.4 shows what limits the risk.
- **The servo has the largest current peaks on the 5 V rail** (about **1.27 A at stall**, calculated from the 3.3 Ω winding resistance measured on 24 Nov; the budget used 650 mA before). A 3 A buck still leaves a margin (the rail peaks at ≈ 1.67 A), but the stall on the 5 V rail has to be checked with a multimeter, and the servo must not be held at a stop for more than a moment.
- **A switching (buck) regulator instead of a linear one:** dropping 11.4 V to 5 V at 0.5 A in a linear regulator would waste ~3.2 W as heat, more than all the electronics use.
- **The TOF sensors take 5 V from the buck rail.** The classic Nano works with 5 V logic, so 5 V sensors and their I²C lines match it directly, and their current no longer passes through the Nano's small on-board regulator. The cost is that they share the rail with the servo, so the 3 A buck margin has to keep that rail steady during servo stalls (see the budget and the failure points below).
- **One common ground** for all boards; the camera board and the Nano share it so the UART link has a clean reference.

### 4.2 Power budget

Values from the datasheets (typical = normal driving; peak = worst case, all at once).

| Load | Rail | Typical | Peak | Typical power | Peak power |
|---|---|---|---|---|---|
| N20 drive motor | 11.4 V | 50 mA (estimate, to measure) | 1.03 A (stall, calculated from the measured winding resistance of 11.7 Ω) | 0.57 W | 11.74 W |
| Arduino Nano (ATmega328P) | 11.4 V (VIN, linear regulator) | ~20 mA | ~30 mA | 0.23 W | 0.34 W |
| 4 × TOF400C | 5 V buck rail | 40 mA | 80 mA | 0.20 W | 0.40 W |
| ESP32-S3-CAM + OV2640 | 5 V | 100 mA | 310 mA | 0.50 W | 1.55 W |
| BMI160 | 5 V | 1 mA | 1.5 mA | 0.005 W | 0.008 W |
| TB6612FNG logic | 5 V | 2 mA | 10 mA | 0.01 W | 0.05 W |
| Steering servo | 5 V | 100 mA | 1.27 A (stall, calculated from the measured winding resistance of 3.3 Ω) | 0.50 W | 6.35 W |
| **5 V buck rail total** | 5 V | **243 mA** | **1.67 A** | 1.22 W | 8.36 W |
| Buck losses (≈ 85 % efficiency) | — | — | — | 0.22 W | 1.47 W |
| **Total from battery** | 11.4 V | **≈ 0.20 A** | **≈ 1.92 A** | **≈ 2.2 W** | **≈ 21.9 W** |

The TOF row is on the 5 V buck rail, so its cost reaches the battery through the buck converter: 40 mA × 5 V ≈ 0.20 W at typical load, about 0.4 W at peak. Compared with the previous design, where the sensors ran from the Nano's 3.3 V pin, the total is about 0.3 W lower at typical load, because the Nano's regulator no longer carries the sensors' current.

**What the numbers tell us**

- **Run time:** usable energy ≈ 80 % × 4.33 Wh ≈ 3.5 Wh. At typical load that is ≈ 3.5 / 2.2 ≈ **1.6 h**; even at a continuous worst case (motor and servo both at stall) it is ≈ **9.5 min**. A round lasts at most 3 minutes, so one charge covers a full competition day of rounds and testing.
- **Discharge rate:** the ≈ 1.9 A peak (motor and servo at stall) is ≈ 5.1 C, far below the battery's 90C rating. The motor is the largest load, so its voltage sag is the one to check on the car.
- **Motor stall current (calculated from the resistance measured on 24 Nov):** the winding resistance of the motor on the car is **11.7 Ω** (multimeter, rotor still, motor disconnected). Ohm's law gives I<sub>stall</sub> = 12 V / 11.7 Ω ≈ **1.03 A**, with a working range of 0.95 to 1.20 A. The budget uses 1.03 A (it was 0.7 A, from the datasheet of another motor of the family). The motor driver and the supply must carry at least **1.2 A per motor**; the TB6612FNG's absolute maximum is 1.2 A per channel (3.2 A for a 10 ms pulse) and its recommended operating current is 1.0 A (see section 4.4), so the stall case fits. Details: [N20_stall_current_measurement.md](datasheets/N20_stall_current_measurement.md).
- **Servo stall current (calculated from the resistance measured on 24 Nov):** the internal DC motor of the MG90 measures **3.3 Ω** (multimeter, rotor still). With about 0.8 V lost in the servo's H-bridge and the wiring (assumed, typical 0.5–1.0 V) the motor sees ≈ 4.2 V, so I<sub>stall</sub> ≈ 4.2 / 3.3 ≈ **1.27 A** (practical range 1.0–1.3 A; ideal 5 V / 3.3 Ω = 1.52 A). The budget used 650 mA before, so the 5 V rail peak rises from 1.05 A to ≈ 1.67 A. At stall the servo dissipates about 5.3 W in the motor and 1.0 W in the bridge: a servo held against a stop for more than a few seconds can burn the bridge or the winding. The current is calculated, not read in series, and the 0.8 V drop is an assumption. Details: [MG90_stall_current_measurement.md](datasheets/MG90_stall_current_measurement.md).
- **Buck margin:** the 5 V rail peaks at ≈ 1.67 A of the CN3903's 3 A (≈ 56 %), so the regulator still does not run near its limit.
- **Nano regulator heat:** the Nano's 5 V linear regulator now only feeds the Nano: (11.4 − 5) V × 20 mA ≈ **0.13 W** typical, ≈ 0.2 W at peak. With a fully charged pack (13.05 V) it is (13.05 − 5) V × 20 mA ≈ **0.16 W** typical, ≈ 0.24 W at peak. It stays cool.
- **Fully charged pack (13.05 V):** the table above uses the nominal 11.4 V. At 13.05 V the typical power is ≈ 2.4 W (the motor and the Nano take the higher voltage; the regulated 5 V rail does not change) and the worst-case peak is ≈ 25 W at ≈ 1.9 A (motor stall ≈ 1.12 A, see section 4.4; servo stall 1.27 A). Both are still far from the battery's limits.
- **Charging:** ≈ 45–50 min with a balance charger (IMAX B6AC) at 0.4 A (≈ 1C).

### 4.3 Failure points we considered

| Risk | What could happen | Mitigation |
|---|---|---|
| Servo peaks on the shared 5 V rail reach the TOF sensors | A servo stall (≈ 1.27 A, calculated) could glitch a distance reading or make a sensor reset | 3 A buck, rail peak ≈ 1.67 A (see budget). Measure the 5 V rail with a multimeter while the servo stalls and the four sensors are ranging |
| **Servo held against a mechanical stop** | At stall the MG90 draws ≈ 1.27 A and heats the winding and the bridge (≈ 5.3 W + 1.0 W); after a few seconds it can burn them. The code limit on the left (5) is 5 servo units beyond the measured stop (10) in `docs/TESTS.md` | Do not hold the servo at a stop. Re-check the limits with `servo_test` and listen for buzzing at full lock; a stall timeout in the servo code is an open item (not changed in this update) |
| Camera 3.3 V TX into the Nano RX | The ESP32-S3 drives 3.3 V; the ATmega328P reads 3.3 V as HIGH at 5 V (input-high threshold about 3.0 V) | No resistor divider. The link is one-way: the camera never listens to the Nano (`NANO_RX_PIN = -1`), so the Nano TX pin carries nothing to the camera |
| **Fully charged battery (13.05 V) above the 12 V ratings** | The N20 (12 V) and the Nano's VIN (12 V recommended) see more than their rating at the start of a charge; the TB6612FNG is only 0.45 V below its 13.5 V operating limit | Measured limits and mitigation in section 4.4: PWM keeps the motor's average voltage low (≈ 7.7 V in the Open Challenge, ≈ 3.6 V in the Obstacle Challenge), the stall stop cuts the motor after 0.5 s, and every part stays below its absolute maximum |
| Battery voltage on a 3.3 V pin | Permanent damage to the main controller (it happened: journal Problem 31) | On the PCB the battery rail only reaches VIN, the TB6612FNG VM pin and the buck input. Journal Problem 31 led to a safety rule: **upload code with the battery off** |
| Servo stall pulls the 5 V rail down | Camera or sensors reset during a run | 3 A buck with large margin (see budget) |
| Motor held at stall for a long time | Gear damage; the motor heats quickly at ≈ 1.03 A stall | The code stops the motor after `STALL_MS`; stall tests use a short hold (< 0.5 s) |
| Loose sensor wires | A TOF stops answering, I²C bus freezes | Locking JST connectors on the PCB; `Wire.setWireTimeout(25000, true)` in the driving programs, so a frozen bus is reset after 25 ms instead of hanging the program, and a ToF that is silent for 0.5 s makes the program restart the sensors (journal Problems 12 and 30) |
| All TOF sensors start with the same I²C address | Address conflict, no readings | XSHUT lines: sensors are switched on one at a time at start-up and each gets its own address (journal Problem 15) |
| Short circuit while handling | Burnt traces or battery damage | Power switch on the main line; XT30 connector on the battery so it can be unplugged quickly |

### 4.4 Fully charged battery: 13.05 V against the 12 V ratings

The nominal 11.4 V in the budget is the pack's average. A freshly charged GNB 3S **HV** pack is higher: **13.05 V**, measured with a multimeter on 24 November 2026 (an HV cell charges to 4.35 V, and 3 × 4.35 V = 13.05 V). The start of a round is the worst moment. Each part against that voltage:

| Part | Rating | At 13.05 V | Verdict |
|---|---|---|---|
| N20 drive motor | 12 V rated; the listing says a higher voltage wears the motor | +8.75 % over the rating at full duty; about 7.7 V average at `RUN_PWM` 150 (Open) and about 3.6 V at `RUN_PWM` 70 (Obstacle) | Over the rating only at PWM 255; the driving programs use 150 and 70 (255 appears only in the short brake, where the motor is not driven) |
| Arduino Nano, VIN | 7–12 V recommended; 6–20 V limit ([Nano documentation](https://docs.arduino.cc/hardware/nano/)) | 1.05 V above the recommended range, 7 V below the limit | Works; the on-board linear regulator dissipates ≈ 0.16 W typical, ≈ 0.24 W at peak |
| TB6612FNG, VM | 13.5 V operating maximum, 15 V absolute maximum ([datasheet](datasheets/TB6612FNG.PDF)) | 0.45 V (3.3 %) under the operating limit | Inside the range with a small margin. A 4S pack, or a charger set above 13.05 V, would not be safe |
| TB6612FNG, output current | 1.0 A operating, 1.2 A absolute maximum per channel | Stall current ≈ 1.03 A at 12 V (calculated, section 4.2), ≈ 1.12 A at 13.05 V (13.05 V ÷ 11.7 Ω) | Above the operating value and below the absolute maximum, for short stalls only |
| CN3903 buck, VIN | 4.5–36 V ([datasheet](datasheets/CN3903.PDF)) | Far inside the range | No problem; the 5 V rail does not change |

**Why we accept it, and what limits the risk**

- **The PWM lowers the motor's average voltage.** The programs never drive the motor at full duty: `RUN_PWM` is 150 of 255 in the Open Challenge and 70 of 255 in the Obstacle Challenge, so the motor averages about 7.7 V and 3.6 V even with a fresh pack.
- **A stall is cut short.** Both driving programs stop the motor when the wheels do not turn for 0.5 s (`STALL_MS`), so the stall current never lasts.
- **The voltage falls as the pack discharges.** It goes from 13.05 V towards the nominal 11.4 V, so 13.05 V is the worst case at the start of a charge.
- **Speed changes with the pack.** The motor turns faster at 13.05 V than at 11.4 V, so a timed delay covers more distance with a fresh pack. This is one more reason the programs measure distance with the encoder, and why a test run should be repeated with a fully charged pack.

**Rules we follow because of this**

1. Charge with the balance charger set to **3S HV** (4.35 V per cell, 13.05 V). Never charge as 4S or above.
2. Measure the pack with a multimeter before the first run of the day and write the voltage in the Battery (V) column of `docs/TESTS.md`.
3. If the motor or the driver supply is ever changed, check VM again: the TB6612FNG has only 0.45 V of margin.

**Not measured yet:** the stall current at 13.05 V is calculated from the 11.7 Ω winding resistance, not read in series with the battery (the same limit as in section 4.2).

---

## 5. Sensor selection and placement

| Sensor | Placement | Reasoning (field geometry) |
|---|---|---|
| TOF FRONT (0x2D) | Front centre, facing forward | Obstacle: after a corner line the car drives on until the front wall is 40 cm away, then starts the reverse turn (a line is seen only within ~16–23 cm, which is not a distance to the wall). Open: held in reset (XSHUT LOW) |
| TOF LEFT / RIGHT (0x2A / 0x2B) | Sides, **straight (0°), facing the wall**, centre ~5 cm from the floor | Open: keep 100 mm from the inner wall; a corner is a reading 250 mm above the wall's learned distance or above 1200 mm. Obstacle: centring between the walls. On the black wall an angled (30°) beam returns almost no light: 30 cm read as ~198 cm; straight it reads 32.0 cm (journal Phase 10). 5 cm is half of the 10 cm wall, so the ROI 16 × 8 cone stays on the wall and off the floor |
| TOF BACK (0x2C) | Rear centre, facing back | Obstacle: ends the back-up after each reverse turn at 20 cm from the rear wall, so every straight starts from the same place. Open: held in reset (XSHUT LOW) |
| BMI160 | Flat on the PCB, near the car's centre | Only yaw is used; mounting it flat makes the Z axis the turning axis |
| Camera | Front, facing forward and slightly down | Sees the pillars of the next section; only a horizontal band of the image is searched so objects outside the field are ignored (journal §6) |

**Why TOF instead of infrared or ultrasonic:** our first sensors were Sharp GP2Y0A21 infrared sensors. Their output is non-linear (distance ∝ V^-1.15), so the noise grows with distance and they were unreliable beyond 50–60 cm, less than one corridor width (journal Problem 3). Ultrasonic sensors have a wide cone that would see the floor and the neighbouring walls. The VL53L1X measures up to 4 m and its measurement area (ROI) can be narrowed in software; we chose the ROI and the mounting angle with measured experiments (journal §8.2, overturned by the better tests of Phase 10).

**Calibration methods**

- **Gyroscope:** 300 samples (≈ 1.5 s, after 0.7 s of settling) at rest when the run is started, with the LED blinking; the average is subtracted as offset (like taring a scale). The scale is `GYRO_SCALE = 1.007` (3-lap test).
- **TOF sensors:** ROI 16 × 8 and mounting angle chosen with tape-measure experiments on the black wall (`tof_roi_budget_test`, journal Phase 10); distance readings (0 or ≥ 4000 mm = no wall; the range status is not checked). The 8-reading window is October history (README §8.3).
- **Encoder:** 9.27 ticks per cm, measured with the motor running (190 ticks over a taped 20.5 cm, 21 Nov).
- **Steering:** centre and limits measured on the real chassis with the `servo_test` sketch, with a 2–3° margin before each mechanical stop.
- **Camera colours:** HSV thresholds tuned live with the camera viewer under the real lighting.

---

## 6. PCB design and manufacturing

### 6.1 Why a custom PCB

We first planned a perforated prototype board (pertinax), but it was too bulky for a car limited to 30 × 20 cm. A custom PCB gives:

- **Clean, short connections** that do not come loose with vibration.
- **A structural base:** its custom outline holds the steering and drive supports, so the board is part of the chassis.
- **Reproducibility:** another team can order exactly the same board from the online project.

The trade-off was time: the boards are made in China and take about 2 weeks to arrive in Ecuador, so every change to the design costs weeks. We reviewed the schematic carefully before ordering to avoid a second revision.

### 6.2 Tools

We used **EasyEDA Pro** (web version) because its learning curve is shorter than KiCad or Proteus, it has a large library of ready footprints for our modules, and it exports directly to **JLCPCB** for manufacturing.

<div align="center">
  <img src="../other/pcb-manufacturing/EasyEDA_1.png" alt="EasyEDA" height="260">
  <img src="../other/pcb-manufacturing/EasyEDA_2.png" alt="EasyEDA Pro" height="260">
  <br><em>1) The board was designed in EasyEDA from the schematic. 2) We used the Pro edition for its extra layout features.</em>
  <br><br>
  <img src="../other/pcb-manufacturing/EasyEDA_3.png" alt="Project" height="260">
  <img src="../other/pcb-manufacturing/EasyEDA_4.png" alt="Export" height="260">
  <br><em>3–4) The finished layout is exported from the project directly to JLCPCB.</em>
  <br><br>
  <img src="../other/pcb-manufacturing/JlcPCB_1.png" alt="JLCPCB order" height="260">
  <br><em>5) Order options: 1.6 mm board thickness; the colour is free to choose.</em>
</div>

### 6.3 Result

<div align="center">
  <img src="pcb_front.jpeg" alt="PCB front" height="300">
  <img src="pcb_back.jpeg" alt="PCB back" height="300">
  <br><em>Manufactured PCB, front and back.</em>
  <br><br>
  <img src="pcb_soldered_top.jpg" alt="Soldered PCB top" height="300">
  <img src="pcb_soldered_back.jpg" alt="Soldered PCB back" height="300">
  <br><em>The same PCB with every module soldered (top) and the wiring on the back. These two photos are from the <strong>previous chassis (v3)</strong>; the board itself has not changed, and they are kept because they are the clearest view of every component soldered in place.</em>
</div>

The boards arrived in under two weeks and No board failure is recorded in this repository; the damaged parts are listed in [other/README.md](../other/README.md). To reproduce them: open the [OSHWLab project](https://oshwlab.com/edu560/project_ehytvvog), choose *Order PCB*, keep 1.6 mm thickness, then solder female headers for the Arduino Nano, the BMI160, the TB6612FNG and the buck converter, and JST connectors for the external components.

### 6.4 PCB blocks

| Block | Layout |
|---|---|
| Arduino Nano | <img src="pcb-blocks/arduino_nano_esp32_pcb.png" height="180"> |
| Camera board connectors | <img src="pcb-blocks/camera_esp32_out_pcb.png" height="180"> |
| TOF sensor connectors | <img src="pcb-blocks/sensors_out_pcb.png" height="180"> |
| BMI160 | <img src="pcb-blocks/bmi160_pcb.png" height="180"> |
| TB6612FNG | <img src="pcb-blocks/tb6612fng_pcb.png" height="180"> |
| Motor + encoder connector | <img src="pcb-blocks/motor_and_encoder_out_pcb.png" height="180"> |
| Servo connector | <img src="pcb-blocks/servo_out_pcb.png" height="180"> |
| Buck converter | <img src="pcb-blocks/voltage_regulator_pcb.png" height="180"> |

---

## 7. Datasheets

| Component | File | Key data used |
|---|---|---|
| Arduino Nano (classic) | [Arduino Nano documentation](https://docs.arduino.cc/hardware/nano/) | Pinout, VIN range, 3.3 V pin limit |
| ESP32-S3-CAM | [GitHub: nulllaborg/esp32s3-cam](https://github.com/nulllaborg/esp32s3-cam) | Camera pin layout, PSRAM, flash size |
| OV2640 | [OV2640DS.pdf](datasheets/OV2640DS.pdf) | Resolutions, RGB565 output |
| VL53L1X (TOF400C) | [DS_vl53l1x.pdf](datasheets/DS_vl53l1x.pdf) | Range, timing budget, ROI, I²C address change |
| BMI160 | [BMI160.pdf](datasheets/BMI160.pdf) | Gyroscope registers, ranges, sensitivity |
| TB6612FNG | [TB6612FNG.PDF](datasheets/TB6612FNG.PDF) | Truth table (forward, reverse, coast, short brake), current limits |
| CN3903 buck | [CN3903.PDF](datasheets/CN3903.PDF) | Input range, 3 A output |
| N20 motor (12 V, 1000 rpm, on the car) | [N20_12V_1000RPM_specs.md](datasheets/N20_12V_1000RPM_specs.md) | Speed, current, encoder; motor-to-wheel ratio counted 1 : 1 (24 Nov; motor gear and differential gear, 30 teeth each), internal gearbox **17 : 1, counted by hand by the team's mechanic**. Stall current: [measured winding resistance 11.7 Ω → 1.03 A](datasheets/N20_stall_current_measurement.md). The old 6 V datasheet was removed on 23 Nov (different motor; it stays in the git history) |
| SG90 servo (original choice, not on the current car) | [SG90.PDF](datasheets/SG90.PDF) | Kept for the history only |
| MG90 metal-gear servo (steering on the current car) | [MG90 / MG90S datasheet (external PDF)](https://www.tinytronics.nl/product_files/000263_Data_Sheet_of_MG90S_Analog_Servo_Motor.pdf) | Pulse range, torque, gear type. Our own stall-current measurement (3.3 Ω → ≈ 1.27 A): [MG90_stall_current_measurement.md](datasheets/MG90_stall_current_measurement.md). Linked, not copied into this folder, because the file is not from the team. The MG90 and MG90S are the same family with the same pulse range |
