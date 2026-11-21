# camera_test (`camera_test.ino`)

Hardware test for the **ESP32-S3-CAM** on its own. The code is the same vision program as `src/obstacle-challenge/obstacle_camera/obstacle_camera.ino` (v3.1). This copy keeps `STREAM_TO_PC = true`, so the pictures go to the computer and you can check the colour detection without the Nano.

**Read the full explanation in `../../obstacle-challenge/obstacle_camera/README.md`.** It describes every stage: colour classes, pillar blocks, the magenta wall rule, the time filter, the distance model, the floor-line checks, the colour probe and the message format. This README lists only what is different in the test.

## What to do with it

1. Close the Arduino Serial Monitor (it blocks the USB port).
2. Power the camera on over the empty floor and wait 5 seconds, so the colours lock.
3. Open `src/tools/camera_viewer/camera_viewer.html` in Chrome or Edge and press **Connect camera**.
4. Put a red pillar, a green pillar, an orange line and a blue line in front of the camera. Check the masks in the viewer.
5. To read numbers instead, set `STREAM_TO_PC = false` and open the Serial Monitor. The status shows the colour probe values (`PROBE near`, `PROBE bottom`).

## Differences from the obstacle sketch

| Item | `camera_test` | `obstacle_camera` |
|---|---|---|
| `STREAM_TO_PC` | `true` (viewer on) | `true` in the repo, set to `false` to drive |
| Purpose | check colours and masks | run on the car |

Everything else (pins, thresholds, message format) is the same, so a change made in one should be copied to the other.

## Notes

- Colours change when the camera is not locked. Wait the 5 seconds before you calibrate.
- Use `viewer.html` when you need pictures. The Serial Monitor only shows numbers while `STREAM_TO_PC = false`.
- The Nano is not needed for this test, but the code still writes its messages on the Nano serial port (D0 line). Leave it unconnected or ignore them.
