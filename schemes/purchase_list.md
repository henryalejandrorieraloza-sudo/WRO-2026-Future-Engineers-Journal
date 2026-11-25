# Purchase list (parts list, Team X67)

This is the team's complete parts list for the car, Revision 27-09-2026, copied from [`materials_list_rev27-09-2026.pdf`](materials_list_rev27-09-2026.pdf). It lists everything bought or made for the car, with the source, the quantity bought, what was actually used and the cost as written in the list. The electronic components are explained one by one in [§1 of the schemes README](README.md#1-bill-of-materials-bom).

Note the difference between **Qty** (how many were bought) and what was used. Several electronic parts were bought as spares; the list says "Only 1 was used" when that is the case.

Costs are copied as written (decimal comma). The list does not state the currency, and the total is as printed.

## Electronics and power

| Item | Part | Description | Source | Qty bought | Usage | Cost |
|---|---|---|---|---|---|---|
| 1 | Arduino Nano | 8-bit 16 MHz ATmega328P microcontroller | Amazon.us | 1 | Sensor readings and motors (microcontroller) | 19,30 |
| 2 | ESP32-S3-CAM | Dual-core 240 MHz processor with vector instructions | Amazon.us | 2 | Only 1 was used (computer vision microcontroller) | 27,99 |
| 3 | OV2640 | 2-megapixel CMOS sensor, 160° view angle | Amazon.us | 2 | Only 1 was used (camera) | 21,99 |
| 4 | TOF400C | ToF distance sensor, 400 cm | Amazon.us | 4 | Wall detection on the 4 sides | 21,89 |
| 5 | BMI160 | 6-axis motion sensor | Amazon.us | 5 | Only 1 was used (yaw angle) | 9,99 |
| 6 | TB6612FNG | Dual 3.2 A motor driver | Amazon.us | 3 | Only 1 was used (motor control) | 7,82 |
| 7 | 3A Mini DC-DC Buck | 5 V voltage regulator | Amazon.us | 10 | Only 1 was used (5 V for each sensor and microcontroller) | 8,99 |
| 8 | MG90 Servo | Metal gear 9 g micro servo | Amazon.us | 5 | Only 1 was used (steering) | 15,49 |
| 9 | N20 1000 rpm motor | N20 1000 rpm 12 V motor with encoder | Amazon.us | 2 | Only 1 was used (car movement) | 23,99 |
| 10 | GNB 3S 380 mAh LiPo | 3S HV 11.4 V 380 mAh 90C LiPo | Evan Robotics | 1 | Battery | 18,00 |
| 11 | Start button | 2-pin push button | Already in possession | 1 | Starts the car program | 0,25 |
| 12 | Power switch | 2-pin power switch | Already in possession | 1 | Turns the car on and off | 0,25 |
| 13 | Custom PCB | Custom design, manufactured by JLCPCB | JlcPCB | 5 | Only 1 was used (connects all electrical components) | 80,00 |
| 44 | XT30 male | XT30 male connector | Already in possession | 1 | Battery to car connector | 1,00 |
| 45 | 24 AWG cable kit | 24 AWG silicone cable, multiple colours | Amazon.us | 1 | Electrical connections | 10,29 |

## Tyres

| Item | Part | Description | Source | Qty bought | Usage | Cost |
|---|---|---|---|---|---|---|
| 14 | Custom tyre | Custom design, made with a 3D-printed mould | DIY | 4 | Traction for the 3D-printed wheels | 0,00 |
| 15 | Silicone part A (C25) | C25 silicone rubber | Pintulac | 1 | Compound for the tyres | 19,86 |
| 16 | Silicone part B (C25) | C25 silicone rubber catalyst | Pintulac | 1 | Compound for the tyres | 3,44 |

## Filament

| Item | Part | Description | Source | Qty bought | Usage | Cost |
|---|---|---|---|---|---|---|
| 17 | Bambu Lab PLA Matte | Ivory white 1.75 mm PLA filament | Evan Robotics | 1 | Mechanical structure | 23,00 |

## 3D-printed parts (mechanical structure)

All of these are printed in PLA and cost 0,00 in the list (own printing).

| Item | File | Qty | Item | File | Qty |
|---|---|---|---|---|---|
| 18 | `base.stl` | 1 | 31 | `differential_support.stl` | 2 |
| 19 | `pcb_big_support_left.stl` | 1 | 32 | `differential_gear.stl` | 1 |
| 20 | `pcb_big_support_right.stl` | 1 | 33 | `differential_box.stl` | 1 |
| 21 | `pcb_front_support.stl` | 2 | 34 | `diff_bevel_gear.stl` | 4 |
| 22 | `front_spacer.stl` | 2 | 35 | `diff_big_axle.stl` | 2 |
| 23 | `rear_spacer.stl` | 2 | 36 | `diff_small_axle.stl` | 2 |
| 24 | `servo_bracket.stl` | 1 | 37 | `front_wheel.stl` | 2 |
| 25 | `steering_gear.stl` | 1 | 38 | `rear_wheel.stl` | 2 |
| 26 | `gear_rack.stl` | 1 | 39 | `front_sensor_bracket.stl` | 1 |
| 27 | `left_steering_knuckle.stl` | 1 | 40 | `rear_sensor_bracket.stl` | 1 |
| 28 | `right_steering_knuckle.stl` | 1 | 41 | `sensor_bracket.stl` | 1 |
| 29 | `motor_gear.stl` | 1 | 42 | `camera_bracket.stl` | 1 |
| 30 | `motor_restrain.stl` | 1 | 43 | (item 43 in the list, file name not given) | 1 |
| | | | 44 | `inner_wheel_mould.stl` (tyre mould) | 4 |
| | | | 45 | `outer_wheel_mould.stl` (tyre mould) | 4 |

Compared with `models/stl/`, the file names match except for two points. Item 28 is spelled `right_steering_kunckle.stl` in the list, and the repository uses `right_steering_knuckle.stl`. The repository also has `front_protrusion.stl`, which the list does not name. Item 43 in the list has no file name, so it may be that part. Check both against [`models/README.md`](../models/README.md) before printing.

## Fasteners and bearings

| Item | Part | Qty bought | Usage | Cost |
|---|---|---|---|---|
| 41 | M3 round-headed screw kit (1050 pieces) | 1 kit | Mechanical structure | 9,99 |
| 41.1 | M3 × 35 mm | 4 | Mechanical structure | |
| 41.2 | M3 × 12 mm | 8 | Mechanical structure | |
| 41.3 | M3 × 8 mm | 9 | Mechanical structure | |
| 41.4 | M3 × 10 mm | 6 | Mechanical structure | |
| 41.5 | M3 × 16 mm | 2 | Mechanical structure | |
| 41.6 | M3 nuts | 10 | Mechanical structure | |
| 42 | M2 cap-headed screw kit (900 pieces) | 1 kit | Mechanical structure | 6,90 |
| 42.1 | M2 × 4 mm | 1 | Mechanical structure | |
| 42.2 | M2 × 6 mm | 12 | Mechanical structure | |
| 42.3 | M2 × 12 mm | 4 | Mechanical structure | |
| 42.4 | M2 washers | 2 | Mechanical structure | |
| 42.5 | M2 nuts | 6 | Mechanical structure | |
| 43 | 684ZZ bearing, 4 × 9 × 4 mm | 20 | Only 9 were used | 9,99 |

## Totals

The list prints a total of **340,42** for all rows.
