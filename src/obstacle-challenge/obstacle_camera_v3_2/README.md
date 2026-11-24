# Obstacle camera v3.2, half resolution (`obstacle_camera_v3_2.ino`)

Sketch for the **ESP32-S3-CAM** (emakefun board, OV2640 camera) that looks at the track and tells the Arduino Nano where the **pillars** and the **floor lines** are. It is the vision half of the Obstacle Challenge. The driving half, [`obstacle_challenge`](../obstacle_challenge/README.md), reads its messages (the first 12 of the 14 fields).

**Status: used by `obstacle_challenge` since 23 Nov 2026, not yet validated on the track with these constants.** Every pixel-count, row and column constant was scaled from the 320 x 240 calibration as a starting guess. Re-check them with `camera_viewer.html` before a long run. The previous 320 x 240 version (v3.1, folder `obstacle_camera`) is no longer in `src/`; it is in the git history.

## What changed from v3.1 (`obstacle_camera`, in the git history)

| Item | v3.1 (`obstacle_camera`) | v3.2 (this sketch) |
|---|---|---|
| Resolution | QVGA 320 x 240 | **QQVGA 160 x 120** (about 4 times fewer pixels per picture) |
| Reason | colour and shape checks run on every pixel | higher frame rate (fps is the main limit of the car) |
| Pixel constants | calibrated at 320 x 240 | halved or quartered as a **starting guess** (each one is commented in the sketch) |
| Colour ranges (HSV) | 19 Nov values | unchanged |
| WiFi live view | on by default | **not in this sketch** (same vision code, no WiFi) |
| Message to the Nano | 14 fields | 14 fields, same format |
| Farthest pillar used (`MAX_AHEAD_CM`) | 130 cm (value in `camera_test.ino`) | **40 cm** |

Each constant in the sketch has a comment with its old value, for example `HALVED from 115`, `QUARTERED from 250`, or `UNCHANGED (colour, not resolution)`.

## Hardware

| Part | Connection |
|---|---|
| Camera | ESP32-S3-CAM, **QQVGA 160 x 120**, RGB565, PSRAM frame buffers (2 buffers, newest frame only) |
| Link to the Nano | **GPIO43 (TX)** to the Nano **D0 (RX)**, 115200 baud |
| USB | Serial Monitor and `camera_viewer.html` (only with `STREAM_TO_PC = true`) |

The camera never listens to the Nano, so `NANO_RX_PIN` is `-1`.

**Board settings (Arduino IDE):** ESP32S3 Dev Module, USB CDC On Boot: Enabled, Flash 8 MB, Partition 8M with SPIFFS (3 MB APP / 1.5 MB SPIFFS), PSRAM: OPI PSRAM.

**Upload:** press BOOT and RESET on the camera.

**Start-up:** power the camera while it looks at the **empty floor**. The camera settings are frozen `LOCK_AFTER_MS` (5 s) after power-on, so the colours stay the same during the run. Calibrate the colours after the lock, not before.

## The message to the Nano

One line for every picture, starting with `$`:

```
$color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
```

| Field | Meaning |
|---|---|
| `color` | 0 none, 1 red, 2 green |
| `x` | Pillar centre in pixels from the middle of the picture, −80 (left) to +79 (right) at 160 x 120 |
| `bottom` | Lowest row of the pillar (bigger = closer) |
| `correct` | 1 if the pillar centre is at least `SAFE_LAT_CM` (17 cm) on its correct side (red: left, green: right) |
| `blueFound`, `blueRow` | Blue line found (0/1) and its lowest row (−1 = none) |
| `seq` | Message counter |
| `width` | Pillar width in pixels |
| `distCm` | Distance ahead of the camera in cm (−1 = no pillar) |
| `latCm` | Sideways distance of the pillar centre in cm (+ = right, − = left) |
| `orangeFound`, `orangeRow` | Orange line found and its lowest row |
| `blueCenterRow`, `orangeCenterRow` | Row of the line straight ahead (−1 = not in the centre strip) |

The first 10 fields are the same as in v2 and v3.1, so the old Nano sketches still read them.

## How it works

### 1. Colours

Each pixel is converted from RGB565 to RGB and then to HSV (hue 0–359, saturation and value 0–255). Each colour has a range in the `RED_*`, `GREEN_*`, `MAGENTA_*`, `ORANGE_*` and `BLUE_*` constants. The test order matters: red, green, magenta, orange, then blue. The first test that matches wins.

### 2. Pillars

Two methods exist, chosen with `USE_BLOCKS`:

- **Blocks (default, `USE_BLOCKS = true`):** the pillar band is split into 2 x 2 pixel blocks. A block is red or green when at least `BLOCK_MIN_PIXELS` of its pixels match. Connected blocks form a patch.
- **Pixels (`USE_BLOCKS = false`):** a flood fill over the single pixels.

Either way the sketch then:

