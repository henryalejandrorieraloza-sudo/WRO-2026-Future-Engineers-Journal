// Color detection test (Block 1 - "the eye")
// Board: emakefun ESP32S3-CAM with OV2640
// Tools: ESP32S3 Dev Module | USB CDC On Boot: Enabled | Flash Size: 8MB
//        Partition: 8M with spiffs (3MB APP/1.5MB SPIFFS) | PSRAM: OPI PSRAM
// No Wi-Fi. Takes 160x120 pictures, finds red and green blobs (BFS),
// and prints the closest blob of each color.

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

const int XCLK_FREQ_HZ = 20000000;  // lower to 10000000 if the image gets noisy with a long cable

// ---------- Image size ----------
const int W = 160;
const int H = 120;

// ---------- Color thresholds (tune these with your cubes!) ----------
// Hue (H) goes 0-359 degrees. Saturation (S) and Value (V) go 0-255.
// Red is special: it sits at both ends of the hue circle (around 0 and around 359).
const int RED_HUE_LOW    = 340;  // red if hue >= this...
const int RED_HUE_HIGH   = 15;   // ...or hue <= this
const int RED_MIN_SAT    = 100;
const int RED_MIN_VAL    = 60;

const int GREEN_HUE_LOW  = 80;
const int GREEN_HUE_HIGH = 160;
const int GREEN_MIN_SAT  = 80;
const int GREEN_MIN_VAL  = 50;

const int MIN_PIXELS      = 30;    // smaller blobs are noise
const int IGNORE_TOP_ROWS = 0;     // later: ignore everything above the walls
const bool SHOW_CENTER_HSV = true; // calibration: prints the color at the center of the image
const int PRINT_EVERY_MS  = 200;   // 5 lines per second

// ---------- Pixel classes ----------
const uint8_t NONE  = 0;
const uint8_t RED   = 1;
const uint8_t GREEN = 2;

uint8_t pixelClass[H][W];   // what color each pixel is
bool visited[H][W];         // for the BFS
uint8_t queueX[W * H];      // BFS queue (coordinates fit in one byte)
uint8_t queueY[W * H];

struct Blob {
  bool found;
  int minX, maxX, minY, maxY;
  int count;
  long sumX;
};

unsigned long lastPrint = 0;
int framesThisPeriod = 0;

// ---------- Color conversion ----------

// Reads one pixel of the RGB565 picture and returns red, green and blue in 0-255
void readPixel(const uint8_t *buf, int x, int y, int &r, int &g, int &b) {
  int i = (y * W + x) * 2;
  uint16_t p = (buf[i] << 8) | buf[i + 1];  // the camera sends the high byte first
  r = ((p >> 11) & 0x1F) << 3;  // 5 bits of red   -> 0-255
  g = ((p >> 5)  & 0x3F) << 2;  // 6 bits of green -> 0-255
  b = ( p        & 0x1F) << 3;  // 5 bits of blue  -> 0-255
}

// Converts RGB to HSV: h = which color (0-359), s = how intense (0-255), v = how bright (0-255)
void rgbToHsv(int r, int g, int b, int &h, int &s, int &v) {
  int maxC = max(r, max(g, b));
  int minC = min(r, min(g, b));
  int delta = maxC - minC;

  v = maxC;
  s = (maxC == 0) ? 0 : (delta * 255) / maxC;

  if (delta == 0) {  // gray, white or black: no color
    h = 0;
    return;
  }
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

// Marks every pixel of the picture as RED, GREEN or NONE
void buildClassMap(const uint8_t *buf) {
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (y < IGNORE_TOP_ROWS) {
        pixelClass[y][x] = NONE;
        continue;
      }
      int r, g, b, h, s, v;
      readPixel(buf, x, y, r, g, b);
      rgbToHsv(r, g, b, h, s, v);
      pixelClass[y][x] = classify(h, s, v);
    }
  }
}

// ---------- Blob search (BFS) ----------

