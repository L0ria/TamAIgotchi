// OLED display helpers (extracted from TamAIgotchi.ino as step 5 of the
// refactoring proposed in issue #18; wrapped in the Display class in step 8
// of 11 of the refactoring plan in #42, issue #51).
//
// The Display class owns the render pipeline + the display-level actions:
//
//   displayMgr.render()                    - THE single render pass
//   displayMgr.showWifiStatus()            - the AP / connected / connecting status screen
//   displayMgr.resetWifiSettingsAndRestart() - 5 s hold escape hatch: wipe settings, reboot
//   displayMgr.startRecording()            - the shared "start a take" block (dedupes the
//                                            IDLE and RESPONSE branches of loop())
//
// render() (formerly the free function renderScreen()) is the ONLY place
// that calls panel_.clearDisplay() / panel_.display() (the single-render-
// pass rule, issue #35, step 5 of 6 of the UI restructure in #29).
//
// The six dependencies (panel, status bar, alien, bubble, WiFi config,
// recorder) are constructor-injected references (issue #51, step 8 of 11
// of the refactoring plan in #42: the free-function externs are gone).
// The shared panel / WiFi objects are members of the `hw` Hardware object
// (hardware.cpp, issue #52, step 9); the `statusBar` / `alien` / `bubble`
// / `recorder` / `led` objects stay defined in TamAIgotchi.ino - the
// Display instance (displayMgr) is constructed after them, the same
// shared-object pattern.
//
// Circular include broken (finding #7 in the #42 audit): display.h no
// longer includes alien.h / recorder.h - it forward-declares the six
// dependency types and takes them by reference in the constructor (defined
// in display.cpp, where the types are complete). statusbar.cpp / alien.cpp
// / recorder.cpp no longer pull in alien.h / recorder.h through display.h.
#pragma once

#include "config.h"  // LED_PIN, D_T*()

// Forward declarations (complete types via the module headers in display.cpp
// + the shims in the host build). Forward-declaring instead of including
// alien.h / recorder.h is what breaks the circular include (finding #7 in
// the #42 audit): display.h no longer drags the alien / recorder headers
// into every translation unit that includes it.
class Adafruit_SSD1306;
class StatusBar;
class AlienAnimation;
class Bubble;
class ESPWifiConfig;
class Recorder;
class Led;

// The display manager: owns the single render pass + the display-level
// actions (WiFi status screen, the WiFi-reset escape hatch, and the shared
// "start a take" block). The six dependencies are constructor-injected
// references (issue #51, step 8 of 11 of the refactoring plan in #42).
class Display {
 public:
  // Construct with references to the six shared objects (all must outlive
  // this instance - they are sketch-lifetime objects). Defined in
  // display.cpp, where the types are complete.
  Display(Adafruit_SSD1306& panel, StatusBar& status, AlienAnimation& alien,
          Bubble& bubble, ESPWifiConfig& wifi, Recorder& rec, Led& led);

  // THE single render pass (issue #35, step 5 of 6 of the UI restructure in
  // #29): exactly one clearDisplay() + one display() per frame, composing
  // the three regions in a fixed order:
  //   1. the status bar (its two lines, drawn by status_.draw())
  //   2. the alien sprite (always present: the stand frame when the idle
  //      animation is not running, the current animation frame while it is)
  //   3. the speech bubble (rectangle + tail + visible window; while the
  //      idle animation runs, the animation's bubble content instead -
  //      drawn without touching the line table, so the response survives)
  // Every state change (status line, bubble text, animation phase, scroll)
  // ends in this one call - never clearDisplay()/display() directly, so the
  // three regions can never be drawn in different frames (flicker /
  // half-states, the single-render-pass rule from #29).
  void render();

  // Show the current WiFi situation on the display (and Serial).
  //  - AP mode:    show the access point name + IP so it can be configured
  //  - connected:  show the IP address assigned by the router
  //  - otherwise:  show that it is still trying to connect
  // Always ends with alien_.markActivity(): showWifiStatus() is the result
  // of a button press (or boot) - re-arm the idle timer (issue #16).
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

 private:
  // The six shared objects (constructor-injected references, issue #51).
  Adafruit_SSD1306& panel_;
  StatusBar& status_;
  AlienAnimation& alien_;
  Bubble& bubble_;
  ESPWifiConfig& wifi_;
  Recorder& rec_;
  Led& led_;
};

// The shared display-manager object (the codebase's existing shared-object
// pattern; the instance is defined in TamAIgotchi.ino next to the other
// shared objects - constructed after them, since it holds references to
// them, the same pattern as the shared `display` / `bubble` / `statusBar`
// objects).
extern Display displayMgr;
