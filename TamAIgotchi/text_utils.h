// Text utilities for the OLED display (extracted from TamAIgotchi.ino as
// step 1 of the refactoring proposed in issue #18).
//
//   wrapText() - word-wrap a String into fixed-width char lines
//
// The display-role helper that used to live here was replaced by the status
// bar (statusShow() / statusError(), issue #32, step 2) and removed during
// the UI restructure in #29 (issues #33 / #36) - this module is wrapText()
// only.
#pragma once

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

// Word-wrap text into fixed-width lines (issue #13): one word per line
// boundary, words longer than the line width are hard-broken, existing
// newlines/tabs become line breaks. Returns the number of lines used.
//
// The core (4-arg) form takes the output as a pointer array so the line
// width is not baked into the array type (issue #31, step 1 of 6 of the UI
// restructure in #29):
//   out[]    - each element points to a buffer of at least `width + 1`
//              chars (the caller owns the storage - the static response
//              table, the bubble's line table, ...)
//   width    - max chars per line (excluding the NUL)
//   maxLines - capacity of out[]
// Used by the speech-bubble widget (bubble.cpp).
int wrapText(const String& text, char* out[], int width, int maxLines);
