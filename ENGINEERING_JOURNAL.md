# Engineering Journal — WRO Future Engineers 2026

> **Team:** _[team name]_ · **Country:** Ecuador (Guayaquil)
> **Members:** Henry Riera Loza (software and electronics integration) · _[teammate]_ (mechanics and chassis) · _[teammate]_
> **Coach:** _[coach name]_
> **Journal version:** 0.3 — 4 October 2026

This journal documents **why** we built our self-driving car the way we did: the problems we found, the options we compared, the data that made us decide, and what changed as a result. It follows the five criteria of the WRO 2026 documentation rubric (General Rules, Appendix C).

**How to read the evidence in this journal**

| Mark | Meaning |
|---|---|
| ✅ | Tested on our hardware; the result is from our own measurements |
| 🧮 | Calculated from field geometry, datasheets or physics |
| ⏳ | Planned or pending test; we say what we will measure |

We keep these marks so judges can tell measured results apart from plans.

---

## Rubric evidence map

Judges can use this table to find the evidence for each criterion quickly.

| Rubric criterion | What level 6 asks for | Where it is in this journal |
|---|---|---|
| 1. Mobility and mechanical design | Torque/speed reasoning, trade-offs, why components were chosen, iterations that improved performance | 1.1 drive choice · 1.2 speed requirement · 1.3 steering-angle calculation · 1.4 mechanical iterations |
| 2. Power and sensor architecture | Power budget, sensor trade-offs, placement justified with field geometry, calibration, failure points, iteration | 2.2 power budget · 2.3 Sharp → TOF decision · 2.4 placement geometry · 2.5 calibration · 2.6 failure points |
| 3. Software architecture and obstacle strategy | State machine with rationale, justified algorithms, edge cases, testing/tuning with metrics | 3.2 state machine · 3.3–3.5 algorithms · 3.6 edge cases · 3.7 evolution of corner detection · 3.8 vision · 3.10 metrics |
| 4. Systems thinking and engineering decisions | Constraints, trade-offs, iteration cycles, risks with mitigation, "we chose X instead of Y because…" based on data | 4.1 subsystem interactions · 4.2 constraints · 4.3 trade-offs with numbers · 4.4 decision record · 4.5 risk table |
| 5. Reproducibility and GitHub quality | Fully reproducible, clear structure, meaningful commits, testing workflow, versioning | 5.1–5.3 build and flash · 5.4 testing workflow (`TESTS.md`) · 5.5 version history |

---

## 0. Problem definition

### 0.1 What the rules require from the vehicle

| Requirement (rule) | Consequence for our design |
|---|---|
| 3 laps, 8 sections per lap, stop **completely inside** the starting section (9.24.2, A.2) | We must count corners **and** control where the car stops after the last corner |
| Driving direction (CW/CCW) drawn before each round (9.3) | The car must detect the direction by itself |
| Corridor width per straight is **1000 mm or 600 mm**, chosen by coin toss (8, Fig. 7b) | The inner wall position changes, so "the centre of the corridor" changes between sections |
| No entering data or calibrating sensors by hand before the round (9.9) | All calibration must run automatically inside the program, with no human input |
| In the Open Challenge the car may **not touch the outer wall** (9.18) | The car must keep a safe distance from the outer wall |
| 3-minute round (9.1) | Sets the minimum speed (see 1.2) |
| ≤ 300 × 200 × 300 mm, ≤ 1.5 kg, 4 wheels, one driving axle, one steering actuator (11.1–11.3) | Small chassis, one drive motor, Ackermann steering |
| No wireless communication during rounds (11.10); only wired links between components (11.17) | Wi-Fi off in competition code; UART cable between boards |
| One power switch, one start button, then wait (9.10–9.11) | `WAITING` state in the program |

### 0.2 Field facts we use in our calculations 🧮

- The track is 3000 × 3000 mm. Each side is split into corner section (1000 mm) + straight section (1000 mm) + corner section.
- Walls are **100 mm high and black** (13.3–13.6). Black surfaces reflect less infrared light, which matters for our TOF sensors (see 2.4).
- Pillars: red RGB (238, 39, 55) and green RGB (68, 214, 44), 50 × 50 × 100 mm (13.19–13.22).
- **Lap length:** with a 1000 mm corridor the car drives roughly along a square of side 2000 mm, so **≈ 8 m per lap, ≈ 24 m for 3 laps**.

### 0.3 System overview

| Subsystem | Component | Status |
|---|---|---|
| Main controller | Arduino Nano ESP32 | ✅ in use |
| Vision controller | ESP32S3-CAM (emakefun): ESP32-S3R8, 8 MB PSRAM, OV2640 camera | ✅ camera works; detection ⏳ |
| Drive | N20 12 V DC gear motor with encoder → rear axle with differential | ✅ tested |
| Motor driver | TB6612FNG | ✅ tested |
| Steering | Ackermann front axle with an MG90 servo (180°), which replaced an SG90 | ⏳ new servo on chassis v3 |
| Distance sensors | 4 × TOF400C (VL53L1X), which replaced 4 × Sharp GP2Y0A21YK0F | ✅ all 4 read together |
| IMU | BMI160 gyroscope | ✅ tested |
| Start | Push button on D10 | |
| Battery | 380 mAh, _[voltage / chemistry]_ | |

> 📷 _Required by rule 7: photos from the front, back, left, right, top and bottom, plus a team photo → `vehicle-photos/`, `team-photos/`._
> 🎥 _Required by rule 7: one YouTube video per challenge with at least 30 s of autonomous driving → links in `video/README.md`._

---

## 1. Mobility and mechanical design

### 1.1 Drive: rear-wheel drive with a mechanical differential

**Problem.** The car needs to drive forward and backward with one driving axle (rule 11.3). Differential-drive robots, with one motor per side, are forbidden (rules 11.3 and 11.5).

**Options considered.**

