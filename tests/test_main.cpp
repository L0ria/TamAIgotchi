// Host test harness for the TamAIgotchi firmware modules (issue #44, step
// 1 of 11 of the refactoring plan in #42).
//
// Provides:
//   - the state behind the shims: host_now_ms (millis()), host_pin_level[]
//     (digitalRead/Write), the Serial object, and the shared `display`
//     object (hardware.h defines it on the device; the host build defines
//     it here so the modules' `extern Adafruit_SSD1306 display;` links)
//   - the assertion-framework implementation (see test_main.h)
//   - main(): runs every registered test, prints a summary, non-zero exit
//     on any failure
#include "test_main.h"
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Shim state (declared extern in tests/shims/Arduino.h)
// ---------------------------------------------------------------------------
unsigned long host_now_ms = 0;
int host_pin_level[64];
SerialClass Serial;
size_t host_free_heap_bytes = 0;   // esp_get_free_heap_size() (Arduino.h shim)
size_t host_heap_free_bytes = 0;   // heap_caps_get_free_size() (esp_heap_caps.h shim)

// The shared hardware object (hardware.cpp on the device; the host build
// defines it here so the modules' `extern Hardware hw;` links) - issue
// #52, step 9 of 11 of the refactoring plan in #42: the six library
// objects (panel / i2s / wifi / openai / chat / audio) that used to be
// defined here (display / wifiConfig / openai / chat / audio) are now the
// private members of `hw`, exactly like the device (hardware.cpp).
// Hardware::init() is never called from the tests - the bring-up is
// device-only (the shims would just count no-op calls).
// The TwoWire object (the real core defines it; the host build defines it
// here so hardware.cpp's `&Wire` in the Hardware constructor links).
#include <Wire.h>
TwoWire Wire;

#include "hardware.h"
Hardware hw;

// The shared status-bar object (TamAIgotchi.ino on the device; the host
// build defines it here so statusbar.cpp's `extern StatusBar statusBar;`
// links) - the same pattern as the shared `hw` object. issue #52, step 9:
// the panel is a constructor-injected reference (hw.panel()).
#include "statusbar.h"
StatusBar statusBar(hw.panel());

// The shared recording-LED object (TamAIgotchi.ino on the device; the
// host build defines it here so led.cpp's `extern Led led;` links) -
// the same pattern as the shared `statusBar` object.
#include "led.h"
Led led(LED_PIN);

// The shared recorder object (TamAIgotchi.ino on the device; the host
// build defines it here so recorder.cpp's `extern Recorder recorder;`
// links) - the same pattern as the shared `statusBar` / `led` objects.
// issue #52, step 9: the OpenAI clients are constructor-injected
// references (hw.chat() / hw.audio()) - the recorder.cpp externs are gone.
#include "recorder.h"
Recorder recorder(hw.chat(), hw.audio());

// The app state machine (TamAIgotchi.ino on the device; the host build
// defines it here so recorder.cpp's `extern RecState recState;` links).
// Starts at IDLE.
RecState recState = IDLE;

// (issue #52, step 9: the OpenAI client globals moved into the `hw`
// Hardware object above - they are now hw.openai() / hw.chat() /
// hw.audio(), the same as on the device (hardware.cpp). The network
// behavior is NOT emulated (see the OpenAI.h shim) - the tests exercise
// the buffer/streaming API, not the SENDING/RESPONSE flow.)

// The shared alien object (TamAIgotchi.ino on the device; the host build
// defines it here so recorder.cpp's `extern AlienAnimation alien;` links).
// issue #52, step 9: the panel is a constructor-injected reference
// (hw.panel()) - the alien.cpp extern is gone.
#include "alien.h"
AlienAnimation alien(hw.panel());

// The ESP object (Arduino.h shim) - defined here so the extern links.
EspClass ESP;

// The WiFi SSID backing store (tests/shims/WiFi.h) - driven by
// host_set_wifi_ssid() so Display::showWifiStatus()'s STA branch is testable.
#include <WiFi.h>
String host_wifi_ssid;

