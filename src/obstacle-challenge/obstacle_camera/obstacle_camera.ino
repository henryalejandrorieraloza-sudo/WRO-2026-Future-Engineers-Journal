// =====================================================================
// OBSTACLE CAMERA (ESP32S3-CAM) - v3.1 (pillars + both floor lines)
// Finds, in every picture:
//   - the closest red / green pillar (same algorithm as Pillar Vision Lab)
//   - red + magenta patches: if a patch is mostly magenta it is a parking
//     wall, never a pillar (same majority vote as the page)
//   - the ORANGE and the BLUE floor lines: biggest patch of each color below LINE_TOP_ROW,
//     only if it is long and thin (a real stripe) AND has white floor above it (the bottom
//     edge of the black wall has wall above), with its lowest row and its row straight
//     ahead (center strip)
// and sends one short text line to the Arduino Nano about 10 times a second:
//   $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,
//    orangeFound,orangeRow,blueCenterRow,orangeCenterRow
//     color: 0 none, 1 red, 2 green     x: -160 (left) .. +159 (right)
//     correct: 1 = pillar center at least SAFE_LAT_CM on its side (red: left, green: right)
//     width:  pillar width in pixels (bigger = closer)
//     distCm: distance ahead of the camera, cm
//     latCm:  sideways distance of the pillar center, cm (+ = right of the car, - = left)
//     blueRow / orangeRow: lowest row of the line (-1 = no line)
//     blueCenterRow / orangeCenterRow: row of the line straight ahead (-1 = not in the center strip)
//   The first 10 fields are the same as v2, so the old Nano sketches still work.
//
// v2 vision improvements (each one has its own switch):
//   1) LOCK_CAMERA      white balance, exposure and gain frozen a few seconds after power on.
//   2) USE_BLOCKS       pillars found on a grid of 4x4 blocks.
//   3) USE_TIME_FILTER  a pillar appears after ON_NEED of the last 4 pictures.
//   4) Mirror / flip done by the camera sensor itself.
// v3:   orange + blue lines, "long and thin" line filter, BLUE_MAX_SAT (green pillar never blue),
//       status text every 0.5 s with the line details.
// v3.1 (28 Oct): validated on the car, no false lines on the empty floor or on the black wall.
//       - BLUE_HUE_LOW 85 -> 110: the floor near the car reaches H 85, the blue line is ~160.
//       - Floor-above check: a line must have white floor (V >= 130) above it in >= 60 % of
//         its columns; the bottom edge of the black wall has dark wall above and is rejected.
//       - Color probe: H, S, V of two fixed boxes printed in the status, to calibrate the
//         colors with exact camera numbers (no viewer needed).
//       - Measured range: orange line from 16 cm, blue line from 9 cm.
// It can also keep sending the pictures to viewer.html over USB (STREAM_TO_PC).
//
// Tools: ESP32S3 Dev Module | USB CDC On Boot: Enabled | Flash Size: 8MB
//        Partition: 8M with spiffs (3MB APP/1.5MB SPIFFS) | PSRAM: OPI PSRAM
// Wiring (team PCB): camera GPIO43 (TX) -> Nano D0 (RX). The camera does not listen to the Nano.
// Upload: BOOT + RESET on the camera, and hold the Nano's RESET while uploading (both boards share D0).
// Power on the camera looking at the empty floor: the colors are frozen 5 s after power on.
// =====================================================================

#include "esp_camera.h"

// ---------- Camera pins (emakefun ESP32S3-CAM) ----------
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

const int XCLK_FREQ_HZ = 10000000;   // 10 MHz: safer with the long flex cable

// ---------- Link to the Nano ----------
const int  NANO_TX_PIN  = 43;        // camera GPIO43 -> Nano D0 (RX), as on the PCB
const int  NANO_RX_PIN  = -1;        // GPIO44 is NOT used: the camera never listens to the Nano
const long NANO_BAUD    = 115200;    // must match the Nano sketch
const bool STREAM_TO_PC = false;     // true = also send pictures to viewer.html (slower). false to drive / test
const unsigned long STREAM_EVERY_MS = 500;   // one picture to the viewer every 0.5 s (the page needs time to read it)

// ---------- Image ----------
const int W = 320;
const int H = 240;
const bool MIRROR_HORIZONTAL = false;   // checked: with true, a pillar on the car's RIGHT gave side < 0
const bool FLIP_VERTICAL     = false;   // both are applied by the sensor (set_hmirror / set_vflip)

// ---------- Real-world sizes (from the measurements) ----------
const float FOCAL_PX    = 100.0;  // measured: 112 read 30->35 and 50->59 cm; 100 is the middle value
const float PILLAR_CM   = 5.0;    // pillar width (rules: 50 x 50 x 100 mm)
const float SAFE_LAT_CM = 17.0;   // car half width 4.25 + 10 cm gap + pillar half width 2.5
const int   X_CENTER_OFFSET = 12; // a pillar straight ahead shows up this many px off center (camera turned ~6 deg)

// ---------- Vision improvements (switch each one off to compare) ----------
// 1) Frozen camera. IMPORTANT: colors change when the camera is frozen, so calibrate the colors
//    in viewer.html AFTER the lock (wait LOCK_AFTER_MS after power on, then connect the viewer).
const bool          LOCK_CAMERA    = true;
const unsigned long LOCK_AFTER_MS  = 5000;   // time the camera has to adjust itself first

// 2) Blocks
const bool USE_BLOCKS       = true;
const int  BLOCK            = 4;     // block size in pixels
const int  BLOCK_MIN_PIXELS = 12;    // a block is red / green if at least this many of its 16 pixels are
const int  MIN_BLOCKS       = 4;     // a pillar patch needs at least this many blocks
const int  EDGE_PAD         = 3;     // px around the patch searched to refine its edges
const int  COL_MIN_PIXELS   = 2;     // a column needs this many pixels to count for the width
const int  GAP_FILL         = 2;     // empty stripes up to this wide inside a pillar are bridged

