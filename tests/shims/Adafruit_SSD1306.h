// Host-side fake of the Adafruit SSD1306 OLED driver (issue #44, step 1 of
// 11 of the refactoring plan in #42).
//
// The firmware modules under test do `#include <Adafruit_SSD1306.h>` and
// reference the shared `display` object (defined in hardware.h on the
// device; provided by tests/test_main.cpp for the host build). This fake:
//
//   - accepts the same constructor call as the real driver
//     (Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1))
//   - records every call (counters + the printed text) so the render tests
//     in steps 2-3 (Bubble / StatusBar) and 8 (Display) can assert on what
//     was drawn
//   - display() increments the public `frames` counter (the single
//     render-pass rule of the firmware)
//
// It is NOT a pixel-accurate emulator: geometry is counted, not rendered.
#pragma once

#include <Arduino.h>  // String

// Color + VCC constants used by the firmware (Adafruit_GFX / SSD1306).
#define WHITE 1
#define BLACK 0
#define SSD1306_SWITCHCAPVCC 0x9C

class Adafruit_SSD1306 {
 public:
  Adafruit_SSD1306(int w = 128, int h = 64, void* spi = nullptr, int rst = -1)
      : w_(w), h_(h) {}

  bool begin(unsigned char vcc, unsigned char addr) { (void)vcc; (void)addr; return true; }

  void setTextSize(int s) { text_size = s; }
  void setTextColor(int c) { text_color = c; }
  void setCursor(int x, int y) { cursor_x = x; cursor_y = y; }

  size_t print(const char* s) { recorded += (s ? s : ""); return s ? std::strlen(s) : 0; }
  size_t print(const String& s) { recorded += s.c_str(); return s.length(); }
  size_t print(char c) { recorded += c; return 1; }
  size_t println() { recorded += "\n"; return 1; }
  size_t println(const char* s) { return print(s) + 1; }
  size_t println(const String& s) { return print(s) + 1; }
  size_t println(char c) { return print(c) + 1; }

  void drawRect(int x, int y, int w, int h, int color) { (void)x; (void)y; (void)w; (void)h; (void)color; draw_rects++; }
  void fillRect(int x, int y, int w, int h, int color) { (void)x; (void)y; (void)w; (void)h; (void)color; fill_rects++; }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color) {
    (void)x0; (void)y0; (void)x1; (void)y1; (void)x2; (void)y2; (void)color; triangles++;
  }
  void drawBitmap(int x, int y, const unsigned char* bitmap, int w, int h, int color) {
    (void)x; (void)y; (void)bitmap; (void)w; (void)h; (void)color; bitmaps++;
  }

  void clearDisplay() { cleared++; }
  void display() { frames++; }  // the render pass

  // --- test hooks (public on purpose: the tests assert on these) ---
  int frames = 0;      // display() calls (render passes)
  int cleared = 0;     // clearDisplay() calls
  int draw_rects = 0;  // drawRect() calls
  int fill_rects = 0;  // fillRect() calls
  int triangles = 0;   // fillTriangle() calls
  int bitmaps = 0;     // drawBitmap() calls
  int text_size = 1;   // last setTextSize()
  int text_color = 0;  // last setTextColor()
  int cursor_x = 0;    // last setCursor()
  int cursor_y = 0;
  std::string recorded;  // everything print()ed, in order

  void reset() {
    frames = cleared = draw_rects = fill_rects = triangles = bitmaps = 0;
    text_size = 1;
    text_color = 0;
    cursor_x = cursor_y = 0;
    recorded.clear();
  }

  int width() const { return w_; }
  int height() const { return h_; }

 private:
  int w_;
  int h_;
};
