// Speech-bubble widget (issue #31, step 1 of 6 of the UI restructure in #29).
#include "bubble.h"
#include <Arduino.h>  // String, Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include "text_utils.h"  // wrapText() (the new width-parameterized core)

// The `display` object is defined in hardware.h (included by the sketch);
// reference it here instead of passing it through every call - the same
// pattern as text_utils.cpp / alien.cpp / recorder.cpp / display.cpp.
extern Adafruit_SSD1306 display;

// The bubble owns its own static line table (BUBBLE_MAX_LINES x 16 B =
// 1 KB of static RAM, issue #29 Q7) - no globals in the sketch. The state
// variables are named bubbleCount / bubbleOffset (not bubbleLineCount /
// bubbleScrollOffset, which are the public accessor functions below).
static char bubbleLines[BUBBLE_MAX_LINES][BUBBLE_CHARS_PER_LINE + 1];
static int  bubbleCount  = 0;  // wrapped lines actually in use
static int  bubbleOffset = 0;  // index of the first visible line

// Word-wrap `text` into the static table at BUBBLE_CHARS_PER_LINE
// chars/line and reset the scroll offset to 0.
void bubbleSetText(const String& text) {
  char* out[BUBBLE_MAX_LINES];
  for (int i = 0; i < BUBBLE_MAX_LINES; i++) out[i] = bubbleLines[i];
  bubbleCount = wrapText(text, out, BUBBLE_CHARS_PER_LINE, BUBBLE_MAX_LINES);
  bubbleOffset = 0;
}

// Move the scroll offset by 1 line (down = +1, up = -1), clamped to
// [0, max(0, count - BUBBLE_VISIBLE_LINES)].
void bubbleScroll(bool down) {
  int maxOffset = (bubbleCount > BUBBLE_VISIBLE_LINES)
                ? bubbleCount - BUBBLE_VISIBLE_LINES
                : 0;
  if (down) {
    if (bubbleOffset < maxOffset) bubbleOffset++;
  } else {
    if (bubbleOffset > 0) bubbleOffset--;
  }
}

// Empty the table and reset the scroll offset to 0.
void bubbleClear() {
  for (int i = 0; i < BUBBLE_MAX_LINES; i++) bubbleLines[i][0] = '\0';
  bubbleCount = 0;
  bubbleOffset = 0;
}

int bubbleLineCount() { return bubbleCount; }
int bubbleScrollOffset() { return bubbleOffset; }

// Jump the scroll offset to the start (first line) or the end (last
// visible window). The double-press jump (issue #34, step 4 of 6 of the
// UI restructure in #29 - option A from #29 Q7): the same scroll button
// pressed twice within ~500 ms jumps to the start / end so long answers
// (~50+ wrapped lines) can be reached without ~45 single presses.
void bubbleJumpTo(bool toEnd) {
  if (toEnd) {
    bubbleOffset = (bubbleCount > BUBBLE_VISIBLE_LINES)
                 ? bubbleCount - BUBBLE_VISIBLE_LINES
                 : 0;
  } else {
    bubbleOffset = 0;
  }
}

// Draw the bubble frame (rectangle + interior clear + tail) + up to
// BUBBLE_VISIBLE_LINES lines of `lines[]` (the first `count` are valid)
// into the current frame. Shared by bubbleRender() (the stored table
// window) and bubbleRenderText() (a transient text, table untouched).
// Text is padded 3 px in from the left edge and starts 3 px below the top
// edge (font 1 = 6x8 px/char).
template <typename LineBuf>
static void bubbleDrawFrame(LineBuf lines, int count) {
  // The bubble rectangle - always the same size, always drawn (issue #29 Q8).
  display.drawRect(BUBBLE_X, BUBBLE_Y, BUBBLE_W, BUBBLE_H, WHITE);

  // Clear the interior before re-drawing the visible window: the bubble is
  // re-rendered in place on every scroll / jump (issue #34, step 4), so a
  // jump back up must not leave stale text from a longer window behind.
  display.fillRect(BUBBLE_X + 1, BUBBLE_Y + 1, BUBBLE_W - 2, BUBBLE_H - 2, BLACK);

  // Tail: a small ~4 px filled triangle from the bubble's left edge
  // (x = BUBBLE_X = 30) toward the alien (x 4..27), vertically centered-ish
  // (issue #29 Q9, cosmetic - a 1-line revert if it looks off on the panel).
  display.fillTriangle(BUBBLE_X - 3, BUBBLE_Y + 18,   // tip (27, 36)
                       BUBBLE_X,     BUBBLE_Y + 14,   // base top (30, 32)
                       BUBBLE_X,     BUBBLE_Y + 22,   // base bottom (30, 40)
                       WHITE);

  if (count > BUBBLE_VISIBLE_LINES) count = BUBBLE_VISIBLE_LINES;
  for (int i = 0; i < count; i++) {
    display.setCursor(BUBBLE_X + 3, BUBBLE_Y + 3 + 8 * i);
    display.print(lines[i]);
  }
}

// Draw the bubble rectangle (always, even when empty) + the tail triangle
// + the visible BUBBLE_VISIBLE_LINES window of the table into the current
// frame. No clearDisplay() / display() of its own (the single render pass
// arrives in step 5; the caller issues display.display()).
void bubbleRender() {
  // The visible window of the table (BUBBLE_VISIBLE_LINES lines), starting
  // at bubbleScrollOffset.
  int maxOffset = (bubbleCount > BUBBLE_VISIBLE_LINES)
                ? bubbleCount - BUBBLE_VISIBLE_LINES
                : 0;
  if (bubbleOffset < 0) bubbleOffset = 0;
  if (bubbleOffset > maxOffset) bubbleOffset = maxOffset;

  bubbleDrawFrame(&bubbleLines[0], bubbleCount - bubbleOffset);
}

// Draw the bubble frame + up to BUBBLE_VISIBLE_LINES lines of `text`
// (word-wrapped at BUBBLE_CHARS_PER_LINE, first lines shown) into the
// current frame WITHOUT touching the line table (bubbleLines /
// bubbleCount / bubbleOffset) - the stored content (e.g. the response)
// survives the call (issue #35 step 5 follow-up, Q4: the idle animation
// must not destroy the response text). text = NULL draws an empty bubble.
// No clearDisplay() / display() of its own (see the header note).
void bubbleRenderText(const char* text) {
  if (text == NULL) {
    bubbleDrawFrame(&bubbleLines[0], 0);  // valid pointer, zero lines
    return;
  }
  char lines[BUBBLE_VISIBLE_LINES][BUBBLE_CHARS_PER_LINE + 1];
  char* out[BUBBLE_VISIBLE_LINES];
  for (int i = 0; i < BUBBLE_VISIBLE_LINES; i++) out[i] = lines[i];
  int count = wrapText(String(text), out, BUBBLE_CHARS_PER_LINE, BUBBLE_VISIBLE_LINES);
  bubbleDrawFrame(&lines[0], count);
}
