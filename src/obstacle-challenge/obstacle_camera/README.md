# Obstacle camera (`obstacle_camera.ino`)

Sketch for the **ESP32-S3-CAM** (emakefun board, OV2640 camera) that looks at the track and tells the Arduino Nano where the **pillars** and the **floor lines** are. It is the vision half of the Obstacle Challenge: `straight_pillars.ino` on the Nano uses its messages to steer.

Every picture (about 10 per second) the camera:

1. Classifies each pixel by colour (red, green, magenta, orange, blue).
2. Finds the closest red or green **pillar**, ignoring magenta patches (the parking wall).
3. Finds the **orange** and **blue** floor lines, accepting only long, thin stripes with white floor above them.
4. Sends one text line to the Nano over UART.

## Hardware

| Part | Connection |
|---|---|
| Camera | ESP32-S3-CAM, QVGA 320 x 240, RGB565, PSRAM frame buffers |
| Link to the Nano | **GPIO43 (TX)** to the Nano **D0 (RX)**, 115200 baud |
| USB | Serial monitor / `viewer.html` (only with `STREAM_TO_PC`) |

The camera never listens to the Nano, so `NANO_RX_PIN` is `-1`.

**Board settings (Arduino IDE):** ESP32S3 Dev Module, USB CDC On Boot: Enabled, Flash 8 MB, Partition 8M with SPIFFS (3 MB APP / 1.5 MB SPIFFS), PSRAM: OPI PSRAM.

**Upload:** press BOOT + RESET on the camera. Hold the Nano's RESET while uploading, because both boards share the D0 line.

**Start-up:** power the camera while it looks at the empty floor. The camera settings are frozen 5 seconds after power-on, so the colours stay the same during the run.

## The message to the Nano

One line every picture, starting with `$`:

```
$color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
```

| Field | Meaning |
|---|---|
| `color` | 0 none, 1 red, 2 green |
| `x` | Pillar centre, −160 (left) to +159 (right), in pixels from the middle |
| `bottom` | Lowest row of the pillar (bigger = closer) |
| `correct` | 1 if the pillar centre is at least `SAFE_LAT_CM` (17 cm) on its correct side (red: left, green: right) |
| `blueFound`, `blueRow` | Blue line found (0/1) and its lowest row (−1 = none) |
| `seq` | Message counter |
| `width` | Pillar width in pixels |
| `distCm` | Distance ahead of the camera, cm (−1 = no pillar) |
| `latCm` | Sideways distance of the pillar centre, cm (+ right, − left) |
| `orangeFound`, `orangeRow` | Orange line found and its lowest row |
| `blueCenterRow`, `orangeCenterRow` | Row of the line straight ahead (−1 = not in the centre strip) |

The first 10 fields are the same as in the older version, so the old Nano sketches still read them.

## How it works

### 1. Colours

Each pixel is converted from RGB565 to RGB and then to HSV (hue 0–359, saturation and value 0–255). Each colour has a range in `RED_*`, `GREEN_*`, `MAGENTA_*`, `ORANGE_*` and `BLUE_*`. The test order matters: red, green, magenta, orange, then blue.

### 2. Pillars

Two methods exist, chosen with `USE_BLOCKS`:

- **Blocks (default):** the pillar band is split into 4 x 4 pixel blocks. A block is red or green when at least 12 of its 16 pixels have that colour. Blocks are grouped by flood fill. This removes single noise pixels and small holes.
- **Pixels (older method):** the same grouping is done pixel by pixel.

For each group the code keeps only the **front pillar** (the columns whose bottom is near the lowest one), then checks:

- The group has enough pixels and enough blocks.
- It does not touch the left or right edge of the picture (`REJECT_AT_EDGE`).
- It is solid enough (`MIN_FILL_PERCENT`, 75 %).
- Its distance is between 5 and 130 cm, and its side is within 70 cm (`USE_PLAUSIBILITY`).

The closest valid pillar wins: the one whose bottom is lowest in the picture.

### 3. Magenta wall

Red and magenta patches are merged by flood fill. If at least 70 % of a patch is magenta, the whole patch is the parking wall and it is not a pillar.

### 4. Time filter

A pillar is accepted only after `ON_NEED` (2) of the last `HISTORY` (4) pictures show it. It is dropped after `OFF_NEED` (3) empty pictures. While it is confirmed but missing from one picture, the last real detection is repeated. This avoids flicker.

### 5. Distance and side

The code uses a pinhole model with `FOCAL_PX = 100` (measured) and the real pillar width `PILLAR_CM = 5`:

- `distance = FOCAL_PX × seen width / width in pixels`, where the seen width includes the two faces of the pillar at an angle.
- `side = distance × (offset in pixels) / FOCAL_PX`.

`X_CENTER_OFFSET = 12` px corrects the fact that the camera is turned about 6°, so a pillar straight ahead does not show at the exact middle.

### 6. Floor lines

For **orange** and **blue** separately:

1. Take the biggest patch below `LINE_TOP_ROW` (140), walking only touching pixels.
2. It must be long (`LINE_MIN_LENGTH` 40 px, diagonal of its box) and thin (`LINE_MAX_THICK` 40 px average thickness). A pillar is short and fat, so it fails here.
3. **Floor-above check:** for each column, the 3–6 rows above the line must be bright (V ≥ 130) in at least 60 % of the columns. The bottom edge of the black wall has dark wall above it, so it fails here.
4. Record the lowest row of the line, and the lowest row inside the **centre strip** (± 10 px around the straight-ahead column).

