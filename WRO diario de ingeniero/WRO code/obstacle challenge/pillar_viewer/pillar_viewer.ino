// =====================================================================
// PILLAR VIEWER (ESP32S3-CAM -> USB -> viewer.html in Chrome)
// Board: emakefun ESP32S3-CAM with OV2640
// Tools: ESP32S3 Dev Module | USB CDC On Boot: Enabled | Flash Size: 8MB
//        Partition: 8M with spiffs (3MB APP/1.5MB SPIFFS) | PSRAM: OPI PSRAM
//
// Same detection as closest_pillar_test (colors, BFS, split of touching
// pillars, closest = lowest bottom). Instead of text, it sends every
// picture over USB to viewer.html, which shows:
//   - the real picture
//   - the red / green pixels painted on top
//   - a box around the closest pillar and GO RIGHT / GO LEFT
//
// HOW TO USE
//   1. Upload this sketch. CLOSE the Serial Monitor (it would block the port).
//   2. Open viewer.html (same folder) in Google Chrome or Microsoft Edge.
//   3. Press "Connect" and choose the usbmodem port.
//
// PACKET FORMAT (all numbers little-endian):
//   4 bytes  magic 0xAA 0x55 0xC3 0x3C
//   1 byte   version (1)
//   2+2      width, height
//   1 byte   flags: bit0 = mirror horizontal, bit1 = flip vertical
//   1 byte   closest found (0/1)
//   1 byte   closest color (1 red, 2 green)
//   2x4      left, right, top, bottom of the closest pillar (int16)
//   2 bytes  pixel count of the closest pillar
//   1 byte   wasSplit (0/1)
//   2 bytes  camera fps x 10
//   W*H*2    picture, RGB565 exactly as the camera sends it (high byte first)
//   W*H/4    color mask, 2 bits per pixel (0 none, 1 red, 2 green), 4 pixels per byte
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

const int XCLK_FREQ_HZ = 20000000;  // lower to 10000000 if you see "FB-SIZE" errors

// ---------- Image size ----------
const int W = 160;
const int H = 120;

// Picture orientation (same meaning as in closest_pillar_test)
// The camera is mounted upside down, so the picture is turned 180 degrees:
// both true = rotate 180. If it ends up only mirrored, set one back to false.
const bool MIRROR_HORIZONTAL = true;
const bool FLIP_VERTICAL     = true;

// ---------- Color thresholds (same values that already worked) ----------
const int RED_HUE_LOW    = 340;
const int RED_HUE_HIGH   = 15;
const int RED_MIN_SAT    = 100;
const int RED_MIN_VAL    = 60;

const int GREEN_HUE_LOW  = 80;
const int GREEN_HUE_HIGH = 160;
const int GREEN_MIN_SAT  = 80;
const int GREEN_MIN_VAL  = 50;

// ---------- Detection settings ----------
const int MIN_PIXELS      = 30;
const int IGNORE_TOP_ROWS = 53;   // rows above this are ignored
const int NEAR_ROWS       = 41;   // rows from H - NEAR_ROWS (79) down are ignored too: search band = rows 53-78
const int BOTTOM_TOL      = 4;

// ---------- Streaming ----------
const int STREAM_EVERY_MS = 100;   // at most 10 pictures per second to the PC

// ---------- Pixel classes ----------
const uint8_t NONE  = 0;
const uint8_t RED   = 1;
const uint8_t GREEN = 2;

uint8_t pixelClass[H][W];
bool    visited[H][W];
uint8_t queueX[W * H];
uint8_t queueY[W * H];
int colTop[W], colBottom[W], colCount[W];

uint8_t maskPacked[W * H / 4];     // the color map, 4 pixels per byte

struct Pillar {
  bool found;
  uint8_t color;
  int left, right;
  int top, bottom;
  int count;
  long sumX;
  bool wasSplit;
};

unsigned long lastStream = 0;
unsigned long lastStatus = 0;
unsigned long packetsSent = 0;
unsigned long fpsStart = 0;
int framesThisPeriod = 0;
float cameraFps = 0;

// ---------- Color conversion (identical to closest_pillar_test) ----------

