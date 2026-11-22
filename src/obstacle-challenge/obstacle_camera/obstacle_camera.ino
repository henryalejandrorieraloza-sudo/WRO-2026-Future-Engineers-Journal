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

#include "esp_camera.h"                 // ESP32 camera driver library

// ---------- Camera pins (emakefun ESP32S3-CAM) ----------
#define PWDN_GPIO_NUM   -1              // power-down pin: not used on this board
#define RESET_GPIO_NUM  -1              // reset pin: not used on this board
#define XCLK_GPIO_NUM   15              // clock the camera needs from the ESP32
#define SIOD_GPIO_NUM    4              // SCCB (I2C-like) data line for camera settings
#define SIOC_GPIO_NUM    5              // SCCB clock line
#define Y9_GPIO_NUM     16              // image data bit 9 (pixel data line)
#define Y8_GPIO_NUM     17              // image data bit 8
#define Y7_GPIO_NUM     18              // image data bit 7
#define Y6_GPIO_NUM     12              // image data bit 6
#define Y5_GPIO_NUM     10              // image data bit 5
#define Y4_GPIO_NUM      8              // image data bit 4
#define Y3_GPIO_NUM      9              // image data bit 3
#define Y2_GPIO_NUM     11              // image data bit 2
#define VSYNC_GPIO_NUM   6              // vertical sync: start of a new frame
#define HREF_GPIO_NUM    7              // horizontal reference: start of a new line
#define PCLK_GPIO_NUM   13              // pixel clock: one pixel per tick

const int XCLK_FREQ_HZ = 10000000;   // 10 MHz: safer with the long flex cable

// ---------- Link to the Nano ----------
const int  NANO_TX_PIN  = 43;        // camera GPIO43 -> Nano D0 (RX), as on the PCB
const int  NANO_RX_PIN  = -1;        // GPIO44 is NOT used: the camera never listens to the Nano
const long NANO_BAUD    = 115200;    // must match the Nano sketch
const bool STREAM_TO_PC = true;     // true = also send pictures to viewer.html (slower). false to drive / test
const unsigned long STREAM_EVERY_MS = 1;   // one picture to the viewer every 0.5 s (the page needs time to read it)

// ---------- Image ----------
const int W = 320;                   // picture width in pixels (QVGA)
const int H = 240;                   // picture height in pixels
const bool MIRROR_HORIZONTAL = false;   // checked on the 19 Nov camera: with true, a pillar on the car's RIGHT gave side < 0 (wrong); false is correct on this board
const bool FLIP_VERTICAL     = false;   // both are applied by the sensor (set_hmirror / set_vflip)

// ---------- Real-world sizes (from the measurements) ----------
const float FOCAL_PX    = 100.0;  // measured: 112 read 30->35 and 50->59 cm; 100 is the middle value
const float PILLAR_CM   = 5.0;    // pillar width (rules: 50 x 50 x 100 mm)
const float SAFE_LAT_CM = 17.0;   // car half width 4.25 + 10 cm gap + pillar half width 2.5
const int   X_CENTER_OFFSET = 12; // a pillar straight ahead shows up this many px off center (camera turned ~6 deg)

// ---------- Vision improvements (switch each one off to compare) ----------
// 1) Frozen camera. IMPORTANT: colors change when the camera is frozen, so calibrate the colors
//    in viewer.html AFTER the lock (wait LOCK_AFTER_MS after power on, then connect the viewer).
const bool          LOCK_CAMERA    = true;   // true = freeze the camera settings after LOCK_AFTER_MS
const unsigned long LOCK_AFTER_MS  = 5000;   // time the camera has to adjust itself first

// 2) Blocks
const bool USE_BLOCKS       = true;  // true = find pillars on the block grid, false = pixel by pixel
const int  BLOCK            = 4;     // block size in pixels
const int  BLOCK_MIN_PIXELS = 12;    // a block is red / green if at least this many of its 16 pixels are
const int  MIN_BLOCKS       = 4;     // a pillar patch needs at least this many blocks
const int  EDGE_PAD         = 3;     // px around the patch searched to refine its edges
const int  COL_MIN_PIXELS   = 2;     // a column needs this many pixels to count for the width
const int  GAP_FILL         = 2;     // empty stripes up to this wide inside a pillar are bridged

// 3) Time filter
const bool USE_TIME_FILTER  = true;  // true = a pillar must appear in several pictures before it counts
const int  HISTORY          = 4;     // last pictures remembered
const int  ON_NEED          = 2;     // pictures (of HISTORY) needed to accept a pillar (2 = reacts faster)
const int  OFF_NEED         = 3;     // empty pictures (of HISTORY) needed to drop it. 2 = reacts faster

// 4) Plausibility: the pillar must make sense in the real world.
//    Uses the known pillar size (5 cm) and the track (corridor 100 cm wide).
const bool USE_PLAUSIBILITY = true;  // true = reject patches that cannot be a real pillar
const int  MIN_AHEAD_CM     = 5;     // closer than this cannot be a pillar the camera measures
const int  MAX_AHEAD_CM     = 130;   // farther than this is not a pillar on this track
const int  MAX_SIDE_CM      = 70;    // |sideways| beyond this is outside the corridor
const bool USE_SHAPE_FILTER = true;  // floor / wall look-alikes are speckled; a pillar is a solid chunk
const int  MIN_FILL_PERCENT = 75;    // a pillar is one solid block: pixels / (width x height) must be at least this %
const bool REJECT_AT_EDGE   = true;  // a patch touching the left / right picture edge is cut, its width is wrong

// 5) Camera lens correction: the wide-angle lens tints the picture edges (green / magenta)
const bool LENS_CORRECTION  = true;  // true = turn on the sensor's lens shading correction

// ===== PASTE HERE what "Copy as Arduino code" gives you in viewer.html =====
// (if you paste again: put back BLUE_HUE_LOW = 110, BLUE_MAX_SAT and the line minimums, the viewer does not have them)
// Red and green are ~70 degrees apart in hue, so the bands never touch.
// Measured with the camera locked (yellow cast): red pillar H 15-23, green pillar H 86-95.
// Probe on 28 Oct: red pillar H 4-22 S 204-255 V 112-136.
const int RED_HUE_LOW    = 345;    // wraps through 0 up to 40
const int RED_HUE_HIGH   = 40;     // was 26: the red pillar reads H 29-34 with the yellow cast
const int RED_MIN_SAT    = 115;    // red pixels need at least this saturation (colorfulness)
const int RED_MIN_VAL    = 95;     // red pixels need at least this brightness
const int RED_MAX_VAL    = 165;    // NEW: brighter than this is the orange line, not a red pillar

const int GREEN_HUE_LOW  = 82;     // green hue range, low end
const int GREEN_HUE_HIGH = 150;    // green hue range, high end
const int GREEN_MIN_SAT  = 120;    // was 150: the green pillar measured S 148
const int GREEN_MIN_VAL  = 130;    // was 90: the dark wall measured V 74-92, the pillar V 194

const int  RED_MIN_PIXELS   = 30;  // a red patch needs at least this many pixels
const int  GREEN_MIN_PIXELS = 30;  // a green patch needs at least this many pixels
const int  IGNORE_TOP_ROWS = 115;  // rows at the top of the picture (far away) that are not searched for pillars
const int  BOTTOM_TOL      = 27;    // columns whose bottom is within this many rows of the lowest one belong to the front pillar
const int  NEAR_ROWS       = 0;    // rows at the bottom of the picture left out of the pillar search
const int  SIDE_ZONE       = 56;   // side zone size used by the older pillar search (kept from v2)

