# WRO code

Code for our WRO Future Engineers 2026 car. Each sketch is in its own folder with the same name, as Arduino requires: open it with **File → Open** and choose the `.ino` file.

## probar hardware (hardware tests)

| Sketch | Board | What it tests |
|---|---|---|
| `tof_sensors_test` | Nano ESP32 | The 4 TOF400C sensors at the same time (FRONT, BACK, LEFT, RIGHT in cm). Library: VL53L1X by Pololu |
| `gyro_yaw_test` | Nano ESP32 | BMI160 gyroscope: calibration and turning angle (yaw) |
| `rear_motor_loop_test` | Nano ESP32 | Rear motor: forward, stop, backward, stop |
| `servo_simple` | Nano ESP32 | Steering servo on D9 (left, centre, right). Measure the centre again with the new servo |
| `camera_webserver_emakefun` | ESP32S3-CAM | See the camera image in a browser over Wi-Fi (bench only). Needs the other files from the manufacturer's zip and a 2.4 GHz Wi-Fi network |

## obstacle challenge

| Sketch | Board | What it does |
|---|---|---|
| `color_detection_test` | ESP32S3-CAM | Detects the red and green cubes and prints where they are. Pending test and calibration |

## Tools settings

**Nano ESP32:** Board → Arduino ESP32 Boards → Arduino Nano ESP32.

**ESP32S3-CAM:**

| Option | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |
| PSRAM | OPI PSRAM |

If the upload stays at `Connecting...`: hold **BOOT**, press and release **RESET**, then release **BOOT**.
