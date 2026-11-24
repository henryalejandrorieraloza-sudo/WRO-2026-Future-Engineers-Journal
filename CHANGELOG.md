# Changelog

Versions of the car and of this repository. Dates are in 2026.

_v1.9.18 groups the documentation work of 23 November (night, evening and day). v1.9.19 (24 November) is the documentation review before the scoring deadline; v1.9.20 (same day) makes the README easier to read._

## v1.9.24 — 24 November · Fully charged battery voltage (13.05 V) documented

Only documents changed; **no code changed**.

- **13.05 V fully charged** (measured; 4.35 V × 3 cells) added to the bill of materials, the power diagrams, the failure-point tables and the README. The documents only gave the 11.4 V nominal value.
- **New `schemes/README.md` §4.4:** each part compared with 13.05 V (N20 motor 12 V, Nano VIN 12 V recommended, TB6612FNG VM 13.5 V operating / 15 V absolute, CN3903 up to 36 V), why the risk is limited (PWM average voltage, stall stop after 0.5 s) and the rules (charge as 3S HV, measure before the first run).
- **Stall current at 13.05 V:** 13.05 V ÷ 11.7 Ω ≈ 1.12 A, inside the 0.95–1.20 A range (`N20_stall_current_measurement.md`). The TB6612FNG's 1.2 A is its absolute maximum per channel (1.0 A recommended), not a continuous rating; the sentence in §4.2 was corrected.
- Journal §24.8 added.

## v1.9.23 — 24 November · Motor stall current measured

Only documents changed; **no code changed**.

- **Measurement:** winding resistance of the N20 on the car = **11.7 Ω** (multimeter, rotor still, motor disconnected). Stall current = 12 V ÷ 11.7 Ω ≈ **1.03 A**, range 0.95 to 1.20 A. New file `schemes/datasheets/N20_stall_current_measurement.md` (with the original PDF `N20_stall_current_measurement_es.pdf`).
- **Power budget (`schemes/README.md` §4.2, README §6.1):** the motor peak is 1.03 A (was 0.7 A from a datasheet of the same family). Battery peak ≈ 1.60 A, ≈ 18.3 W (was 1.27 A, 14.5 W); worst-case run time ≈ 11 min (was 14); discharge ≈ 4.2 C (was 3.3 C). The driver and the supply must carry ≥ 1.2 A per motor.
- **Updated:** `schemes/datasheets/N20_12V_1000RPM_specs.md`, `docs/TESTS.md` (two rows), README §5.2 and §10.3, "Not yet measured" and checklist rows, journal §24.7.
- **Honest limit:** the 1.03 A is calculated from a resistance reading, not read in series with the battery.

## v1.9.22 — 24 November · Width with and without wheels

Only documents changed; **no code changed**.

- **Width:** the car is **9 cm wide including the wheels** and **7 cm without them** (measured on 24 Nov). The 9 cm used in the code (`SAFE_LAT_CM`, half width 4.5 cm) is the width with wheels, which is the right one for clearance. `docs/TESTS.md` now has one row for each width (the old "track width 9 cm" row said the same as the car width) and README §5.1 states both.

## v1.9.21 — 24 November · Flow diagrams checked against the code

Only documents changed; **no code changed**. All 7 Mermaid diagrams were compared with `open_challenge.ino` and `obstacle_challenge.ino` and render without errors.

- **`obstacle_challenge` README §4.2 (STRAIGHT loop):** the flowchart now shows the two checks that run first (stop 0.45 s after the last line; camera watchdog: speed target 0 but the steering still runs), the last-line branch (after 12 turns the next line is remembered, not a new PRE_TURN), the pillar test as the code does it (current pillar, or the next one once level with it), the integral only in LANE mode, and the 8 s no-corner safety.
- **README §8.2 (Open state machine):** `DONE → STRAIGHT` is started by the START button or `g` (the diagram said only `g`).
- **README §9.1 (round flowchart):** the first line of the run locks the colour.
- **README §7.1:** the camera link sends one short text line per frame (not "a few bytes").
- **Text checked against the code:** the Wi-Fi sketch `camera_webserver_emakefun` is no longer described as present (README §3.1); `src/README.md` names the sketch that really drives D5 (`turn_measure_moving`); `video/video.md` no longer says the Obstacle video includes parking; gyro calibration time in the Obstacle README (§6.1).
- **Full read of the README (§4–§15), `schemes/README.md`, `docs/COMMITS.md`:** every constant quoted in §9.10 and §10.4 (`STALL_MS`, `MAX_STEER`, `LAT_MAX_DEG`, `ARRIVE_BEFORE_CM`, `TARGET_CM` and 25 more) matches the sketches. Fixed: the risk row "Gyro error accumulates over 12 turns" still named the October wall re-alignment (§10.3); the version list in §14 now includes v1.9.21.
- Checked and unchanged: Open state diagram (both READMEs) and Obstacle state diagram (both READMEs).

## v1.9.20 — 24 November · README easier to read

Only documents changed; **no code changed**.