// (issue #52, step 9: the shared WiFi-config object moved into the
// `hw` Hardware object above - it is now hw.wifi(), the same as on the
// device (hardware.cpp).)

// The shared WiFi object (tests/shims/WiFi.h) - defined here so the extern links.
WiFiClass WiFi;

// The shared bubble object (TamAIgotchi.ino on the device; the host build
// defines it here so display.cpp's `bubble_` reference + the modules'
// `extern Bubble bubble;` link) - moved from test_bubble.cpp in step 8
// (issue #51): display.cpp now reaches `bubble` through the Display class,
// so the shared object lives with the other shared objects.
// issue #52, step 9: the panel is a constructor-injected reference
// (hw.panel()) - the bubble.cpp extern is gone.
#include "bubble.h"
Bubble bubble(hw.panel());

// The shared display-manager object (TamAIgotchi.ino on the device; the
// host build defines it here so the modules' `extern Display displayMgr;`
// links). Constructed AFTER the six objects it references (display /
// statusBar / alien / bubble / wifiConfig / recorder / led) - the same
// shared-object pattern as the device (issue #51, step 8 of 11 of the
// refactoring plan in #42). display.cpp is compiled into the host build
// (added to run_tests.sh), so the single render pass is now real and the
// tests can assert on the SSD1306 shim's frames / cleared counters.
#include "display.h"
Display displayMgr(hw.panel(), statusBar, alien, bubble, hw.wifi(), recorder, led);

void host_set_pin(int pin, int level) {
  if (pin >= 0 && pin < 64) host_pin_level[pin] = level;
}

// ---------------------------------------------------------------------------
// Assertion-framework implementation
// ---------------------------------------------------------------------------
static long g_checks_passed = 0;
static long g_checks_failed = 0;
static std::string g_current_test;

void check(bool cond, const char* what, const char* file, int line) {
  if (cond) {
    g_checks_passed++;
  } else {
    g_checks_failed++;
    std::printf("  FAIL [%s] %s\n        at %s:%d\n", g_current_test.c_str(),
                what, file, line);
  }
}

void check_eq(const char* a, const char* b, const char* what, const char* file,
              int line) {
  std::string sa = a ? a : "(null)";
  std::string sb = b ? b : "(null)";
  if (sa == sb) {
    g_checks_passed++;
  } else {
    g_checks_failed++;
    std::printf("  FAIL [%s] %s: \"%s\" != \"%s\"\n        at %s:%d\n",
                g_current_test.c_str(), what, sa.c_str(), sb.c_str(), file,
                line);
  }
}

void check_eq_long(long a, long b, const char* what, const char* file, int line) {
  if (a == b) {
    g_checks_passed++;
  } else {
    g_checks_failed++;
    std::printf("  FAIL [%s] %s: %ld != %ld\n        at %s:%d\n",
                g_current_test.c_str(), what, a, b, file, line);
  }
}

using TestFn = void (*)();
static std::vector<std::pair<std::string, TestFn>> g_tests;

void register_test(const char* name, TestFn fn) { g_tests.emplace_back(name, fn); }

// ---------------------------------------------------------------------------
// main: run every registered test, print a summary, exit non-zero on failure
// ---------------------------------------------------------------------------
int main() {
  // Pin table defaults to HIGH (released / pull-up), like the real hardware.
  for (int i = 0; i < 64; i++) host_pin_level[i] = HIGH;

  std::printf("== TamAIgotchi host tests (%zu tests) ==\n", g_tests.size());
  for (const auto& t : g_tests) {
    g_current_test = t.first;
    long before_failed = g_checks_failed;
    t.second();
    std::printf("%s %s\n", (g_checks_failed == before_failed) ? "PASS" : "FAIL",
                t.first.c_str());
  }

  std::printf("== %ld checks passed, %ld failed ==\n", g_checks_passed,
              g_checks_failed);
  if (g_checks_failed) {
    std::printf("RESULT: FAIL\n");
    return 1;
  }
  std::printf("RESULT: OK\n");
  return 0;
}
