// Tests for the Display class (issue #51, step 8 of 11 of the refactoring
// plan in #42).
//
// Pins the behavior of the render pipeline + the display-level actions:
//   - render() issues exactly ONE clearDisplay() + ONE display() per call
//     (the single-render-pass rule, issue #35, step 5 - now testable via
//     the fake display's frames / cleared counters)
//   - startRecording() with the buffer NOT allocated -> false, no RECORDING
//     state, no LED on
//   - startRecording() with WiFi NOT connected -> false, showWifiStatus()
//     called (the connecting status line is shown)
//   - startRecording() with WiFi connected + buffer allocated -> true,
//     beginStreaming() called (take position reset), LED on, the
//     "Recording (max 10 s)" status line set
//
// The shared `displayMgr` object is defined in tests/test_main.cpp (the
// host build's equivalent of the instance in TamAIgotchi.ino); the SSD1306
// shim records the draw calls the render assertions use. The startRecording
// tests use LOCAL Display instances with LOCAL dependencies so the buffer /
// WiFi / LED state is fully controlled (the shared `recorder` already has a
// buffer allocated by test_recorder, so the "buffer not allocated" case
// needs a fresh Recorder).
#include "test_main.h"
#include "hardware.h"  // hw (the shared Hardware object, issue #52, step 9)
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <ESPWifiConfig.h>  // ESPWifiConfig + STA_MODE / AP_MODE
#include "display.h"        // Display
#include "recorder.h"       // Recorder
#include "statusbar.h"      // StatusBar
#include "bubble.h"         // Bubble
#include "alien.h"          // AlienAnimation
#include "led.h"            // Led
#include "config.h"         // WIFI_AP_NAME, WIFI_SETUP_PORT, LED_PIN
#include "messages.h"       // MSG_WIFI_CONNECTING, MSG_RECORDING

// The shared panel object (a member of the shared `hw` Hardware object,
// defined in tests/test_main.cpp for the host build - issue #52, step 9);
// the render assertions read the SSD1306 shim's recorded counters.
extern Hardware hw;
// The shared display manager (defined in tests/test_main.cpp).
extern Display displayMgr;
// The app state machine (defined in tests/test_main.cpp) - startRecording()
// sets it to RECORDING on success.
extern RecState recState;

// A set of LOCAL Display dependencies with a controlled WiFi / buffer / LED
// state. `d` is constructed LAST (declaration order) so it can hold
// references to the other five. The ESPWifiConfig shim has no default
// constructor, so it is built with the 7-arg form (like hardware.h).
struct LocalDeps {
  Adafruit_SSD1306 panel;
  StatusBar status;
  AlienAnimation alien;
  Bubble bubble;
  ESPWifiConfig wifi;
  Recorder rec;
  Led led;
  Display d;
  LocalDeps()
      : status(panel),
        alien(panel),
        bubble(panel),
        wifi(WIFI_AP_NAME, WIFI_SETUP_PORT, -1, false, "", "", true),
        rec(hw.chat(), hw.audio()),
        led(LED_PIN),
        d(panel, status, alien, bubble, wifi, rec, led) {}
};

// --- render() (single render pass) -----------------------------------------

TEST(display_render_single_clear_and_display_per_call) {
  // Reset the fake display counters, then issue exactly one render pass.
  hw.panel().reset();
  CHECK_EQ_INT(hw.panel().frames, 0);
  CHECK_EQ_INT(hw.panel().cleared, 0);

  displayMgr.render();

  // Exactly one clearDisplay() + one display() (the single-render-pass rule,
  // issue #35, step 5) - no more, no less.
  CHECK_EQ_INT(hw.panel().cleared, 1);
  CHECK_EQ_INT(hw.panel().frames, 1);
}

TEST(display_render_two_calls_two_frames) {
  // Two render() calls -> two clearDisplay() + two display() (one each).
  hw.panel().reset();
  displayMgr.render();
  displayMgr.render();
  CHECK_EQ_INT(hw.panel().cleared, 2);
  CHECK_EQ_INT(hw.panel().frames, 2);
}

// --- startRecording() (buffer not allocated) --------------------------------

TEST(startRecording_buffer_not_allocated_returns_false) {
  LocalDeps deps;
  // A fresh Recorder has no buffer (bufferAllocated() == false).
  CHECK(!deps.rec.bufferAllocated());
  // WiFi connected, so the ONLY failure is the missing buffer.
  deps.wifi.ESP_mode = STA_MODE;
  deps.wifi.wifi_connected = true;
  recState = IDLE;
  deps.led.off();

  bool ok = deps.d.startRecording();

  CHECK(!ok);                 // returns false
  CHECK(recState != RECORDING); // no RECORDING state
  CHECK(!deps.led.isOn());     // no LED on
}

// --- startRecording() (WiFi not connected) ----------------------------------

TEST(startRecording_wifi_not_connected_returns_false) {
  LocalDeps deps;
  // Buffer allocated, so the ONLY failure is the missing WiFi link.
  CHECK(deps.rec.initRecBuffer());
  CHECK(deps.rec.bufferAllocated());
  deps.wifi.ESP_mode = STA_MODE;
  deps.wifi.wifi_connected = false;
  recState = IDLE;
  deps.led.off();

  bool ok = deps.d.startRecording();

  CHECK(!ok);                 // returns false
  CHECK(recState != RECORDING); // no RECORDING state
  CHECK(!deps.led.isOn());     // no LED on
  // showWifiStatus() was called: with STA mode + not connected it shows the
  // "Connecting to WiFi..." status line (the AP / connection status screen).
  CHECK(deps.status.line1() == String(MSG_WIFI_CONNECTING));
}

// --- startRecording() (WiFi connected + buffer allocated) --------------------

TEST(startRecording_wifi_connected_buffer_ok_returns_true) {
  LocalDeps deps;
  // Buffer allocated + WiFi connected -> success.
  CHECK(deps.rec.initRecBuffer());
  CHECK(deps.rec.bufferAllocated());
  deps.wifi.ESP_mode = STA_MODE;
  deps.wifi.wifi_connected = true;
  recState = IDLE;
  deps.led.off();

  // Pre-set the take position to non-zero so the reset by beginStreaming()
  // is detectable (a fresh Recorder already has position 0).
  deps.rec.noteChunk(1234);
  CHECK_EQ_INT(1234, deps.rec.recordedBytes());

  bool ok = deps.d.startRecording();

  CHECK(ok);                    // returns true
  CHECK(recState == RECORDING); // RECORDING state set
  CHECK(deps.led.isOn());       // LED on
  CHECK_EQ_INT(0, deps.rec.recordedBytes()); // beginStreaming() reset the position
  // The "Recording (max 10 s)" status line is set (overwrites the WiFi line
  // shown by showWifiStatus() earlier in the same call).
  CHECK(deps.status.line1() == String(MSG_RECORDING));
}
