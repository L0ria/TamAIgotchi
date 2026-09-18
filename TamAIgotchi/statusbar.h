// Status bar (issue #32, step 2 of 6 of the UI restructure in #29;
// wrapped in the StatusBar class in step 3 of 11 of the refactoring plan
// in #42, issue #46).
//
// The top two display lines (y=0 and y=8, font size 1) are reserved for
// status: WiFi state (AP name + setup address / SSID + IP / connecting),
// I2S bring-up, and the boot & WiFi errors. The bottom half is reserved
// for the alien + bubble (the alien moves there in step 5).
//
// Status rule (issue #29): status messages must fit
// STATUS_CHARS_PER_LINE chars x 2 lines (21 x 2 at font size 1). Longer
// text goes to the bubble (step 5) or Serial - never rely on the SSD1306
// driver clipping. show() / error() truncate to 21 chars, so an over-long
// message can no longer spill into the alien/bubble area.
//
//   statusBar.show(l1, l2)  - the two status lines (y=0 / y=8), each
//                             truncated to STATUS_CHARS_PER_LINE, both
//                             mirrored to Serial, then the single render
//                             pass renderScreen() (issue #35, step 5)
//   statusBar.error(title, detail) - 2-line error form: line 1 = title,
//                             line 2 = detail (truncated); the FULL title
//                             + detail always go to Serial
//   statusBar.clear()       - blank the two lines + renderScreen()
//   statusBar.draw()        - redraw the two stored lines (no Serial, no
//                             panel push) - called by renderScreen()
//                             (issue #35, step 5) on every frame
//
// The shared `display` object is referenced via extern, the same pattern
// as the other modules (bubble.cpp / text_utils.cpp / alien.cpp /
// recorder.cpp / display.cpp).
#pragma once

#include <Arduino.h>  // String (complete type: the stored lines l1_/l2_ are
// String members - unlike Bubble, which stores a char table)
#include "config.h"  // STATUS_CHARS_PER_LINE

// The status-bar widget: the two stored status lines + rendering (see the
// header note for the API). The stored lines (l1_ / l2_) are private;
// line1() / line2() / lineCount() expose them read-only.
class StatusBar {
 public:
  // Render the two status lines (y=0 / y=8), mirror both to Serial, then
  // push the full frame through the single render pass renderScreen()
  // (issue #35, step 5). Each line is truncated to STATUS_CHARS_PER_LINE
  // (21 chars) - the two lines are blanked (not cleared) first, so the
  // rest of the frame (alien / bubble) is preserved. The 1-arg overload
  // is the same call with an empty line 2 (no default argument: String is
  // only forward-declared here, so the empty-string temporary is built in
  // the .cpp).
  void show(const String& l1, const String& l2);
  void show(const String& l1);

  // 2-line error form: line 1 = title, line 2 = detail (each truncated to
  // STATUS_CHARS_PER_LINE). The FULL title + detail are always logged to
  // Serial (the truncated on-screen text is not enough to debug with).
  void error(const String& title, const String& detail);

  // Blank the two status lines and push the frame through the single
  // render pass (issue #35, step 5).
  void clear();

  // Draw the two stored status lines at the top of the screen (line 1 at
  // y=0, line 2 at y=8) into the current frame. No Serial, no panel push -
  // called by renderScreen() (issue #35, step 5) on every frame.
  void draw();

  // Number of stored lines in use (1 = line 1 only, 2 = both lines,
  // 0 = cleared).
  int lineCount() const;

  // The two stored lines (read-only; empty when cleared).
  const String& line1() const;
  const String& line2() const;

 private:
  String l1_;
  String l2_;

  // Truncate `s` to at most STATUS_CHARS_PER_LINE chars (no ellipsis -
  // plain hard cut, like the bubble's line table) and return it.
  static String fit(const String& s);

  // Blank the two status lines (top 16 px of the 128x64 panel) without
  // touching the rest of the frame.
  static void blank();
};

// The shared status-bar object (the codebase's existing shared-object
// pattern; the instance is defined in TamAIgotchi.ino next to the other
// shared objects, the same pattern as the shared `bubble` object).
extern StatusBar statusBar;