// Finds all separate blobs of one color and returns the CLOSEST one:
// the one whose bottom is lowest in the image (largest maxY)
Blob findClosestBlob(uint8_t color) {
  memset(visited, 0, sizeof(visited));
  const int dx[4] = {1, -1, 0, 0};
  const int dy[4] = {0, 0, 1, -1};

  Blob best = {false, 0, 0, 0, 0, 0, 0};

  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (pixelClass[y][x] != color || visited[y][x]) continue;

      // New blob: flood it with BFS
      Blob b = {true, x, x, y, y, 0, 0};
      int head = 0, tail = 0;
      queueX[tail] = x; queueY[tail] = y; tail++;
      visited[y][x] = true;

      while (head < tail) {
        int cx = queueX[head], cy = queueY[head]; head++;
        b.count++;
        b.sumX += cx;
        if (cx < b.minX) b.minX = cx;
        if (cx > b.maxX) b.maxX = cx;
        if (cy < b.minY) b.minY = cy;
        if (cy > b.maxY) b.maxY = cy;

        for (int k = 0; k < 4; k++) {
          int nx = cx + dx[k], ny = cy + dy[k];
          if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
          if (pixelClass[ny][nx] != color || visited[ny][nx]) continue;
          visited[ny][nx] = true;
          queueX[tail] = nx; queueY[tail] = ny; tail++;
        }
      }

      if (b.count < MIN_PIXELS) continue;  // too small: noise

      // Keep the closest blob (lowest bottom). If tied, keep the bigger one.
      if (!best.found || b.maxY > best.maxY ||
          (b.maxY == best.maxY && b.count > best.count)) {
        best = b;
      }
    }
  }
  return best;
}

void printBlob(const char *name, const Blob &b) {
  Serial.print(name);
  if (!b.found) {
    Serial.print(" none            ");
    return;
  }
  int centerX = b.sumX / b.count;
  int height  = b.maxY - b.minY + 1;
  Serial.printf(" x=%3d height=%3d bottom=%3d px=%4d", centerX, height, b.maxY, b.count);
}

// Average color of a 10x10 square in the middle of the image (for calibration)
void printCenterHsv(const uint8_t *buf) {
  long sumR = 0, sumG = 0, sumB = 0;
  int n = 0;
  for (int y = H / 2 - 5; y < H / 2 + 5; y++) {
    for (int x = W / 2 - 5; x < W / 2 + 5; x++) {
      int r, g, b;
      readPixel(buf, x, y, r, g, b);
      sumR += r; sumG += g; sumB += b; n++;
    }
  }
  int h, s, v;
  rgbToHsv(sumR / n, sumG / n, sumB / n, h, s, v);
  Serial.printf(" | CENTER H=%3d S=%3d V=%3d", h, s, v);
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
  config.pixel_format = PIXFORMAT_RGB565;  // raw colors (no JPEG)
  config.frame_size   = FRAMESIZE_QQVGA;   // 160 x 120
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.fb_count     = 2;
  config.grab_mode    = CAMERA_GRAB_LATEST;  // always the newest picture

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED, error 0x%x\n", err);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  // Wait until the Serial Monitor is open (up to 15 s), so nothing gets lost
  unsigned long start = millis();
  while (!Serial && millis() - start < 15000) delay(10);
  delay(500);

  Serial.println();
  Serial.println("===== COLOR DETECTION TEST =====");

  if (!startCamera()) {
    while (true) {
      Serial.println("Camera did not start. Press RESET to try again.");
      delay(3000);
    }
  }
  Serial.println("Camera OK. Show the red and green cubes to the camera.");
  lastPrint = millis();
}

void loop() {
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == NULL) {
    Serial.println("Could not get a picture");
    delay(200);
    return;
  }

  if (frame->width != W || frame->height != H) {
    Serial.printf("Unexpected picture size %dx%d\n", frame->width, frame->height);
    esp_camera_fb_return(frame);
    delay(1000);
    return;
  }

  buildClassMap(frame->buf);
  Blob red   = findClosestBlob(RED);
  Blob green = findClosestBlob(GREEN);
  framesThisPeriod++;

  if (millis() - lastPrint >= PRINT_EVERY_MS) {
    printBlob("RED  ", red);
    Serial.print(" | ");
    printBlob("GREEN", green);
    if (SHOW_CENTER_HSV) printCenterHsv(frame->buf);
    float fps = framesThisPeriod * 1000.0 / (millis() - lastPrint);
    Serial.printf(" | %.0f fps\n", fps);
    framesThisPeriod = 0;
    lastPrint = millis();
  }

  esp_camera_fb_return(frame);  // always give the picture back
}
