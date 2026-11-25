"""Generate schemes/schematic.svg (and PNG via rsvg-convert) for the Team X67 car.

Rev 2, November 2026: classic Arduino Nano (ATmega328P), MG90 servo, four TOF400C
(VL53L1X) distance sensors and one BMI160 on a shared I2C bus.

Conventions
- Coloured wires are drawn only for point-to-point connections.
- Pins with the same net name (+5 V, GND, VBAT, SDA, SCL) are connected even when
  no wire is drawn between them (net labels, as in standard schematics).
- Every pin name and XSHUT address is printed next to the pin it belongs to.

Usage: python3 make_schematic.py  (writes schematic.svg next to this file)
"""
import os
import subprocess

W, H = 1440, 1000
COL = {
    "vbat": "#e67e22", "v5": "#c0392b", "gnd": "#2c3e50", "cam": "#16a085",
    "servo": "#8e44ad", "btn": "#7f8c8d", "drv": "#27ae60", "mot": "#a04000",
    "enc": "#2980b9", "i2c": "#1f618d", "xs": "#5d6d7e",
}
FONT = "DejaVu Sans, Arial, sans-serif"
out = []


def add(s):
    out.append(s)


def rect(x, y, w, h, rx=6, fill="#f4f7f9", stroke="#2c3e50", sw=2):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
        f'stroke="{stroke}" stroke-width="{sw}"/>')


def text(x, y, s, size=13, anchor="start", weight="normal", fill="#1b1b1b"):
    add(f'<text x="{x}" y="{y}" font-family="{FONT}" font-size="{size}" '
        f'text-anchor="{anchor}" font-weight="{weight}" fill="{fill}">{s}</text>')


def comp(x, y, w, h, title, sub=""):
    rect(x, y, w, h)
    text(x + w / 2, y + 24, title, 15, "middle", "bold")
    if sub:
        text(x + w / 2, y + 42, sub, 11, "middle", "normal", "#555")


def dot(x, y, c):
    add(f'<circle cx="{x}" cy="{y}" r="3.8" fill="{c}"/>')


def wire(pts, c, sw=2.6):
    d = "M " + " L ".join(f"{x} {y}" for x, y in pts)
    add(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="{sw}" '
        f'stroke-linejoin="round" stroke-linecap="round"/>')
    dot(*pts[0], c)
    dot(*pts[-1], c)


def tag(x, y, s, c, anchor="start", size=12):
    text(x, y, s, size, anchor, "bold", c)


def stub(x, y, dx, label, c, anchor=None):
    """Short wire from a pin to a net label placed at (x+dx, y)."""
    wire([(x, y), (x + dx, y)], c, 2)
    if anchor is None:
        anchor = "start" if dx > 0 else "end"
    off = 5 if dx > 0 else -5
    tag(x + dx + off, y + 4, label, c, anchor)


def pin_in(x, y, side, label, c):
    """Pin label printed inside a component next to its pin."""
    dot(x, y, c)
    if side == "L":
        text(x + 10, y + 4, label, 11, "start", "normal", "#333")
    else:
        text(x - 10, y + 4, label, 11, "end", "normal", "#333")


# ---------- title ----------
text(40, 42, "Team X67 · Enner Valencia — electrical schematic (rev 2, 24 Nov 2026)", 21, "start", "bold")
text(40, 64, "Classic Arduino Nano (ATmega328P) · MG90 servo · 4 × TOF400C (VL53L1X) · BMI160 · "
             "ESP32-S3-CAM · TB6612FNG · CN3903 buck", 13, "start", "normal", "#555")

# ---------- power ----------
comp(60, 120, 190, 100, "Battery", "3S LiPo 11.4 V · 380 mAh")
pin_in(250, 150, "R", "+", COL["vbat"])
pin_in(250, 190, "R", "−", COL["gnd"])
wire([(250, 150), (330, 150)], COL["vbat"])
wire([(250, 190), (280, 190)], COL["gnd"], 2)
tag(284, 194, "GND", COL["gnd"])

text(405, 110, "Power switch", 14, "middle", "bold")
rect(330, 120, 150, 60)
pin_in(330, 150, "L", "IN", COL["vbat"])
pin_in(480, 150, "R", "OUT", COL["vbat"])
wire([(480, 150), (500, 150), (500, 135), (520, 135)], COL["vbat"])

comp(520, 100, 220, 120, "CN3903 buck converter", "11.4 V → 5 V · 3 A")
pin_in(520, 135, "L", "IN+", COL["vbat"])
pin_in(520, 195, "L", "IN−", COL["gnd"])
pin_in(740, 135, "R", "OUT+", COL["v5"])
pin_in(740, 195, "R", "OUT−", COL["gnd"])
wire([(520, 195), (490, 195)], COL["gnd"], 2)
tag(486, 199, "GND", COL["gnd"], "end")
stub(740, 135, 30, "+5 V", COL["v5"])
stub(740, 195, 30, "GND", COL["gnd"])