// 3) Time filter
const bool USE_TIME_FILTER  = true;
const int  HISTORY          = 4;     // last pictures remembered
const int  ON_NEED          = 2;     // pictures (of HISTORY) needed to accept a pillar (2 = reacts faster)
const int  OFF_NEED         = 3;     // empty pictures (of HISTORY) needed to drop it. 2 = reacts faster

// 4) Plausibility: the pillar must make sense in the real world.
//    Uses the known pillar size (5 cm) and the track (corridor 100 cm wide).
const bool USE_PLAUSIBILITY = true;
const int  MIN_AHEAD_CM     = 5;     // closer than this cannot be a pillar the camera measures
const int  MAX_AHEAD_CM     = 130;   // farther than this is not a pillar on this track
const int  MAX_SIDE_CM      = 70;    // |sideways| beyond this is outside the corridor
const bool USE_SHAPE_FILTER = true;  // floor / wall look-alikes are speckled; a pillar is a solid chunk
const int  MIN_FILL_PERCENT = 75;    // a pillar is one solid block: pixels / (width x height) must be at least this %
const bool REJECT_AT_EDGE   = true;  // a patch touching the left / right picture edge is cut, its width is wrong

// 5) Camera lens correction: the wide-angle lens tints the picture edges (green / magenta)
const bool LENS_CORRECTION  = true;

// ===== PASTE HERE what "Copy as Arduino code" gives you in viewer.html =====
// (if you paste again: put back BLUE_HUE_LOW = 110, BLUE_MAX_SAT and the line minimums, the viewer does not have them)
// Red and green are ~70 degrees apart in hue, so the bands never touch.
// Measured with the camera locked (yellow cast): red pillar H 15-23, green pillar H 86-95.
// Probe on 28 Oct: red pillar H 4-22 S 204-255 V 112-136.
const int RED_HUE_LOW    = 345;    // wraps through 0 up to 26
const int RED_HUE_HIGH   = 26;
const int RED_MIN_SAT    = 115;
const int RED_MIN_VAL    = 95;

const int GREEN_HUE_LOW  = 82;     // pillar measured H 86-95 (locked); 75 let the yellow-green floor in
const int GREEN_HUE_HIGH = 150;
const int GREEN_MIN_SAT  = 150;    // floor measured S 88-156, pillar S 163+
const int GREEN_MIN_VAL  = 90;

const int  RED_MIN_PIXELS   = 30;
const int  GREEN_MIN_PIXELS = 30;
const int  IGNORE_TOP_ROWS = 126;
const int  BOTTOM_TOL      = 8;
const int  NEAR_ROWS       = 32;
const int  SIDE_ZONE       = 56;

// Lines, measured 2026-10-27 on the screenshots and 2026-10-28 with the probe (camera locked):
//   orange line H 32-44 S 145-173 V 168-184 | floor H 60-85 S 81-130 V 128-220
//   blue line H up to ~165, S 46-113, V from 72 (dark gray-teal) | green pillar S 152+
const int ORANGE_HUE_LOW  = 29;    // red pillar ends at 22-26
const int ORANGE_HUE_HIGH = 52;    // floor starts at 60
const int ORANGE_MIN_SAT  = 115;   // line S 131-173
const int ORANGE_MIN_VAL  = 110;

const int BLUE_HUE_LOW    = 110;   // floor reaches H 85 at the bottom of the picture; blue line is ~160 -> 25 deg margin
const int BLUE_HUE_HIGH   = 200;
const int BLUE_MIN_SAT    = 30;
const int BLUE_MAX_SAT    = 120;   // blue line S 46-113, green pillar S 152+ -> never mixed
const int BLUE_MIN_VAL    = 65;    // lower lets the black wall in

const int MAGENTA_HUE_LOW  = 328;
const int MAGENTA_HUE_HIGH = 353;
const int MAGENTA_MIN_SAT  = 84;
const int MAGENTA_MIN_VAL  = 84;

const int FLOOR_TOP_ROW      = 124;
const int ORANGE_MIN_PIXELS  = 50;    // at ~16 cm the line has only ~120-165 px
const int BLUE_MIN_PIXELS    = 60;    // safe now: BLUE_HUE_LOW = 110 keeps the floor out, floor-above check keeps the wall out
const int LINE_ROW_MIN       = 6;
const int LINE_TRIGGER_ROW   = 236;
const int MAGENTA_MIN_PIXELS = 250;
const int MAGENTA_MAJORITY   = 70;
const int TURN_CONFIRM_FRAMES = 3;
// ===== end of the pasted part =====

// ---------- Floor lines (full lap, step 1) ----------
// A floor line is a long, thin stripe. These numbers decide what counts as a line.
const int LINE_TOP_ROW     = 140;  // lines are searched only from this row down (the closest ~23 cm).
                                   // The far line is a thin streak mixed with the wall. Must be >= FLOOR_TOP_ROW.
const int LINE_MIN_LENGTH  = 40;   // px: a line patch must be at least this long (diagonal of its box)
const int LINE_MAX_THICK   = 40;   // px: average thickness (pixels / length). A pillar or a blob is fatter
const int LINE_CENTER_HALF = 10;   // px: half width of the straight-ahead strip (center + X_CENTER_OFFSET)
const int LINE_COL_MIN     = 2;    // px: a column of that strip needs this many line pixels to count
// Floor above the line: a floor line has bright (white) floor above it; the bottom edge of the black wall does not
const int ABOVE_FROM        = 3;    // rows above the line's top edge where the check starts
const int ABOVE_TO          = 6;    // ... and where it ends
const int FLOOR_MIN_VAL     = 130;  // brightness (V) the floor above must have (floor V 128-220, black wall much lower)
const int ABOVE_MIN_PERCENT = 60;   // % of the line's columns that must have floor above
const int ABOVE_MIN_COLS    = 10;   // at least this many columns must be checkable
// Color probe: prints the H, S, V range of two small boxes, to calibrate colors with exact camera numbers
const int PROBE_X       = W / 2 + X_CENTER_OFFSET;  // straight-ahead column
const int PROBE_NEAR_Y  = 170;  // row ~5 cm ahead of the camera: put each line / pillar here
const int PROBE_LOW_Y   = 228;  // row at the bottom of the picture (where the false blue appeared)
const int PROBE_HALF_W  = 6;    // box half width, px
const int PROBE_HALF_H  = 2;    // box half height, px
const unsigned long STATUS_EVERY_MS = 500;   // status text to the Serial Monitor (only with STREAM_TO_PC = false)