- **Start here (README, top):** five links for an evaluator with little time: the two driving videos, the measured validation results (§11.2), how to build and upload (§12) and the constraints and trade-offs (§10).
- **History moved out of the README:** the October Open Challenge design (old §8.1), the WALL / NO WALL / NOT SURE window (old part of §8.3) and the 25 October decision tree (old §8.7) moved unchanged to the new [journal §25](docs/ENGINEERING_JOURNAL.md#25-archive-of-earlier-open-challenge-designs). §8.1 and §8.7 keep their numbers as one-paragraph pointers, so §8.2–§8.6 and every existing link stay the same. The old note that called all of §8.1–§8.7 "history" was wrong for §8.2–§8.6, which describe `open_challenge`; it is gone.
- **Checks:** every relative link and heading anchor in all `.md` files resolves (0 broken). The key numbers match across the README, `docs/TESTS.md`, `schemes/README.md` and the program READMEs: 248 g, 17.5 × 9 cm, 9.27 ticks per cm, ±40° steering, ROI 16 × 8, 28 STL files.

## v1.9.19 — 24 November · Every document checked against the competition code and the rubric

Rule used: the three programs that run on the car (`open_challenge`, `obstacle_challenge`, `obstacle_camera_v3_2`) are the truth; every document that disagreed was changed. **No code logic changed** (comments only in `obstacle_camera_v3_2.ino`; comment and start message in `servo_test.ino`). Details in journal §24.

- **Speed reasoning corrected (README §5.2, `docs/TESTS.md`):** the 12.6 cm/s of the entry below divided by the N20 gearbox ratio twice. The 1000 rpm rating is already the output speed, so the no-load top speed is ≈ 2.1 m/s at 12 V. The 22 Nov 3-lap run (8 s no-corner stop active, 1000–1800 mm inner walls) confirms the car is far faster than 12.6 cm/s. The "too slow" risk is replaced by "too fast for the sensors" (speeds capped in software). Torque margin recomputed for a ≈ 17 : 1 gearbox.
- **Steering geometry:** ±40° (measured) and the 10.7 cm turning radius replace the old 30°; checked against the 600 mm corridor (README §5.1, §5.3); v3 → v4 rows added to the iteration table (§5.5).
- **Power:** README §6.1 peak aligned with `schemes/README.md` §4.2 (1.27 A, 14.5 W).
- **Sensors:** README §6.3 and `schemes/README.md` §5 now say what each sensor does in each program (front ToF: Obstacle turn at 40 cm; rear ToF: back-up to 20 cm; both off in Open); encoder calibration method added.
- **Software:** Obstacle state diagram with the reason for each state (README §9.1); function layers and loop timing of both programs (README §7.3); state diagram in the `open_challenge` README; current corner rule of `open_challenge` stated before the October history (§8.3); October designs (§8.3, §8.7) folded into collapsible history blocks; §8.6 edge cases rewritten from the code.
- **Systems thinking:** new constraints table (README §10.0); stale "we chose X instead of Y" rows replaced (inner wall at 100 mm, line as an event + front ToF, 160 × 120 camera, locked line colour, reverse turn); risk table cleaned (3-column rows fixed); §10.4 rewritten with the pillar-dodging settings and their reasons.
- **Testing:** tuning loop (README §11.1) and a table of measured validation results with sources (§11.2).
- **Status and checklist:** README §13 rewritten (validated vs team report), §14.1 updated for 24 Nov, commit counts corrected (180 on 23 Nov, 96 by the Claude account).
- **Sub-READMEs:** `obstacle_challenge` (`camY`, ignore window, link to `open_challenge`), `obstacle_go_out_of_parking` (597 ticks, centre 85, `servo_test` link), `obstacle_camera_v3_2` (status, viewer resolution), `camera_test` (broken link, differences table), `servo_test` (centre 85), `motor_encoder_test` (encoder pins vs driving programs), `gyro_90_test` (1.06 vs 1.007).
- **Links and encoding:** two broken journal → README anchors fixed; `models/README.md` re-encoded as UTF-8 (a stray Latin-1 `§`); journal §16 next steps updated.

## Unreleased — 23 November (late) · Reproducibility and consistency of the documents

> Superseded on 24 Nov (v1.9.19): the speed item below was a calculation error.

- **Speed:** the 1 : 1 figure of 2.1 m/s is no longer presented as the expected speed. The encoder count implies about 17 : 1, which gives about 12.6 cm/s with no load. A three-lap round needs about 13 cm/s, so this is listed as an open risk (README §5.2, §10.3; stopwatch test in `docs/TESTS.md`).
- **Steering:** the open check (midpoint of the stops is 90, not 85) is also listed in README §13.
- **Submission checklist:** the third-commit row (README §14.1) and `docs/COMMITS.md` now state what is committed and what is still missing.

- **Turn sketch added:** `src/hardware-tests/turn_measure_moving/` holds the sketch behind the six 90° in-motion gyro runs in `docs/TESTS.md`. Its README states its constants (`YAW_SCALE` 1.00, `TICKS_PER_CM` 15.1, steering centre 120) and that they differ from the current code.
- **Test log:** the 22 Nov three-lap run is narrowed to `3010306` or `c3441cd`; both have `STEER_CENTER` 85 and `TICKS_PER_CM` 9.27, and their parent `30f5ca9` has 90 and 15.1.
- **Open check:** the steering stops 10 and 170 average 90, not the centre 85. Listed in `docs/TESTS.md` for measurement.
- **Index:** `src/README.md` lists the turn sketch in the hardware-tests table.
- No value was added without a measurement, and no competition program changed.

## v1.9.18 — 23 November (night) · Car width 9 cm everywhere, docs aligned with the code

- **Width:** the car is **9 cm wide** (measured and confirmed on 23 Nov). The 9.5 cm in the README, the journal, the models, the photos and the test log is corrected to 9 cm.
- **Half width:** `SAFE_LAT_CM = 17` is 4.5 + 10 + 2.5 cm. The comments in `obstacle_camera_v3_2`, `camera_test` and `obstacle_challenge` said 4.25 cm; they now say 4.5 cm. No value changed.
- **Focal length:** `FOCAL_PX = 50` is confirmed by the team as the correct value for 160 × 120; the "not measured" notes are removed from the README, the camera README, the test log and the journal.
- **Videos:** durations in `video/video.md` corrected to the real ones: Open Challenge 34 s (rule 7 asks for at least 30 s of autonomous driving) and Obstacle Challenge 2:14.
- **Motor datasheet:** `schemes/datasheets/N20_motors.pdf` (6 V, 33 rpm, a different motor) is removed; the `schemes/README.md` source table points to `N20_12V_1000RPM_specs.md`.
- **Rule 9.21 (README §3.1):** the reading of the reverse corner turns against rule 9.21, App. A.4 cases 1–6 and the published Q&A is written down: it stays inside the same corner section, so the team states that it complies.
- **Test log:** the width row is now confirmed; the "to measure" and conflict notes are removed.
- **Checklist (README §14.1):** the deadline sentence now says that the dates are counted back from 8 December (rule 7: 2 months, 1 month, 3 weeks, 2 weeks). No date changed.

## v1.9.18 — 23 November (evening) · Motor data, power budget and measurement plan

- **Motor:** the datasheet of the same family (BJ-N20-12-1000, 7 PPR) is recorded, with the reasons why the 1:30 ratio is not yet proven. The 2.4 kg·cm stall torque is no longer a design value.
- **Torque (README §5.2):** the required 8 mN·m at the wheel is compared with the rated (≈ 16 mN·m) and stall (≈ 42 mN·m) torques of that datasheet.
- **Power budget (`schemes/README.md` §4.2):** the motor's peak is the stall current of 0.7 A, not 350 mA. Battery peak ≈ 1.27 A, ≈ 14.5 W; worst-case run time ≈ 14 min.
- **Risks (README §10.3):** motor data, long stall, steering symmetry and wheel diameter added.
- **Test log (`docs/TESTS.md`):** blank tables for motor, steering, car geometry and mass, ToF, gyroscope, camera and suggested edge cases. No value was filled in without a measurement.
- **Mass:** 248 g, weighed on 23 Nov 2026 with the battery (see `docs/TESTS.md`).

## v1.9.18 — 23 November · README checked line by line against the three competition sketches

- **Videos:** thumbnails of the Obstacle and Open Challenge videos added in `video/thumbnails/` and linked to the YouTube videos in `video/video.md`.
- **Parts:** `models/README.md` states that one car uses **38 printed pieces** (26 distinct parts), without the 8 tyre-mould copies.
- **Checked against the code:** `open_challenge`, `obstacle_challenge` and `obstacle_camera_v3_2` are the reference. Constants and behaviour in the README now match them.
- **Corrected:** reverse gear (Obstacle turns in reverse, Open forward only); steering limits (left 10, right 170); the camera link is one-way (camera TX → Nano RX, no resistor divider needed); the gyro output rate is not set in the code; the turn, the stop and the side-sensor rules described as current now match the code; the 13 cm/s speed arithmetic.
- **History marked:** the 8-reading window, the undo, the 1 s and 500 ms delays and the PD turn are documented as October design, not as the current code.
- **Added:** a table of the limits and fallbacks of `obstacle_challenge` (README §9.10).
- **Counts updated:** 144 commits, 69 by the Claude account, 28 STL files (26 printed and 2 tyre moulds).
- **Status:** the organisers' link was sent and its receipt confirmed on 23 November.
- **Test status:** the team compiled the three competition sketches in Arduino IDE 2.x on 23 November (no errors). `open_challenge` was tested (run of 22 November). The team reports that `obstacle_challenge` works on the car, including its edge cases; no numbers are recorded yet. The parking sketch is not tested.
- **Systems thinking:** README §10.5 added: five chains of decisions from the engineering journal (sensor angle, steering, encoder, power, failures, reverse gear) and their effect on the current code. The I²C timeout (25 ms) and the parking note in §10.3 corrected.
- **Comments only:** stale and duplicated comments in `open_challenge`, `obstacle_challenge` and `obstacle_camera_v3_2` rewritten (old values such as "was 320" or "HALVED from" removed, duplicated function headers merged). No code line changed (checked with `git diff`).

## v1.9.17 — 23 November · Unused constants and obsolete comments removed
- **Constants removed where they were not used:** `STOP_OFFSET_CM`, `FALLBACK_CM` and `MAX_FINAL_CM` in `obstacle_challenge`; `SIDE_ZONE`, `LINE_TRIGGER_ROW`, `MAGENTA_MIN_PIXELS` and `TURN_CONFIRM_FRAMES` in `obstacle_camera_v3_2`. The three first names are still used by `open_challenge`, so they stay there.
- **Obsolete comments removed or corrected:** the "PASTE HERE" block note in the camera sketch, old colour-threshold notes, references to the removed `straight_pillars` sketch in the Open Challenge and parking sketches, and the `obstacle_challenge` README sentence about the constants.
- **No logic changed:** only declarations and comments. Brace counts were checked before and after. Compilation on the Arduino IDE is still to be done.

## v1.9.16 — 23 November · Videos added and documents reconciled with the code
- **Videos:** Open Challenge `4p94mWkXhsk` and Obstacle Challenge `ccAdp2c7_7Y` added to `video/` and the root README.
- **Docs corrected to match the sketches** (no `.ino` behaviour changed): TOF addresses of the driving programs (0x2A–0x2D), gyro calibration (300 samples), `servo_calibration` → `servo_test`, constants table of §8.0, old design sections marked as history, camera x range (−80…+79), `camera_test` = v3.1, gyro scale 1.007 in both programs, reverse-turn rule note. Details in journal §21.

## v1.9.15 — 23 November · Obstacle Challenge consolidated in `src/obstacle-challenge`
- **`obstacle_challenge` added:** the driving program of the round. It combines the floor-line laps (the first line sets the direction, every corner is a 90° turn in reverse with the gyro and the front and rear ToF), the pillar dodging on an odometry map, and the wall centring between pillars. It has its own README with the constants, the state and flow diagrams, the camera fields it uses and the stop messages.
- **`obstacle_camera_v3_2` is the vision used by `obstacle_challenge`.** Its farthest pillar (`MAX_AHEAD_CM`) changed from 130 to **40 cm**. Its README now names `obstacle_challenge` and describes the plausibility range and the 12 fields the Nano reads.
- **Only the current Obstacle code stays in `src/obstacle-challenge`:** `obstacle_challenge`, `obstacle_camera_v3_2` and `obstacle_go_out_of_parking`. The earlier sketches (`obstacle_camera` v3.1, `straight_pillars`, `straight_pillars_measurements`, `line_test`) are in the git history. The Open Challenge and the hardware tests were not changed by this version.
- **`obstacle_go_out_of_parking` README:** links and the centre-servo reference now point to the current programs.
- **Documentation made consistent with the code:** README §9 (Obstacle Challenge strategy) rewritten for the program that is in `src`, with the vision, the map, the wall centring, the laps and the parking start described from the code; `src/README.md` (Obstacle section and the layout); the camera viewer's help text; the module table and the status table in the root README; this entry and journal §20.

## v1.9.14 — 22 November · One set of car constants, three-lap run recorded
- **`open_challenge` now uses the same constants as `straight_pillars`:** `TICKS_PER_CM` 15.1 → **9.27** (motor-running measurement) and `STEER_CENTER` 90 → **85** (measured on this chassis). The stop position and the steering are expected to change; the first run after this change must be checked on the track.
- **The same centre (85) in the other sketches:** the parking sketch (`obstacle_go_out_of_parking`, was 90), `line_test` (was 120) and `servo_test` (was 90).
- **Three-lap run recorded** in the journal (§19.2): three laps completed on the first attempt, stop in the starting section near the same wall, narrow corridors. Lap time and direction are still to be added.
- README §8.0 table, §13, the `open_challenge` and parking READMEs, and `src/README.md` updated to the new values.

## v1.9.13 — 22 November · Submission checklist: comments, constants, parking README
- **`open_challenge` comments now match the values.** `TURN_PWM = 90` no longer says "90 was too weak" (lower values were too weak). `INNER_TARGET_MM = 100` now states that 100 is the value used in the 3-lap run (22 Nov). README updated to match. The value is unchanged.
- **Constants that differ between programs are documented** (README §8.0): encoder 15.1 (hand-measured) against 9.27 (motor running); gyro `GYRO_SCALE = 1.007` against `YAW_SCALE = 1.06`, each with its own measurement; steering centre 90 against 85; and the steering limit names. No value was changed, because each change alters how the car drives and needs a track test first (journal §19.1).
- **README §13 encoder row fixed:** it said "Planned — next: measure ticks per mm" in one row and "Done" in another. The ticks per cm are measured; the distance-based delays are the planned part.
- **New `README.md` for `obstacle_go_out_of_parking`** (what each step does, pins, calibration, stop messages, known gaps). The sketch is still untested on the track. Its `REVERSE_TARGET_TICKS = 1024` is marked as a placeholder, not a measured value, and the README notes that the start button is still missing.
- **`schemes/README.md`:** the SG90 datasheet is marked as the original servo, not the one on the current car. The MG90 datasheet still has to be added to `datasheets/`.

## v1.9.12 — 22 November · English throughout, half-resolution camera sketch, coherence review
- **The whole repository is in English:** comments, serial messages and the names of the scenario-roulette page's CSS classes and HTML ids (`legend`, `line`, `rule-note`, `chart`).
- **`obstacle_camera_v3_2` added** (already on `main` in ca0054e, listed here for the history): the v3.1 vision code at QQVGA (160 × 120), about four times fewer pixels. Not validated on the track; v3.1 remains the reference.
- **`straight_pillars_medidas` renamed to `straight_pillars_measurements`** (folder and sketch). Comments translated. The stale "pending" note on `YAW_SCALE` / `YAW_REVERSED` was replaced by the 21 Nov result.
- **Coherence review:** README steering values, ticks per cm in the `straight_pillars` README, video status and the rubric checklist were updated to match the code. The journal (§19.1) lists the open items that were found but not changed: `open_challenge` still uses 15.1 ticks/cm and steering centre 90, and `line_test` still uses centre 120.

## v1.9.11 — 22 November · WiFi live view off in the competition build
- **`WIFI_STREAM` set back to `false`** (`obstacle_camera.ino`). The v1.9.10 change had turned it on by default, which would have let the camera start a Wi-Fi radio during a round. Rule 11.6 of the WRO 2026 Future Engineers General Rules forbids radio communication during rounds. The README already said the competition code never starts Wi-Fi, so the code now matches the documentation.
- The Wi-Fi block stays in the sketch for bench tests (`WIFI_STREAM = true`, plus your own SSID and password). `obstacle_camera/README.md` and the journal (§19) were corrected to match.

## v1.9.10 — 22 November · WiFi live view now shows the segmentation (later set back to off, see v1.9.11)
- **`WIFI_STREAM` turned on by default** (`obstacle_camera.ino`): whenever the camera is powered, `http://<printed IP>/` streams live with no USB cable, instead of needing to be switched on for each bench test.
- **New: `WIFI_STREAM_OVERLAY`** paints the same pillar/line segmentation colours `camera_viewer.html` draws over USB — pillar blocks blended 50 % red/green, accepted orange/blue line pixels painted solid, a yellow box around the pillar actually chosen — directly onto the picture before it is sent over WiFi, via the new self-contained `drawSegmentationOverlay()`. It only *reads* the detection globals the vision code already filled that loop (`blockCode`, `code`, `pick`); it changes nothing the car decides.
- Cost is unchanged while nobody is watching (one cheap "anyone connected?" check per loop); while a browser *is* watching, the overlay + JPEG-encoding still slow the vision loop, so close the tab before a timed run if every frame matters.
- **Still needs your own WiFi name/password** filled into `WIFI_STREAM_SSID` / `WIFI_STREAM_PASS` (2.4 GHz only) before flashing — those stay as placeholders, since nothing in the repo knows that.
- Updated `obstacle_camera/README.md` and `src/README.md` to match.

## v1.9.9 — 22 November · Live camera view over WiFi, no USB cable needed
- **New, self-contained `WIFI_STREAM` block in `obstacle_camera.ino`** (off by default): connects to a 2.4 GHz WiFi network and serves a live MJPEG view at `http://<ip>/`, viewable from any phone or laptop browser while the car drives around untethered. Reuses the exact frame already captured for pillar/line detection (no extra camera capture) and only pays the JPEG-encoding cost while a browser is actually connected and watching.
- Touches nothing else in the sketch except two call sites (`connectWifiStream()` in `setup()`, `serveWifiStream(frame)` in `loop()` right before the frame is returned), same self-contained-block pattern as the front-TOF line-stop feature.
- Documented in `obstacle_camera/README.md` under "Live view over WiFi", including the trade-off (lowers fps to the Nano while a viewer is connected — bench/calibration use only, off for a timed run).

## v1.9.8 — 21 November · Don't stop diagonal: straighten to yaw = 0 first
- **`FRONT_STOP_MM` raised from 400 to 1200** (40 cm → 120 cm): the car now stops with more clearance from whatever is ahead after the floor line.
- **New: the car is no longer allowed to stop at an angle.** Once the front TOF reads under `FRONT_STOP_MM`, it only brakes if also within the new `STOP_YAW_TOL_DEG` (2°, kept tight on purpose) of yaw = 0 — the same direction it faced at START. If it is close to the wall but still diagonal, it steers at full lock (`±MAX_STEER`) toward yaw = 0 instead of easing into it with the normal PD gains, to fix the heading in the least time possible, and only stops once both conditions hold together.
- Fixed a stale comment directly above `handleLineStop()` left over from the very first version of this feature, which still said the car "does NOT try to steer back to any particular lateral position" — contradicting the recentring behaviour added in the previous commit.
- Updated `straight_pillars/README.md` §5, `PARAMETERS.md` §9, and `docs/ENGINEERING_JOURNAL.md` §18.3 to match.

## v1.9.7 — 21 November · After the floor line: ignore pillars, drive to the wall, stop
- **Revised the "stop on the floor line" feature from v1.9.6**, same day, twice: an immediate stop on seeing the line was replaced with `handleLineStop()`, a self-contained block (new constants/globals + `startFrontTof()` + `handleLineStop()`, placed just above `setup()`) that: powers on the **front VL53L1X TOF sensor** (same wiring as `open_challenge.ino`) the first time a line is confirmed; from then on **ignores the pillar map and every colour message** and steers back toward the **home lane position** (`camPosY() = 0`, the same `atan2`/look-ahead formula and PD gains `headingGoal()` already uses to return to the home lane — not a fixed compass heading, so the car still recentres even if it was mid-dodge when the line was seen); and only brakes once the front sensor reads less than `FRONT_STOP_MM` (now 120 cm).
- The only line changed outside that new block is the trigger in `loop()`, which now calls `handleLineStop(dt)` instead of stopping immediately — everything else in the sketch (pillar map, camera parsing, odometry) is untouched.
- Updated `src/obstacle-challenge/straight_pillars/README.md` §5 and `PARAMETERS.md` §9, and `docs/ENGINEERING_JOURNAL.md` §18.3.

## v1.9.6 — 21 November · Gyro calibrated, dodge-crash fixed, stop on the floor line
- **`straight_pillars.ino` fully re-validated on the track and brought up to date in the repo** (the committed version was stale: `RETURN_AFTER_CM = 5`, `TICKS_PER_CM = 15.1`, `YAW_SCALE = 1.00`, 10-field camera parsing). Replaced with the current, track-tested sketch and rewrote every comment as detailed English explaining the algorithm, the calibration history and the bug fix below.
- **Gyroscope calibrated with a real 90° hand turn:** new stand-alone test sketch [`gyro_90_test`](src/hardware-tests/gyro_90_test) (reuses `straight_pillars.ino`'s own register-level BMI160 driver). Measured yaw = 85° for an intended 90° turn → `YAW_SCALE = 90/85 = 1.06` (was the placeholder `1.00`); sign confirmed correct (`YAW_REVERSED = false`).
- **`TICKS_PER_CM` re-measured with the motor actually running** (190 ticks / 20.5 cm = 9.27), replacing the 20 Nov value (12.67) that had been measured by hand-rolling the car — different wheel slip, unreliable for a self-driving sketch.
- **Fixed a crash: the car clipped the pillar right after dodging it.** Root cause: `headingGoal()` drops its protective "never steer toward the pillar while beside it" clamp the exact instant the pillar is declared cleared, so the lateral goal jumps instantly and the PD controller fires a hard correction with almost no clearance. Fixed by raising `RETURN_AFTER_CM` from 30 to 45 cm, which delays the (still sharp) return swing until there is real distance from the pillar. A smoothing alternative was prototyped but was not needed. Details: `docs/ENGINEERING_JOURNAL.md` §18.2, `src/obstacle-challenge/straight_pillars/PARAMETERS.md` §10.
- **New feature: the car now brakes on the blue or the orange floor line.** `obstacle_camera.ino` already sent `blueFound`/`orangeFound` in every message; `straight_pillars.ino`'s `readCamera()` only parsed 10 of the 14 message fields and discarded `blueFound` without ever reading `orangeFound`. Extended the parsing to 12 fields, added persistent `camBlueFound`/`camOrangeFound`, and added a `STOP_ON_LINE`-gated check at the top of `loop()` that brakes, centres the steering and ends the run as soon as either line is confirmed. Open question recorded for the next track test: both flags trigger on any detection anywhere in the frame, not only a centred/close one — may need switching to `blueCenterRow`/`orangeCenterRow` if that proves too early.
- **`obstacle_camera.ino` corrected to match the confirmed-working sketch:** `MIRROR_HORIZONTAL` (was `true`, should be `false`) and `BOTTOM_TOL` (was 8, should be 27) — the rest of the committed sketch already matched.
- Updated `src/obstacle-challenge/straight_pillars/README.md`, `PARAMETERS.md`, `src/README.md` and `docs/ENGINEERING_JOURNAL.md` (new §18, Phase 13) to match.

## v1.9.5 — 21 November · Full STEP assembly added to the repo, STL count corrected
- **`models/ENNER_VALENCIA_FINAL.step`** added directly to the repository (≈48 MB) so the full assembly no longer depends on an external Drive link for reproducibility; the Drive link is kept as a mirror.
- **STL count corrected everywhere:** `README.md` and `models/README.md` said "25 STL files"; the `stl/` folder actually has **27** (25 printed parts of the car + 2 tyre moulds, `inner_wheel_mould.stl` and `outer_wheel_mould.stl`, which were already documented in §4 but not included in the total). Both files now say 27 and break it down.

## v1.9.4 — 21 November · Open Challenge documentation checked line by line against `open_challenge.ino`
- **README §7.2 module table:** the `TOF driver` row claimed `open_challenge` only uses "range valid" readings. The actual sketch does not check the VL53L1X's range-status flag at all — `updateTof()` filters by distance magnitude only (`0 < mm < 4000`). Corrected the row to describe what the code does; the "range valid" filtering stays attributed to `tof_and_gyro_test` and to the *earlier* (superseded) Open Challenge design of §8.1–8.7 and the journal, which did check it.
- **`src/open-challenge/open_challenge/README.md`:** the start-up gyro calibration was described as "about 1 s"; `calibrate()` actually takes about 2 s (700 ms settling + 300 samples × ~5 ms). `STOP_OFFSET_CM` was described as affecting "the final stop" in general; it only shifts the exact `LAPS ≥ 2` stop (`LSTART − D1 + STOP_OFFSET_CM`) — the `LAPS = 1` WALL estimate and the `FALLBACK_CM` stop ignore it. Both corrected.
- Everything else checked against the code (settings tables, wiring, state diagram, corner/turn/lane logic, D1/WALL/LSTART odometry, safety stops) already matched; no other changes needed.

## v1.9.3 — 20 November · Chassis v4, documentation consistency pass
- **Chassis v4:** the car is now **17.5 cm long** (120 mm base plus a front protrusion screwed to it), to enlarge the Obstacle Challenge parking-lot space; width (9 cm) and height (6 cm) are unchanged; the mass is 248 g. Every size reference in `README.md`, `models/README.md` and `v-photos/README.md`, and the rule 9.17/11.1 compliance row, updated to match (175 × 95 × 60 mm). Steering, drive and tyres are unchanged from v3.
- **`models/README.md`:** `rear_spacer`, `differential_support` and `diff_big_axle` corrected to **quantity 2** (they come in pairs, like the other symmetric parts); new **v4** row in the design-iterations table with the new top-view CAD render (`models/iterations/cad_v4.png`); `models/enner_valencia_cad.png` replaced with the v4 top-view render. The printed photo of v4 is still pending.
- **`schemes/README.md`:** added real photos of the **Arduino Nano** and the **power switch** to the BOM. Removed every remaining reference to the retired **Arduino Nano ESP32** (BOM row, datasheet, schematic and wiring captions) — the car has run on the classic Arduino Nano since chassis 2 (journal Problem 31), and the rest of the documentation already reflected that. The obsolete `physical_wiring.jpeg` photo was retired (parts changed since it was taken); the wiring section now points to the vehicle's own top-view photo instead. `pcb_soldered_top.jpg` / `pcb_soldered_back.jpg` are now explicitly labelled as photos of the **previous chassis (v3)**, kept only to show the components soldered in place.
- **`v-photos/README.md`:** removed the outdated "all parts laid out" photo (`components_exploded.jpg`), taken before several part models changed.
- Full pass over the repository to check every document against the current code and hardware: no remaining mismatches found; all documentation and code confirmed to already be in English.

## v1.9.2 — 20 November · Parking exit sketch added
- **`obstacle_go_out_of_parking`** (`src/obstacle-challenge/obstacle_go_out_of_parking/`): leaves the parking space with the two side ToF sensors, a 45° gyro turn toward the open side, a countersteer to 10° and a timed reverse on the encoder count. Listed in `src/README.md` §5. Not yet tested on the track; `REVERSE_TARGET_TICKS` must be measured first.

## v1.9.1 — 20 November · One camera viewer, `camera_test`
- **Only one viewer left:** `src/tools/camera_viewer/camera_viewer.html` is now the new page (pillars and floor lines, accepted-line fill, line guides, reason for each rejected line, automatic reconnect after a RESET of the camera, IGNORED-top slider set to 115). *Pillar Vision Lab* (`src/obstacle-challenge/pillar_viewer/`: `viewer.html` and `pillar_viewer.ino`) was removed; it stays in the history at commit `7442dd5`.
- **`camera_test` added** (`src/hardware-tests/camera_test/`): the same vision code as `obstacle_camera` (v3.1, `IGNORE_TOP_ROWS = 115`), for testing the camera with the viewer.
- README (§9.9, §12.5, tables) and `src/README.md` updated; links to the removed folder point to the history.

## v1.9 — 20 November · Documentation review against the rubric, Open Challenge code removed, camera viewer
- **Review against the *Future Engineers 2026 Documentation Rubric*** (five criteria, point by point). New table at the top of the README (*How this repository answers the documentation rubric*) says where each criterion is covered.
- **18 broken links repaired.** Links to sketches removed earlier (`servo_*`, `tof_*`, `drive_until_left_open`) and to the Open Challenge sketches now point to permalinks in the git history, in `README.md`, `src/README.md`, `models/README.md` and the journal. A script checks every relative link of every `.md` file: 0 broken.
- **Open Challenge: the seven earlier sketches replaced by one program, `open_challenge`** (`src/open-challenge/open_challenge/`): gyro heading hold, fixed distance to the inner wall, corner detection by a TOF jump, 90° turns, 12 turns and a stop at the start position measured with the encoder. Uses only the left and right TOF. Described in README §8.0 with a state diagram. The earlier sketches stay in the history (`git checkout f5aa0e6 -- src/open-challenge`) and `src/README.md` §4 links each one.
- **Hardware tests replaced by three:** `motor_encoder_test`, `servo_test`, `tof_and_gyro_test` (the first one translated to English). The previous tests (`rear_motor_loop_test`, `tof_servo_sensors_test`, `camera_webserver_emakefun`) are in the history at commit `685ccdc`.
- **Camera viewer added** (`src/tools/camera_viewer/camera_viewer.html`): the page used to test the camera board. Shows the picture, the pillar mask, the orange / blue line mask, the ignored rows, the status of each floor line and the RGB / HSV of any pixel. Reads both the short (25-byte) and the long (37-byte) packet of `obstacle_camera`. Documented in README §12.5 and `src/README.md` §6.
- **README:** Mermaid flowchart of one straight with pillars (§9.1); colour table of §9.2 now matches the thresholds in `obstacle_camera` (19 Nov, new camera); camera frame rate stated consistently (~12.5 fps); STBY described the same way in every file; module table (§7.2), status table (§13) and submission checklist (§14.1) updated.
- **Journal:** new section 17 (Phase 12, 29 Oct – 20 Nov: motor and camera failures, 4 Nov smoke, new encoder count, TOF problems, new camera set-up) and Problems 63–69 in the summary table.

## v1.8.3 — 20 November · Whole repository on the classic Arduino Nano
- **Every sketch that runs on the main controller targets the classic Arduino Nano (ATmega328P)**: no ESP32-only calls (`Serial.printf`, LEDC, native-USB waits, `D`-prefixed pins) remain in `src/`. The only non-Nano sketches are the three for the ESP32-S3-CAM vision board, which cannot move to a Nano (2 KB of RAM against 150 KB per picture); `src/README.md` now says so.
- `tof_servo_sensors_test`: translated to English (repository convention), added the I²C timeout used by the other sketches, and replaced the old Ackermann servo angles (10° / 90° / 170°) by ±25° around the rack centre (120°), so the test cannot push the servo against the end of the rack. It is now listed in the `src/README.md` table.
- `src/README.md`: Arduino IDE setting now requires Arduino AVR Boards 1.8.3 or newer (`Wire.setWireTimeout`).
- Main README (design decisions) and `schemes/README.md` describe the classic Nano as the controller; the Nano ESP32 only appears as history (journal Problems 6 and 31) and as the origin of the PCB footprint.
- `obstacle_camera`: colour thresholds of the orange and blue lines measured again on 19 Nov with the new camera (orange H 55–90, blue H 165–215); still to validate on the track.
- Vehicle photos replaced with the 20 Nov set (six views of the final car).

## v1.8.2 — 17 November · TOF sensors back on the 5 V rail
- **Power:** the four TOF400C sensors are powered from the +5 V buck rail again, not the Nano's 3.3 V pin. The classic Nano works with 5 V logic, so the sensors connect to it directly. Power budget recalculated (≈ 2.2 W typical, ≈ 10.5 W peak, ≈ 1.6 h per charge). The new failure point is servo peaks on the shared 5 V rail, to be checked with a multimeter during a servo stall.
- Journal section 9.4, the risk table in `schemes/README.md` §4.3 and the main README were updated to match.

## v1.8.1 — 11–12 November · TOF + servo test, old tests removed, viewer with line layers
- Added `tof_servo_sensors_test` (Nano): servo sweep while the two side TOF sensors print their distance.
- Removed the old sensor, gyro and servo test sketches (`tof_gyro_sensors_test`, `tof_left_right_print`, `tof_roi_budget_test`, `servo_simple`, `servo_only_test`, `servo_calibration`, `drive_until_left_open`); they stay in the history at commit `06f5685`.
- Pillar Vision Lab (`viewer.html`) updated with orange, blue and magenta layers.

## v1.8 — 5 November · Repository reorganisation
- **Sketches regrouped by purpose:** `src/hardware-tests/` now holds only component tests; the driving algorithms moved to the new `src/open-challenge/` folder (`wall_follow_nearest_pid`, `wall_keep_both_pid`, `wall_follow_nearest_gyro`, `wall_follow_gyro_corner_front`, `turn_left_90_pid_brake`, and the two renamed sketches below).
- **Descriptive names instead of nicknames:** `exito_glorioso` is now `corner_turn_front_trigger` and `exito_rotundo` is now `corner_turn_timed`. Code, headers, journal tables and links were updated; the behaviour is unchanged.
- `src/README.md` rewritten: system overview, Nano pin map, one table per folder with an explicit status column, IDE settings and conventions.
- Status symbols in the READMEs replaced by plain text (*Done*, *In progress*, *Planned*, *Pending*).

## v1.7.1 — 5 November · Sketches moved to the classic Arduino Nano
- `drive_until_left_open`, `rear_motor_loop_test` and `servo_simple` now target the **classic Arduino Nano (ATmega328P)**: numeric pins instead of `D6`/`D9` names, `Wire.begin()` without pins, no `Serial.printf`, and no wait for a native USB port.
- `src/README.md`: board column and Arduino IDE settings now list only the classic Nano and the ESP32-S3 CAM (the Nano ESP32 setup and `dfu-util` notes were removed). The journal keeps the history of the Nano ESP32 (Problems 6 and 31).

## v1.7 — 28 October · Floor lines without false detections
- **Camera v3.1** (`obstacle_camera`), validated on the car: no false line on the empty floor (3 places) or looking at the black wall; orange seen from **16 cm**, blue from **9 cm**; 12.5 fps.
- **Floor-above check:** a line must have white floor (V ≥ 130) above it in ≥ 60 % of its columns. The bottom edge of the black wall looks exactly like a far blue line, but it has dark wall above it and is now rejected.
- **Blue starts at hue 110°** (was 85°): the floor next to the car reaches H 85° (car shadow + lens vignetting) and became a false blue line of 3 000–10 000 px when the afternoon light changed.
- **Colour probe** in the status print: H, S, V of two fixed boxes, so colours are measured with the camera's own numbers instead of guessing from screenshots. Measured: floor H 60–85°, orange line H 32–44° S 145–173, red pillar H 4–22° S 204–255, blue line H up to ~165°.
- Tried and **reverted** (journal Problems 59–60): a 40/80-column least-squares centre row, joining line pieces across 12 px gaps (merged floor specks into a 6 000 px blob and dropped the camera to 4.5 fps), and joining aligned pieces. Lesson: change one thing at a time and re-test the empty floor after every change.
- README §9.2 and §9.6 updated; journal Phase 11 §13.6 with Problems 57–62.

## v1.6 — 27 October · Obstacle Challenge: pillars in centimetres, one straight validated, floor lines
- **One straight with pillars validated on the track** (`straight_pillars`, every case passes): odometry map (encoder + gyro), each pillar put on the map, lateral goal 17 cm on the correct side → heading → gyro PD. Replaces the "correct third" plan, because a pillar leaves the picture 15 cm before the car passes it.
- **Camera v3** (`obstacle_camera`): camera locked after 5 s, 4 × 4 blocks, time filter, red / magenta vote, pillar distance in cm (`FOCAL_PX = 100`, two faces, 12 px centre offset), orange and blue lines with a long-and-thin test, a least-squares centre row, and `BLUE_MAX_SAT = 120` so the green pillar is never a blue line. 14-field message (the first 10 unchanged).
- **Measured:** encoder 15.1 ticks / cm (the old 19.6 made the map 23 % short); line row → distance `276.2 / (row − 129.2) − 1.8` (error ≤ 1 cm); the low camera sees the lines only up to ~22 cm.
- Added `line_test` (Nano, motor off). `straight_pillars` reads up to 80 characters per message so the longer v3 message never overflows.
- README **§9 rewritten** with every formula and a worked example (pinhole model, odometry map, steering law, row → cm, corner geometry), the roadmap to the full Obstacle Challenge with its points (§9.10), new decisions and risks (§10.2–10.3), status (§13). Journal **Phase 11** (section 13) with Problems 47–58.

## v1.5 — 25 October (night) · Next Open Challenge algorithm: decision tree with encoder delays
- README **§8.7**: full decision tree of the next version (first straight on the nearest wall → opening fixes the direction → N encoder turns → 90° turn → outer wall until the inner wall appears → loop on the inner wall with N / M encoder delays → 12 turns, X encoder turns, active brake), with the reason for every element.
- Journal **§12.10**: why not reinforcement learning (no simulator, sim-to-real gap, rules can be written), the control methods considered, and why every timed wait becomes an encoder distance (USB vs battery difference).
- Next steps: measure encoder ticks per mm and the turning radius before coding.

## v1.4 — 25 October · Every setting justified
- README **§10.4 "Why each setting has its value"**: every Open Challenge setting with its value, reason and evidence (measurement, field geometry or rule).
- Journal **§12.8**: settings that had no justification or were proved wrong by the data, and what replaced them (12 turns instead of 13; target ≈ 250 mm instead of 600 mm; thresholds tuned on floor reflections removed; one-reading decisions replaced by an 8-reading window; gyro calibration unified to 1000 samples).
- Fixed outdated descriptions: TOF FRONT / BACK roles (safety brake and first-turn check), side sensors straight at ~5 cm, gyro calibration, speed reasoning (50 ms readings, ≤ 0.4 m/s while deciding), encoder wired but not used yet (with the reason and plan), Long vs Short TOF mode.

## v1.3 — 25 October · Rules compliance review
- Re-read the full *WRO 2026 Future Engineers General Rules* (15 Jan 2026) and Appendix C, and checked the repository against them.
- README: corrected the challenge description (corridors 1000 / 600 mm ± 100 mm, 3-minute rounds, no touching the outer wall in the Open Challenge, parking lot rules); new **§3.1 Rules compliance** table (size, mass, drive, steering, no wireless, start procedure, no data entry, reverse driving, stop in the starting section); new **§14.1 Submission checklist** with the rule 7 deadlines counted from the International Final (8 Dec 2026).
- Open Challenge: the competition number of turns is **12** (a 13th turn would leave the starting section, rule 9.24.3); edge cases for the 600 mm corridor and the outer wall added.
- `video/video.md`: one video per challenge (≥ 30 s autonomous) listed as required; Open and Obstacle videos pending.
- `camera_webserver_emakefun` marked as bench test only (Wi-Fi not allowed in rounds, rule 11.10).

## v1.2 — 25 October · The side sensors could not see the black wall → simpler, safer algorithm
- **One complete corner validated** (22–23 Oct, "works excellent"): turn side by opening or by comparing **valid** readings only, 90° gyro turn, FRONT/BACK check that undoes a wrong first turn, then `sideWallRun()` (find the inner wall → deviate to the target → precise P+I+D) and brake when the inner wall ends.
- **Open Challenge loop** (24 Oct): 13 turns with absolute headings, final 1 s straight and active brake. The car got closer to the inner wall every lap (gyro error ~1.5° per turn > the ±8° the wall PID can correct after ~6 turns). Fixed by using **the wall as the heading reference every straight** (`REALIGN_WITH_WALL`), plus an integral zone, asymmetric gains, a danger zone and a filtered D term.
- **Root cause of the wrong-side decisions found with data** (24–25 Oct, `tof_roi_budget_test`): with the side TOF angled 30° and ROI 1 × 6, the black wall at 30, 40, 50 and 60 cm and **no wall** all read ~2 m ("signal fail"). Control test: a white sheet on the wall at the same angle read 34–36 cm, all valid. Sensor **straight**: black wall at 30 cm → 32.0 cm, 30/30 valid. ROI **16 × 8**: 38.1 cm at 40 cm (30/30), 8/30 valid at 60 cm → reliable range ~45 cm.
- **Decision:** side sensors straight (0°) at ~5 cm from the floor, ROI 16 × 8, only "range valid" readings. The algorithm becomes simpler: WALL / NO WALL / NOT SURE from a window of 8 readings replaces the 2600 mm opening, 165 cm close/far and 500 mm jump thresholds.
- **Safety first:** NOT SURE → stop and measure again; decisions with the car still; FRONT safety in every phase; timeouts; lower speed during decisions. Goal: finish the 3 laps safely before going faster.
- Added `tof_left_right_print` and `tof_roi_budget_test`. Journal: Phases 9 and 10 (sections 11–12), Problems 40–46; journal §8.2 marked as overturned. README §6.3, §8, §10 and §13 updated.

## v1.1 — 21 October (night) · Timed corner, no front sensor
- **Validated on the track:** `corner_turn_timed`.
- **Straighter driving:** an integral was added to the gyro heading loop (`KI_HEADING = 2.5`, max 10° of steering). It cancels a constant steering offset (servo centre not exact), which made the car drift slowly to the left.
- **Smooth wall switch:** the car keeps the last `REF_HISTORY = 5` wall readings of each side. When the opening is detected (side > 2400 mm, 3 valid readings in a row) the PID follows the opposite wall at the **average of its last 5 readings before the opening**, instead of the distance at that instant, so it no longer drifts when it switches wall.
- **Corner by time instead of the front sensor:** after `RUN_MS = 1800 ms` the car brakes. If an opening was seen, it turns 90° toward it (the opening always wins, it is never re-evaluated). If not, it averages `COMPARE_SAMPLES = 10` readings per side with the car still and turns toward the farther side (invalid reading = no wall).
- Why: comparing the walls with a single reading sometimes chose the wrong side; averaging with the car stopped and trusting the detected opening first fixed it.
- Settings: `MOTOR_SPEED = 100`, `MAX_HEADING_DEG = 5`, turn at `TURN_SPEED = 60` / `SLOW_SPEED = 35`.

## v1.0 — 21 October · First complete straight and corner
- **Validated on the track:** `corner_turn_front_trigger` never hit a wall and always chose the correct turn side.
- Flow: gyro wall following on the nearest wall → a side TOF above `SIDE_OPEN_MM = 2400` (3 valid readings) marks the opening and switches the PID to the opposite wall without braking → the front TOF below `FRONT_STOP_MM = 1400` starts the validated 90° turn (`turn_left_90_pid_brake`, now left or right) toward the opening → active brake.
- Fixes from today's tests: the front TOF sometimes missed the wall because readings with `SigmaFail` and similar statuses were rejected → the front sensor now accepts **any** reading with a distance (40–4000 mm). If no opening was detected, the turn side is chosen by comparing both side walls (the farther one, invalid = no wall).
- Safety: after `MAX_RUN_MS = 40 s` it compares both sides and turns anyway; LED D13 shows when the front TOF is not giving a usable reading (diagnosis without a USB cable).
- Settings: `MOTOR_SPEED = 50`, `MAX_HEADING_DEG = 5`, `KI_DIST = 0.04`, turn at `TURN_SPEED = 40` / `SLOW_SPEED = 35`, `TURN_STEER_DEG = 60`.

## v0.9 — 20 October · Corner detection with the front sensor (in testing)
- Added `wall_follow_gyro_corner_front`: the side TOF sensors only **detect** the corner (reading > 3000 mm, 3 valid readings in a row → `cornerDetected = true`, turn side saved); the **front** TOF decides **when** to brake (front < `FRONT_STOP_MM`, 2 valid readings in a row). After the corner is detected the car stops following the wall and drives straight with the gyroscope.
- Why: the side sensors are angled about 30° forward, so they see the opening L × tan 30° ≈ 0.58 × L before the wall really ends (L = distance to the wall). Braking or turning on the side reading alone would happen at a different place each time.
- Setbacks found while building it: (1) treating invalid TOF readings (-1) as "open" stopped the car at the start even with an impossible threshold → invalid readings are now ignored; (2) a counter set to 100000 overflowed the Nano's 16-bit `int` (max 32 767) and became negative, so the car stopped immediately; (3) side readings above the opening threshold no longer enter the distance PID.
- Current test values: `OPEN_THRESHOLD_MM = 3000`, `FRONT_STOP_MM = 4000`, `MAX_VALID_MM = 4000`. Tested sensor: `front_tof_test`-style raw readout (prints the exact `range_mm`).

## v0.8 — 20 October · Smooth wall following with the gyroscope
- **Validated on the track:** `wall_follow_nearest_gyro` keeps the distance to the nearest wall straight and smooth at `MOTOR_SPEED = 60`.
- Two-loop controller: the side TOF gives a small target heading (max 8°), the BMI160 gyroscope steers to it about 200 times per second.
- Iterations documented: zigzag (KP too high) → too slow (too much smoothing) → S-shaped waves (a distance sensor cannot measure heading) → late correction (delay) → constant offset (KI) → gyroscope.
- TOF timing budget 33 ms and non-blocking reads (`dataReady()`); Serial output limited to 10 Hz.
- Journal: Phase 8 (section 10), Problems 35–39.

## v0.7 — 18 October · Chassis v3, 3D models and tyres
- **3D models published:** 25 STL files of every printed part in `models/stl/` with a render of each part, the STEP assembly link, the CAD of chassis v1, v2 and v3, printing notes and the assembly order (`models/README.md`).
- **Steering iteration documented:** v1 Ackermann → v2 Ackermann with a steering rack → v3 parallel **rack and pinion** (12-tooth pinion, module 1 mm). Reason: the Ackermann linkage damaged three servos (journal Problem 33).
- **Custom silicone tyres:** casting process with C25 silicone in printed moulds, formula B = ¾ A (30 g → 23 drops), 5–6 h cure (`models/README.md §4`, `other/tyres/`).
- **Six official vehicle views** of the final car plus all parts laid out (`v-photos/`). Size 13 × 9 × 6 cm, mass 248 g.
- **Power:** TOF sensors now powered from the Nano's 3.3 V pin; power budget recalculated (≈ 2.5 W typical, ≈ 11.0 W peak, ≈ 1.4 h per charge) and new failure points (3.3 V pin current, 5 V UART into the 3.3 V camera).
- **Corrections:** side TOF sensors raised 1.5 cm (not ~2 cm); main controller is the classic Arduino Nano everywhere in the docs; mechanical section rewritten with torque reasoning, 1 : 1 differential, gear geometry and the Ackermann vs parallel trade-off.
- Team and car names explained (Team X67, *Enner Valencia*). Build-process photos, soldered PCB and retired components added (`other/`, `schemes/`).
- Journal: Phase 7 (section 9), Problems 33–34; Problems 17 and 29 closed.

## v0.6 — 15 October · Pinion-and-rack steering on the classic Nano
- Steering changed from Ackermann to a parallel pinion-and-rack mechanism driven by the MG90 servo. Provisional values: centre 120°, left limit 180°, right limit 50° (left is now the larger angle).
- Main board is now a classic Arduino Nano (ATmega328P) after the Nano ESP32 was damaged.
- Added `servo_only_test` (steering sweep), `wall_follow_nearest_pid` (keep the distance to the nearest wall, 10-reading reference) and `wall_keep_both_pid` (keep both start distances).
- Tested settings on the new chassis: `MOTOR_REVERSED = true`, `INVERT_STEER = true`, `MAX_VALID_MM = 2000`.

## v0.5 — 7 October · Final strategy
- Open Challenge: distance references measured once at the start and kept with a PD controller (outer wall after the first corner); corners by sudden jump with the TOF ROI 1 × 6; 90° turns by gyroscope; after the 12th corner a short final straight and active brake.
- Obstacle Challenge: steer until the closest pillar is in the correct third of the image; corners detected by the floor lines with a 90° gyro turn and a corner counter. Parking still being designed.
- Journal §8.5 records what changed and why.

## v0.4 — 7 October · Complete documentation
- Merged the electrical documentation of the team into this repository: PCB schematic, wiring diagram, PCB photos, PCB manufacturing process, datasheets and component pictures (`schemes/`, `other/`).
- Added a power budget with run-time and discharge-rate analysis, and a failure-point table.
- Corrected the pin map: TB6612FNG STBY is tied to +5 V on the PCB; encoder C1/C2 on D3/D2; camera UART on D0/D1.
- Added team photos, vehicle photos, competition photos and the video link (`t-photos/`, `v-photos/`, `video/`).
- Rewrote the README following the five criteria of the documentation rubric: mobility, power and sensors, software, systems thinking and reproducibility. Added a state-machine diagram, a subsystem diagram, a "why X instead of Y" table, a risk table and testing metrics.

## v0.3 — 5 October · Chassis 2
- First tests on the second chassis: TOF ROI chosen by experiment (ROI 1 × 6), side sensors raised ~2 cm.
- Added `servo_calibration` and the first driving test `drive_until_left_open`.
- Added the WRO 2026 Scenario Roulette web tool.
- Journal: Problems 29–32.

## v0.2 — 4 October · Vision
- Pillar detection finished on the ESP32-S3-CAM (HSV + BFS blobs, closest pillar, correct-side check).
- Pillar Vision Lab web tool for live calibration.
- Repository restructured following the WRO Future Engineers template.

## v0.1 — 1–3 October · TOF sensors and gyroscope
- Sharp infrared sensors replaced by 4 × TOF400C (VL53L1X) with unique I²C addresses.
- Own BMI160 register-level driver with yaw integration.
- New Open Challenge state machine (WAITING → STRAIGHT → TURNING → FINISHED).

## v0.0 — 15–22 September · First algorithm
- Turning-direction logic with Sharp sensors, developed with hand tests while the chassis was being built.
- Motor driver and steering servo tests.
- First version of the custom PCB designed in EasyEDA and manufactured by JLCPCB (schematic rev 1.0, 8 July).
