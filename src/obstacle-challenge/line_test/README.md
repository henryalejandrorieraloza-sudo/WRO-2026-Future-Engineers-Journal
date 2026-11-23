# line_test

Measurement sketch for the **camera's floor-line results** on the Arduino Nano. The motor stays off and you move the car by hand over the orange and blue lines. The sketch prints, five times per second, what the camera sees.

It was used on 27 October 2026 to measure the table *row → distance* of the floor lines.

## What it does
1. Reads the messages from the camera (`$...` lines) on the UART (`D0`, 115200 baud).
2. Keeps the last values of the pillar and of both lines.
3. Every 200 ms prints one status line:

```
ORANGE YES low 218 center 205 (12/20)  |  BLUE no (3/20)  |  pillar RED 42 cm
```

- `YES` / `no`: whether the line was in the last message.
- `low`: lowest row of the line in the picture (bigger = closer to the car).
- `center`: row of the line straight ahead, or `-` if it is not in the center strip.
- `(12/20)`: in how many of the last 20 messages the line was seen.

The motor driver pins are set to stop the motor, and the steering servo is centered at 120° (an old provisional value; the measured centre of this chassis is 85, see `straight_pillars/PARAMETERS.md`).

## Message format it expects
The camera sends 14 fields (see `obstacle_camera`, version 3 or later):
```
$color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
```
The old 10-field message is recognised. In that case the sketch tells you to upload the newer camera code.

If no valid message arrives for 500 ms, the line `NO CAMERA` is printed.

## Wiring
| Signal | Nano pin |
|---|---|
| From the camera (TX GPIO43 → RX) | D0 |
| Steering servo | D9 |
| Motor PWMA / AIN1 / AIN2 | D6 / D7 / D8 (kept off) |

## How to use
1. Upload the sketch to the Nano. **Hold the camera's RESET button during the whole upload**, because the camera's serial line on D0 blocks the upload.
2. Power the camera with its own code running (`obstacle_camera`).
3. Open the Serial Monitor at 115200 baud and move the car by hand over the orange and blue lines at different distances.
4. Note the `low` value for each distance. That gives the row-to-distance table of the floor lines.
5. Check that the `(n/20)` counters go up when the line is under the camera, and stay low elsewhere.

## Notes
- Motor off and wheels straight all the time. The servo only moves to the center.
- The counters are reset every print period, so the numbers show the recent stability, not the total.