// ---------- Working area ----------
// Pillars: rows IGNORE_TOP_ROWS .. H-NEAR_ROWS-1. Lines and magenta: FLOOR_TOP_ROW .. H-1.
// Everything is computed once in the rows that cover both.
const int AREA_TOP = (IGNORE_TOP_ROWS < FLOOR_TOP_ROW) ? IGNORE_TOP_ROWS : FLOOR_TOP_ROW;
const int AREA_H   = H - AREA_TOP;
const int BAND_TOP = IGNORE_TOP_ROWS;
const int BAND_END = H - NEAR_ROWS;          // first row that is NOT in the pillar band

// Block grid, covers the pillar band only
const int BW = W / BLOCK;
const int BH = (BAND_END - BAND_TOP) / BLOCK;

// Pixel codes in the working area
const uint8_t C_NONE = 0, C_RED = 1, C_GREEN = 2, C_MAGENTA = 3, C_BLUE = 4, C_ORANGE = 5;

uint8_t  *code;          // AREA_H x W, one code per pixel
uint8_t  *seen;          // BFS visited marks
uint32_t *queue;         // BFS queue (pixel index in the area)
uint8_t  *maskPacked;    // pillar map for the viewer, 2 bits per pixel
uint8_t  *valueMap;      // AREA_H x W, brightness (V) of each pixel, for the floor-above check
int colTop[W], colBottom[W], colCount[W];
int rowCount[H];

uint8_t  blockCode[BW * BH];     // C_NONE / C_RED / C_GREEN per block
uint8_t  blockSeen[BW * BH];
uint16_t blockQueue[BW * BH];

struct Pillar {
  bool found;
  uint8_t color;
  int left, right, top, bottom;
  int count;
  long sumX;
  bool wasSplit;
};

// Floor line found in one picture (orange or blue)
const uint8_t LINE_OK = 0, LINE_NONE = 1, LINE_FEW = 2, LINE_SHORT = 3, LINE_THICK = 4, LINE_WALL = 5;
struct LineInfo {
  bool found;
  int lowest;        // lowest row of the patch (any column)
  int centerRow;     // lowest row inside the center strip (straight ahead), -1 if not there
  int count;         // pixels in the patch
  int length;        // px, diagonal of the patch box
  int thick;         // px, average thickness = count / length
  uint8_t reason;    // LINE_OK / NONE / FEW / SHORT / THICK / WALL (for the status print)
  int aboveFloor;    // columns with white floor above the line (floor-above check)
  int aboveChecked;  // columns that could be checked
};

// Time filter memory
uint8_t history[HISTORY];        // color seen in each of the last pictures (0 = none)
int     historyPos = 0;
uint8_t confirmedColor = C_NONE;
Pillar  lastSeen[3];             // last real detection of red (1) and green (2)

bool cameraLocked = false;
unsigned long bootMs = 0;
unsigned long seq = 0;
unsigned long lastStream = 0, lastStatus = 0, fpsStart = 0;
int framesThisPeriod = 0;
float cameraFps = 0;
uint8_t rawColorForStatus = 0;

// ---------- Colors (same math as viewer.html) ----------

void readPixel(const uint8_t *buf, int x, int y, int &r, int &g, int &b) {
  int i = (y * W + x) * 2;
  uint16_t p = (buf[i] << 8) | buf[i + 1];
  r = ((p >> 11) & 0x1F) << 3;
  g = ((p >> 5)  & 0x3F) << 2;
  b = ( p        & 0x1F) << 3;
}

void rgbToHsv(int r, int g, int b, int &h, int &s, int &v) {
  int maxC = max(r, max(g, b));
  int minC = min(r, min(g, b));
  int delta = maxC - minC;
  v = maxC;
  s = (maxC == 0) ? 0 : (delta * 255) / maxC;
  if (delta == 0) { h = 0; return; }
  if (maxC == r)      h = 60 * (g - b) / delta;
  else if (maxC == g) h = 60 * (b - r) / delta + 120;
  else                h = 60 * (r - g) / delta + 240;
  if (h < 0) h += 360;
}

bool inHue(int h, int lo, int hi) {              // lo > hi means the range wraps past 360
  return lo <= hi ? (h >= lo && h <= hi) : (h >= lo || h <= hi);
}

bool isRed(int h, int s, int v)     { return inHue(h, RED_HUE_LOW, RED_HUE_HIGH) && s >= RED_MIN_SAT && v >= RED_MIN_VAL; }
bool isGreen(int h, int s, int v)   { return h >= GREEN_HUE_LOW && h <= GREEN_HUE_HIGH && s >= GREEN_MIN_SAT && v >= GREEN_MIN_VAL; }
bool isMagenta(int h, int s, int v) { return inHue(h, MAGENTA_HUE_LOW, MAGENTA_HUE_HIGH) && s >= MAGENTA_MIN_SAT && v >= MAGENTA_MIN_VAL; }
bool isOrange(int h, int s, int v)  { return inHue(h, ORANGE_HUE_LOW, ORANGE_HUE_HIGH) && s >= ORANGE_MIN_SAT && v >= ORANGE_MIN_VAL; }
bool isBlue(int h, int s, int v)    { return inHue(h, BLUE_HUE_LOW, BLUE_HUE_HIGH) && s >= BLUE_MIN_SAT && s <= BLUE_MAX_SAT && v >= BLUE_MIN_VAL; }