| Option | Advantage | Disadvantage |
|---|---|---|
| Solid rear axle | Simplest | In a turn the outer wheel must travel further, so one wheel slips; the turning radius becomes unpredictable |
| **Rear axle with a differential** ✔ | Each rear wheel turns at its own speed in a corner, so no slipping | Slightly more complex mechanics |
| Two motors on one axle | More torque | Extra weight and current; not needed for a ~24 m run |

**Decision.** One N20 motor drives the rear axle through a differential. The N20 has an **encoder**, which we can later use to measure distance travelled (see 3.6, edge case E9).

### 1.2 Speed and torque reasoning 🧮

**Minimum speed.** 3 laps ≈ 24 m in 180 s, so the car must average at least **0.13 m/s** just to finish. A reasonable target with a safety margin (repairs, slower corners) is **0.3–0.5 m/s** on straights.

**Maximum useful speed depends on the sensors, not on the motor.** Our TOF sensors deliver one new reading every **100 ms**, and the median filter needs about **3 new readings** before a sudden change shows up in its output (see 4.3). So:

| Speed | Distance travelled per TOF reading | Distance before a corner is detected (≈ 3 readings) |
|---|---|---|
| 0.3 m/s | 3 cm | ≈ 9 cm |
| 0.5 m/s | 5 cm | ≈ 15 cm |
| 1.0 m/s | 10 cm | ≈ 30 cm |

In a 600 mm corridor, detecting a corner 30 cm late leaves almost no room to turn. **We will therefore start tuning at ~0.3–0.5 m/s and only increase speed if the lap success rate stays high (metric M1).**

**Torque check.** ⏳ _To fill in with measured values:_
- Mass of the car `m` = _[kg]_ (weigh it), wheel radius `r` = _[m]_.
- Force needed to accelerate: `F = m × a`. For example, to reach 0.5 m/s in 0.5 s, `a = 1 m/s²`.
- Required wheel torque: `T = F × r` (plus rolling friction). Compare with the N20 datasheet torque × gear ratio.
- Measure the real speed: time over 1 m at `MOTOR_SPEED = 150` and `255` → table in `TESTS.md`.

### 1.3 Steering: Ackermann geometry and the angle we need 🧮

**Why Ackermann.** In a turn, the inner front wheel must steer more than the outer one so that both roll around the same centre. Without this, the tyres scrub, the car loses grip, and the turning radius changes between runs.

**How much steering angle do we need?** The worst case is a **narrow (600 mm) corridor**. To turn 90° inside the corner, the centre of the car follows an arc around the inner wall corner. If the car drives in the middle of the corridor, that arc has a radius of about **R ≈ 300 mm**. For a car with wheelbase `L`, the steering angle is:

```
δ = atan(L / R)
```

| Wheelbase L | Steering angle needed for R = 300 mm |
|---|---|
| 120 mm | 21.8° |
| 150 mm | 26.6° |
| 180 mm | 31.0° |

The outer side of the car (half the width ≈ 100 mm) then passes at ≈ 400 mm from the inner corner, which still fits inside the 600 mm corridor.

**Conclusion:** our steering must reach **at least ≈ 25–30°** at the wheels, and a short wheelbase helps. ⏳ _Measure `L` and the real maximum wheel angle on chassis v3 and fill in the table._

### 1.4 Mechanical iterations

| Version | Change | Why (observed problem) | Result |
|---|---|---|---|
| v1 | SG90 servo held only with nuts | — | ✅ **Failure observed (22 Sept):** the servo body moved inside its mount when steering, so part of the servo travel was lost. The same command gave different wheel angles. |
| v2 | MG90 servo (metal gears, more torque) | The SG90 mount had play and plastic gears | Better, but the servo was damaged on 3 Oct |
| v3 ⏳ | New chassis: new servo and relocated, raised side TOF sensors | Damaged servo + sensor beams passing over the walls (see 2.4) | Delivery 5 Oct. The servo centre and limits must be measured again. |

**Steering centre ✅.** On chassis v1 the wheels were straight at **120°**, not at the theoretical 90°. We found it by sweeping the servo with `servo_simple` and checking the wheels against a straight edge. Provisional limits: 90° (left) and 150° (right), i.e. ±30° of servo travel.

**Lesson learned.** A loose servo mount cannot be fixed with software: no PID tuning compensates for a mechanical play that changes each time. Mechanical rigidity comes first.

> 📐 _To add: dimensioned drawing (wheelbase, track width, steering pivots, servo linkage) → `schemes/`._

---

## 2. Power and sensor architecture

### 2.1 Wiring and pin map

| Signal | Nano ESP32 pin | Notes |
|---|---|---|
| TB6612 PWMA (motor speed) | D6 | PWM 0–255 |
| TB6612 AIN1 / AIN2 (direction) | D7 / D8 | HIGH/LOW = forward · LOW/HIGH = backward · LOW/LOW = coast · HIGH/HIGH = active brake |
| TB6612 STBY | D5 ⏳ to confirm | Must be HIGH, otherwise the driver is disabled |
| Steering servo | D9 | 50 Hz, pulse 500–2500 µs |
| Start button | D10 | |
| I²C SDA / SCL (4 × TOF + BMI160) | A4 / A5 | Shared bus at 400 kHz |
| TOF LEFT / RIGHT / BACK / FRONT XSHUT | A3 / A2 / A0 / A1 | I²C addresses 0x30 / 0x31 / 0x32 / 0x33 |

**Nano ESP32 detail ✅.** Digital pins must be written with the `D` prefix (`D9`, not `9`). The plain number refers to a different internal GPIO of the ESP32-S3, and this caused erratic behaviour in our early tests.

> 🔌 _To add: wiring diagram (Fritzing or a clean hand drawing) → `schemes/wiring.png`._

### 2.2 Power budget ⏳

The table below is the method we will follow. We will fill the measured column with a multimeter in series with each consumer.

| Consumer | Datasheet value | Measured typical | Measured peak | Notes |
|---|---|---|---|---|
| N20 motor at MOTOR_SPEED 150 / 255 | | | | Peak = stall current at start |
| MG90 servo (moving / stalled) | | | | Stall current appears when the wheels are blocked against a wall |
| Nano ESP32 | | | | |
| ESP32S3-CAM (camera on, Wi-Fi off) | | | | Wi-Fi off also saves current |
| 4 × VL53L1X | | | | |
| BMI160 | | | | |
| **Total** | | | | |

