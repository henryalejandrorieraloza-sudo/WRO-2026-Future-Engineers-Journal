# camera_test (`camera_test.ino`)

Hardware test for the **ESP32-S3-CAM** on its own. The code is the v3.1 vision program at **320 × 240** (the earlier `obstacle_camera` sketch, now in the git history). It keeps `STREAM_TO_PC = true`, so the pictures go to the computer and you can check the colour detection without the Nano, in [`camera_viewer.html`](../../tools/camera_viewer/camera_viewer.html), which is drawn for 320 × 240.

**The pipeline is explained stage by stage in [`obstacle_camera_v3_2/README.md`](../../obstacle-challenge/obstacle_camera_v3_2/README.md)** (colour classes, pillar blocks, the magenta wall rule, the time filter, the distance model, the floor-line checks, the colour probe and the message format). The competition sketch `obstacle_camera_v3_2` runs the same stages at 160 × 120. This README lists only what is different in the test.

## What to do with it

1. Close the Arduino Serial Monitor (it blocks the USB port).
2. Power the camera on over the empty floor and wait 5 seconds, so the colours lock.
3. Open `src/tools/camera_viewer/camera_viewer.html` in Chrome or Edge and press **Connect camera**.
4. Put a red pillar, a green pillar, an orange line and a blue line in front of the camera. Check the masks in the viewer.
5. To read numbers instead, set `STREAM_TO_PC = false` and open the Serial Monitor. The status shows the colour probe values (`PROBE near`, `PROBE bottom`).

## Differences from the obstacle sketch

| Item | `camera_test` | `obstacle_camera_v3_2` (competition) |
|---|---|---|
| Resolution | 320 × 240 | 160 × 120 |
| `STREAM_TO_PC` | `true` (viewer on) | `false` (status lines on the Serial Monitor) |
| `MAX_AHEAD_CM` (farthest pillar sent) | 130 cm | 40 cm |
| `MIRROR_HORIZONTAL` | `true` (camera of October) | `false` (camera of 19 Nov) |
| Pixel constants (rows, pixel counts) | 320 × 240 values | halved / quartered |
| Purpose | check colours and masks in the viewer | run on the car |

Pins, HSV colour ranges and the 14-field message format are the same. If the board is mounted like the 19 Nov camera, set `MIRROR_HORIZONTAL = false` here too before trusting the side of a pillar.

## Notes

- Colours change when the camera is not locked. Wait the 5 seconds before you calibrate.
- Use `camera_viewer.html` when you need pictures. The Serial Monitor only shows numbers while `STREAM_TO_PC = false`.
- The Nano is not needed for this test, but the code still writes its messages on the Nano serial port (D0 line). Leave it unconnected or ignore them.