// One code per pixel of the working area. Red is tested first (like the page),
// then green, then magenta / orange / blue. The brightness is kept for the floor-above check.
void buildCodeMap(const uint8_t *buf) {
  for (int ay = 0; ay < AREA_H; ay++) {
    int y = AREA_TOP + ay;
    for (int x = 0; x < W; x++) {
      int r, g, b, h, s, v;
      readPixel(buf, x, y, r, g, b);
      rgbToHsv(r, g, b, h, s, v);
      uint8_t c = C_NONE;
      if (isRed(h, s, v))          c = C_RED;
      else if (isGreen(h, s, v))   c = C_GREEN;
      else if (isMagenta(h, s, v)) c = C_MAGENTA;
      else if (isOrange(h, s, v))  c = C_ORANGE;
      else if (isBlue(h, s, v))    c = C_BLUE;
      code[ay * W + x] = c;
      valueMap[ay * W + x] = (uint8_t)v;
    }
  }
}

// Red-or-magenta patches (touching pixels): if at least MAGENTA_MAJORITY % of a patch is
// magenta, the whole patch is a parking wall -> its red pixels are no pillar any more.
void mergeRedMagenta() {
  memset(seen, 0, AREA_H * W);
  for (int start = 0; start < AREA_H * W; start++) {
    uint8_t c0 = code[start];
    if ((c0 != C_RED && c0 != C_MAGENTA) || seen[start]) continue;
    int head = 0, tail = 0, nRed = 0, nMg = 0;
    queue[tail++] = start; seen[start] = 1;
    while (head < tail) {
      int i = queue[head++];
      if (code[i] == C_RED) nRed++; else nMg++;
      int x = i % W, ay = i / W;
      int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1,
                    (ay + 1 < AREA_H) ? i + W : -1, (ay > 0) ? i - W : -1 };
      for (int k = 0; k < 4; k++) {
        int j = nb[k];
        if (j < 0 || seen[j] || (code[j] != C_RED && code[j] != C_MAGENTA)) continue;
        seen[j] = 1; queue[tail++] = j;
      }
    }
    bool magenta = (long)nMg * 100 >= (long)MAGENTA_MAJORITY * (nRed + nMg);
    for (int k = 0; k < tail; k++) {
      int i = queue[k];
      code[i] = magenta ? C_MAGENTA : (code[i] == C_RED ? C_RED : C_NONE);
    }
  }
}

// Is this area pixel a pillar pixel of this color? (only inside the pillar band)
inline bool pillarPixel(int i, uint8_t color) {
  int y = AREA_TOP + i / W;
  return y >= BAND_TOP && y < BAND_END && code[i] == color;
}

void pillarPosition(int x, int width, float &aheadCm, float &sideCm);   // defined below

// Does this patch make sense as a real pillar? Rejects what cannot be one: cut by the picture
// edge, closer / farther than the camera can measure, or outside the corridor.
bool plausible(const Pillar &p) {
  if (!USE_PLAUSIBILITY) return true;
  if (REJECT_AT_EDGE && (p.left <= 0 || p.right >= W - 1)) return false;
  int width = p.right - p.left + 1;
  if (USE_SHAPE_FILTER) {
    int height = p.bottom - p.top + 1;
    if ((long)p.count * 100 < (long)MIN_FILL_PERCENT * width * height) return false;
  }
  int x = (int)(p.sumX / p.count) - W / 2;
  float aheadCm, sideCm;
  pillarPosition(x, width, aheadCm, sideCm);
  if (aheadCm < MIN_AHEAD_CM || aheadCm > MAX_AHEAD_CM) return false;
  if (fabs(sideCm) > MAX_SIDE_CM) return false;
  return true;
}

// From the per-column numbers (colTop / colBottom / colCount) of one patch, take only the
// FRONT pillar: the columns whose bottom is within BOTTOM_TOL of the lowest one.
Pillar frontPillarOfBlob(uint8_t color, int minX, int maxX) {
  Pillar p = {false, color, 0, 0, 0, 0, 0, 0, false};
  int best = -1;
  for (int x = minX; x <= maxX; x++)
    if (colCount[x] > 0 && (best < 0 || colBottom[x] > colBottom[best])) best = x;
  if (best < 0) return p;
  int lowest = colBottom[best];
  int left = best, right = best;
  while (left - 1 >= minX && colCount[left - 1] > 0 && colBottom[left - 1] >= lowest - BOTTOM_TOL) left--;
  while (right + 1 <= maxX && colCount[right + 1] > 0 && colBottom[right + 1] >= lowest - BOTTOM_TOL) right++;
  p.left = left; p.right = right; p.bottom = lowest; p.top = H;
  for (int x = left; x <= right; x++) {
    if (colTop[x] < p.top) p.top = colTop[x];
    p.count += colCount[x];
    p.sumX  += (long)x * colCount[x];
  }
  p.found = p.count >= (color == C_RED ? RED_MIN_PIXELS : GREEN_MIN_PIXELS);
  p.wasSplit = (left > minX || right < maxX);
  return p;
}

bool isCloser(const Pillar &a, const Pillar &b) {
  if (!b.found) return true;
  if (a.bottom != b.bottom) return a.bottom > b.bottom;
  return a.count > b.count;
}

// ---------- Pillars, pixel by pixel (v1 method, used when USE_BLOCKS = false) ----------
Pillar findClosestPillar() {
  memset(seen, 0, AREA_H * W);
  Pillar closest = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};
  for (int start = 0; start < AREA_H * W; start++) {
    uint8_t color = code[start];
    if ((color != C_RED && color != C_GREEN) || seen[start] || !pillarPixel(start, color)) continue;
    for (int i = 0; i < W; i++) { colTop[i] = H; colBottom[i] = -1; colCount[i] = 0; }
    int minX = W, maxX = -1, total = 0, head = 0, tail = 0;
    queue[tail++] = start; seen[start] = 1;
    while (head < tail) {
      int i = queue[head++];
      int x = i % W, y = AREA_TOP + i / W;
      total++; colCount[x]++;
      if (y < colTop[x])    colTop[x] = y;
      if (y > colBottom[x]) colBottom[x] = y;
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1, i + W, i - W };
      for (int k = 0; k < 4; k++) {
        int j = nb[k];
        if (j < 0 || j >= AREA_H * W || seen[j] || !pillarPixel(j, color)) continue;
        seen[j] = 1; queue[tail++] = j;
      }
    }
    if (total < (color == C_RED ? RED_MIN_PIXELS : GREEN_MIN_PIXELS)) continue;
    Pillar p = frontPillarOfBlob(color, minX, maxX);
    if (p.found && plausible(p) && isCloser(p, closest)) closest = p;
  }
  return closest;
}