void readPixel(const uint8_t *buf, int x, int y, int &r, int &g, int &b) {
  if (MIRROR_HORIZONTAL) x = W - 1 - x;
  if (FLIP_VERTICAL)     y = H - 1 - y;
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

uint8_t classify(int h, int s, int v) {
  bool redHue = (h >= RED_HUE_LOW || h <= RED_HUE_HIGH);
  if (redHue && s >= RED_MIN_SAT && v >= RED_MIN_VAL) return RED;
  bool greenHue = (h >= GREEN_HUE_LOW && h <= GREEN_HUE_HIGH);
  if (greenHue && s >= GREEN_MIN_SAT && v >= GREEN_MIN_VAL) return GREEN;
  return NONE;
}

void buildClassMap(const uint8_t *buf) {
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (y < IGNORE_TOP_ROWS || y >= H - NEAR_ROWS) { pixelClass[y][x] = NONE; continue; }   // outside the search band
      int r, g, b, h, s, v;
      readPixel(buf, x, y, r, g, b);
      rgbToHsv(r, g, b, h, s, v);
      pixelClass[y][x] = classify(h, s, v);
    }
  }
}

// ---------- Front pillar + closest pillar (identical logic) ----------

Pillar frontPillarOfBlob(uint8_t color, int minX, int maxX) {
  Pillar p = {false, color, 0, 0, 0, 0, 0, 0, false};
  int best = -1;
  for (int x = minX; x <= maxX; x++) {
    if (colCount[x] > 0 && (best < 0 || colBottom[x] > colBottom[best])) best = x;
  }
  if (best < 0) return p;

  int lowest = colBottom[best];
  int left = best, right = best;
  while (left - 1 >= minX && colCount[left - 1] > 0 && colBottom[left - 1] >= lowest - BOTTOM_TOL) left--;
  while (right + 1 <= maxX && colCount[right + 1] > 0 && colBottom[right + 1] >= lowest - BOTTOM_TOL) right++;

  p.left = left;
  p.right = right;
  p.bottom = lowest;
  p.top = H;
  for (int x = left; x <= right; x++) {
    if (colTop[x] < p.top) p.top = colTop[x];
    p.count += colCount[x];
    p.sumX  += (long)x * colCount[x];
  }
  p.found = (p.count >= MIN_PIXELS);
  p.wasSplit = (left > minX || right < maxX);
  return p;
}

bool isCloser(const Pillar &a, const Pillar &b) {
  if (!b.found) return true;
  if (a.bottom != b.bottom) return a.bottom > b.bottom;
  return a.count > b.count;
}

Pillar findClosestPillar() {
  memset(visited, 0, sizeof(visited));
  const int dx[4] = {1, -1, 0, 0};
  const int dy[4] = {0, 0, 1, -1};
  Pillar closest = {false, NONE, 0, 0, 0, 0, 0, 0, false};

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      uint8_t color = pixelClass[y][x];
      if (color == NONE || visited[y][x]) continue;

      for (int i = 0; i < W; i++) { colTop[i] = H; colBottom[i] = -1; colCount[i] = 0; }
      int minX = x, maxX = x, total = 0;
      int head = 0, tail = 0;
      queueX[tail] = x; queueY[tail] = y; tail++;
      visited[y][x] = true;

      while (head < tail) {
        int cx = queueX[head], cy = queueY[head]; head++;
        total++;
        colCount[cx]++;
        if (cy < colTop[cx])    colTop[cx] = cy;
        if (cy > colBottom[cx]) colBottom[cx] = cy;
        if (cx < minX) minX = cx;
        if (cx > maxX) maxX = cx;
        for (int k = 0; k < 4; k++) {
          int nx = cx + dx[k], ny = cy + dy[k];
          if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
          if (visited[ny][nx] || pixelClass[ny][nx] != color) continue;
          visited[ny][nx] = true;
          queueX[tail] = nx; queueY[tail] = ny; tail++;
        }
      }

      if (total < MIN_PIXELS) continue;

      Pillar p = frontPillarOfBlob(color, minX, maxX);
      if (p.found && isCloser(p, closest)) closest = p;
    }
  }
  return closest;
}

