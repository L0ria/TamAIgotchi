// Status bar (issue #32, step 2 of 6 of the UI restructure in #29).
//
// The top two display lines (y=0 and y=8, font size 1) are reserved for
// status: WiFi state (AP name + setup address / SSID + IP / connecting),
// I2S bring-up, and the boot & WiFi errors. The bottom half is reserved
// for the alien + bubble (the alien moves there in step 5).
//
// Status rule (issue #29): status messages must fit
// STATUS_CHARS_PER_LINE chars x 2 lines (21 x 2 at font size 1). Longer
// text goes to the bubble (step 5) or Serial - never rely on the SSD1306
// driver clipping. statusShow() / statusError() truncate to 21 chars, so
// an over-long message can no longer spill into the alien/bubble area.
//
//   statusShow(l1, l2="")  - the two status lines (y=0 / y=8), each
//                            truncated to STATUS_CHARS_PER_LINE, both
//                            mirrored to Serial, then display.display()
//   statusError(title, detail) - 2-line error form: line 1 = title,
//                            line 2 = detail (truncated); the FULL title
//                            + detail always go to Serial
//   statusClear()          - blank the two lines (helper for the single
//                            render pass that arrives in step 5)
//
// The shared `display` object is referenced via extern, the same pattern
// as the other modules (bubble.cpp / text_utils.cpp / alien.cpp /
// recorder.cpp / display.cpp).
#pragma once

#include "config.h"  // STATUS_CHARS_PER_LINE

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

// Render the two status lines at the top of the screen (line 1 at y=0,
// line 2 at y=8) and mirror BOTH lines to Serial, then flush the panel.
// Each line is truncated to STATUS_CHARS_PER_LINE (21 chars) - the two
// lines are blanked (not cleared) first, so the rest of the frame
// (alien / bubble) is preserved. No clearDisplay() (the single render
// pass arrives in step 5). The 1-arg overload is the same call with an
// empty line 2 (no default argument: String is only forward-declared
// here, so the empty-string temporary is built in the .cpp).
void statusShow(const String& l1, const String& l2);
void statusShow(const String& l1);

// 2-line error form: line 1 = title, line 2 = detail (each truncated to
// STATUS_CHARS_PER_LINE). The FULL title + detail are always logged to
// Serial (the truncated on-screen text is not enough to debug with).
void statusError(const String& title, const String& detail);

// Blank the two status lines and flush the panel (helper for the single
// render pass in step 5).
void statusClear();