**What we will check with these numbers.**
1. **Runtime:** 380 mAh ÷ total typical current must be well above one round (3 min) plus practice attempts (4 min each).
2. **Brown-outs:** servo and motor current peaks can make the supply voltage dip and reset the ESP32. If a reset happens during a run, the car restarts in `WAITING` and the round is lost. Mitigation: a separate regulator or a large capacitor for the logic.
3. **Regulator rating:** the logic regulator must handle the summed peak of both ESP32 boards.

### 2.3 Sensor decision: Sharp IR → TOF laser

**Problem ✅.** Between 15 and 21 Sept we tested 4 Sharp GP2Y0A21YK0F analog sensors. We converted the voltage with `distance = 27.86 × V^-1.15` (12-bit ADC, 3.3 V).

**What the data showed ✅.**
- Beyond **~50–60 cm**, readings were too noisy to compare left and right reliably.
- **Why:** the curve `V^-1.15` is very flat at long distance. A small change in voltage (noise) becomes a large change in distance. The error grows exactly in the range we need.
- **Why it matters 🧮:** in a 1000 mm corridor, the inner wall can be **~60–80 cm** from the car. That range is where the Sharp sensor is weakest.

**Options considered.**

| Sensor | Range | Output | Trade-off |
|---|---|---|---|
| Sharp GP2Y0A21YK0F | 10–80 cm, reliable only up to ~50–60 cm in our tests | Analog, non-linear | Simple wiring, but noisy where we need it |
| Ultrasonic | Long | Pulse timing | Wide beam; echoes from corners; slow update |
| **VL53L1X TOF (TOF400C)** ✔ | Up to 4 m (datasheet, Long mode) | Digital mm over I²C | Needs unique I²C addresses (XSHUT wiring); weaker on black surfaces |

**Decision.** TOF400C (VL53L1X), because it gives a direct distance in millimetres with no non-linear conversion, and its range covers our worst case with a large margin.

### 2.4 Sensor placement justified with field geometry

#### Why four sensors

| Sensor | Job |
|---|---|
| LEFT and RIGHT | Corner detection (the inner wall ends) and lateral control |
| FRONT | Emergency stop and backup turn rule; later, stopping position in the finish section |
| BACK | Parking manoeuvre and recovery in the Obstacle Challenge |

#### Side sensors rotated 30° toward the front 🧮

If a side sensor points at an angle of 30° toward the front and the wall is at a lateral distance `y`:
- The beam hits the wall **`y × tan 30° ≈ 0.58 × y` ahead** of the car. At `y = 40 cm`, the corner is "seen" **≈ 23 cm earlier** than with a sensor pointing straight sideways.
- The sensor reads `y / cos 30° ≈ 1.15 × y`. Our lateral controller uses **changes** relative to a reference taken by the same sensor (see 3.3), so this constant factor does not cause an error. It only scales the gain by 1.15.
- If the car turns slightly, one side reading grows and the other shrinks. The rotated sensors therefore also react to the heading of the car, not only to its position.

#### Vertical angle: the "beam over the wall" problem ✅ + 🧮

**Iteration 1 (3 Oct) ✅.** The sensors were mounted **~0.8 cm** above the floor and tilted **8° upward** to avoid seeing the floor. Result: at long distance, the sensors read **~300 cm** instead of the wall distance. When we placed an object on top of the wall, the reading became correct. This confirmed that the beam was passing **over** the 10 cm wall.

**Why it happened 🧮.** The VL53L1X does not measure along a thin line; it sees a cone. In our code the region of interest (ROI) is 4 wide × 13 high. That is about 15° wide and, interpolating between the 4-SPAD (~15°) and 16-SPAD (~27°) values, about 24° high, so ±12° vertically. With an 8° upward tilt, the upper edge of the cone points about 20° upward:

```
height of the upper edge at distance d = 0.8 cm + d × tan(20°)
at d = 60 cm  →  0.8 + 21.8 ≈ 22.6 cm   (the wall is only 10 cm high)
```

A large part of the cone was above the wall. The weak return from the black wall then lost against the far background.

**Iteration 2 (decision, ⏳ to verify).** Raise the sensors about **3 cm** (to ≈ 4 cm, close to the middle of the 10 cm wall) and mount them **horizontally (0°)**. With the beam centred on the wall, the spill above the wall and below toward the floor is balanced.

**Remaining risk we will test.** With a horizontal beam, the lower part of the cone reaches the floor at roughly `h / tan(half-angle)` (≈ 20–30 cm for h ≈ 4 cm). A white mat could return a shorter false distance. Test plan (`TESTS.md`, T4): wall at 20, 40, 60 and 80 cm; ROI heights 4, 8 and 13; record mean and spread. Then choose the combination that reads the wall correctly at all four distances.

#### Black walls and weak signal ✅

Black walls reflect little infrared light, so the sensor often reports the status `SignalFail` even when the distance is correct. Our code **accepts `SignalFail` readings if the value is greater than 0**, and relies on the median filter to remove bad values. Throwing away every `SignalFail` reading would leave us without data exactly when we need it.

### 2.5 Calibration methods

| Sensor | Method | Automatic or manual |
|---|---|---|
| Gyroscope (BMI160) | With the car still, average the raw Z-rate (500 samples × 5 ms ≈ 2.5 s in `gyro_yaw_test`; 200 samples in `open_challenge_v2`). This average is the bias, and it is subtracted from every later reading, like the "tare" button on a scale. | **Automatic**, runs inside the program after power-on in the `WAITING` state, with no input from the team. We will confirm with the judges that this is acceptable under rule 9.9, which forbids manual calibration on the vehicle. |
| TOF sensors | One fixed offset per sensor (`OFFSET_CM`), measured against a ruler at known distances. If a sensor reads 51.2 cm at 50.0 cm, its offset is −1.2. | Set in the code during the workshop, never at the competition table |
| Steering servo | Sweep with `servo_simple`, find the angle where the wheels are straight (120° on chassis v1) and the mechanical limits | In the code; repeated for every new servo or chassis |
| Camera colours | Point the camera at each pillar and the mat, read the average H/S/V of the central 10 × 10 pixels (`SHOW_CENTER_HSV`), and set thresholds between the measured values | In the code. Rule 13.18 allows colour calibration during practice time. |