// ---------- Sending one picture to the PC ----------

void writeU16(uint16_t v) { Serial.write((uint8_t)(v & 0xFF)); Serial.write((uint8_t)(v >> 8)); }
void writeI16(int16_t v)  { writeU16((uint16_t)v); }

void packMask() {
  memset(maskPacked, 0, sizeof(maskPacked));
  for (int i = 0; i < W * H; i++) {
    uint8_t c = pixelClass[i / W][i % W];
    maskPacked[i >> 2] |= (c & 0x03) << ((i & 3) * 2);
  }
}

void sendPacket(const uint8_t *picture, const Pillar &p) {
  const uint8_t magic[4] = {0xAA, 0x55, 0xC3, 0x3C};
  Serial.write(magic, 4);
  Serial.write((uint8_t)1);                                   // version
  writeU16(W);
  writeU16(H);
  Serial.write((uint8_t)((MIRROR_HORIZONTAL ? 1 : 0) | (FLIP_VERTICAL ? 2 : 0)));
  Serial.write((uint8_t)(p.found ? 1 : 0));
  Serial.write(p.color);
  writeI16(p.left);
  writeI16(p.right);
  writeI16(p.top);
  writeI16(p.bottom);
  writeU16((uint16_t)p.count);
  Serial.write((uint8_t)(p.wasSplit ? 1 : 0));
  writeU16((uint16_t)(cameraFps * 10));

  // Big blocks go in pieces so the USB buffer keeps up
  const int CHUNK = 1024;
  int pictureBytes = W * H * 2;
  for (int i = 0; i < pictureBytes; i += CHUNK) {
    Serial.write(picture + i, min(CHUNK, pictureBytes - i));
  }
  packMask();
  Serial.write(maskPacked, sizeof(maskPacked));
}

// ---------- Setup and loop ----------

bool startCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
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
  config.frame_size   = FRAMESIZE_QQVGA;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  // Pictures go in PSRAM when it is enabled (Tools -> PSRAM: OPI PSRAM).
  // If PSRAM is off, one 160x120 picture (38 KB) still fits in normal RAM.
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count    = 2;
    Serial.printf("PILLAR VIEWER: PSRAM found (%u KB free)\n", (unsigned)(ESP.getFreePsram() / 1024));
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count    = 1;
    Serial.println("PILLAR VIEWER: no PSRAM! Check Tools -> PSRAM: OPI PSRAM. Using normal RAM instead.");
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) Serial.printf("PILLAR VIEWER: camera init error 0x%x\n", err);
  return err == ESP_OK;
}

void setup() {
  Serial.setTxBufferSize(16384);   // big USB output buffer: one picture is ~43 KB (default is only 256 bytes)
  Serial.begin(115200);   // over native USB the number is ignored: it goes as fast as USB allows
  delay(1500);
  Serial.println("PILLAR VIEWER: starting camera...");
  if (!startCamera()) {
    while (true) {
      Serial.println("Camera did not start. Press RESET.");   // visible in the viewer's log box
      delay(3000);
    }
  }
  Serial.println("PILLAR VIEWER: camera OK, sending pictures");
  fpsStart = millis();
}

void loop() {
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == NULL) { delay(50); return; }
  if (frame->width != W || frame->height != H) {
    esp_camera_fb_return(frame);
    delay(500);
    return;
  }

  buildClassMap(frame->buf);
  Pillar closest = findClosestPillar();

  framesThisPeriod++;
  if (millis() - fpsStart >= 1000) {
    cameraFps = framesThisPeriod * 1000.0 / (millis() - fpsStart);
    framesThisPeriod = 0;
    fpsStart = millis();
  }

  // Send at most 10 pictures per second
  if (millis() - lastStream >= STREAM_EVERY_MS) {
    sendPacket(frame->buf, closest);
    packetsSent++;
    lastStream = millis();
  }

  // A short text line every 2 s (shows up in the viewer's "Board messages")
  if (millis() - lastStatus >= 2000) {
    Serial.printf("\nstatus: camera %.1f fps, %lu pictures sent\n", cameraFps, packetsSent);
    lastStatus = millis();
  }

  esp_camera_fb_return(frame);
}