// ---------- Pillars on a grid of blocks ----------
// A block (BLOCK x BLOCK pixels, only inside the pillar band) is red / green when at least
// BLOCK_MIN_PIXELS of its pixels have that color. Isolated noise pixels and small holes vanish.
void buildBlocks() {
  for (int by = 0; by < BH; by++) {
    for (int bx = 0; bx < BW; bx++) {
      int nRed = 0, nGreen = 0;
      for (int dy = 0; dy < BLOCK; dy++) {
        const uint8_t *row = code + (BAND_TOP + by * BLOCK + dy - AREA_TOP) * W + bx * BLOCK;
        for (int dx = 0; dx < BLOCK; dx++) {
          if (row[dx] == C_RED) nRed++;
          else if (row[dx] == C_GREEN) nGreen++;
        }
      }
      uint8_t c = C_NONE;
      if (nRed >= BLOCK_MIN_PIXELS && nRed >= nGreen) c = C_RED;
      else if (nGreen >= BLOCK_MIN_PIXELS)            c = C_GREEN;
      blockCode[by * BW + bx] = c;
    }
  }
}

Pillar findClosestPillarBlocks() {
  Pillar closest = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};
  memset(blockSeen, 0, sizeof(blockSeen));
  for (int start = 0; start < BW * BH; start++) {
    uint8_t color = blockCode[start];
    if (color == C_NONE || blockSeen[start]) continue;

    // 1) connected blocks of this color
    int head = 0, tail = 0;
    int minBx = BW, maxBx = -1, minBy = BH, maxBy = -1;
    blockQueue[tail++] = start; blockSeen[start] = 1;
    while (head < tail) {
      int i = blockQueue[head++];
      int bx = i % BW, by = i / BW;
      if (bx < minBx) minBx = bx;
      if (bx > maxBx) maxBx = bx;
      if (by < minBy) minBy = by;
      if (by > maxBy) maxBy = by;
      int nb[4] = { (bx + 1 < BW) ? i + 1 : -1, (bx > 0) ? i - 1 : -1,
                    (by + 1 < BH) ? i + BW : -1, (by > 0) ? i - BW : -1 };
      for (int k = 0; k < 4; k++) {
        int j = nb[k];
        if (j < 0 || blockSeen[j] || blockCode[j] != color) continue;
        blockSeen[j] = 1; blockQueue[tail++] = j;
      }
    }
    if (tail < MIN_BLOCKS) continue;

    // 2) refine with the real pixels around the patch: exact left / right edges, bottom row
    int x0 = max(0, minBx * BLOCK - EDGE_PAD);
    int x1 = min(W - 1, maxBx * BLOCK + BLOCK - 1 + EDGE_PAD);
    int y0 = max(BAND_TOP, BAND_TOP + minBy * BLOCK - (BLOCK - 1));
    int y1 = min(BAND_END - 1, BAND_TOP + (maxBy + 1) * BLOCK - 1 + (BLOCK - 1));
    for (int x = 0; x < W; x++) { colTop[x] = H; colBottom[x] = -1; colCount[x] = 0; }
    for (int y = y0; y <= y1; y++) {
      const uint8_t *row = code + (y - AREA_TOP) * W;
      for (int x = x0; x <= x1; x++) {
        if (row[x] != color) continue;
        colCount[x]++;
        if (y < colTop[x])    colTop[x] = y;
        if (y > colBottom[x]) colBottom[x] = y;
      }
    }
    for (int x = x0; x <= x1; x++)
      if (colCount[x] < COL_MIN_PIXELS) { colCount[x] = 0; colBottom[x] = -1; colTop[x] = H; }
    // A thin empty stripe inside the patch (a shiny reflection, a bad column) must not cut the
    // pillar in two: columns of an empty run up to GAP_FILL wide, with pillar on both sides,
    // copy their left neighbor.
    for (int x = x0 + 1; x <= x1; x++) {
      if (colCount[x] != 0 || colCount[x - 1] == 0) continue;
      int e = x;
      while (e + 1 <= x1 && colCount[e + 1] == 0) e++;
      if (e + 1 <= x1 && e - x + 1 <= GAP_FILL && abs(colBottom[e + 1] - colBottom[x - 1]) <= BOTTOM_TOL)
        for (int k = x; k <= e; k++) { colCount[k] = 1; colBottom[k] = colBottom[x - 1]; colTop[k] = colTop[x - 1]; }
      x = e;
    }

    Pillar p = frontPillarOfBlob(color, x0, x1);
    if (p.found && plausible(p) && isCloser(p, closest)) closest = p;
  }
  return closest;
}

// ---------- Time filter: ON_NEED of the last HISTORY pictures ----------
// Returns the pillar to report. While a pillar is confirmed but missing in the current
// picture, the last real detection of that color is repeated (at most a few pictures old).
Pillar timeFilter(const Pillar &raw) {
  history[historyPos] = raw.found ? raw.color : C_NONE;
  historyPos = (historyPos + 1) % HISTORY;
  if (raw.found) lastSeen[raw.color] = raw;

  int nNone = 0, nRed = 0, nGreen = 0;
  for (int i = 0; i < HISTORY; i++) {
    if (history[i] == C_RED) nRed++;
    else if (history[i] == C_GREEN) nGreen++;
    else nNone++;
  }
  if (nRed >= ON_NEED)        confirmedColor = C_RED;
  else if (nGreen >= ON_NEED) confirmedColor = C_GREEN;
  else if (nNone >= OFF_NEED) confirmedColor = C_NONE;
  // otherwise: keep the previous decision

  Pillar none = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};
  if (confirmedColor == C_NONE) return none;
  Pillar p = lastSeen[confirmedColor];
  p.found = p.count > 0;
  return p;
}