// Lines, measured 19 Nov 2026 on the viewer screenshots (greenish light, camera locked):
//   orange line H 64-75 (median 70) S 95-160 V 107-174 | floor H ~111-125 S 70-90 (pale green, not a line)
//   blue line H 191-208 (median 197) S 110-170 V 47-143 | green pillar H 82-150
// The old thresholds (orange H 29-52, blue S <= 120) were for the yellow light of October and miss these lines.
const int ORANGE_HUE_LOW  = 55;    // red pillar is H <= 40; floor starts at H ~110
const int ORANGE_HUE_HIGH = 90;    // buildCodeMap tests green first, so a green pillar (H 82+) is never orange
const int ORANGE_MIN_SAT  = 90;    // orange line needs at least this saturation
const int ORANGE_MIN_VAL  = 100;   // orange line needs at least this brightness

const int BLUE_HUE_LOW    = 165;   // floor ~111-125, blue line 191-208
const int BLUE_HUE_HIGH   = 215;   // blue hue range, high end
const int BLUE_MIN_SAT    = 80;    // the floor (S 70-90) and the dark wall stay out
const int BLUE_MAX_SAT    = 200;   // the blue line measured S 110-170; green pillar is never at H 180+
const int BLUE_MIN_VAL    = 40;    // one capture had V 47-79 in the line; the black wall needs the floor-above check

const int MAGENTA_HUE_LOW  = 328;  // magenta hue range, low end (parking wall)
const int MAGENTA_HUE_HIGH = 353;  // magenta hue range, high end
const int MAGENTA_MIN_SAT  = 84;   // magenta needs at least this saturation
const int MAGENTA_MIN_VAL  = 84;   // magenta needs at least this brightness

const int FLOOR_TOP_ROW      = 124;   // the floor part of the picture starts at this row
const int ORANGE_MIN_PIXELS  = 50;    // at ~16 cm the line has only ~120-165 px
const int BLUE_MIN_PIXELS    = 60;    // safe now: BLUE_HUE_LOW = 110 keeps the floor out, floor-above check keeps the wall out
const int LINE_ROW_MIN       = 6;     // a row needs this many line pixels to count as a row of the line
const int LINE_TRIGGER_ROW   = 236;   // row used to trigger the line check (near the bottom of the picture)
const int MAGENTA_MIN_PIXELS = 250;   // a magenta patch needs at least this many pixels
const int MAGENTA_MAJORITY   = 70;    // % of a red+magenta patch that must be magenta to make it a wall
const int TURN_CONFIRM_FRAMES = 3;    // frames needed to confirm a turn (kept from the older code)
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
// Color probe: prints the H, S, V range (min-max) of two small boxes, to calibrate colors with exact camera numbers
const int PROBE_X       = W / 2 + X_CENTER_OFFSET;  // straight-ahead column
const int PROBE_NEAR_Y  = 170;  // row ~5 cm ahead of the camera: put each line / pillar here
const int PROBE_LOW_Y   = 228;  // row at the bottom of the picture (where the false blue appeared)
const int PROBE_HALF_W  = 6;    // box half width, px
const int PROBE_HALF_H  = 2;    // box half height, px
const unsigned long STATUS_EVERY_MS = 500;   // status text to the Serial Monitor (only with STREAM_TO_PC = false)

// ---------- Working area ----------
// Pillars: rows IGNORE_TOP_ROWS .. H-NEAR_ROWS-1. Lines and magenta: FLOOR_TOP_ROW .. H-1.
// Everything is computed once in the rows that cover both.
const int AREA_TOP = (IGNORE_TOP_ROWS < FLOOR_TOP_ROW) ? IGNORE_TOP_ROWS : FLOOR_TOP_ROW;   // first row of the working area
const int AREA_H   = H - AREA_TOP;             // number of rows in the working area
const int BAND_TOP = IGNORE_TOP_ROWS;          // first row of the pillar band
const int BAND_END = H - NEAR_ROWS;          // first row that is NOT in the pillar band

// Block grid, covers the pillar band only
const int BW = W / BLOCK;                      // number of blocks across
const int BH = (BAND_END - BAND_TOP) / BLOCK;  // number of blocks down the pillar band

// Pixel codes in the working area
const uint8_t C_NONE = 0, C_RED = 1, C_GREEN = 2, C_MAGENTA = 3, C_BLUE = 4, C_ORANGE = 5;   // one number per color class

uint8_t  *code;          // AREA_H x W, one code per pixel
uint8_t  *seen;          // BFS visited marks
uint32_t *queue;         // BFS queue (pixel index in the area)
uint8_t  *maskPacked;    // pillar map for the viewer, 2 bits per pixel
uint8_t  *valueMap;      // AREA_H x W, brightness (V) of each pixel, for the floor-above check
uint8_t  *pick;          // AREA_H x W: C_ORANGE / C_BLUE for the pixels of the lines accepted in this picture
uint8_t  *lineMask;      // viewer: accepted orange (1) / blue (2) pixels, 2 bits per pixel
int colTop[W], colBottom[W], colCount[W];   // per column: top pixel row, bottom pixel row, pixel count
int rowCount[H];                            // per row: pixel count (used for the line rows)

uint8_t  blockCode[BW * BH];     // C_NONE / C_RED / C_GREEN per block
uint8_t  blockSeen[BW * BH];     // flood-fill marks for the blocks
uint16_t blockQueue[BW * BH];    // flood-fill queue for the blocks

struct Pillar {                  // one pillar patch found in a picture
  bool found;                    // true if it passed all checks
  uint8_t color;                 // C_RED or C_GREEN
  int left, right, top, bottom;  // edges of the patch, in picture pixels
  int count;                     // number of pixels
  long sumX;                     // sum of the x of all pixels (used for the center)
  bool wasSplit;                 // true if the patch touches the edge of the search area
};

// Floor line found in one picture (orange or blue)
const uint8_t LINE_OK = 0, LINE_NONE = 1, LINE_FEW = 2, LINE_SHORT = 3, LINE_THICK = 4, LINE_WALL = 5;   // reasons for a line result
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
int     historyPos = 0;          // next slot of the history to overwrite
uint8_t confirmedColor = C_NONE; // the color the time filter currently accepts
Pillar  lastSeen[3];             // last real detection of red (1) and green (2)

bool cameraLocked = false;       // true once the camera settings are frozen
unsigned long bootMs = 0;        // time of the power-on (millis)
unsigned long seq = 0;           // message counter, sent to the Nano
unsigned long lastStream = 0, lastStatus = 0, fpsStart = 0;   // times of the last picture, status line and fps window
int framesThisPeriod = 0;        // frames counted in the current fps window
float cameraFps = 0;             // measured frames per second
uint8_t rawColorForStatus = 0;   // color found before the time filter (for the status)

// ---------- Colors (same math as viewer.html) ----------

void readPixel(const uint8_t *buf, int x, int y, int &r, int &g, int &b) {   // get the red, green, blue of one pixel
  int i = (y * W + x) * 2;               // each pixel is 2 bytes (RGB565)
  uint16_t p = (buf[i] << 8) | buf[i + 1];   // join the two bytes into one 16-bit value
  r = ((p >> 11) & 0x1F) << 3;           // top 5 bits = red, scaled to 0-255
  g = ((p >> 5)  & 0x3F) << 2;           // middle 6 bits = green, scaled to 0-255
  b = ( p        & 0x1F) << 3;           // bottom 5 bits = blue, scaled to 0-255
}

