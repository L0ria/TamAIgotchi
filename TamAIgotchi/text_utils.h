// Text utilities for the OLED display (extracted from TamAIgotchi.ino as
// step 1 of the refactoring proposed in issue #18).
//
//   combinedOutput() - print a line to Serial AND the display
//   wrapText()       - word-wrap a String into fixed-width char lines
//
// (displayError() was removed in step 3 of the UI restructure in #29,
// issue #33 - the 2-line status-bar form statusError() replaced it.)
//
// Both use the shared `display` object declared in TamAIgotchi.ino.
#pragma once
#include "config.h"  // RESPONSE_CHARS_PER_LINE

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

void combinedOutput(int x, int y, char* line, bool clrscr);

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
// Used by the scrollable response view (respLines[]) and the speech-bubble
// widget (bubble.cpp).
int wrapText(const String& text, char* out[], int width, int maxLines);

// 3-arg convenience form (the original signature, issue #13): wraps at
// RESPONSE_CHARS_PER_LINE. The current caller (recorder.cpp, the response
// table) passes a 2-D char array and keeps using this one untouched.
int wrapText(const String& text, char lines[][RESPONSE_CHARS_PER_LINE + 1], int maxLines);

