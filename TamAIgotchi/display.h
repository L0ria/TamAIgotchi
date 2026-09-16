// OLED display helpers (extracted from TamAIgotchi.ino as step 5 of the
// refactoring proposed in issue #18).
//
//   showWifiStatus()           - the AP / connected / connecting status screen
//   resetWifiSettingsAndRestart() - 5 s hold escape hatch: wipe settings, reboot
//   startRecording()           - the shared "start a take" block (dedupes the
//                                IDLE and RESPONSE branches of loop())
//
// All three use the shared `display` / `wifiConfig` objects (defined in
// hardware.h, referenced via extern in display.cpp) and the speech-bubble
// widget (bubble.{h,cpp}, issue #31 - startRecording() clears it when a
// new take starts from the RESPONSE state, issue #34, step 4).
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

// Start a new recording take (dedupes the ~15-line block that used to be
// copied verbatim in the IDLE and RESPONSE branches of loop()): reset the
// take position, switch to RECORDING, turn the LED on and show the
// "Recording (max 10 s)" status line (issue #33, step 3 of the UI
// restructure in #29 - the elapsed-seconds counter is refreshed from the
// RECORDING branch of loop()).
// Returns false (and shows the error) if the recording buffer was not
// allocated at boot, or if the WiFi is not connected (the AP / connection
// status screen is shown instead) - the caller must not start a take.
bool startRecording();