# ---------- Arduino Nano ----------
comp(560, 300, 260, 460, "Arduino Nano", "classic · ATmega328P · 5 V logic")
# left side
pin_in(560, 330, "L", "VIN", COL["vbat"])
pin_in(560, 360, "L", "GND", COL["gnd"])
pin_in(560, 400, "L", "D0 RX", COL["cam"])
pin_in(560, 480, "L", "D9 SERVO", COL["servo"])
pin_in(560, 540, "L", "D10 START", COL["btn"])
pin_in(560, 580, "L", "A0 XSHUT", COL["xs"])
pin_in(560, 610, "L", "A1 XSHUT", COL["xs"])
pin_in(560, 640, "L", "A2 XSHUT", COL["xs"])
pin_in(560, 670, "L", "A3 XSHUT", COL["xs"])
pin_in(560, 700, "L", "A4 SDA", COL["i2c"])
pin_in(560, 730, "L", "A5 SCL", COL["i2c"])
wire([(560, 330), (530, 330)], COL["vbat"], 2)
tag(526, 334, "VBAT", COL["vbat"], "end")
wire([(560, 360), (530, 360)], COL["gnd"], 2)
tag(526, 364, "GND", COL["gnd"], "end")
# right side
pin_in(820, 320, "R", "D6 PWMA", COL["drv"])
pin_in(820, 350, "R", "D7 AIN1", COL["drv"])
pin_in(820, 380, "R", "D8 AIN2", COL["drv"])
pin_in(820, 430, "R", "D3 ENC C1", COL["enc"])
pin_in(820, 460, "R", "D2 ENC C2", COL["enc"])

# ---------- camera ----------
comp(60, 230, 200, 120, "ESP32-S3-CAM", "OV2640 camera")
pin_in(260, 270, "R", "TX", COL["cam"])
pin_in(260, 300, "R", "5V", COL["v5"])
pin_in(260, 320, "R", "GND", COL["gnd"])
wire([(260, 270), (400, 270), (400, 400), (560, 400)], COL["cam"])
stub(260, 300, 30, "+5 V", COL["v5"])
stub(260, 320, 30, "GND", COL["gnd"])

# ---------- servo ----------
comp(60, 440, 200, 110, "Steering servo", "MG90 · rack and pinion")
pin_in(260, 480, "R", "SIG", COL["servo"])
pin_in(260, 505, "R", "VCC", COL["v5"])
pin_in(260, 530, "R", "GND", COL["gnd"])
wire([(260, 480), (560, 480)], COL["servo"])
stub(260, 505, 30, "+5 V", COL["v5"])
stub(260, 530, 30, "GND", COL["gnd"])

# ---------- start button ----------
comp(60, 600, 190, 70, "Start button")
pin_in(250, 620, "R", "SIG", COL["btn"])
pin_in(250, 650, "R", "GND", COL["gnd"])
wire([(250, 620), (380, 620), (380, 540), (560, 540)], COL["btn"])
stub(250, 650, 30, "GND", COL["gnd"])

# ---------- TB6612FNG motor driver ----------
comp(1000, 280, 220, 180, "TB6612FNG")
pin_in(1000, 320, "L", "PWMA", COL["drv"])
pin_in(1000, 350, "L", "AIN1", COL["drv"])
pin_in(1000, 380, "L", "AIN2", COL["drv"])
pin_in(1220, 330, "R", "A01", COL["mot"])
pin_in(1220, 360, "R", "A02", COL["mot"])
text(1110, 425, "channel B not used", 11, "middle", "normal", "#555")
wire([(820, 320), (1000, 320)], COL["drv"])
wire([(820, 350), (1000, 350)], COL["drv"])
wire([(820, 380), (1000, 380)], COL["drv"])
for bx, lab, c in [(1040, "VBAT", COL["vbat"]), (1100, "+5 V", COL["v5"]),
                   (1160, "+5 V", COL["v5"]), (1200, "GND", COL["gnd"])]:
    add(f'<path d="M {bx} 460 L {bx} 482" stroke="{c}" stroke-width="2"/>')
    tag(bx, 498, lab, c, "middle", 11)
text(1110, 258, "VM = battery · VCC = +5 V · STBY tied to +5 V", 11, "middle", "normal", "#555")

# ---------- N20 motor and encoder ----------
comp(1280, 300, 140, 90, "N20 motor")
text(1350, 412, "12 V · 1000 rpm", 11, "middle", "normal", "#555")
pin_in(1280, 330, "L", "M1", COL["mot"])
pin_in(1280, 360, "L", "M2", COL["mot"])
wire([(1220, 330), (1280, 330)], COL["mot"])
wire([(1220, 360), (1280, 360)], COL["mot"])

comp(1000, 540, 180, 100, "Encoder", "Hall, N20 motor")
pin_in(1000, 570, "L", "C1", COL["enc"])
pin_in(1000, 600, "L", "C2", COL["enc"])
pin_in(1180, 570, "R", "VCC", COL["v5"])
pin_in(1180, 600, "R", "GND", COL["gnd"])
wire([(820, 430), (890, 430), (890, 570), (1000, 570)], COL["enc"])
wire([(820, 460), (860, 460), (860, 600), (1000, 600)], COL["enc"])
stub(1180, 570, 30, "+5 V", COL["v5"])
stub(1180, 600, 30, "GND", COL["gnd"])

