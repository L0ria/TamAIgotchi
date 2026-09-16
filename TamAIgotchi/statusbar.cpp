// Status bar (issue #32, step 2 of 6 of the UI restructure in #29).
#include "statusbar.h"
#include <Arduino.h>  // String, Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object

// The `display` object is defined in hardware.h (included by the sketch);
// reference it here instead of passing it through every call - the same
// pattern as bubble.cpp / text_utils.cpp / alien.cpp / recorder.cpp.
extern Adafruit_SSD1306 display;

// Truncate `s` to at most STATUS_CHARS_PER_LINE chars (no ellipsis -
// plain hard cut, like the bubble's line table) and return it.
static String statusFit(const String& s) {
  if (s.length() > (unsigned)STATUS_CHARS_PER_LINE)
    return s.substring(0, STATUS_CHARS_PER_LINE);
  return s;
}

// Blank the two status lines (top 16 px of the 128x64 panel) without
// touching the rest of the frame.
static void statusBlank() {
  display.fillRect(0, 0, SCREEN_WIDTH, 16, BLACK);
}

// Render the two status lines (y=0 / y=8), mirror both to Serial, flush.
void statusShow(const String& l1, const String& l2) {
  String a = statusFit(l1);
  String b = statusFit(l2);
  Serial.println(a);
  Serial.println(b);

  statusBlank();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(a);
  if (b.length()) {
    display.setCursor(0, 8);
    display.print(b);
  }
  display.display();
}

// 1-arg form: line 2 empty.
void statusShow(const String& l1) {
  statusShow(l1, String(""));
}

// 2-line error form: title on line 1, detail on line 2 (each truncated
// to STATUS_CHARS_PER_LINE); the full title + detail go to Serial.
void statusError(const String& title, const String& detail) {
  Serial.print(F("ERROR: "));
  Serial.println(title);
  if (detail.length()) {
    Serial.print(F("       "));
    Serial.println(detail);
  }
  statusShow(title, detail);
}

// Blank the two status lines and flush (helper for the step-5 render pass).
void statusClear() {
  statusBlank();
  display.display();
}
