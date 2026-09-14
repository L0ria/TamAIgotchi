// Text utilities for the OLED display (extracted from TamAIgotchi.ino as
// step 1 of the refactoring proposed in issue #18).
//
//   combinedOutput() - print a line to Serial AND the display
//   wrapText()       - word-wrap a String into fixed-width char lines
//   displayError()   - show a titled error box on the display + Serial
//
// All three use the shared `display` object declared in TamAIgotchi.ino.
#pragma once
#include "config.h"  // RESPONSE_CHARS_PER_LINE

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

void combinedOutput(int x, int y, char* line, bool clrscr);

// Word-wrap text into fixed-width lines (issue #13): one word per line
// boundary, words longer than the line width are hard-broken, existing
// newlines/tabs become line breaks. Returns the number of lines used.
// Used by the scrollable response view (respLines[]) and reused by
// displayError() for its detail text.
int wrapText(const String& text, char lines[][RESPONSE_CHARS_PER_LINE + 1], int maxLines);

// Show an error on the OLED (title line 1, wrapped detail lines 2-4) and
// mirror the full message to Serial. The detail text is wrapped at word
// boundaries to fit the 128 px display (21 chars/line at font size 1).
// The screen stays until the next button press (the button flow re-shows
// the WiFi status first).
void displayError(const String& title, const String& detail);