void rgbToHsv(int r, int g, int b, int &h, int &s, int &v) {   // convert RGB to hue (0-359), saturation and value (0-255)
  int maxC = max(r, max(g, b));          // biggest channel
  int minC = min(r, min(g, b));          // smallest channel
  int delta = maxC - minC;               // difference between them
  v = maxC;                              // value = brightness = biggest channel
  s = (maxC == 0) ? 0 : (delta * 255) / maxC;   // saturation: 0 for gray, 255 for pure color
  if (delta == 0) { h = 0; return; }     // gray: no hue
  if (maxC == r)      h = 60 * (g - b) / delta;          // hue in the red zone
  else if (maxC == g) h = 60 * (b - r) / delta + 120;    // hue in the green zone
  else                h = 60 * (r - g) / delta + 240;    // hue in the blue zone
  if (h < 0) h += 360;                   // keep the hue between 0 and 359
}

bool inHue(int h, int lo, int hi) {              // lo > hi means the range wraps past 360
  return lo <= hi ? (h >= lo && h <= hi) : (h >= lo || h <= hi);   // true if h is inside the range
}

bool isRed(int h, int s, int v)     { return inHue(h, RED_HUE_LOW, RED_HUE_HIGH) && s >= RED_MIN_SAT && v >= RED_MIN_VAL && v <= RED_MAX_VAL; }   // red pillar color test
bool isGreen(int h, int s, int v)   { return h >= GREEN_HUE_LOW && h <= GREEN_HUE_HIGH && s >= GREEN_MIN_SAT && v >= GREEN_MIN_VAL; }   // green pillar color test
bool isMagenta(int h, int s, int v) { return inHue(h, MAGENTA_HUE_LOW, MAGENTA_HUE_HIGH) && s >= MAGENTA_MIN_SAT && v >= MAGENTA_MIN_VAL; }   // magenta (parking wall) test
bool isOrange(int h, int s, int v)  { return inHue(h, ORANGE_HUE_LOW, ORANGE_HUE_HIGH) && s >= ORANGE_MIN_SAT && v >= ORANGE_MIN_VAL; }   // orange floor line test
bool isBlue(int h, int s, int v)    { return inHue(h, BLUE_HUE_LOW, BLUE_HUE_HIGH) && s >= BLUE_MIN_SAT && s <= BLUE_MAX_SAT && v >= BLUE_MIN_VAL; }   // blue floor line test

// One code per pixel of the working area. Red is tested first (like the page),
// then green, then magenta / orange / blue. The brightness is kept for the floor-above check.
void buildCodeMap(const uint8_t *buf) {
  for (int ay = 0; ay < AREA_H; ay++) {              // each row of the working area
    int y = AREA_TOP + ay;                           // the picture row it is
    for (int x = 0; x < W; x++) {                    // each column
      int r, g, b, h, s, v;                          // channels and HSV values of this pixel
      readPixel(buf, x, y, r, g, b);                 // read the pixel
      rgbToHsv(r, g, b, h, s, v);                    // convert it to HSV
      uint8_t c = C_NONE;                            // default: no color class
      if (isRed(h, s, v))          c = C_RED;        // test the colors in this order
      else if (isGreen(h, s, v))   c = C_GREEN;
      else if (isMagenta(h, s, v)) c = C_MAGENTA;
      else if (isOrange(h, s, v))  c = C_ORANGE;
      else if (isBlue(h, s, v))    c = C_BLUE;
      code[ay * W + x] = c;                          // store the class of this pixel
      valueMap[ay * W + x] = (uint8_t)v;             // store its brightness
    }
  }
}

// Red-or-magenta patches (touching pixels): if at least MAGENTA_MAJORITY % of a patch is
// magenta, the whole patch is a parking wall -> its red pixels are no pillar any more.
void mergeRedMagenta() {
  memset(seen, 0, AREA_H * W);                       // clear the visited marks
  for (int start = 0; start < AREA_H * W; start++) { // look for a new red or magenta patch
    uint8_t c0 = code[start];                        // class of this pixel
    if ((c0 != C_RED && c0 != C_MAGENTA) || seen[start]) continue;   // skip other colors and already-visited pixels
    int head = 0, tail = 0, nRed = 0, nMg = 0;       // queue pointers and counts of red / magenta pixels
    queue[tail++] = start; seen[start] = 1;          // start the flood fill here
    while (head < tail) {                            // flood fill over touching pixels
      int i = queue[head++];                         // take the next pixel
      if (code[i] == C_RED) nRed++; else nMg++;      // count it as red or magenta
      int x = i % W, ay = i / W;                     // its column and row in the area
      int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1,   // neighbors: right, left,
                    (ay + 1 < AREA_H) ? i + W : -1, (ay > 0) ? i - W : -1 };   // down, up (-1 = outside)
      for (int k = 0; k < 4; k++) {                  // check the 4 neighbors
        int j = nb[k];
        if (j < 0 || seen[j] || (code[j] != C_RED && code[j] != C_MAGENTA)) continue;   // skip the outside, visited or other colors
        seen[j] = 1; queue[tail++] = j;              // add it to the patch
      }
    }
    bool magenta = (long)nMg * 100 >= (long)MAGENTA_MAJORITY * (nRed + nMg);   // is the patch mostly magenta?
    for (int k = 0; k < tail; k++) {                 // rewrite every pixel of the patch
      int i = queue[k];
      code[i] = magenta ? C_MAGENTA : (code[i] == C_RED ? C_RED : C_NONE);   // magenta patch -> wall; red stays red; magenta pixels of a red patch -> none
    }
  }
}

// Is this area pixel a pillar pixel of this color? (only inside the pillar band)
inline bool pillarPixel(int i, uint8_t color) {
  int y = AREA_TOP + i / W;                        // the picture row of this pixel
  return y >= BAND_TOP && y < BAND_END && code[i] == color;   // true if it is in the band and has this color
}

void pillarPosition(int x, int width, float &aheadCm, float &sideCm);   // defined below

// Does this patch make sense as a real pillar? Rejects what cannot be one: cut by the picture
// edge, closer / farther than the camera can measure, or outside the corridor.
bool plausible(const Pillar &p) {
  if (!USE_PLAUSIBILITY) return true;              // check disabled: accept everything
  if (REJECT_AT_EDGE && (p.left <= 0 || p.right >= W - 1)) return false;   // touches the side of the picture: width is wrong
  int width = p.right - p.left + 1;                // width in pixels
  if (USE_SHAPE_FILTER) {                          // shape check
    int height = p.bottom - p.top + 1;             // height in pixels
    if ((long)p.count * 100 < (long)MIN_FILL_PERCENT * width * height) return false;   // too many holes: not a solid pillar
  }
  int x = (int)(p.sumX / p.count) - W / 2;         // center x of the patch, relative to the middle of the picture
  float aheadCm, sideCm;                           // real distance and side, in cm
  pillarPosition(x, width, aheadCm, sideCm);       // convert the picture numbers to cm
  if (aheadCm < MIN_AHEAD_CM || aheadCm > MAX_AHEAD_CM) return false;   // too close or too far
  if (fabs(sideCm) > MAX_SIDE_CM) return false;    // outside the corridor
  return true;                                     // passed every check
}

