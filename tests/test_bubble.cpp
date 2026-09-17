// Tests for the Bubble class (issue #45, step 2 of 11 of the refactoring
// plan in #42).
//
// Pins the behavior of the 1:1 class wrap:
//   - setText() resets lineCount()/scrollOffset() to 0 and wraps exactly
//     like the wrapText() baseline (tests/test_text_utils.cpp)
//   - scroll() clamps at both ends (no wrap-around)
//   - jumpTo(false) -> offset 0; jumpTo(true) -> last visible window
//   - clear() empties the table + offset
//   - renderText() does not modify the stored table (Q4 rule: the stored
//     response survives - count_/offset_ unchanged after renderText)
//   - render() draws the frame (drawRect + fillRect + fillTriangle) and
//     never calls clearDisplay()/display() (single-render-pass rule)
//
// The shared `bubble` object is defined in tests/test_main.cpp (the host
// build's equivalent of the instance in TamAIgotchi.ino); the SSD1306 shim
// records the draw calls the render assertions use.
#include "test_main.h"
#include <Arduino.h>

#include <string>
#include <Adafruit_SSD1306.h>
#include "bubble.h"

// The shared bubble object (TamAIgotchi.ino on the device; the host build
// defines it here so the modules' `extern Bubble bubble;` links) - the
// same pattern as the shared `display` object.
Bubble bubble;

// The shared display object (defined in tests/test_main.cpp for the host
// build); the render assertions read the SSD1306 shim's recorded counters.
extern Adafruit_SSD1306 display;

// Helper: a text of `n` distinct words of `len` chars each ("aaaa bbbb ...").
// At BUBBLE_CHARS_PER_LINE = 15, two 7-char words fit one line, three do
// not - so the line count is deterministic and matches the wrapText
// baseline.
static std::string words(int n, int len) {
  std::string s;
  for (int i = 0; i < n; i++) {
    if (i) s += " ";
    for (int j = 0; j < len; j++) s += 'a' + (i % 26);
  }
  return s;
}

// --- setText -----------------------------------------------------------------

TEST(bubble_setText_empty_resets_count_and_offset) {
  // 14 words of 7 chars = 7 lines -> maxOffset 2, so the scroll below
  // actually moves (and proves the offset was not already 0).
  bubble.setText(String(words(14, 7).c_str()));
  bubble.scroll(true);
  bubble.scroll(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 2);
  bubble.setText(String());
  CHECK_EQ_INT(bubble.lineCount(), 0);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
}

TEST(bubble_setText_wraps_like_wrapText_baseline) {
  // 10 words of 7 chars: two fit per 15-char line -> 5 lines.
  String text(words(10, 7).c_str());
  bubble.setText(text);
  CHECK_EQ_INT(bubble.lineCount(), 5);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
}

TEST(bubble_setText_hard_breaks_overlong_word) {
  // A 40-char word at width 15: hard-broken 15+15+10 -> 3 lines.
  bubble.setText(String(words(1, 40).c_str()));
  CHECK_EQ_INT(bubble.lineCount(), 3);
}

TEST(bubble_setText_truncates_at_max_lines) {
  // 200 words of 7 chars = 100 wrapped lines > BUBBLE_MAX_LINES (64):
  // the table holds exactly 64 lines (silent truncation, same as before).
  bubble.setText(String(words(200, 7).c_str()));
  CHECK_EQ_INT(bubble.lineCount(), BUBBLE_MAX_LINES);
}

// --- scroll ------------------------------------------------------------------

TEST(bubble_scroll_clamps_at_both_ends) {
  bubble.setText(String(words(10, 7).c_str()));  // 5 lines -> maxOffset 0
  bubble.scroll(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);  // clamped: nothing to scroll to
  bubble.scroll(false);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);  // clamped at 0, no wrap-around

  bubble.setText(String(words(12, 7).c_str()));  // 6 lines -> maxOffset 1
  bubble.scroll(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 1);
  bubble.scroll(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 1);  // clamped at maxOffset
  bubble.scroll(false);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
  bubble.scroll(false);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);  // clamped at 0
}

// --- jumpTo ------------------------------------------------------------------

TEST(bubble_jumpTo_start_and_end) {
  bubble.setText(String(words(20, 7).c_str()));  // 10 lines -> maxOffset 5
  bubble.jumpTo(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 5);  // last visible window
  bubble.jumpTo(false);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);  // first line
}

TEST(bubble_jumpTo_end_short_text_stays_zero) {
  bubble.setText(String(words(4, 7).c_str()));  // 2 lines -> maxOffset 0
  bubble.jumpTo(true);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
  bubble.jumpTo(false);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
}

// --- clear -------------------------------------------------------------------

TEST(bubble_clear_empties_table_and_offset) {
  bubble.setText(String(words(20, 7).c_str()));
  bubble.jumpTo(true);
  CHECK_EQ_INT(bubble.lineCount(), 10);
  bubble.clear();
  CHECK_EQ_INT(bubble.lineCount(), 0);
  CHECK_EQ_INT(bubble.scrollOffset(), 0);
}

// --- renderText (Q4: the stored table survives) -------------------------------

TEST(bubble_renderText_does_not_touch_stored_table) {
  bubble.setText(String(words(20, 7).c_str()));  // 10 lines
  bubble.jumpTo(true);
  int countBefore = bubble.lineCount();
  int offsetBefore = bubble.scrollOffset();

  // The idle-animation path: a transient text (and the empty bubble) must
  // not modify the stored table (issue #35 step 5 follow-up, Q4).
  display.reset();
  bubble.renderText("transient idle hello");
  CHECK_EQ_INT(bubble.lineCount(), countBefore);
  CHECK_EQ_INT(bubble.scrollOffset(), offsetBefore);

  display.reset();
  bubble.renderText(NULL);  // empty bubble phase
  CHECK_EQ_INT(bubble.lineCount(), countBefore);
  CHECK_EQ_INT(bubble.scrollOffset(), offsetBefore);
}

// --- render (frame + single-render-pass rule) --------------------------------

TEST(bubble_render_draws_frame_without_display_pass) {
  bubble.setText(String(words(10, 7).c_str()));  // 5 lines, all visible
  display.reset();
  bubble.render();
  CHECK_EQ_INT(display.draw_rects, 1);      // the bubble rectangle
  CHECK_EQ_INT(display.fill_rects, 1);      // the interior clear
  CHECK_EQ_INT(display.triangles, 1);       // the tail
  CHECK_EQ_INT(display.frames, 0);          // no display() of its own
  CHECK_EQ_INT(display.cleared, 0);         // no clearDisplay() of its own
}

TEST(bubble_render_empty_still_draws_frame) {
  bubble.clear();
  display.reset();
  bubble.render();
  CHECK_EQ_INT(display.draw_rects, 1);  // always the same size, always drawn
  CHECK_EQ_INT(display.frames, 0);
}

TEST(bubble_render_scrolled_window_draws_visible_lines) {
  bubble.setText(String(words(20, 7).c_str()));  // 10 lines
  bubble.jumpTo(true);                            // window = lines 6..10
  display.reset();
  bubble.render();
  // The visible window (lines 6..10) is printed, not the whole table:
  // the last line's word ("ttttttt") is drawn, the first line's word
  // ("aaaaaaa") is not.
  CHECK(display.recorded.find("ttttttt") != std::string::npos);
  CHECK(display.recorded.find("aaaaaaa") == std::string::npos);
  CHECK_EQ_INT(display.frames, 0);
}