### 2.6 Failure points found and fixed

Five devices share one I²C bus (4 TOF + BMI160), so the bus is a **single point of failure**.

| # | Symptom ✅ | Root cause | Fix |
|---|---|---|---|
| 1 | Readings `-1` / "no update" | Every VL53L1X starts at address 0x29. Several sensors on at once answered together. | At start-up: turn all 4 off, then turn them on **one by one** and assign 0x30–0x33 before turning on the next |
| 2 | Three sensors still at 0x29 | We believed XSHUT was inverted. It is not: **LOW = off, HIGH = on** | Corrected on 3 Oct |
| 3 | Sensor never initialised correctly | The TOF400C uses the **VL53L1X** chip; our first code used the VL53L0X library | Switched to the Pololu VL53L1X library (1 Oct) |
| 4 | Program frozen at start-up, even with all TOF off | Stuck I²C bus (wiring or module) | `Wire.setTimeOut(50)` so a stuck bus does not freeze the program; diagnostic sketches to test modules one by one |
| 5 | TOF range only ~40 cm (2 Oct) | Short timing budget, wide cone and ambient light under study | Timing budget raised to 100 ms, narrower ROI, Long mode → all 4 sensors read correctly on 3 Oct |

---

## 3. Software architecture and obstacle strategy

### 3.1 Two controllers, two jobs

| Board | Job | Why this split |
|---|---|---|
| Nano ESP32 | Sensors, state machine, PID, motor, servo | The control loop must run at a steady rate. A slow image must never delay a steering command. |
| ESP32S3-CAM | Capture images, find red and green pillars, send the result | Images need the 8 MB PSRAM and a lot of CPU time |

**Link:** UART over a cable ⏳. Only wired communication between components is allowed (rule 11.17). Wi-Fi was used **only on the bench** to view the camera image and is not used in the competition code (rule 11.10).

### 3.2 Open Challenge state machine (`open_challenge_v2`)

```
 power on
    │
    ▼
 WAITING ── calibrate gyro (car still), start TOF sensors
    │  start button (D10)
    ▼
 STRAIGHT ◄─────────────────────────────────────────────┐
    │  save refDistL / refDistR at the start of the straight │
    │  PD control keeps those distances                      │
    │                                                        │
    │  side reading jumps > JUMP_THRESHOLD (150 mm)          │
    │  or FRONT < 150 mm (emergency)                         │
    ▼                                                        │
 TURNING                                                     │
    │  servo at the limit toward the corner, motor on        │
    │  distance sensors ignored                              │
    │  gyro: turned 90° since the turn started? ─────────────┘
    │        → servo to centre, corners++, wait 150 ms, new references
    │
    │  corners == 12
    ▼
 FINISHED ── motor stops
```

**Rationale for each state.**

| State | Why it exists |
|---|---|
| `WAITING` | Required by rule 9.11 (wait for the start button). The car is still, so it is the right moment to measure the gyro bias. |
| `STRAIGHT` | Keeps the car parallel to the walls without assuming where the inner wall is (see 3.3) |
| `TURNING` | Separate state because while turning, the side readings hit walls at changing angles and mean nothing for control |
| `FINISHED` | 12 corners = 3 laps × 4 corners |

### 3.3 Straight driving: keep the distances, do not centre

**Problem.** The inner wall position changes randomly between sections (1000 or 600 mm corridor), and the car can start in any of the starting zones, closer to one wall or the other.

**Options.**

| Option | Behaviour | Problem |
|---|---|---|
| Wall-following on one side at a fixed distance | Simple | The fixed distance can be wrong for the next section |
| Centring between both walls | Symmetric | After each corner the "centre" moves, so the car swerves to reach the new centre |
| **Keep the distances measured at the start of each straight** ✔ | The car keeps the line it already has | Needs a minimum distance limit near the outer wall (edge case E2) |

**Control law (PD).**

```
errorL       = distL − refDistL
errorR       = distR − refDistR
lateralError = errorL − errorR
correction   = KP × lateralError + KD × (lateralError − previousError) / dt
servoAngle   = SERVO_CENTER + correction           (initial KP = 0.8, KD = 0.3)
```

- **Why both sides:** if the car drifts right, `errorR` goes down and `errorL` goes up. The two errors add up, so the correction is twice as sensitive as with one side.
- **Why PD and not P only:** a P controller on a car with steering tends to oscillate from side to side. The D term reacts to how fast the error is changing and damps the oscillation.
- **Why not PID:** an integral term is useful against a constant error, but here the references are reset at every straight, so there is little time for a constant error to build up. It also adds windup during corners. ⏳ _We will add it only if testing shows a constant offset._

### 3.4 Turning by angle, measured with the gyroscope

**Options.** Turn for a fixed time, or turn until the gyro says 90°.

**Decision: by angle.** A turn by time depends on battery voltage, speed and grip, which all change during a run. The gyro measures the actual rotation.

**How the gyro is read ✅ (`gyro_yaw_test`).**
- Registers read directly over I²C at 400 kHz, without a library.
- Range **±500 °/s** → sensitivity 65.6 LSB per °/s. Output data rate 100 Hz.
- Yaw is integrated each loop: `yaw += (rawZ − bias) / 65.6 × dt`.

**Why ±500 °/s 🧮.** Our fastest turn rate is about `v / R = 0.5 m/s ÷ 0.3 m ≈ 1.7 rad/s ≈ 95 °/s`. A ±500 °/s range covers it with margin for bumps. A ±2000 °/s range, which the DFRobot library used, would make each step **4 times coarser** for no benefit.

