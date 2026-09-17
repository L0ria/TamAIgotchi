// Baseline tests for wrapText() (issue #44, step 1 of 11 of the
// refactoring plan in #42).
//
// These pin the CURRENT behavior of the 4-arg core form:
//   int wrapText(const String& text, char* out[], int width, int maxLines);
// so steps 2-10 (which re-wrap the call sites) cannot silently change the
// wrapping. They must pass before and after the refactor.
//
// NOTE: each out[i] must be a DISTINCT buffer of at least (width+1) bytes -
// the firmware contract (the caller owns the storage). The tests use a 2-D
// array so the lines do not clobber each other.
#include "test_main.h"
#include <Arduino.h>
#include "text_utils.h"

// Helper: wrap `text` into `n` distinct buffers of `bufsize` bytes each.
// Returns the number of lines used.
static int wrap(const char* text, char* out[], int width, int maxLines) {
  return wrapText(String(text), out, width, maxLines);
}

TEST(wrapText_empty_input_zero_lines) {
  char t[2][8];
  char* out[2] = {t[0], t[1]};
  CHECK_EQ_INT(wrap("", out, 7, 2), 0);
}

TEST(wrapText_single_word_shorter_than_width) {
  char t[2][8];
  char* out[2] = {t[0], t[1]};
  int n = wrap("hello", out, 7, 2);
  CHECK_EQ_INT(n, 1);
  CHECK_EQ(t[0], "hello");
}

TEST(wrapText_word_exactly_the_width) {
  char t[2][8];
  char* out[2] = {t[0], t[1]};
  int n = wrap("1234567", out, 7, 2);
  CHECK_EQ_INT(n, 1);
  CHECK_EQ(t[0], "1234567");
}

TEST(wrapText_wraps_at_width) {
  // "hello world" at width 5: "hello" fits, "world" does not fit on the
  // same line (5 + 1 + 5 > 5) -> line break.
  char t[4][8];
  char* out[4] = {t[0], t[1], t[2], t[3]};
  int n = wrap("hello world", out, 5, 4);
  CHECK_EQ_INT(n, 2);
  CHECK_EQ(t[0], "hello");
  CHECK_EQ(t[1], "world");
}

TEST(wrapText_hard_breaks_overlong_word) {
  // A 10-char word at width 3 is hard-broken into 3+3+3+1 (4 lines).
  char t[6][4];
  char* out[6] = {t[0], t[1], t[2], t[3], t[4], t[5]};
  int n = wrap("abcdefghij", out, 3, 6);
  CHECK_EQ_INT(n, 4);
  CHECK_EQ(t[0], "abc");
  CHECK_EQ(t[1], "def");
  CHECK_EQ(t[2], "ghi");
  CHECK_EQ(t[3], "j");
}

TEST(wrapText_newline_produces_line_break) {
  char t[4][8];
  char* out[4] = {t[0], t[1], t[2], t[3]};
  int n = wrap("ab\ncd", out, 7, 4);
  CHECK_EQ_INT(n, 2);
  CHECK_EQ(t[0], "ab");
  CHECK_EQ(t[1], "cd");
}

TEST(wrapText_tab_separates_words) {
  // A tab is a word separator (like a space) but, unlike a newline, it does
  // NOT force a line break: "ab\tcd" wraps to a single line "ab cd".
  char t[4][8];
  char* out[4] = {t[0], t[1], t[2], t[3]};
  int n = wrap("ab\tcd", out, 7, 4);
  CHECK_EQ_INT(n, 1);
  CHECK_EQ(t[0], "ab cd");
}

TEST(wrapText_maxlines_truncation) {
  // 5 words of 3 chars at width 3 -> 5 lines, but maxLines = 2.
  char t[2][4];
  char* out[2] = {t[0], t[1]};
  int n = wrap("aaa bbb ccc ddd eee", out, 3, 2);
  CHECK_EQ_INT(n, 2);
  CHECK_EQ(t[0], "aaa");
  CHECK_EQ(t[1], "bbb");
}

TEST(wrapText_maxlines_no_buffer_overflow) {
  // A single over-long word that would need 10 lines, but maxLines = 3:
  // the function must stop at 3 lines and not write past out[2].
  char t[3][4];
  char* out[3] = {t[0], t[1], t[2]};
  int n = wrap("aaaaaaaaaaaaaaaaaaaaaa", out, 3, 3);
  CHECK_EQ_INT(n, 3);
  CHECK_EQ(t[0], "aaa");
  CHECK_EQ(t[1], "aaa");
  CHECK_EQ(t[2], "aaa");
}