// From the per-column numbers (colTop / colBottom / colCount) of one patch, take only the
// FRONT pillar: the columns whose bottom is within BOTTOM_TOL of the lowest one.
Pillar frontPillarOfBlob(uint8_t color, int minX, int maxX) {
  Pillar p = {false, color, 0, 0, 0, 0, 0, 0, false};   // empty result for this color
  int best = -1;                                   // column with the lowest bottom
  for (int x = minX; x <= maxX; x++)               // look at every column of the patch
    if (colCount[x] > 0 && (best < 0 || colBottom[x] > colBottom[best])) best = x;
  if (best < 0) return p;                          // no column: nothing found
  int lowest = colBottom[best];                    // the lowest row of the patch
  int left = best, right = best;                   // grow the front part left and right
  while (left - 1 >= minX && colCount[left - 1] > 0 && colBottom[left - 1] >= lowest - BOTTOM_TOL) left--;     // extend to the left
  while (right + 1 <= maxX && colCount[right + 1] > 0 && colBottom[right + 1] >= lowest - BOTTOM_TOL) right++;   // extend to the right
  p.left = left; p.right = right; p.bottom = lowest; p.top = H;   // store the edges (top starts at the bottom of the picture)
  for (int x = left; x <= right; x++) {            // add up the columns of the front pillar
    if (colTop[x] < p.top) p.top = colTop[x];      // highest pixel
    p.count += colCount[x];                        // total pixels
    p.sumX  += (long)x * colCount[x];              // weighted sum of x for the center
  }
  p.found = p.count >= (color == C_RED ? RED_MIN_PIXELS : GREEN_MIN_PIXELS);   // enough pixels?
  p.wasSplit = (left > minX || right < maxX);      // true if the front part is smaller than the whole patch
  return p;
}

bool isCloser(const Pillar &a, const Pillar &b) {  // is pillar a closer to the car than pillar b?
  if (!b.found) return true;                       // anything beats "nothing"
  if (a.bottom != b.bottom) return a.bottom > b.bottom;   // lower in the picture = closer
  return a.count > b.count;                        // same height: the bigger one wins
}

// ---------- Pillars, pixel by pixel (v1 method, used when USE_BLOCKS = false) ----------
Pillar findClosestPillar() {
  memset(seen, 0, AREA_H * W);                     // clear the visited marks
  Pillar closest = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};   // closest pillar found so far
  for (int start = 0; start < AREA_H * W; start++) {           // look for a red or green pixel
    uint8_t color = code[start];
    if ((color != C_RED && color != C_GREEN) || seen[start] || !pillarPixel(start, color)) continue;   // skip other pixels
    for (int i = 0; i < W; i++) { colTop[i] = H; colBottom[i] = -1; colCount[i] = 0; }   // reset the column data
    int minX = W, maxX = -1, total = 0, head = 0, tail = 0;   // patch limits, pixel count and queue pointers
    queue[tail++] = start; seen[start] = 1;        // start the flood fill
    while (head < tail) {                          // flood fill over touching pixels
      int i = queue[head++];                       // next pixel
      int x = i % W, y = AREA_TOP + i / W;         // its column and picture row
      total++; colCount[x]++;                      // count it
      if (y < colTop[x])    colTop[x] = y;         // highest pixel of this column
      if (y > colBottom[x]) colBottom[x] = y;      // lowest pixel of this column
      if (x < minX) minX = x;                      // leftmost column so far
      if (x > maxX) maxX = x;                      // rightmost column so far
      int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1, i + W, i - W };   // the 4 neighbors
      for (int k = 0; k < 4; k++) {                // check each neighbor
        int j = nb[k];
        if (j < 0 || j >= AREA_H * W || seen[j] || !pillarPixel(j, color)) continue;   // skip outside, visited or other pixels
        seen[j] = 1; queue[tail++] = j;            // add it to the patch
      }
    }
    if (total < (color == C_RED ? RED_MIN_PIXELS : GREEN_MIN_PIXELS)) continue;   // too small: noise
    Pillar p = frontPillarOfBlob(color, minX, maxX);   // keep only the front part of this patch
    if (p.found && plausible(p) && isCloser(p, closest)) closest = p;   // keep it if it is the closest so far
  }
  return closest;                                  // the closest valid pillar (or none)
}

// ---------- Pillars on a grid of blocks ----------
// A block (BLOCK x BLOCK pixels, only inside the pillar band) is red / green when at least
// BLOCK_MIN_PIXELS of its pixels have that color. Isolated noise pixels and small holes vanish.
void buildBlocks() {
  for (int by = 0; by < BH; by++) {                // each block row
    for (int bx = 0; bx < BW; bx++) {              // each block column
      int nRed = 0, nGreen = 0;                    // red and green pixels inside this block
      for (int dy = 0; dy < BLOCK; dy++) {         // each pixel row of the block
        const uint8_t *row = code + (BAND_TOP + by * BLOCK + dy - AREA_TOP) * W + bx * BLOCK;   // start of that row inside the block
        for (int dx = 0; dx < BLOCK; dx++) {       // each pixel of the row
          if (row[dx] == C_RED) nRed++;            // count red
          else if (row[dx] == C_GREEN) nGreen++;   // count green
        }
      }
      uint8_t c = C_NONE;                          // default: no block color
      if (nRed >= BLOCK_MIN_PIXELS && nRed >= nGreen) c = C_RED;   // enough red pixels: red block
      else if (nGreen >= BLOCK_MIN_PIXELS)            c = C_GREEN; // enough green pixels: green block
      blockCode[by * BW + bx] = c;                 // store the block color
    }
  }
}

Pillar findClosestPillarBlocks() {
  Pillar closest = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};   // closest pillar found so far
  memset(blockSeen, 0, sizeof(blockSeen));         // clear the block visited marks
  for (int start = 0; start < BW * BH; start++) {  // look for a new colored block
    uint8_t color = blockCode[start];
    if (color == C_NONE || blockSeen[start]) continue;   // skip empty and visited blocks

    // 1) connected blocks of this color
    int head = 0, tail = 0;                        // queue pointers
    int minBx = BW, maxBx = -1, minBy = BH, maxBy = -1;   // block-level limits of the patch
    blockQueue[tail++] = start; blockSeen[start] = 1;     // start the flood fill
    while (head < tail) {                          // flood fill over touching blocks
      int i = blockQueue[head++];                  // next block
      int bx = i % BW, by = i / BW;                // its block column and block row
      if (bx < minBx) minBx = bx;                  // update the limits
      if (bx > maxBx) maxBx = bx;
      if (by < minBy) minBy = by;
      if (by > maxBy) maxBy = by;
      int nb[4] = { (bx + 1 < BW) ? i + 1 : -1, (bx > 0) ? i - 1 : -1,   // the 4 neighbor blocks
                    (by + 1 < BH) ? i + BW : -1, (by > 0) ? i - BW : -1 };
      for (int k = 0; k < 4; k++) {                // check each neighbor
        int j = nb[k];
        if (j < 0 || blockSeen[j] || blockCode[j] != color) continue;   // skip outside, visited or other colors
        blockSeen[j] = 1; blockQueue[tail++] = j;  // add it to the patch
      }
    }
    if (tail < MIN_BLOCKS) continue;               // too few blocks: noise

    // 2) refine with the real pixels around the patch: exact left / right edges, bottom row
    int x0 = max(0, minBx * BLOCK - EDGE_PAD);     // left edge of the search, with padding
    int x1 = min(W - 1, maxBx * BLOCK + BLOCK - 1 + EDGE_PAD);   // right edge of the search
    int y0 = max(BAND_TOP, BAND_TOP + minBy * BLOCK - (BLOCK - 1));   // top edge of the search
    int y1 = min(BAND_END - 1, BAND_TOP + (maxBy + 1) * BLOCK - 1 + (BLOCK - 1));   // bottom edge of the search
    for (int x = 0; x < W; x++) { colTop[x] = H; colBottom[x] = -1; colCount[x] = 0; }   // reset the column data
    for (int y = y0; y <= y1; y++) {               // each row of the search area
      const uint8_t *row = code + (y - AREA_TOP) * W;   // start of that row in the code map
      for (int x = x0; x <= x1; x++) {             // each column of the search area
        if (row[x] != color) continue;             // only pixels of this color
        colCount[x]++;                             // count it in this column
        if (y < colTop[x])    colTop[x] = y;       // highest pixel of the column
        if (y > colBottom[x]) colBottom[x] = y;    // lowest pixel of the column
      }
    }
    for (int x = x0; x <= x1; x++)                 // remove columns with too few pixels
      if (colCount[x] < COL_MIN_PIXELS) { colCount[x] = 0; colBottom[x] = -1; colTop[x] = H; }
    // A thin empty stripe inside the patch (a shiny reflection, a bad column) must not cut the
    // pillar in two: columns of an empty run up to GAP_FILL wide, with pillar on both sides,
    // copy their left neighbor.
    for (int x = x0 + 1; x <= x1; x++) {           // look for gaps between two pillar columns
      if (colCount[x] != 0 || colCount[x - 1] == 0) continue;   // a gap must start right after a pillar column
      int e = x;                                   // the last column of the gap
      while (e + 1 <= x1 && colCount[e + 1] == 0) e++;   // find where the gap ends
      if (e + 1 <= x1 && e - x + 1 <= GAP_FILL && abs(colBottom[e + 1] - colBottom[x - 1]) <= BOTTOM_TOL)   // small gap, same bottom on both sides
        for (int k = x; k <= e; k++) { colCount[k] = 1; colBottom[k] = colBottom[x - 1]; colTop[k] = colTop[x - 1]; }   // fill the gap from the left neighbor
      x = e;                                       // continue after the gap
    }

    Pillar p = frontPillarOfBlob(color, x0, x1);   // keep only the front part of this patch
    if (p.found && plausible(p) && isCloser(p, closest)) closest = p;   // keep it if it is the closest so far
  }
  return closest;                                  // the closest valid pillar (or none)
}

