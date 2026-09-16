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

// Draw the bubble rectangle (always, even when empty) + the tail triangle
// + the visible BUBBLE_VISIBLE_LINES window of the table into the current
// frame. No clearDisplay() / display() of its own (the single render pass
// arrives in step 5; the caller issues display.display()).
void bubbleRender() {
  // The bubble rectangle - always the same size, always drawn (issue #29 Q8).
  display.drawRect(BUBBLE_X, BUBBLE_Y, BUBBLE_W, BUBBLE_H, WHITE);

  // Tail: a small ~4 px filled triangle from the bubble's left edge
  // (x = BUBBLE_X = 30) toward the alien (x 4..27), vertically centered-ish
  // (issue #29 Q9, cosmetic - a 1-line revert if it looks off on the panel).
  display.fillTriangle(BUBBLE_X - 3, BUBBLE_Y + 18,   // tip (27, 36)
                       BUBBLE_X,     BUBBLE_Y + 14,   // base top (30, 32)
                       BUBBLE_X,     BUBBLE_Y + 22,   // base bottom (30, 40)
                       WHITE);

  // The visible window of the table (BUBBLE_VISIBLE_LINES lines), starting
  // at bubbleScrollOffset. Text is padded 3 px in from the left edge and
  // starts 3 px below the top edge (font 1 = 6x8 px/char).
  int maxOffset = (bubbleCount > BUBBLE_VISIBLE_LINES)
                ? bubbleCount - BUBBLE_VISIBLE_LINES
                : 0;
  if (bubbleOffset < 0) bubbleOffset = 0;
  if (bubbleOffset > maxOffset) bubbleOffset = maxOffset;

  for (int i = 0; i < BUBBLE_VISIBLE_LINES; i++) {
    int idx = bubbleOffset + i;
    if (idx >= bubbleCount) break;
    display.setCursor(BUBBLE_X + 3, BUBBLE_Y + 3 + 8 * i);
    display.print(bubbleLines[idx]);
  }
}