**Why no library ✅.** The DFRobot_BMI160 library printed only 0 on our board without reporting any error, and each reading took 15–20 ms because of internal delays. Our register code printed correct values and is much faster.

**Why drift does not accumulate.** Each turn is measured **relative to the heading at the start of that turn**, and a turn lasts about one second. Bias errors can only accumulate during that second, not over the whole 3-minute run. On straights, the distance controller keeps the car parallel to the walls.

### 3.5 Corner detection with TOF sensors

**Principle 🧮.** At every corner, the **inner** wall ends while the **outer** wall continues into the corner section. The side whose reading jumps suddenly is therefore always the inner side, which is also the direction of the turn. This works the same in CW and CCW rounds, and in 600 mm or 1000 mm corridors.

- Jump threshold `JUMP_THRESHOLD_MM = 150` (initial, ⏳ to tune).
- FRONT < 150 mm is only an **emergency** rule. If the car gets that close to the front wall, it has missed the corner.

### 3.6 Edge cases

| # | Situation | Risk | Handling | Status |
|---|---|---|---|---|
| E1 | The car starts in a zone close to the next corner | Corner arrives before the references stabilise | References are taken immediately in `WAITING` → `STRAIGHT` | ✅ in v2 |
| E2 | The car starts very close to the outer wall | It would keep that small distance and could touch the outer wall (forbidden in the Open Challenge, rule 9.18) | Clamp `refDist` to a minimum (e.g. 15 cm) | ⏳ |
| E3 | A single wrong reading (spike) looks like a jump | False corner | Median filter of 5; require 2 consecutive readings above the threshold | Filter ✅ in test code · confirmation ⏳ |
| E4 | A sensor returns timeout / invalid | `-1` could be read as a huge jump | Keep the last valid value; never compute a jump from an invalid reading | ⏳ |
| E5 | Both sides jump at the same time | Physically impossible at a real corner → sensor fault | Ignore and keep driving straight | ⏳ |
| E6 | The direction decided at corner 1 | All 4 corners of a round have the same direction | **Latch** the direction after the first corner. From then on, the sensors only decide *when* to turn, not *which side*. This was the rule in our validated Sharp logic. | ✅ versions C/D · ⏳ port to v2 |
| E7 | The corner jump is missed | The car drives into the front wall | FRONT < 150 mm → turn toward the latched direction | ✅ emergency stop in v2 · latched turn ⏳ |
| E8 | The turn overshoots because the servo needs time to return to the centre | The car ends the turn crooked | Start returning the servo a few degrees before 90° (parameter tuned on the track) | ⏳ |
| E9 | After the 12th corner the car stops at once, still partly in the corner section | **Loses the 3 stop points:** the car must be completely inside the finish section (A.2) | After corner 12, drive on until the car is well inside the straight, using the FRONT distance (the outer wall ahead is ≈ 1500 mm from the middle of the straight) or the encoder, then brake actively (HIGH/HIGH) | ⏳ **found during this review — must fix** |
| E10 | References are taken while the car is still crooked after a turn | Wrong references for the whole straight | Wait 150 ms after the turn before taking the new references | ✅ in v2 |

### 3.7 Evolution of the corner-detection algorithm

This is our main software iteration cycle. Each version was kept or rejected **because of test results**.

| Ver. | Idea | Test and result ✅ | Decision |
|---|---|---|---|
| A | Standard deviation of the readings: an open side should be noisier | Real data overlapped: an open side sometimes had a standard deviation as low as a real wall | ❌ Rejected — the feature does not separate the two classes |
| B | Compare LEFT vs RIGHT distance and turn to the larger one | Sharp readings beyond ~50–60 cm too noisy; comparison gave false decisions | ❌ Rejected — comparing two noisy values doubles the problem |
| C | Binary per side: WALL (≤ 50 cm) or OPEN. If one side opens before FRONT sees a wall, turn to that side and **latch** it. Otherwise turn away from the side that still has a wall. | Worked in manual simulation | ✔ Kept as backup rule |
| D | **Sudden-jump:** compare each reading with the previous one; a large increase = the wall ended | Worked in real tests after lowering the jump threshold from **25 cm to 10 cm** (wall threshold 62 cm) | ✅ **Validated** (Sharp hardware) |
| D′ | "Correction" of D: compare with a fixed reference instead of the previous reading, to also catch slow changes | In real tests it performed **worse** than D | ↩ **Reverted to D** |
| E | Jump detection on TOF data (150 mm), FRONT as emergency | ⏳ Track test on chassis v3 | Current version |

**What we learned from D′.** On paper, D′ looked more correct. With the moving reference of D, a slow change is never detected, and that turned out to be an **advantage**: slow changes come from the car drifting, not from a corner, so ignoring them avoids false corners. Since then, a change stays in the code only if it improves the real robot.

**Manual simulation.** Before the chassis existed, we moved the sensor board by hand along a corridor of boxes, simulating straights and corners, and read the decisions on the Serial Monitor. This let software development continue while the mechanics team built the chassis.

> 📎 _To add: Serial Monitor screenshots of versions A–D and the manual-simulation video → `docs/evidence/`._

### 3.8 Obstacle Challenge: vision pipeline (`color_detection_test`)

| Step | What we do | Why |
|---|---|---|
| 1 | Capture **160 × 120 RGB565** (QQVGA) at 20 MHz XCLK, frame buffer in PSRAM, `GRAB_LATEST` | A small image means more frames per second. A 50 mm pillar is still many pixels at track distances. `GRAB_LATEST` always gives the newest image, never an old one from the queue. |
| 2 | Convert each pixel to **HSV** | In RGB, a shadow changes all three channels. In HSV, the colour (hue) stays almost the same and only brightness (V) changes, so the same pillar stays "red" in light or shadow. |
| 3 | Classify pixels: **red** = H ≥ 340° or H ≤ 15°, S ≥ 100, V ≥ 60 · **green** = H 80–160°, S ≥ 80, V ≥ 50 | Red wraps around 0° on the hue circle, so it needs two ranges. Minimum S and V reject the white mat (low S) and black walls (low V). |
| 4 | Group neighbouring pixels into blobs with **BFS** (flood fill), ignore blobs < 30 pixels | Separate pillars become separate objects; small blobs are noise |
| 5 | Choose the **nearest pillar** = blob whose bottom edge is lowest in the image (largest Y) | With a forward-looking camera, objects closer to the car appear lower in the image. The nearest pillar is the one we must react to first. |
| 6 | Output colour, x, height, bottom → later over UART | |