// ---------- Time filter: ON_NEED of the last HISTORY pictures ----------
// Returns the pillar to report. While a pillar is confirmed but missing in the current
// picture, the last real detection of that color is repeated (at most a few pictures old).
Pillar timeFilter(const Pillar &raw) {
  history[historyPos] = raw.found ? raw.color : C_NONE;   // save what this picture showed
  historyPos = (historyPos + 1) % HISTORY;        // move to the next slot (circular)
  if (raw.found) lastSeen[raw.color] = raw;       // remember the last real detection of this color

  int nNone = 0, nRed = 0, nGreen = 0;            // how many pictures of the history show each
  for (int i = 0; i < HISTORY; i++) {
    if (history[i] == C_RED) nRed++;              // red picture
    else if (history[i] == C_GREEN) nGreen++;     // green picture
    else nNone++;                                 // nothing
  }
  if (nRed >= ON_NEED)        confirmedColor = C_RED;     // enough red pictures: confirm red
  else if (nGreen >= ON_NEED) confirmedColor = C_GREEN;   // enough green pictures: confirm green
  else if (nNone >= OFF_NEED) confirmedColor = C_NONE;    // enough empty pictures: drop the pillar
  // otherwise: keep the previous decision

  Pillar none = {false, C_NONE, 0, 0, 0, 0, 0, 0, false};   // empty result
  if (confirmedColor == C_NONE) return none;      // nothing confirmed
  Pillar p = lastSeen[confirmedColor];            // the last real detection of the confirmed color
  p.found = p.count > 0;                          // it is valid if it had pixels
  return p;
}

// ---------- Floor lines (orange and blue) ----------
// A floor line is a long, thin stripe. For each color: take the biggest patch below
// LINE_TOP_ROW, check it is long and thin and has floor above it, then measure where it is.

// Walks the patch of this color that contains 'start' (only rows from firstRow down).
// Only touching pixels (4 neighbors) belong to the same patch.
// The pixels end up in queue[0 .. return value - 1].
int walkPatch(int start, uint8_t color, int firstRow) {
  int head = 0, tail = 0;                          // queue pointers
  queue[tail++] = start; seen[start] = 1;          // start the flood fill
  while (head < tail) {                            // flood fill over touching pixels
    int i = queue[head++];                         // next pixel
    int x = i % W, ay = i / W;                     // its column and area row
    int nb[4] = { (x + 1 < W) ? i + 1 : -1, (x > 0) ? i - 1 : -1,   // right, left,
                  (ay + 1 < AREA_H) ? i + W : -1, (ay > firstRow) ? i - W : -1 };   // down, up (not above firstRow)
    for (int k = 0; k < 4; k++) {                  // check the 4 neighbors
      int j = nb[k];
      if (j < 0 || seen[j] || code[j] != color) continue;   // skip outside, visited or other colors
      seen[j] = 1; queue[tail++] = j;              // add it to the patch
    }
  }
  return tail;                                     // number of pixels in the patch
}

// Floor-above check. For each column of the line: look ABOVE_FROM..ABOVE_TO rows above the
// line's top edge and take the darkest of those pixels. If it is at least FLOOR_MIN_VAL bright,
// there is white floor above in that column. A painted floor line has floor above it;
// the bottom edge of the black wall has dark wall above it.
void checkFloorAbove(int minX, int maxX, int &checked, int &floorCols) {
  checked = 0; floorCols = 0;                      // reset the counters
  for (int x = minX; x <= maxX; x++) {             // each column of the line
    if (colCount[x] == 0) continue;                // no line pixel here: nothing to check
    int yFrom = colTop[x] - ABOVE_TO;              // highest row looked at
    int yTo   = colTop[x] - ABOVE_FROM;            // lowest row looked at
    if (yFrom < AREA_TOP) continue;                // outside the working area: cannot check
    int darkest = 255;                             // the darkest brightness found above the line
    for (int y = yFrom; y <= yTo; y++) {           // look at the rows above the line
      int v = valueMap[(y - AREA_TOP) * W + x];    // brightness of that pixel
      if (v < darkest) darkest = v;                // keep the darkest one
    }
    checked++;                                     // this column could be checked
    if (darkest >= FLOOR_MIN_VAL) floorCols++;     // bright enough: white floor above
  }
}

