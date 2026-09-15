// OLED display helpers (extracted from TamAIgotchi.ino as step 5 of the
// refactoring proposed in issue #18).
//
//   showWifiStatus()           - the AP / connected / connecting status screen
//   resetWifiSettingsAndRestart() - 5 s hold escape hatch: wipe settings, reboot
//   renderResponseWindow()     - the scrollable "Response: x/y" window (issue #13)
//   startRecording()           - the shared "start a take" block (dedupes the
//                                IDLE and RESPONSE branches of loop())
//
// All four use the shared `display` / `wifiConfig` objects (defined in
// hardware.h, referenced via extern in display.cpp) and the scrollable
// response table (respLines[] / respLineCount / scrollOffset, owned by the
// sketch and shared via extern - see recorder.h for the same pattern).
#pragma once

#include "config.h"      // RESPONSE_*, LED_PIN, D_T*()
#include "recorder.h"    // RecState (startRecording() sets recState)

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

// Show the current WiFi situation on the display (and Serial).
//  - AP mode:    show the access point name + IP so it can be configured
//  - connected:  show the IP address assigned by the router
//  - otherwise:  show that it is still trying to connect
// Always ends with alien.markActivity(): showWifiStatus() is the result of
// a button press (or boot) - re-arm the idle timer (issue #16).
void showWifiStatus();

// Escape hatch: wipe the stored WiFi (and web) credentials and reboot.
// With no saved SSID the library drops into AP mode, so the device
// comes back up serving the setup page again. Used by the 5 s
// long-press of the WiFi-config button when a wrong password was saved
// and the device would otherwise keep retrying forever.
// resetAllSettings() covers all registered settings (built-in + the LocalAI
// URL/key user slots), so the config.h defaults come back after the reboot.
// Does not return (ESP.restart()).
void resetWifiSettingsAndRestart();

// Render the current response window (issue #13). The default font is
// 6x8 px, so the 128x64 screen holds 21 chars x 8 lines. Line 0 is the
// "Response: x/y" header (x = first visible line, y = total lines); line 1
// is a blank separator; the next RESPONSE_VISIBLE_LINES lines are the
// window starting at scrollOffset. Lines are printed consecutively
// (println auto-advances 8 px), matching showWifiStatus().
void renderResponseWindow();

// Start a new recording take (dedupes the ~15-line block that used to be
// copied verbatim in the IDLE and RESPONSE branches of loop()): reset the
// take position, switch to RECORDING, turn the LED on and show the
// "Recording / max 10 s" screen.
// Returns false (and shows the error) if the recording buffer was not
// allocated at boot, or if the WiFi is not connected (the AP / connection
// status screen is shown instead) - the caller must not start a take.
bool startRecording();