# ---------- sensors on the I2C bus ----------
sensors = [
    ("TOF400C · LEFT", "addr 0x2A · XSHUT → A3", 60),
    ("TOF400C · FRONT", "addr 0x2D · XSHUT → A1", 280),
    ("TOF400C · BACK", "addr 0x2C · XSHUT → A0", 500),
    ("TOF400C · RIGHT", "addr 0x2B · XSHUT → A2", 720),
]
SY = 830
for title, sub, x in sensors:
    comp(x, SY, 200, 110, title, sub)
    add(f'<path d="M {x+60} {SY} L {x+60} {SY-16}" stroke="{COL["i2c"]}" stroke-width="2"/>')
    add(f'<path d="M {x+140} {SY} L {x+140} {SY-16}" stroke="{COL["i2c"]}" stroke-width="2"/>')
    tag(x + 60, SY - 20, "SDA", COL["i2c"], "middle", 11)
    tag(x + 140, SY - 20, "SCL", COL["i2c"], "middle", 11)
    for dx, lab, c in [(40, "+5 V", COL["v5"]), (100, "GND", COL["gnd"]), (160, "XS", COL["xs"])]:
        add(f'<path d="M {x+dx} {SY+110} L {x+dx} {SY+128}" stroke="{c}" stroke-width="2"/>')
        tag(x + dx, SY + 146, lab, c, "middle", 11)

comp(940, SY, 200, 110, "BMI160", "gyroscope · addr 0x69")
for dx, lab, c in [(40, "+5 V", COL["v5"]), (100, "GND", COL["gnd"])]:
    add(f'<path d="M {940+dx} {SY+110} L {940+dx} {SY+128}" stroke="{c}" stroke-width="2"/>')
    tag(940 + dx, SY + 146, lab, c, "middle", 11)
add(f'<path d="M {940+60} {SY} L {940+60} {SY-16}" stroke="{COL["i2c"]}" stroke-width="2"/>')
add(f'<path d="M {940+140} {SY} L {940+140} {SY-16}" stroke="{COL["i2c"]}" stroke-width="2"/>')
tag(940 + 60, SY - 20, "SDA", COL["i2c"], "middle", 11)
tag(940 + 140, SY - 20, "SCL", COL["i2c"], "middle", 11)

# bus names next to the Nano (the wires to the sensors are net labels)
tag(526, 704, "SDA bus →", COL["i2c"], "end", 11)
tag(526, 734, "SCL bus →", COL["i2c"], "end", 11)
text(40, 92, "XS = XSHUT pin: set LOW at start-up, then released one by one to give each sensor its address.",
     11, "start", "normal", "#555")

# ---------- notes and legend ----------
rect(1000, 60, 420, 150, fill="#fffdf5", stroke="#b7950b")
text(1014, 84, "How to read this diagram", 14, "start", "bold")
for i, line in enumerate([
    "• Coloured lines are real wires between the parts shown.",
    "• Pins with the same net name (+5 V, GND, VBAT, SDA, SCL)",
    "   are connected: the net label replaces a long wire.",
    "• Power: one 5 V rail from the CN3903 buck output (red).",
    "• The Nano is powered from the battery through VIN.",
]):
    text(1014, 106 + i * 20, line, 12, "start", "normal", "#333")

rect(1170, 720, 240, 210, fill="#ffffff", stroke="#7f8c8d")
text(1184, 744, "Colour key", 14, "start", "bold")
for i, (name, c) in enumerate([
    ("Battery · VBAT", COL["vbat"]), ("+5 V rail", COL["v5"]), ("GND", COL["gnd"]),
    ("Motor PWM / direction", COL["drv"]), ("Motor output", COL["mot"]),
    ("Encoder", COL["enc"]), ("Camera UART", COL["cam"]), ("Servo", COL["servo"]),
    ("Start button", COL["btn"]), ("I²C (SDA, SCL)", COL["i2c"]), ("XSHUT", COL["xs"]),
]):
    y = 766 + i * 16
    add(f'<path d="M 1184 {y-4} L 1212 {y-4}" stroke="{c}" stroke-width="4"/>')
    text(1220, y, name, 11, "start", "normal", "#333")

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">'
       f'<rect width="{W}" height="{H}" fill="#ffffff"/>' + "\n".join(out) + "</svg>")
here = os.path.dirname(os.path.abspath(__file__))
svg_path = os.path.join(here, "schematic.svg")
png_path = os.path.join(here, "schematic.png")
with open(svg_path, "w", encoding="utf-8") as f:
    f.write(svg)
subprocess.run(["rsvg-convert", "-z", "2", "-o", png_path, svg_path], check=True)
print("wrote", svg_path, png_path)
