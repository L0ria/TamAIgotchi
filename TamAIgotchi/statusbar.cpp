// Status bar (issue #32, step 2 of 6 of the UI restructure in #29).
#include "statusbar.h"
#include <Arduino.h>  // String, Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include "display.h"   // renderScreen() (issue #35, step 5: the single pass)

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

// The two status lines currently shown (renderScreen(), issue #35, step 5,
// re-draws them on every frame from this stored state).
static String statusL1 = "";
static String statusL2 = "";

// Draw the two status lines (y=0 / y=8) from the stored state into the
// current frame. No display.display() - renderScreen() owns the panel push
// (issue #35, step 5 of 6 of the UI restructure in #29).
void statusShow() {
  statusBlank();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(statusL1);
  if (statusL2.length()) {
    display.setCursor(0, 8);
    display.print(statusL2);
  }
}

// Render the two status lines (y=0 / y=8), mirror both to Serial, then
// push the full frame through the single render pass (renderScreen(),
// issue #35, step 5). Each line is truncated to STATUS_CHARS_PER_LINE
// (21 chars); the two lines are blanked (not cleared) first, so the rest
// of the frame (alien / bubble) is preserved by renderScreen().
void statusShow(const String& l1, const String& l2) {
  String a = statusFit(l1);
  String b = statusFit(l2);
  Serial.println(a);
  Serial.println(b);

  statusL1 = a;
  statusL2 = b;
  renderScreen();
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

// Blank the two status lines and push the frame through the single render
// pass (issue #35, step 5).
void statusClear() {
  statusL1 = "";
  statusL2 = "";
  renderScreen();
}