// ---------- Floor lines (orange and blue) ----------
// A floor line is a long, thin stripe. For each color: take the biggest patch below
// LINE_TOP_ROW, check it is long and thin and has floor above it, then measure where it is.

// Walks the patch of this color that contains 'start' (only rows from firstRow down).
// Only touching pixels (4 neighbors) belong to the same patch.
// The pixels end up in queue[0 .. return value - 1].
int walkPatch(int start, uint8_t color, int firstRow) {
  int head = 0, tail = 0;
  queue[tail++] = start; seen[start] = 1;
  while (head < tail) {
    int i = queue[head++];
    int x = i % W, ay = i / W;
    int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1,
                  (ay + 1 < AREA_H) ? i + W : -1, (ay > firstRow) ? i - W : -1 };
    for (int k = 0; k < 4; k++) {
      int j = nb[k];
      if (j < 0 || seen[j] || code[j] != color) continue;
      seen[j] = 1; queue[tail++] = j;
    }
  }
  return tail;
}

// Floor-above check. For each column of the line: look ABOVE_FROM..ABOVE_TO rows above the
// line's top edge and take the darkest of those pixels. If it is at least FLOOR_MIN_VAL bright,
// there is white floor above in that column. A painted floor line has floor above it;
// the bottom edge of the black wall has dark wall above it.
void checkFloorAbove(int minX, int maxX, int &checked, int &floorCols) {
  checked = 0; floorCols = 0;
  for (int x = minX; x <= maxX; x++) {
    if (colCount[x] == 0) continue;
    int yFrom = colTop[x] - ABOVE_TO;                    // highest row looked at
    int yTo   = colTop[x] - ABOVE_FROM;                  // lowest row looked at
    if (yFrom < AREA_TOP) continue;                      // outside the working area: cannot check
    int darkest = 255;
    for (int y = yFrom; y <= yTo; y++) {
      int v = valueMap[(y - AREA_TOP) * W + x];
      if (v < darkest) darkest = v;
    }
    checked++;
    if (darkest >= FLOOR_MIN_VAL) floorCols++;
  }
}

LineInfo findLine(uint8_t color, int minPixels) {
  LineInfo line = {false, -1, -1, 0, 0, 0, LINE_NONE, 0, 0};
  const int firstRow = LINE_TOP_ROW - AREA_TOP;          // first area row searched

  // 1) The biggest patch of this color
  memset(seen, 0, AREA_H * W);
  int bestStart = -1, bestN = 0;
  for (int start = firstRow * W; start < AREA_H * W; start++) {
    if (code[start] != color || seen[start]) continue;
    int n = walkPatch(start, color, firstRow);
    if (n > bestN) { bestN = n; bestStart = start; }
  }
  line.count = bestN;
  if (bestStart < 0) return line;                         // LINE_NONE
  if (bestN < minPixels) { line.reason = LINE_FEW; return line; }

  // 2) Walk it again and measure it: box, pixels per column / per row, top and bottom of each column
  memset(seen, 0, AREA_H * W);
  int n = walkPatch(bestStart, color, firstRow);
  for (int x = 0; x < W; x++) { colCount[x] = 0; colBottom[x] = -1; colTop[x] = H; }
  memset(rowCount, 0, sizeof(rowCount));
  int minX = W, maxX = -1, minY = H, maxY = -1;
  for (int k = 0; k < n; k++) {
    int i = queue[k];
    int x = i % W, y = AREA_TOP + i / W;
    colCount[x]++;
    if (y > colBottom[x]) colBottom[x] = y;
    if (y < colTop[x])    colTop[x] = y;
    rowCount[y]++;
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  }
  int boxW = maxX - minX + 1, boxH = maxY - minY + 1;
  line.length = (int)(sqrtf((float)(boxW * boxW + boxH * boxH)) + 0.5f);
  line.thick  = (n + line.length / 2) / line.length;

  // 3) Is it a line? Long and thin. (A pillar or a blob is short and fat.)
  if (line.length < LINE_MIN_LENGTH) { line.reason = LINE_SHORT; return line; }
  if (line.thick  > LINE_MAX_THICK)  { line.reason = LINE_THICK; return line; }

  // 4) Is it on the floor? White floor above it (the black wall's bottom edge has wall above)
  checkFloorAbove(minX, maxX, line.aboveChecked, line.aboveFloor);
  if (line.aboveChecked < ABOVE_MIN_COLS ||
      (long)line.aboveFloor * 100 < (long)ABOVE_MIN_PERCENT * line.aboveChecked) {
    line.reason = LINE_WALL;
    return line;
  }

  // 5) Where is it? Lowest row overall, and lowest row straight ahead (center strip)
  for (int y = H - 1; y >= LINE_TOP_ROW; y--) if (rowCount[y] >= LINE_ROW_MIN) { line.lowest = y; break; }
  int cx = W / 2 + X_CENTER_OFFSET;                       // real straight-ahead column
  for (int x = max(0, cx - LINE_CENTER_HALF); x <= min(W - 1, cx + LINE_CENTER_HALF); x++)
    if (colCount[x] >= LINE_COL_MIN && colBottom[x] > line.centerRow) line.centerRow = colBottom[x];

  line.found  = line.lowest >= 0;
  line.reason = line.found ? LINE_OK : LINE_FEW;
  return line;
}

const char *lineReason(const LineInfo &l) {
  switch (l.reason) {
    case LINE_OK:    return "yes";
    case LINE_FEW:   return "no (few pixels)";
    case LINE_SHORT: return "no (too short)";
    case LINE_THICK: return "no (too thick)";
    case LINE_WALL:  return "no (wall above)";
    default:         return "no";
  }
}

// ---------- Message to the Nano ----------
int roundInt(float v) { return v >= 0 ? (int)(v + 0.5f) : (int)(v - 0.5f); }