LineInfo findLine(uint8_t color, int minPixels) {
  LineInfo line = {false, -1, -1, 0, 0, 0, LINE_NONE, 0, 0};   // empty result, default reason "none"
  const int firstRow = LINE_TOP_ROW - AREA_TOP;    // first area row searched

  // 1) The biggest patch of this color
  memset(seen, 0, AREA_H * W);                     // clear the visited marks
  int bestStart = -1, bestN = 0;                   // the biggest patch: start pixel and size
  for (int start = firstRow * W; start < AREA_H * W; start++) {   // look at every pixel below the first row
    if (code[start] != color || seen[start]) continue;            // skip other colors and visited pixels
    int n = walkPatch(start, color, firstRow);     // measure the patch that starts here
    if (n > bestN) { bestN = n; bestStart = start; }   // keep it if it is the biggest so far
  }
  line.count = bestN;                              // pixels in the biggest patch
  if (bestStart < 0) return line;                  // LINE_NONE: no patch at all
  if (bestN < minPixels) { line.reason = LINE_FEW; return line; }   // too few pixels

  // 2) Walk it again and measure it: box, pixels per column / per row, top and bottom of each column
  memset(seen, 0, AREA_H * W);                     // clear the visited marks again
  int n = walkPatch(bestStart, color, firstRow);   // walk the biggest patch; its pixels are now in queue[]
  for (int x = 0; x < W; x++) { colCount[x] = 0; colBottom[x] = -1; colTop[x] = H; }   // reset the column data
  memset(rowCount, 0, sizeof(rowCount));           // reset the row counts
  int minX = W, maxX = -1, minY = H, maxY = -1;    // the box of the patch
  for (int k = 0; k < n; k++) {                    // each pixel of the patch
    int i = queue[k];
    int x = i % W, y = AREA_TOP + i / W;           // its column and picture row
    colCount[x]++;                                 // count it in its column
    if (y > colBottom[x]) colBottom[x] = y;        // lowest pixel of the column
    if (y < colTop[x])    colTop[x] = y;           // highest pixel of the column
    rowCount[y]++;                                 // count it in its row
    if (x < minX) minX = x;                        // update the box
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  }
  int boxW = maxX - minX + 1, boxH = maxY - minY + 1;   // width and height of the box
  line.length = (int)(sqrtf((float)(boxW * boxW + boxH * boxH)) + 0.5f);   // diagonal of the box, rounded
  line.thick  = (n + line.length / 2) / line.length;      // average thickness = pixels / length (rounded)

  // 3) Is it a line? Long and thin. (A pillar or a blob is short and fat.)
  if (line.length < LINE_MIN_LENGTH) { line.reason = LINE_SHORT; return line; }   // too short
  if (line.thick  > LINE_MAX_THICK)  { line.reason = LINE_THICK; return line; }   // too fat

  // 4) Is it on the floor? White floor above it (the black wall's bottom edge has wall above)
  checkFloorAbove(minX, maxX, line.aboveChecked, line.aboveFloor);   // count the columns with floor above
  if (line.aboveChecked < ABOVE_MIN_COLS ||
      (long)line.aboveFloor * 100 < (long)ABOVE_MIN_PERCENT * line.aboveChecked) {   // too few columns with floor above
    line.reason = LINE_WALL;                       // it is the wall's edge, not a floor line
    return line;
  }

  // 5) Where is it? Lowest row overall, and lowest row straight ahead (center strip)
  for (int y = H - 1; y >= LINE_TOP_ROW; y--) if (rowCount[y] >= LINE_ROW_MIN) { line.lowest = y; break; }   // lowest row with enough line pixels
  int cx = W / 2 + X_CENTER_OFFSET;                // real straight-ahead column
  for (int x = max(0, cx - LINE_CENTER_HALF); x <= min(W - 1, cx + LINE_CENTER_HALF); x++)   // the center strip
    if (colCount[x] >= LINE_COL_MIN && colBottom[x] > line.centerRow) line.centerRow = colBottom[x];   // lowest line row in the strip
  line.found  = line.lowest >= 0;                  // found if a row was found
  line.reason = line.found ? LINE_OK : LINE_FEW;   // reason for the status print
  // the accepted line: keep its pixels for the viewer (queue[0..n-1] is this patch)
  if (line.found) for (int k = 0; k < n; k++) pick[queue[k]] = color;   // mark the accepted pixels for the viewer
  return line;
}

const char *lineReason(const LineInfo &l) {        // text description of why a line was accepted or rejected
  switch (l.reason) {
    case LINE_OK:    return "yes";                 // accepted
    case LINE_FEW:   return "no (few pixels)";     // too few pixels
    case LINE_SHORT: return "no (too short)";      // too short
    case LINE_THICK: return "no (too thick)";      // too fat
    case LINE_WALL:  return "no (wall above)";     // no floor above it
    default:         return "no";                  // anything else
  }
}

// ---------- Message to the Nano ----------
int roundInt(float v) { return v >= 0 ? (int)(v + 0.5f) : (int)(v - 0.5f); }   // round to the nearest whole number (also for negatives)

// Real position of a pillar from its center x (px from the middle) and its width (px)
void pillarPosition(int x, int width, float &aheadCm, float &sideCm) {
  float xc = x - X_CENTER_OFFSET;                  // px from the real straight-ahead line
  float a  = atanf(fabs(xc) / FOCAL_PX);           // angle from straight ahead (radians)
  float seenCm = PILLAR_CM * (cosf(a) + sinf(a));  // two faces seen from the side (the pillar looks wider at an angle)
  aheadCm = FOCAL_PX * seenCm / width;             // distance: pinhole model, distance = focal * real size / size in pixels
  sideCm  = aheadCm * xc / FOCAL_PX;               // sideways distance from the same triangle
}

// $color,x,bottom,correct,blueFound,blueRow,seq,width,distCm,latCm,orangeFound,orangeRow,blueCenterRow,orangeCenterRow
void sendToNano(const Pillar &p, const LineInfo &blue, const LineInfo &orange) {
  int color = 0, x = 0, bottom = -1, correct = 0, width = 0, distCm = -1, latCm = 0;   // default values: no pillar
  if (p.found) {                                   // a pillar was found
    color  = (p.color == C_RED) ? 1 : 2;           // 1 = red, 2 = green
    x      = (int)(p.sumX / p.count) - W / 2;      // center x relative to the middle of the picture
    bottom = p.bottom;                             // lowest row of the pillar
    width  = p.right - p.left + 1;                 // width in pixels
    float aheadCm, sideCm;                         // real distance and side
    pillarPosition(x, width, aheadCm, sideCm);     // convert to cm
    distCm = roundInt(aheadCm);                    // distance, whole cm
    latCm  = roundInt(sideCm);                     // side, whole cm
    // red is passed on its right -> the pillar must end up on the car's LEFT (latCm negative)
    correct = (p.color == C_RED) ? (latCm <= -SAFE_LAT_CM) : (latCm >= SAFE_LAT_CM);   // 1 if the car is far enough on the right side
  }
  // The first 10 fields are the same as before, so the old Nano sketches still work.
  Serial1.printf("$%d,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%d\n", color, x, bottom, correct,   // send the message
                 blue.found ? 1 : 0, blue.found ? blue.lowest : -1, seq, width, distCm, latCm,   // blue line fields
                 orange.found ? 1 : 0, orange.found ? orange.lowest : -1,                        // orange line fields
                 blue.found ? blue.centerRow : -1, orange.found ? orange.centerRow : -1);        // center rows
}

// ---------- Picture to viewer.html (same packet as pillar_viewer) ----------
// Serial.write() may accept FEWER bytes than asked when the USB buffer is full.
// writeAll() keeps offering the rest until everything is accepted; if the computer stops
// reading for more than maxWaitMs it gives up on this picture (the next one starts clean).
bool writeAll(const uint8_t *data, size_t len, unsigned long maxWaitMs) {
  size_t sent = 0;                                 // bytes sent so far
  unsigned long t0 = millis();                     // time of the last progress
  while (sent < len) {                             // until everything is sent
    size_t n = Serial.write(data + sent, len - sent);   // try to send the rest
    if (n == 0) {                                  // the buffer is full: nothing sent
      if (millis() - t0 > maxWaitMs) return false; // waited too long: give up
      delay(1);                                    // wait a moment and try again
    } else {
      sent += n;                                   // count what was sent
      t0 = millis();                               // progress: reset the timer
    }
  }
  return true;                                     // everything was sent
}

void putU16(uint8_t *h, int &k, uint16_t v) { h[k++] = v & 0xFF; h[k++] = v >> 8; }   // write a 16-bit value, low byte first

void setMaskPixel(int x, int y, uint8_t c) {       // set the 2-bit code of one pixel in the pillar mask
  int pi = y * W + x;                              // index of the pixel in the whole picture
  maskPacked[pi >> 2] |= (c & 0x03) << ((pi & 3) * 2);   // 4 pixels per byte, 2 bits each
}

void setLinePixel(int x, int y, uint8_t c) {       // set the 2-bit code of one pixel in the line mask
  int pi = y * W + x;                              // index of the pixel
  lineMask[pi >> 2] |= (c & 0x03) << ((pi & 3) * 2);   // 4 pixels per byte, 2 bits each
}

