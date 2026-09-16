// Text utilities for the OLED display (extracted from TamAIgotchi.ino as
// step 1 of the refactoring proposed in issue #18).
#include "text_utils.h"
#include <Arduino.h>  // String, Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object

// The `display` object is defined in TamAIgotchi.ino (the sketch entry
// point); reference it here instead of passing it through every call.
extern Adafruit_SSD1306 display;

void combinedOutput(int x, int y, char* line, bool clrscr) {
  if(clrscr) {
    display.clearDisplay();
  }
  Serial.println(line);
  display.setCursor(x, y);
  display.println(line);
  display.display();
}

// Word-wrap text into fixed-width lines (issue #13): one word per line
// boundary, words longer than the line width are hard-broken, existing
// newlines/tabs become line breaks. Returns the number of lines used.
//
// Core (4-arg) form (issue #31, step 1 of 6 of the UI restructure in #29):
// the output is a pointer array so the line width is not baked into the
// array type. Each out[i] must point to a buffer of at least `width + 1`
// chars (the caller owns the storage). Used by the scrollable response
// view (respLines[]), the speech-bubble widget (bubble.cpp) and
// displayError().
int wrapText(const String& text, char* out[], int width, int maxLines) {
  int count = 0;
  String word;
  String line;
  auto flushLine = [&]() {
    if (line.length() && count < maxLines) {
      line.toCharArray(out[count], width + 1);
      count++;
    }
    line = "";
  };
  auto addWord = [&]() {
    if (!word.length()) return;
    if (word.length() > width) {
      // Hard-break an over-long word (no spaces to break on).
      for (unsigned int i = 0; i < word.length(); i += width) {
        if (count >= maxLines) return;
        String chunk = word.substring(i, i + width);
        chunk.toCharArray(out[count], width + 1);
        count++;
      }
      word = "";
      return;
    }
    if (line.length() && line.length() + word.length() + 1 > width) flushLine();
    if (line.length()) line += " ";
    line += word;
    word = "";
  };
  for (unsigned int i = 0; i < text.length() && count < maxLines; i++) {
    char c = text.charAt(i);
    if (c == ' ' || c == '\n' || c == '\t') {
      addWord();
      if (c == '\n' && count < maxLines) flushLine();
    } else {
      word += c;
    }
  }
  addWord();
  flushLine();
  return count;
}

// 3-arg convenience form (the original signature, issue #13): wraps at
// RESPONSE_CHARS_PER_LINE. Kept so the current callers (displayError() and
// recorder.cpp, both passing a 2-D char array) are untouched (issue #31).
int wrapText(const String& text, char lines[][RESPONSE_CHARS_PER_LINE + 1], int maxLines) {
  char* out[RESPONSE_MAX_LINES];
  for (int i = 0; i < maxLines; i++) out[i] = lines[i];
  return wrapText(text, out, RESPONSE_CHARS_PER_LINE, maxLines);
}

// Show an error on the OLED (title line 1, wrapped detail lines 2-4) and
// mirror the full message to Serial. The detail text is wrapped at word
// boundaries to fit the 128 px display (21 chars/line at font size 1).
// The screen stays until the next button press (the button flow re-shows
// the WiFi status first).
void displayError(const String& title, const String& detail) {
  Serial.print(F("ERROR: "));
  Serial.println(title);
  if (detail.length()) {
    Serial.print(F("       "));
    Serial.println(detail);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(title);

  char errLines[3][RESPONSE_CHARS_PER_LINE + 1];
  int n = wrapText(detail, errLines, 3);
  for (int i = 0; i < n; i++) {
    display.setCursor(0, 16 + 16 * i);
    display.println(errLines[i]);
  }
  display.display();
}