1. Keeps only the **front** pillar of each patch (the columns whose bottom is within `BOTTOM_TOL` of the lowest one).
2. Runs the **plausibility** checks (size, distance between `MIN_AHEAD_CM` (5 cm) and `MAX_AHEAD_CM` (40 cm), side within `MAX_SIDE_CM` (70 cm), solid shape with `MIN_FILL_PERCENT`, and no touching the picture edge). A pillar farther than 40 cm is not sent.
3. Keeps the closest valid patch (the lowest bottom row wins).

### 3. Magenta parking walls

Red and magenta pixels that touch form one patch. If at least `MAGENTA_MAJORITY` % of the patch is magenta, the whole patch is a parking wall and is never reported as a pillar.

### 4. Time filter

With `USE_TIME_FILTER = true`, a pillar is confirmed when `ON_NEED` of the last `HISTORY` pictures show it. It is dropped after `OFF_NEED` empty pictures. While it is confirmed but missing in one picture, the last real detection is repeated.

### 5. Floor lines (orange and blue)

For each colour:

1. Find the **biggest patch** of that colour below `LINE_TOP_ROW` (the closest part of the floor).
2. Check it is **long** (`LINE_MIN_LENGTH`) and **thin** (average thickness at most `LINE_MAX_THICK`). A pillar or a blob is short and fat.
3. Check there is **white floor above it** (`checkFloorAbove`): in at least `ABOVE_MIN_PERCENT` % of its columns, the pixels just above the line must be bright (`FLOOR_MIN_VAL`). The bottom edge of the black wall has dark wall above it, so it is rejected.
4. Measure the **lowest row** of the line, and the **row straight ahead** inside the centre strip (`LINE_CENTER_HALF`).

The sketch also prints the reason when a line is rejected (`few pixels`, `too short`, `too thick`, `wall above`).

### 6. Distance and side

The pillar position is computed with a **pinhole model**: `distance = FOCAL_PX × seenCm / width`, where `seenCm` is the pillar width seen from the angle (two faces). The side distance comes from similar triangles. `FOCAL_PX` = 50 is **not measured again** for QQVGA: it was halved from 100 and must be checked on the track.

## Settings to check first on the track

| Constant | Why check it |
|---|---|
| `FOCAL_PX` (50) | Half of the 100 px measured at 320 × 240; confirmed by the team on 23 Nov 2026. It sets every distance in cm |
| `X_CENTER_OFFSET` (6) | Halved from 12. Shows where a pillar straight ahead appears in the picture |
| `BLOCK_MIN_PIXELS`, `MIN_BLOCKS`, `RED_MIN_PIXELS`, `GREEN_MIN_PIXELS` | Smaller pixel counts at QQVGA. Too low = noise pillars, too high = pillars missed far away |
| `LINE_TOP_ROW`, `ORANGE_MIN_PIXELS`, `BLUE_MIN_PIXELS` | The lines are fewer pixels at QQVGA |
| `ABOVE_FROM`, `ABOVE_TO` | The check looks at fewer rows above the line |
| `LOCK_AFTER_MS`, colour ranges | Calibrate again after the lock, under the track lights |

## Debug output (Serial Monitor, every 0.5 s)

With `STREAM_TO_PC = false` the sketch prints lines like this (example format, the values are illustrative):

```
status: camera 12.4 fps (locked) | pillar RED 42 cm ahead, 17 cm side
  ORANGE yes | center 205 | low 218 | len 42 | thick 4 | px 160 | above 12/20
  BLUE   no (wall above) | center -1 | low -1 | len 0 | thick 0 | px 0 | above 0/0
  PROBE near (x 86, y 85): H 60-66  S 90-130  V 110-170
  PROBE bottom (x 86, y 114): H ...
```

- `fps`: frames per second measured over one second.
- `above a/b`: columns with floor above the line, out of the columns that could be checked.
- **`PROBE`**: the H, S, V range of two small boxes (at the straight-ahead column, near the car and at the bottom). Put a line or a pillar under the box to read its exact colour numbers, then update the ranges.

## How to use

1. Upload `obstacle_camera_v3_2` with the board settings above.
2. Power the camera while it looks at the empty floor, and wait 5 s for the lock.
3. Optional: set `STREAM_TO_PC = true`, close the Serial Monitor, and open `camera_viewer.html` in Chrome or Edge to check the masks and the line rows.
4. Power the Nano and the camera together. When `obstacle_challenge` runs, its status line shows the pillar (`pillar R` or `G`) and the line counts (`line B/O`) as soon as messages from this sketch arrive.

## Limits

- **Not validated on the track.** The 320 x 240 tuning does not carry over exactly to 160 x 120.
- The camera fps increases, but each pixel covers more real distance. At 20 cm a 5 cm pillar is about 12 pixels wide here, against about 25 at 320 x 240, so distance estimates are coarser.
- No WiFi stream in this version. Use the USB viewer. The WiFi live view of v3.1 is in the git history (`obstacle_camera`), and it is off in the competition build (rule 11.6: no radio during rounds).
- The sketch keeps its own memory in PSRAM. If the camera does not start, check `Tools → PSRAM: OPI PSRAM` and press RESET.