uint16_t lineU16(int v) { return v < 0 ? 0xFFFF : (uint16_t)v; }   // -1 = 0xFFFF in the packet

// Packet to viewer.html: header (37 bytes) + picture (153600) + pillar mask (19200) + line mask (19200)
void sendPacket(const uint8_t *picture, const Pillar &p, const LineInfo &blue, const LineInfo &orange) {
  memset(lineMask, 0, W * H / 4);                  // clear the line mask
  for (int ay = 0; ay < AREA_H; ay++)              // each row of the area
    for (int x = 0; x < W; x++) {                  // each column
      uint8_t c = pick[ay * W + x];                // accepted line class of this pixel
      if (c == C_ORANGE)    setLinePixel(x, AREA_TOP + ay, 1);   // orange = 1
      else if (c == C_BLUE) setLinePixel(x, AREA_TOP + ay, 2);   // blue = 2
    }
  memset(maskPacked, 0, W * H / 4);                // clear the pillar mask
  if (USE_BLOCKS) {                                // the viewer shows the blocks the car really uses
    for (int by = 0; by < BH; by++)                // each block row
      for (int bx = 0; bx < BW; bx++) {            // each block column
        uint8_t c = blockCode[by * BW + bx];       // color of the block
        if (c == C_NONE) continue;                 // empty block: nothing to draw
        for (int dy = 0; dy < BLOCK; dy++)         // paint every pixel of the block in the mask
          for (int dx = 0; dx < BLOCK; dx++)
            setMaskPixel(bx * BLOCK + dx, BAND_TOP + by * BLOCK + dy, c);
      }
  } else {                                         // pixel mode: show the pillar pixels themselves
    for (int i = 0; i < AREA_H * W; i++) {         // each pixel of the area
      uint8_t c = code[i];                         // its class
      if ((c == C_RED || c == C_GREEN) && pillarPixel(i, c)) setMaskPixel(i % W, AREA_TOP + i / W, c);   // red or green pillar pixel
    }
  }
  uint8_t hdr[40];                                 // header buffer
  int k = 0;                                       // position in the header
  hdr[k++] = 0xAA; hdr[k++] = 0x55; hdr[k++] = 0xC3; hdr[k++] = 0x3C;   // magic bytes: start of packet
  hdr[k++] = 1;                                    // packet version
  putU16(hdr, k, W); putU16(hdr, k, H);            // picture width and height
  hdr[k++] = 1;                                    // bit0 = 1: ready (v3.1 has no calibration step)
  hdr[k++] = p.found ? 1 : 0;                      // pillar found flag
  hdr[k++] = p.color;                              // pillar color
  putU16(hdr, k, (uint16_t)p.left);  putU16(hdr, k, (uint16_t)p.right);   // pillar left and right edges
  putU16(hdr, k, (uint16_t)p.top);   putU16(hdr, k, (uint16_t)p.bottom);  // pillar top and bottom
  putU16(hdr, k, (uint16_t)p.count);               // pillar pixel count
  hdr[k++] = p.wasSplit ? 1 : 0;                   // split flag
  putU16(hdr, k, (uint16_t)(cameraFps * 10));      // fps times 10
  // floor lines, bytes 25-36
  hdr[k++] = orange.found ? 1 : 0;  hdr[k++] = orange.reason;   // orange found flag and reason
  putU16(hdr, k, lineU16(orange.found ? orange.lowest : -1)); putU16(hdr, k, lineU16(orange.found ? orange.centerRow : -1));   // orange lowest row and center row
  hdr[k++] = blue.found ? 1 : 0;    hdr[k++] = blue.reason;     // blue found flag and reason
  putU16(hdr, k, lineU16(blue.found ? blue.lowest : -1));     putU16(hdr, k, lineU16(blue.found ? blue.centerRow : -1));   // blue lowest row and center row
  const unsigned long WAIT_MS = 150;               // longest wait for the computer to read each part
  if (!writeAll(hdr, k, WAIT_MS)) return;          // send the header; stop if it fails
  if (!writeAll(picture, W * H * 2, WAIT_MS)) return;   // send the picture (2 bytes per pixel)
  if (!writeAll(maskPacked, W * H / 4, WAIT_MS)) return;   // send the pillar mask
  writeAll(lineMask, W * H / 4, WAIT_MS);          // send the line mask
}

// ---------- Camera ----------
// Freeze white balance, exposure and gain at the values the camera found by itself.
void lockCamera() {
  sensor_t *cam = esp_camera_sensor_get();         // the sensor settings
  cam->set_whitebal(cam, 0);                       // turn off automatic white balance
  cam->set_awb_gain(cam, 0);                       // turn off automatic white balance gain
  cam->set_exposure_ctrl(cam, 0);                  // turn off automatic exposure
  cam->set_aec2(cam, 0);                           // turn off the second exposure control
  cam->set_gain_ctrl(cam, 0);                      // turn off automatic gain
  cameraLocked = true;                             // remember that it is locked
  if (!(STREAM_TO_PC && Serial)) Serial.println("\nCAMERA LOCKED: white balance, exposure and gain are frozen.");   // tell the Serial Monitor (not while streaming)
}

bool startCamera() {
  camera_config_t config = {};                     // camera configuration, all fields zero first
  config.ledc_channel = LEDC_CHANNEL_0;            // clock channel used for XCLK
  config.ledc_timer   = LEDC_TIMER_0;              // clock timer used for XCLK
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;   // map the data pins
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;                // clock pin
  config.pin_pclk  = PCLK_GPIO_NUM;                // pixel clock pin
  config.pin_vsync = VSYNC_GPIO_NUM;               // frame sync pin
  config.pin_href  = HREF_GPIO_NUM;                // line sync pin
  config.pin_sccb_sda = SIOD_GPIO_NUM;             // settings bus data
  config.pin_sccb_scl = SIOC_GPIO_NUM;             // settings bus clock
  config.pin_pwdn  = PWDN_GPIO_NUM;                // power-down pin (not used)
  config.pin_reset = RESET_GPIO_NUM;               // reset pin (not used)
  config.xclk_freq_hz = XCLK_FREQ_HZ;              // clock frequency
  config.pixel_format = PIXFORMAT_RGB565;          // 16 bits per pixel, the format the code reads
  config.frame_size   = FRAMESIZE_QVGA;            // 320x240
  config.grab_mode    = CAMERA_GRAB_LATEST;        // always take the newest frame
  config.fb_location  = CAMERA_FB_IN_PSRAM;        // store the frames in PSRAM (external memory)
  config.fb_count     = 2;                         // two frame buffers
  if (esp_camera_init(&config) != ESP_OK) return false;   // start the camera; fail if it does not answer

  sensor_t *cam = esp_camera_sensor_get();         // the sensor settings
  cam->set_hmirror(cam, MIRROR_HORIZONTAL ? 1 : 0);   // flips done by the sensor (free)
  cam->set_vflip(cam, FLIP_VERTICAL ? 1 : 0);      // vertical flip (off)
  cam->set_saturation(cam, 2);                     // colors a bit more vivid, less grain
  cam->set_lenc(cam, LENS_CORRECTION ? 1 : 0);     // lens shading correction (edge tint)
  cam->set_whitebal(cam, 1);                       // automatic until the lock
  cam->set_awb_gain(cam, 1);                       // automatic white balance gain until the lock
  cam->set_exposure_ctrl(cam, 1);                  // automatic exposure until the lock
  cam->set_aec2(cam, 1);                           // second exposure control until the lock
  cam->set_gain_ctrl(cam, 1);                      // automatic gain until the lock
  cam->set_gainceiling(cam, GAINCEILING_4X);       // highest gain allowed: 4x
  return true;                                     // camera ready
}

