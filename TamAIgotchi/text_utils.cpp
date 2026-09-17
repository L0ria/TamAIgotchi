// Text utilities for the OLED display (extracted from TamAIgotchi.ino as
// step 1 of the refactoring proposed in issue #18).
#include "text_utils.h"
#include <Arduino.h>  // String, Serial, F()

// Word-wrap text into fixed-width lines (issue #13): one word per line
// boundary, words longer than the line width are hard-broken, existing
// newlines/tabs become line breaks. Returns the number of lines used.
//
// Core (4-arg) form (issue #31, step 1 of 6 of the UI restructure in #29):
// the output is a pointer array so the line width is not baked into the
// array type. Each out[i] must point to a buffer of at least `width + 1`
// chars (the caller owns the storage). Used by the scrollable response
// widget (bubble.cpp).
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