**Initial thresholds are a starting point, not a result.** ⏳ T7 in `TESTS.md` will measure the real H/S/V of both pillars and the mat under the venue lighting.

**Planned decision rule ⏳.** Red pillar → pass on its right, so steer until the pillar is on the left side of the image. Green pillar → pass on its left, so keep it on the right side. The pillar's x position becomes the target of the steering controller.

**Planned work.** Block 1 calibration → Block 2 decision logic on the Nano with simulated camera data → Block 3 UART link → parking (magenta blocks, rule 13.27).

### 3.9 Code modules

| Folder / sketch | Board | Purpose |
|---|---|---|
| `probar hardware/rear_motor_loop_test` | Nano ESP32 | Motor and driver: forward 2 s → stop 1 s → backward 2 s → stop 1 s |
| `probar hardware/servo_simple` | Nano ESP32 | Servo with a manual 50 Hz pulse; used to find the steering centre |
| `probar hardware/gyro_yaw_test` | Nano ESP32 | Gyro bias calibration and yaw integration |
| `probar hardware/tof_sensors_test` | Nano ESP32 | All 4 TOF sensors: address assignment, ROI, median filter, offsets |
| `probar hardware/camera_webserver_emakefun` | ESP32S3-CAM | Live camera image in a browser (bench only, Wi-Fi) |
| `obstacle challenge/color_detection_test` | ESP32S3-CAM | Red/green pillar detection with Serial output |
| `open_challenge_v2` ⏳ to add to the repo | Nano ESP32 | Full Open Challenge program |

All code is written and commented in English (rule 7: judges may not have our tools, so the comments must explain the code).

### 3.10 Performance metrics

| Metric | How we measure it | Target | Result |
|---|---|---|---|
| M1 — Lap success rate | 10 runs, random direction and corridor widths; count runs with 3 complete laps | ≥ 8/10 | ⏳ |
| M2 — Correct turn direction at corner 1 | Same 10 runs, CW and CCW | 10/10 | ⏳ |
| M3 — Correct stop in the finish section | Same runs (edge case E9) | ≥ 8/10 | ⏳ |
| M4 — Wall touches per run | Count during M1 | 0 | ⏳ |
| M5 — Turn accuracy | Rotate the car by hand exactly 90°, 4 times; compare with the gyro | ±3° | ⏳ |
| M6 — Gyro drift at rest | Car still for 60 s after calibration | < 1° per minute | ⏳ |
| M7 — Lap time | Stopwatch | Improve only after M1 is met | ⏳ |
| M8 — Pillar colour accuracy | 20 images per colour, at 3 distances | ≥ 95 % | ⏳ |

---

## 4. Systems thinking and engineering decisions

### 4.1 Subsystem map and interactions

```
 Battery ──► power switch ──┬──► TB6612 ──► N20 motor ──► differential ──► rear wheels
                            │
                            ├──► Nano ESP32 ◄── I²C (400 kHz) ── 4 × TOF + BMI160
                            │      │ ├──► D9 servo ──► Ackermann steering
                            │      │ └──◄ D10 start button
                            │      │
                            │   UART (planned)
                            │      │
                            └──► ESP32S3-CAM ◄── OV2640 camera
```

**Interactions we discovered by testing ✅.**

| Cause (subsystem A) | Effect (subsystem B) | What we learned |
|---|---|---|
| Servo play in its mount (mechanics) | Unrepeatable steering (control) | Mechanics must be fixed first; software cannot compensate |
| TOF tilt and height (mounting) | Walls "disappear" at long range, which looks like a corner (software) | A false corner can come from a mechanical cause; the fix was mechanical |
| One shared I²C bus (wiring) | One bad module or cable freezes all 5 sensors (reliability) | Timeouts, unique addresses, testing modules one by one |
| Sensor update rate and filter (sensors) | Maximum speed (mobility) | Speed is limited by detection latency, not by the motor (see 1.2 and 4.3) |
| Native USB on the Nano ESP32 / ESP32-S3 (electronics) | First Serial messages are lost (debugging) | Wait for the Serial connection in `setup()` |

### 4.2 Constraints

| Constraint | Type | How it shaped the design |
|---|---|---|
| 300 × 200 × 300 mm, ≤ 1.5 kg | Rules | Small motor (N20), compact sensors |
| 600 mm corridors | Field | Steering angle ≥ 25–30° and a short wheelbase (1.3) |
| 3-minute round | Rules | Average speed ≥ 0.13 m/s (1.2) |
| Random direction and inner wall | Rules | Detect the direction on board; keep distances instead of centring |
| No manual data entry or calibration (9.9) | Rules | All calibration automatic |
| No wireless (11.10) | Rules | Wired UART between boards |
| 100 ms TOF update + filter | Hardware | Limits speed before a corner (4.3) |
| Chassis not ready before the national final | Time | Software developed with manual simulation, in parallel with the mechanics |
| Team experience: strong in algorithms, new to robotics hardware | Team | Each component tested alone first (`probar hardware`) before integration |

### 4.3 Trade-offs, with numbers 🧮

**1. Noise vs. reaction time (TOF timing budget and median filter).**

| Setting | Noise | Delay before a corner shows up |
|---|---|---|
| Budget 33 ms, no filter | High (spikes cause false corners) | ~33 ms |
| Budget 100 ms, median of 5 (current test code) | Low | ~3 readings ≈ 300 ms → **15 cm at 0.5 m/s** |
| Budget 100 ms, median of 3 (option for the race code) | Medium | ~2 readings ≈ 200 ms → 10 cm at 0.5 m/s |