// Real position of a pillar from its center x (px from the middle) and its width (px)
void pillarPosition(int x, int width, float &aheadCm, float &sideCm) {
  float xc = x - X_CENTER_OFFSET;                       // px from the real straight-ahead line
  float a  = atanf(fabs(xc) / FOCAL_PX);               // angle from straight ahead
  float seenCm = PILLAR_CM * (cosf(a) + sinf(a));       // two faces seen from the side
  aheadCm = FOCAL_PX * seenCm / width;
  sideCm  = aheadCm * xc / FOCAL_PX;
}

// $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
void sendToNano(const Pillar &p, const LineInfo &blue, const LineInfo &orange) {
  int color = 0, x = 0, bottom = -1, correct = 0, width = 0, distCm = -1, latCm = 0;
  if (p.found) {
    color  = (p.color == C_RED) ? 1 : 2;
    x      = (int)(p.sumX / p.count) - W / 2;
    bottom = p.bottom;
    width  = p.right - p.left + 1;
    float aheadCm, sideCm;
    pillarPosition(x, width, aheadCm, sideCm);
    distCm = roundInt(aheadCm);
    latCm  = roundInt(sideCm);
    // red is passed on its right -> the pillar must end up on the car's LEFT (latCm negative)
    correct = (p.color == C_RED) ? (latCm <= -SAFE_LAT_CM) : (latCm >= SAFE_LAT_CM);
  }
  // The first 10 fields are the same as before, so the old Nano sketches still work.
  Serial1.printf("$%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%d\n", color, x, bottom, correct,
                 blue.found ? 1 : 0, blue.found ? blue.lowest : -1, seq, width, distCm, latCm,
                 orange.found ? 1 : 0, orange.found ? orange.lowest : -1,
                 blue.found ? blue.centerRow : -1, orange.found ? orange.centerRow : -1);
}

// ---------- Picture to viewer.html (same packet as pillar_viewer) ----------
// Serial.write() may accept FEWER bytes than asked when the USB buffer is full.
// writeAll() keeps offering the rest until everything is accepted; if the computer stops
// reading for more than maxWaitMs it gives up on this picture (the next one starts clean).
bool writeAll(const uint8_t *data, size_t len, unsigned long maxWaitMs) {
  size_t sent = 0;
  unsigned long t0 = millis();
  while (sent < len) {
    size_t n = Serial.write(data + sent, len - sent);
    if (n == 0) {
      if (millis() - t0 > maxWaitMs) return false;
      delay(1);
    } else {
      sent += n;
      t0 = millis();
    }
  }
  return true;
}

void putU16(uint8_t *h, int &k, uint16_t v) { h[k++] = v & 0xFF; h[k++] = v >> 8; }

void setMaskPixel(int x, int y, uint8_t c) {
  int pi = y * W + x;                                    // index in the whole picture
  maskPacked[pi >> 2] |= (c & 0x03) << ((pi & 3) * 2);
}

void sendPacket(const uint8_t *picture, const Pillar &p) {
  memset(maskPacked, 0, W * H / 4);
  if (USE_BLOCKS) {                                      // the viewer shows the blocks the car really uses
    for (int by = 0; by < BH; by++)
      for (int bx = 0; bx < BW; bx++) {
        uint8_t c = blockCode[by * BW + bx];
        if (c == C_NONE) continue;
        for (int dy = 0; dy < BLOCK; dy++)
          for (int dx = 0; dx < BLOCK; dx++)
            setMaskPixel(bx * BLOCK + dx, BAND_TOP + by * BLOCK + dy, c);
      }
  } else {
    for (int i = 0; i < AREA_H * W; i++) {
      uint8_t c = code[i];
      if ((c == C_RED || c == C_GREEN) && pillarPixel(i, c)) setMaskPixel(i % W, AREA_TOP + i / W, c);
    }
  }
  uint8_t hdr[32];
  int k = 0;
  hdr[k++] = 0xAA; hdr[k++] = 0x55; hdr[k++] = 0xC3; hdr[k++] = 0x3C;
  hdr[k++] = 1;
  putU16(hdr, k, W); putU16(hdr, k, H);
  hdr[k++] = 0;                                          // flip flags: the sensor already flips, picture is final
  hdr[k++] = p.found ? 1 : 0;
  hdr[k++] = p.color;
  putU16(hdr, k, (uint16_t)p.left);  putU16(hdr, k, (uint16_t)p.right);
  putU16(hdr, k, (uint16_t)p.top);   putU16(hdr, k, (uint16_t)p.bottom);
  putU16(hdr, k, (uint16_t)p.count);
  hdr[k++] = p.wasSplit ? 1 : 0;
  putU16(hdr, k, (uint16_t)(cameraFps * 10));
  const unsigned long WAIT_MS = 150;
  if (!writeAll(hdr, k, WAIT_MS)) return;
  if (!writeAll(picture, W * H * 2, WAIT_MS)) return;
  writeAll(maskPacked, W * H / 4, WAIT_MS);
}

// ---------- Camera ----------
// Freeze white balance, exposure and gain at the values the camera found by itself.
void lockCamera() {
  sensor_t *cam = esp_camera_sensor_get();
  cam->set_whitebal(cam, 0);
  cam->set_awb_gain(cam, 0);
  cam->set_exposure_ctrl(cam, 0);
  cam->set_aec2(cam, 0);
  cam->set_gain_ctrl(cam, 0);
  cameraLocked = true;
  if (!(STREAM_TO_PC && Serial)) Serial.println("\nCAMERA LOCKED: white balance, exposure and gain are frozen.");
}

bool startCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = XCLK_FREQ_HZ;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QVGA;      // 320x240
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.fb_count     = 2;
  if (esp_camera_init(&config) != ESP_OK) return false;

  sensor_t *cam = esp_camera_sensor_get();
  cam->set_hmirror(cam, MIRROR_HORIZONTAL ? 1 : 0);   // flips done by the sensor (free)
  cam->set_vflip(cam, FLIP_VERTICAL ? 1 : 0);
  cam->set_saturation(cam, 2);                        // colors a bit more vivid, less grain
  cam->set_lenc(cam, LENS_CORRECTION ? 1 : 0);        // lens shading correction (edge tint)
  cam->set_whitebal(cam, 1);                          // automatic until the lock
  cam->set_awb_gain(cam, 1);
  cam->set_exposure_ctrl(cam, 1);
  cam->set_aec2(cam, 1);
  cam->set_gain_ctrl(cam, 1);
  cam->set_gainceiling(cam, GAINCEILING_4X);
  return true;
}