void setup() {                                     // runs once at power-on
  Serial.setTxBufferSize(16384);                   // large USB output buffer for the pictures
  Serial.begin(115200);                            // USB serial to the computer
  Serial.setTxTimeoutMs(0);                        // never wait for the computer
  Serial1.begin(NANO_BAUD, SERIAL_8N1, NANO_RX_PIN, NANO_TX_PIN);   // TX only, to the Nano
  delay(1500);                                     // short pause so the serial port is ready
  Serial.println("OBSTACLE CAMERA v3.1: starting...");

  code       = (uint8_t *)ps_malloc(AREA_H * W);   // reserve memory for the code map (PSRAM)
  seen       = (uint8_t *)ps_malloc(AREA_H * W);   // reserve memory for the visited marks
  queue      = (uint32_t *)ps_malloc(AREA_H * W * sizeof(uint32_t));   // reserve memory for the flood-fill queue
  maskPacked = (uint8_t *)ps_malloc(W * H / 4);    // reserve memory for the pillar mask
  valueMap   = (uint8_t *)ps_malloc(AREA_H * W);   // reserve memory for the brightness map
  pick       = (uint8_t *)ps_malloc(AREA_H * W);   // reserve memory for the accepted-line marks
  lineMask   = (uint8_t *)ps_malloc(W * H / 4);    // reserve memory for the line mask
  if (!psramFound() || !code || !seen || !queue || !maskPacked || !valueMap || !pick || !lineMask || !startCamera()) {   // check memory and camera
    while (true) { Serial.println("Camera or PSRAM did not start. Check Tools -> PSRAM: OPI PSRAM, then RESET."); delay(3000); }   // stop here and keep warning
  }
  Serial.printf("OBSTACLE CAMERA v3.1: OK, pillar band rows %d-%d (%d x %d blocks), lines from row %d\n",
                BAND_TOP, BAND_END - 1, BW, BH, LINE_TOP_ROW);   // show the working setup
  Serial.printf("lock=%d blocks=%d timeFilter=%d (lock in %lu ms)\n",
                LOCK_CAMERA, USE_BLOCKS, USE_TIME_FILTER, LOCK_AFTER_MS);   // show the switches
  bootMs = millis();                               // remember the power-on time (for the lock)
  fpsStart = millis();                             // start the fps window
}

// Prints the H, S, V range (min-max) of the pixels in a small box centered at (cx, cy)
void printProbe(const char *name, const uint8_t *buf, int cx, int cy) {
  int hMin = 999, hMax = -1, sMin = 999, sMax = -1, vMin = 999, vMax = -1;   // ranges, starting outside the normal values
  for (int y = cy - PROBE_HALF_H; y <= cy + PROBE_HALF_H; y++) {   // each row of the box
    for (int x = cx - PROBE_HALF_W; x <= cx + PROBE_HALF_W; x++) { // each column of the box
      int r, g, b, h, s, v;                        // channels and HSV values
      readPixel(buf, x, y, r, g, b);               // read the pixel
      rgbToHsv(r, g, b, h, s, v);                  // convert to HSV
      if (h < hMin) hMin = h;                      // update the hue range
      if (h > hMax) hMax = h;
      if (s < sMin) sMin = s;                      // update the saturation range
      if (s > sMax) sMax = s;
      if (v < vMin) vMin = v;                      // update the value range
      if (v > vMax) vMax = v;
    }
  }
  Serial.printf("  PROBE %-6s (x %d, y %d): H %d-%d  S %d-%d  V %d-%d\n",   // print the ranges
                name, cx, cy, hMin, hMax, sMin, sMax, vMin, vMax);
}

void loop() {                                      // runs again and again
  if (LOCK_CAMERA && !cameraLocked && millis() - bootMs >= LOCK_AFTER_MS) lockCamera();   // freeze the camera once it has adjusted

  camera_fb_t *frame = esp_camera_fb_get();        // get the newest picture
  if (frame == NULL) { delay(20); return; }        // no picture yet: wait a moment
  if (frame->width != W || frame->height != H) { esp_camera_fb_return(frame); delay(200); return; }   // wrong size: skip it

  memset(pick, 0, AREA_H * W);                     // clear the accepted-line marks for this picture
  buildCodeMap(frame->buf);                        // classify every pixel
  mergeRedMagenta();                               // turn mostly-magenta red patches into wall
  Pillar raw;                                      // the pillar found in this picture, before the time filter
  if (USE_BLOCKS) { buildBlocks(); raw = findClosestPillarBlocks(); }   // block method
  else            { raw = findClosestPillar(); }                        // pixel method
  rawColorForStatus = raw.found ? raw.color : C_NONE;   // remember the raw color for the status
  Pillar closest = USE_TIME_FILTER ? timeFilter(raw) : raw;   // apply the time filter (or not)
  LineInfo blue   = findLine(C_BLUE, BLUE_MIN_PIXELS);        // find the blue floor line
  LineInfo orange = findLine(C_ORANGE, ORANGE_MIN_PIXELS);    // find the orange floor line
  seq++;                                           // next message number
  sendToNano(closest, blue, orange);               // send the message to the Nano

  framesThisPeriod++;                              // count this frame
  if (millis() - fpsStart >= 1000) {               // every second: update the fps
    cameraFps = framesThisPeriod * 1000.0 / (millis() - fpsStart);   // frames per second
    framesThisPeriod = 0; fpsStart = millis();     // start a new window
  }
  // Pictures only when viewer.html has the port open (Serial is true), so the camera never
  // waits for a computer that is not there (that stopped the messages to the Nano).
  if (STREAM_TO_PC && Serial && millis() - lastStream >= STREAM_EVERY_MS) { sendPacket(frame->buf, closest, blue, orange); lastStream = millis(); }   // send a picture to the viewer
  if (millis() - lastStatus >= STATUS_EVERY_MS && !(STREAM_TO_PC && Serial)) {   // text would land inside the picture stream
    int wPx = closest.found ? closest.right - closest.left + 1 : 0;   // pillar width in pixels
    int xPx = closest.found ? (int)(closest.sumX / closest.count) - W / 2 : 0;   // pillar center x
    float aheadCm = -1, sideCm = 0;                // distance and side for the status
    if (wPx) pillarPosition(xPx, wPx, aheadCm, sideCm);   // compute them when a pillar exists
    Serial.printf("\nstatus: camera %.1f fps %s | pillar %s %d cm ahead, %d cm side\n",   // main status line
                  cameraFps, cameraLocked ? "(locked)" : "(auto)",
                  closest.found ? (closest.color == C_RED ? "RED" : "GREEN") : "none",
                  roundInt(aheadCm), roundInt(sideCm));
    Serial.printf("  ORANGE %s | center %d | low %d | len %d | thick %d | px %d | above %d/%d\n", lineReason(orange),   // orange line status
                  orange.centerRow, orange.lowest, orange.length, orange.thick, orange.count,
                  orange.aboveFloor, orange.aboveChecked);
    Serial.printf("  BLUE   %s | center %d | low %d | len %d | thick %d | px %d | above %d/%d\n", lineReason(blue),   // blue line status
                  blue.centerRow, blue.lowest, blue.length, blue.thick, blue.count,
                  blue.aboveFloor, blue.aboveChecked);
    printProbe("near", frame->buf, PROBE_X, PROBE_NEAR_Y);      // color probe near the car
    printProbe("bottom", frame->buf, PROBE_X, PROBE_LOW_Y);     // color probe at the bottom
    lastStatus = millis();                         // remember when the status was printed
  }
  esp_camera_fb_return(frame);                     // give the picture back to the camera
}
