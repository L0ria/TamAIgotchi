// Status bar (issue #32, step 2 of 6 of the UI restructure in #29;
// wrapped in the StatusBar class in step 3 of 11 of the refactoring plan
// in #42, issue #46 - 1:1 wrap, behavior unchanged).
#include "statusbar.h"
#include <Arduino.h>  // String, Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include "display.h"   // displayMgr.render() (issue #51, step 8: the single pass)

// The `display` object is defined in hardware.h (included by the sketch);
// reference it here instead of passing it through every call - the same
// pattern as bubble.cpp / text_utils.cpp / alien.cpp / recorder.cpp.
extern Adafruit_SSD1306 display;

// Truncate `s` to at most STATUS_CHARS_PER_LINE chars (no ellipsis -
// plain hard cut, like the bubble's line table) and return it.
String StatusBar::fit(const String& s) {
  if (s.length() > (unsigned)STATUS_CHARS_PER_LINE)
    return s.substring(0, STATUS_CHARS_PER_LINE);
  return s;
}

// Blank the two status lines (top 16 px of the 128x64 panel) without
// touching the rest of the frame.
void StatusBar::blank() {
  display.fillRect(0, 0, SCREEN_WIDTH, 16, BLACK);
}

// Draw the two status lines (y=0 / y=8) from the stored state into the
// current frame. No display.display() - displayMgr.render() owns the panel push
// (issue #35, step 5 of 6 of the UI restructure in #29).
void StatusBar::draw() {
  blank();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(l1_);
  if (l2_.length()) {
    display.setCursor(0, 8);
    display.print(l2_);
  }
}

// Render the two status lines (y=0 / y=8), mirror both to Serial, then
// push the full frame through the single render pass
// (displayMgr.render(), issue #51, step 8). Each line is truncated to STATUS_CHARS_PER_LINE
// (21 chars); the two lines are blanked (not cleared) first, so the rest
// of the frame (alien / bubble) is preserved by displayMgr.render().
void StatusBar::show(const String& l1, const String& l2) {
  String a = fit(l1);
  String b = fit(l2);
  Serial.println(a);
  Serial.println(b);

  l1_ = a;
  l2_ = b;
  displayMgr.render();
}

// 1-arg form: line 2 empty.
void StatusBar::show(const String& l1) {
  show(l1, String(""));
}

// 2-line error form: title on line 1, detail on line 2 (each truncated
// to STATUS_CHARS_PER_LINE); the full title + detail go to Serial.
void StatusBar::error(const String& title, const String& detail) {
  Serial.print(F("ERROR: "));
  Serial.println(title);
  if (detail.length()) {
    Serial.print(F("       "));
    Serial.println(detail);
  }
  show(title, detail);
}

// Blank the two status lines and push the frame through the single render
// pass (issue #35, step 5).
void StatusBar::clear() {
  l1_ = "";
  l2_ = "";
  displayMgr.render();
}

int StatusBar::lineCount() const {
  if (l1_.length()) return l2_.length() ? 2 : 1;
  return 0;
}

const String& StatusBar::line1() const { return l1_; }
const String& StatusBar::line2() const { return l2_; }
