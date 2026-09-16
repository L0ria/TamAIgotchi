// Speech-bubble widget (issue #31, step 1 of 6 of the UI restructure in #29).
//
// Provides the geometry, text table, scrolling and rendering for the bubble
// that will host ALL content (prompt, response, the idle "hello") in the
// later steps. Pure addition - nothing calls it yet, so this step has zero
// behavior change (compile-only verification).
//
//   bubbleSetText(text)  - word-wrap into the static 64-line table (15
//                          chars/line), reset the scroll offset to 0
//   bubbleScroll(down)   - move the offset by 1 line, clamped to
//                          [0, max(0, count - BUBBLE_VISIBLE_LINES)]
//   bubbleClear()        - empty table, offset 0
//   bubbleLineCount()    - number of wrapped lines in use (the "Response
//                          x/y" status counter, step 4)
//   bubbleScrollOffset() - index of the first visible line
//   bubbleRender()       - draw the bubble rectangle (always, even when
//                          empty) + tail triangle + the visible 5-line
//                          window into the current frame
//
// The bubble is always the same size and always drawn (even when empty) -
// no popping rectangle (issue #29 Q8). A small ~4 px tail triangle on its
// left edge points toward the alien (issue #29 Q9, cosmetic).
//
// The bubble owns its own static line table (BUBBLE_MAX_LINES x 16 B =
// 1 KB of static RAM) - no globals in the sketch. The shared `display`
// object is referenced via extern, the same pattern as the other modules
// (text_utils.cpp / alien.cpp / recorder.cpp / display.cpp).
//
// bubbleRender() does NOT clearDisplay() or display() on its own - the
// single render pass (one clearDisplay() + one display() per frame) arrives
// in step 5. For now it may be called after the caller has drawn the rest
// of the scene, and the caller issues display.display().
#pragma once

#include "config.h"  // BUBBLE_* geometry + table size

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

// Word-wrap `text` into the static 64-line table at BUBBLE_CHARS_PER_LINE
// chars/line (via the new width-parameterized wrapText()) and reset the
// scroll offset to 0. Lines beyond BUBBLE_MAX_LINES are dropped (same
// silent-truncation behavior as the response view today).
void bubbleSetText(const String& text);

// Move the scroll offset by 1 line (down = +1, up = -1), clamped to
// [0, max(0, count - BUBBLE_VISIBLE_LINES)].
void bubbleScroll(bool down);

// Empty the table and reset the scroll offset to 0.
void bubbleClear();

// Number of wrapped lines actually in use (for the "Response x/y" status
// counter, step 4).
int bubbleLineCount();

// Index of the first visible line (for the "Response x/y" status counter,
// step 4).
int bubbleScrollOffset();

// Jump the scroll offset to the start (toEnd = false: the first line) or
// the end (toEnd = true: the last visible window). Used by the
// double-press jump (issue #34, step 4 of 6 of the UI restructure in #29
// - option A from #29 Q7).
void bubbleJumpTo(bool toEnd);

// Draw the bubble rectangle (always, even when empty) + the tail triangle
// + the visible BUBBLE_VISIBLE_LINES window of the table into the current
// frame. No clearDisplay() / display() of its own (see the header note).
void bubbleRender();
