// Tests for the StatusBar class (issue #46, step 3 of 11 of the
// refactoring plan in #42).
//
// Pins the behavior of the 1:1 class wrap:
//   - show() truncates each line to exactly STATUS_CHARS_PER_LINE (21)
//     chars - no spill (status rule, issue #29)
//   - show(l1) leaves line 2 empty
//   - error() mirrors the FULL (un-truncated) title + detail to Serial
//     while storing the truncated lines
//   - clear() blanks both stored lines
//   - draw() renders from the stored state (fillRect blank + print of the
//     stored lines) and never calls clearDisplay()/display()
//     (single-render-pass rule, issue #35, step 5)
//
// The shared `statusBar` object is defined in tests/test_main.cpp (the
// host build's equivalent of the instance in TamAIgotchi.ino); the SSD1306
// shim records the draw calls the render assertions use, and the Serial
// shim's capture buffer is what the mirror-to-Serial assertions read.
#include "test_main.h"
#include <Arduino.h>

#include <string>
#include <Adafruit_SSD1306.h>
#include "statusbar.h"

// The shared display object (defined in tests/test_main.cpp for the host
// build); the render assertions read the SSD1306 shim's recorded counters.
extern Adafruit_SSD1306 display;

// Helper: a string of exactly `n` distinct chars ("abcdefghijklmnopqrstuvwxyz...").
static std::string letters(int n) {
  std::string s;
  for (int i = 0; i < n; i++) s += 'a' + (i % 26);
  return s;
}

// --- show (truncation) ------------------------------------------------------

TEST(statusbar_show_truncates_to_exactly_21_chars) {
  // 22-char input -> exactly 21 chars stored, no spill (status rule:
  // 21 chars x 2 lines, never driver clipping).
  String l1(letters(22).c_str());  // "abcdefghijklmnopqrstuv" (22)
  statusBar.show(l1, String());
  CHECK_EQ_INT(statusBar.line1().length(), 21);
  CHECK_EQ(statusBar.line1().c_str(), letters(21).c_str());
  CHECK(statusBar.line1().c_str()[20] == 'u');  // hard cut at 21, no ellipsis
  CHECK_EQ_INT(statusBar.line2().length(), 0);  // no spill into line 2

  // An over-long line 2 is truncated the same way (no cross-line spill).
  statusBar.show(String(), String(letters(30).c_str()));
  CHECK_EQ_INT(statusBar.line1().length(), 0);
  CHECK_EQ_INT(statusBar.line2().length(), 21);
  CHECK_EQ(statusBar.line2().c_str(), letters(21).c_str());
}

TEST(statusbar_show_exact_21_chars_unchanged) {
  // Exactly 21 chars fit - stored verbatim.
  statusBar.show(String(letters(21).c_str()), String(letters(21).c_str()));
  CHECK_EQ_INT(statusBar.line1().length(), 21);
  CHECK_EQ_INT(statusBar.line2().length(), 21);
  CHECK_EQ_INT(statusBar.lineCount(), 2);
}

// --- show (1-arg form) ------------------------------------------------------

TEST(statusbar_show_one_arg_leaves_line2_empty) {
  statusBar.show(String("hello"));
  CHECK_EQ(statusBar.line1().c_str(), "hello");
  CHECK_EQ_INT(statusBar.line2().length(), 0);  // line 2 empty
  CHECK_EQ_INT(statusBar.lineCount(), 1);
}

TEST(statusbar_show_mirrors_both_lines_to_serial) {
  Serial.clearCapture();
  statusBar.show(String("line one"), String("line two"));
  const std::string& cap = Serial.captured();
  CHECK(cap.find("line one\n") != std::string::npos);  // both lines mirrored
  CHECK(cap.find("line two\n") != std::string::npos);
}

// --- error ------------------------------------------------------------------

TEST(statusbar_error_mirrors_full_text_to_serial) {
  // The FULL title + detail go to Serial (the truncated on-screen text is
  // not enough to debug with); the stored lines are truncated to 21.
  Serial.clearCapture();
  String title(letters(40).c_str());   // 40 chars
  String detail(letters(60).c_str());  // 60 chars
  statusBar.error(title, detail);

  const std::string& cap = Serial.captured();
  CHECK(cap.find(std::string("ERROR: ") + title.c_str()) != std::string::npos);  // full title
  CHECK(cap.find(std::string("       ") + detail.c_str()) != std::string::npos);  // full detail

  // Stored lines: truncated to exactly 21 chars.
  CHECK_EQ_INT(statusBar.line1().length(), 21);
  CHECK_EQ_INT(statusBar.line2().length(), 21);
  CHECK_EQ(statusBar.line1().c_str(), letters(21).c_str());
  CHECK_EQ(statusBar.line2().c_str(), letters(21).c_str());
}

TEST(statusbar_error_empty_detail_still_shows_title) {
  Serial.clearCapture();
  statusBar.error(String("I2S failed"), String());
  const std::string& cap = Serial.captured();
  CHECK(cap.find("ERROR: I2S failed\n") != std::string::npos);
  CHECK_EQ(statusBar.line1().c_str(), "I2S failed");
  CHECK_EQ_INT(statusBar.line2().length(), 0);
}

// --- clear ------------------------------------------------------------------

TEST(statusbar_clear_blanks_both_lines) {
  statusBar.show(String("line one"), String("line two"));
  CHECK_EQ_INT(statusBar.lineCount(), 2);
  statusBar.clear();
  CHECK_EQ_INT(statusBar.line1().length(), 0);  // both lines blanked
  CHECK_EQ_INT(statusBar.line2().length(), 0);
  CHECK_EQ_INT(statusBar.lineCount(), 0);
}

// --- draw (stored state + single-render-pass rule) --------------------------

TEST(statusbar_draw_renders_stored_lines) {
  statusBar.show(String("stored one"), String("stored two"));
  display.reset();
  statusBar.draw();
  CHECK_EQ_INT(display.fill_rects, 1);  // the top-16px blank
  CHECK(display.recorded.find("stored one") != std::string::npos);
  CHECK(display.recorded.find("stored two") != std::string::npos);
  CHECK_EQ_INT(display.frames, 0);      // no display() of its own
  CHECK_EQ_INT(display.cleared, 0);     // no clearDisplay() of its own
}

TEST(statusbar_draw_empty_state_still_blanks) {
  statusBar.clear();
  display.reset();
  statusBar.draw();
  CHECK_EQ_INT(display.fill_rects, 1);  // blank even with no stored content
  CHECK_EQ_INT(display.frames, 0);
  CHECK_EQ_INT(display.cleared, 0);
}