Accepted line pixels are kept only for the viewer.

### 7. Colour probe

Every half second (only when not streaming) the status prints the H, S and V range of two small fixed boxes: one at row 170 (about 5 cm ahead) and one at row 228 (the bottom of the picture). Use them to set the colour ranges with real numbers.

### 8. Lock

`LOCK_CAMERA` freezes white balance, exposure and gain 5 seconds after power-on. The colour ranges were measured with the camera locked, so do not calibrate before the lock.

## Settings

| Constant | Default | Meaning |
|---|---|---|
| `STREAM_TO_PC` | true | Send pictures to `viewer.html` over USB. Set to `false` when driving, because the stream is slower |
| `WIFI_STREAM` | false | Serve a live MJPEG view over WiFi at `http://<ip>/`, no USB cable needed — see "Live view over WiFi" below. Off by default: turn it on only for bench testing, never for a timed run |
| `LOCK_CAMERA` / `LOCK_AFTER_MS` | true / 5000 | Freeze the camera settings and when |
| `USE_BLOCKS` | true | Block method for the pillars |
| `USE_TIME_FILTER` | true | Require several pictures before a pillar counts |
| `USE_PLAUSIBILITY`, `USE_SHAPE_FILTER` | true | Size, side and solidity checks |
| `LENS_CORRECTION` | true | Lens shading correction (removes the tint at the edges) |
| `FOCAL_PX` | 100 | Pinhole focal length in pixels (measured) |
| `PILLAR_CM` | 5 | Pillar width |
| `SAFE_LAT_CM` | 17 | Side distance needed for `correct = 1` |
| `X_CENTER_OFFSET` | 12 | Camera turn correction, pixels |
| `RED_*`, `GREEN_*` | see file | Colour ranges, measured with the camera locked |
| `ORANGE_*`, `BLUE_*` | see file | Line colour ranges, measured 19 Nov 2026 |
| `LINE_TOP_ROW` | 140 | Lines are searched from this row down (about 23 cm ahead) |
| `LINE_MIN_LENGTH`, `LINE_MAX_THICK` | 40, 40 | Long-and-thin test |
| `FLOOR_MIN_VAL`, `ABOVE_MIN_PERCENT` | 130, 60 | Floor-above test |
| `ORANGE_MIN_PIXELS`, `BLUE_MIN_PIXELS` | 50, 60 | Minimum line size |

The file contains a block marked "PASTE HERE", which is copied from `viewer.html`. If you paste again, keep `BLUE_HUE_LOW = 110`, `BLUE_MAX_SAT` and the line minimums, because the viewer does not have them.

## Serial output

With `STREAM_TO_PC = false`, every 0.5 seconds:

```
status: camera 10.2 fps (locked) | pillar RED 42 cm ahead, 5 cm side
  ORANGE yes | center 201 | low 214 | len 118 | thick 3 | px 210 | above 18/20
  BLUE   no (wall above) | center -1 | low -1 | ...
  PROBE near   (x 172, y 170): H 60-75  S 95-160  V 107-174
```

With `STREAM_TO_PC = true`, the same USB port carries binary picture packets (header, picture, pillar mask, line mask) for `viewer.html`. Use the viewer in that case, not the text monitor.

## How to use

1. Flash the camera with the settings above.
2. Power it on over the empty floor and wait 5 seconds (colours lock).
3. Connect the camera TX to the Nano RX (D0) and power the Nano.
4. To check the colours: set `STREAM_TO_PC = false` and read the `PROBE` lines, or open `viewer.html` with `STREAM_TO_PC = true`.
5. To drive, set `STREAM_TO_PC = false` (faster messages).

## Live view over WiFi

An independent, self-contained block (no changes to the pillar/line code): set `WIFI_STREAM = true`, fill in `WIFI_STREAM_SSID` / `WIFI_STREAM_PASS` with a real 2.4 GHz network (the ESP32-S3 cannot join a 5 GHz network), flash, and open the Serial Monitor once just to read the printed IP address (e.g. `http://192.168.1.42/`). Open that address in any phone or laptop browser on the same WiFi — no app, no extra page, it just plays as a live video (standard MJPEG stream). The car does not need to stay plugged into the computer: this is the whole point, so you can watch what the camera sees while it drives around the track.

- It reuses the exact frame already captured for the pillar/line detection — nothing is captured twice.
- With no browser connected, the cost is one cheap "anyone watching?" check per loop — effectively free.
- While someone *is* watching, every frame also gets JPEG-encoded and sent over WiFi, which takes real time (tens of ms) and lowers the frames/second reaching the Nano. Fine for calibration on the bench; turn it back to `false` before a timed run.
- Only one viewer at a time is served (simplest possible server, by design, to keep the extra code small and self-contained); opening a second browser tab just waits until the first one disconnects.

## Known limits

- Colours depend on the light. The ranges were measured under one lighting; a change of light means recalibrating with the probes.
- The distance model assumes the camera is level and the pillar is on the floor.
- The floor-above check depends on the white floor being bright (V ≥ 130). In a dark room the lines are rejected.
- Lines are only searched from row 140, so a line farther than about 23 cm is not seen.
- `TURN_CONFIRM_FRAMES`, `SIDE_ZONE`, `NEAR_ROWS` and `LINE_TRIGGER_ROW` are kept from older versions. They are declared but do not change the result in this version.