⏳ We will choose using M1 (success rate) and M4 (wall touches) on the track.

**2. Cone width vs. robustness (ROI).** A narrow cone (ROI 4) escapes less over the wall but receives less light from black walls (more `SignalFail`). A wide cone (ROI 16) receives more light but hits the floor and passes over the wall. Current choice: **4 wide × 13 high**, under test (T4).

**3. Speed vs. reliability.** Ranking uses points first and time only to break ties (rule 10.5). A slower car that completes 3 laps scores more than a fast car that crashes. We will only increase speed after M1 ≥ 8/10.

**4. Simplicity vs. accuracy (gyro).** Integrating only the Z axis is simple and fast. A fusion IMU (e.g. BNO085) would correct drift on chip but adds cost and integration work. Because our turns are relative and short (3.4), drift is small, so we keep the BMI160 for now.

### 4.4 Decision record — "we chose X instead of Y because…"

| # | We chose… | Instead of… | Because (evidence) |
|---|---|---|---|
| D1 | TOF VL53L1X | Sharp IR | ✅ Sharp readings were unreliable beyond ~50–60 cm; we need up to ~80 cm |
| D2 | Sudden-jump corner detection | L vs R comparison / standard deviation | ✅ Both alternatives gave false decisions with real data |
| D3 | Moving reference (D) | Fixed reference (D′) | ✅ D′ performed worse in real tests |
| D4 | Jump threshold 10 cm (Sharp) | 25 cm | ✅ 25 cm missed corners in real tests |
| D5 | Turn by gyro angle | Turn by time | 🧮 Time-based turns depend on battery voltage and speed |
| D6 | Keep distances from the start of each straight | Centring | 🧮 The inner wall moves between sections; centring makes the car swerve after each corner |
| D7 | BMI160 via registers at ±500 °/s | DFRobot library at ±2000 °/s | ✅ The library returned only 0 and was 15–20 ms slower; 🧮 ±500 gives 4× finer resolution and still covers ~95 °/s |
| D8 | Manual 50 Hz servo pulse | ESP32Servo / LEDC | ✅ The library was not installed and LEDC did not compile on our ESP32 core 2.x; the manual pulse worked first time |
| D9 | Sensors ≈ 4 cm high, 0° tilt | 0.8 cm high, 8° up | ✅ With 8° the beam passed over the wall (~300 cm readings) · 🧮 cone geometry |
| D10 | Separate camera board | One controller | 🧮 Image processing must not delay the control loop |
| D11 | Manufacturer's camera example | Generic Espressif example | ✅ The generic one crashed in a loop (Guru Meditation LoadProhibited) with our 8 MB flash/PSRAM setup |
| D12 | HSV colour thresholds | RGB thresholds | 🧮 Hue is more stable under shadows and lighting changes |
| D13 | Accept `SignalFail` readings > 0 | Discard them | ✅ Black walls often give `SignalFail` with a correct distance |

### 4.5 Risks and mitigation

| Risk / failure mode | Probability | Impact | Mitigation | Status |
|---|---|---|---|---|
| Car stops partly outside the finish section (E9) | High | −3 points per round | Drive into the straight before stopping, then active brake | ⏳ |
| False corner from a spike or a beam over the wall | Medium | Wrong turn → round lost | Mounting height, median filter, 2-reading confirmation | Partly ✅ |
| Touching the outer wall (Open Challenge) | Medium | Rule violation | Minimum reference distance (E2) | ⏳ |
| I²C bus freeze | Low | Car stops | Timeouts, unique addresses, secure connectors | ✅ |
| Brown-out reset from motor or servo current peaks | Unknown | Round lost | Power budget measurement, separate logic regulator | ⏳ |
| Servo damage or play | Medium (has happened) | Unrepeatable steering | Stronger servo, rigid mount, spare servo, re-measure centre | ⏳ chassis v3 |
| Venue lighting changes pillar colours | Medium | Wrong side → round ends | HSV, recalibrate during practice time | ⏳ |
| Upload stuck at `Connecting...` (ESP32S3-CAM) | Medium | Lost practice time | BOOT + RESET procedure documented (5.2) | ✅ |
| Lost debug messages (native USB) | High | Slower debugging | Wait up to 5–15 s for the Serial Monitor | ✅ |

---

## 5. Reproducibility

### 5.1 Software setup

1. Install Arduino IDE 2.
2. Boards Manager: **Arduino ESP32 Boards** (Nano ESP32) and **esp32 by Espressif** (ESP32S3-CAM).
3. Library Manager: **VL53L1X by Pololu**. No other external libraries are needed: the gyro and the servo are driven without libraries.
4. Each sketch is inside a folder with the same name (Arduino requirement). Open the `.ino` with File → Open.

### 5.2 Board settings

**Nano ESP32:** Tools → Board → Arduino ESP32 Boards → Arduino Nano ESP32.

**ESP32S3-CAM:**

| Option | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| PSRAM | OPI PSRAM |

If the upload stays at `Connecting...`: hold **BOOT**, press and release **RESET**, then release **BOOT**.

Camera pins (ESP32S3_EYE layout): SDA 4, SCL 5, XCLK 15, VSYNC 6, HREF 7, PCLK 13, Y2–Y9 = 11, 9, 8, 10, 12, 18, 17, 16.

### 5.3 Build order for another team

1. Assemble the chassis, then measure the wheelbase and the maximum steering angle (≥ 25–30°, section 1.3).
2. Wire everything following the pin map (2.1).
3. Run the hardware tests in the order of 5.4. Do not continue until each test passes.
4. Set `SERVO_CENTER`, `OFFSET_CM` and the colour thresholds from your own measurements.
5. Upload `open_challenge_v2` and tune KP, KD and `JUMP_THRESHOLD_MM` following `TESTS.md`.

### 5.4 Testing workflow

Detailed procedures and result tables are in **`TESTS.md`**. Summary:

| Test | Sketch | Pass criterion |
|---|---|---|
| T1 Motor | `rear_motor_loop_test` | Forward, stop, backward, stop, in a loop |
| T2 Servo | `servo_simple` | Wheels straight at `CENTER_ANGLE`; limits reached without forcing |
| T3 Gyro | `gyro_yaw_test` | Still: drift < 1°/min · rotated 90° by hand: 90 ± 3° |
| T4 TOF | `tof_sensors_test` | All 4 "OK"; wall at 20/40/60/80 cm read within ±2 cm |
| T5 Speed | (motor sketch + stopwatch) | Speed table at PWM 150 and 255 |
| T6 Open Challenge | `open_challenge_v2` | Metrics M1–M4 |
| T7 Colours | `color_detection_test` | Measured H/S/V for red, green, mat; M8 ≥ 95 % |

### 5.5 Version history (release notes)

| Version | Date | Main changes | Driven by |
|---|---|---|---|
| v0.1 | 15–21 Sept 2026 | Sharp sensors; algorithm versions A → D; manual simulation | Need for an on-board turn-direction decision |
| v0.2 | 22 Sept 2026 (national final) | Motor test, manual servo pulse, steering centre 120° measured | Failing servo libraries |
| v0.3 | 1–4 Oct 2026 | TOF sensors with unique addresses, register-level gyro, state machine v2, camera detection pipeline, test code organised | Sharp range limits (D1), servo play |
| v1.0 ⏳ | _[date]_ | Chassis v3, tuned Open Challenge, first Obstacle Challenge version | |

---

## 6. Development log

### 15–21 Sept 2026 — Turning direction with Sharp sensors
- **Context:** the chassis was not ready. We worked with 4 Sharp IR sensors on the Nano ESP32 (12-bit ADC) and moved them by hand (manual simulation).
- **Tested:** standard deviation as a wall/open indicator → the classes overlapped → rejected (A).
- **Tested:** L vs R distance comparison → noise beyond ~50–60 cm → rejected (B).
- **Developed:** binary WALL/OPEN with a latched decision (C) → worked in simulation.
- **Developed:** sudden-jump detection (D). Real tests: the jump threshold was lowered from 25 cm to 10 cm and the wall threshold set at 62 cm → validated.
- **Tested:** fixed-reference version (D′) → worse → reverted to D.
- **Open question:** the RIGHT sensor once read a wall (~37–45 cm) where none was expected. To check physically.
- **Conclusion:** the decision logic worked, but the Sharp range was the limiting factor → we researched TOF sensors.

### 22 Sept 2026 — National final day
- Motor driver verified with the motor at full speed.
- Servo: ESP32Servo was not installed and LEDC did not work with our core 2.x → we wrote `servo_simple` with a manual pulse → it worked. **Steering centre = 120°.**
- Observed servo play in its mount → mechanical failure logged.
- Started FRONT sensor tests.

### 1 Oct 2026 — New hardware, new algorithm
- Sharp → 4 × TOF400C; SG90 → MG90.
- State machine v2: STRAIGHT / TURNING / FINISHED, reference-distance PD, 150 mm jump detection, 90° gyro turns, 12-corner counter.
- Discovered the chip is a VL53L1X → library changed.
- The I²C bus froze even with all TOF sensors off → diagnostic sketches.

### 2 Oct 2026 — Components one by one
- FRONT TOF and gyro working together at the default address 0x29.
- TOF range only ~40 cm → range diagnostic (timing budget, ROI, ambient light).
- `rear_motor_loop_test` written and validated.
- Gyro: the register version works; the DFRobot library returns only 0 → kept the register version.

### 3 Oct 2026 — All four TOF sensors working
- XSHUT is **not** inverted; the wrong assumption caused address conflicts → fixed with the one-by-one address assignment.
- **All 4 TOF sensors read correctly at the same time** (median filter of 5, ROI 4 × 13, 100 ms budget).
- Beam over the wall with an 8° tilt → decision to raise the sensors ~3 cm and mount them at 0°.
- `gyro_yaw_test` validated.
- Steering servo damaged → chassis v3 started (delivery 5 Oct).
- Camera board identified: emakefun ESP32S3-CAM, ESP32S3_EYE pin layout.

### 4 Oct 2026 — Camera and documentation review
- The generic Espressif CameraWebServer crashed in a loop; the manufacturer's example worked (camera initialised, OV2640 confirmed, Wi-Fi connected; web page only partially loaded with a weak signal) → Wi-Fi dropped.
- `color_detection_test` written (HSV + BFS). Test pending.
- Code organised into `probar hardware` and `obstacle challenge`.
- **Documentation review against the WRO rubric:** this found edge case **E9** (the car would stop partly outside the finish section) and the missing power budget. Both were added to the plan.

---

## 7. Next steps

**Software and testing**
- [ ] Chassis v3: measure the servo centre, the limits, the wheelbase and the maximum wheel angle (1.3, T2).
- [ ] TOF at ≈ 4 cm and 0°: run test T4 (heights and ROI) and set `OFFSET_CM`.
- [ ] Confirm the STBY pin.
- [ ] Implement E2, E3, E4, E6, E8 and **E9** in `open_challenge_v2`.
- [ ] Tune KP, KD and the jump threshold; record M1–M4.
- [ ] Measure the power budget (2.2).
- [ ] Camera: calibrate the colours (T7) → Block 2 (decision) → Block 3 (UART) → parking.

**Documentation (required by rule 7)**
- [ ] Vehicle photos from 6 sides and a team photo.
- [ ] YouTube video for each challenge (≥ 30 s of autonomous driving).
- [ ] Wiring diagram and dimensioned mechanical drawing.
- [ ] README of at least 5000 characters on GitHub.
- [ ] GitHub commits: the first one at least 2 months before the competition with at least 1/5 of the final code, the second 1 month before, the third 2 weeks before. Use clear commit messages, e.g. "Add TOF address assignment", "Tune jump threshold to 10 cm".

**Future improvements under study**
- Use the motor encoder to measure each straight on lap 1, so the car knows where every corner is on laps 2 and 3.
- An IMU with on-chip sensor fusion (BNO085) if the drift measured in M6 is too high.