void setup() {
  Serial.setTxBufferSize(16384);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);          // never wait for the computer
  Serial1.begin(NANO_BAUD, SERIAL_8N1, NANO_RX_PIN, NANO_TX_PIN);   // TX only, to the Nano
  delay(1500);
  Serial.println("OBSTACLE CAMERA v3.1: starting...");

  code       = (uint8_t *)ps_malloc(AREA_H * W);
  seen       = (uint8_t *)ps_malloc(AREA_H * W);
  queue      = (uint32_t *)ps_malloc(AREA_H * W * sizeof(uint32_t));
  maskPacked = (uint8_t *)ps_malloc(W * H / 4);
  valueMap   = (uint8_t *)ps_malloc(AREA_H * W);
  if (!psramFound() || !code || !seen || !queue || !maskPacked || !valueMap || !startCamera()) {
    while (true) { Serial.println("Camera or PSRAM did not start. Check Tools -> PSRAM: OPI PSRAM, then RESET."); delay(3000); }
  }
  Serial.printf("OBSTACLE CAMERA v3.1: OK, pillar band rows %d-%d (%d x %d blocks), lines from row %d\n",
                BAND_TOP, BAND_END - 1, BW, BH, LINE_TOP_ROW);
  Serial.printf("lock=%d blocks=%d timeFilter=%d (lock in %lu ms)\n",
                LOCK_CAMERA, USE_BLOCKS, USE_TIME_FILTER, LOCK_AFTER_MS);
  bootMs = millis();
  fpsStart = millis();
}

// Prints the H, S, V range (min-max) of the pixels in a small box centered at (cx, cy)
void printProbe(const char *name, const uint8_t *buf, int cx, int cy) {
  int hMin = 999, hMax = -1, sMin = 999, sMax = -1, vMin = 999, vMax = -1;
  for (int y = cy - PROBE_HALF_H; y <= cy + PROBE_HALF_H; y++) {
    for (int x = cx - PROBE_HALF_W; x <= cx + PROBE_HALF_W; x++) {
      int r, g, b, h, s, v;
      readPixel(buf, x, y, r, g, b);
      rgbToHsv(r, g, b, h, s, v);
      if (h < hMin) hMin = h;
      if (h > hMax) hMax = h;
      if (s < sMin) sMin = s;
      if (s > sMax) sMax = s;
      if (v < vMin) vMin = v;
      if (v > vMax) vMax = v;
    }
  }
  Serial.printf("  PROBE %-6s (x %d, y %d): H %d-%d  S %d-%d  V %d-%d\n",
                name, cx, cy, hMin, hMax, sMin, sMax, vMin, vMax);
}

void loop() {
  if (LOCK_CAMERA && !cameraLocked && millis() - bootMs >= LOCK_AFTER_MS) lockCamera();

  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == NULL) { delay(20); return; }
  if (frame->width != W || frame->height != H) { esp_camera_fb_return(frame); delay(200); return; }

  buildCodeMap(frame->buf);
  mergeRedMagenta();
  Pillar raw;
  if (USE_BLOCKS) { buildBlocks(); raw = findClosestPillarBlocks(); }
  else            { raw = findClosestPillar(); }
  rawColorForStatus = raw.found ? raw.color : C_NONE;
  Pillar closest = USE_TIME_FILTER ? timeFilter(raw) : raw;
  LineInfo blue   = findLine(C_BLUE, BLUE_MIN_PIXELS);
  LineInfo orange = findLine(C_ORANGE, ORANGE_MIN_PIXELS);
  seq++;
  sendToNano(closest, blue, orange);

  framesThisPeriod++;
  if (millis() - fpsStart >= 1000) {
    cameraFps = framesThisPeriod * 1000.0 / (millis() - fpsStart);
    framesThisPeriod = 0; fpsStart = millis();
  }
  // Pictures only when viewer.html has the port open (Serial is true), so the camera never
  // waits for a computer that is not there (that stopped the messages to the Nano).
  if (STREAM_TO_PC && Serial && millis() - lastStream >= STREAM_EVERY_MS) { sendPacket(frame->buf, closest); lastStream = millis(); }
  if (millis() - lastStatus >= STATUS_EVERY_MS && !(STREAM_TO_PC && Serial)) {   // text would land inside the picture stream
    int wPx = closest.found ? closest.right - closest.left + 1 : 0;
    int xPx = closest.found ? (int)(closest.sumX / closest.count) - W / 2 : 0;
    float aheadCm = -1, sideCm = 0;
    if (wPx) pillarPosition(xPx, wPx, aheadCm, sideCm);
    Serial.printf("\nstatus: camera %.1f fps %s | pillar %s %d cm ahead, %d cm side\n",
                  cameraFps, cameraLocked ? "(locked)" : "(auto)",
                  closest.found ? (closest.color == C_RED ? "RED" : "GREEN") : "none",
                  roundInt(aheadCm), roundInt(sideCm));
    Serial.printf("  ORANGE %s | center %d | low %d | len %d | thick %d | px %d | above %d/%d\n", lineReason(orange),
                  orange.centerRow, orange.lowest, orange.length, orange.thick, orange.count,
                  orange.aboveFloor, orange.aboveChecked);
    Serial.printf("  BLUE   %s | center %d | low %d | len %d | thick %d | px %d | above %d/%d\n", lineReason(blue),
                  blue.centerRow, blue.lowest, blue.length, blue.thick, blue.count,
                  blue.aboveFloor, blue.aboveChecked);
    printProbe("near", frame->buf, PROBE_X, PROBE_NEAR_Y);
    printProbe("bottom", frame->buf, PROBE_X, PROBE_LOW_Y);
    lastStatus = millis();
  }
  esp_camera_fb_return(frame);
}
