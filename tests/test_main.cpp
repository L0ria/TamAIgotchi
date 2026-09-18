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

// The shared display object (hardware.h on the device; the host build
// defines it here so the modules' `extern Adafruit_SSD1306 display;` links).
Adafruit_SSD1306 display;  // defaults 128x64; the fake ignores the HW args

// The shared status-bar object (TamAIgotchi.ino on the device; the host
// build defines it here so statusbar.cpp's `extern StatusBar statusBar;`
// links) - the same pattern as the shared `display` object.
#include "statusbar.h"
StatusBar statusBar;

// Host stub for the single render pass (display.cpp, step 8 of 11): the
// modules' show()/clear() call it; the host build has no display.cpp, so
// it is a no-op here (the tests assert on the stored state + the SSD1306
// shim's recorded draw calls, not on the frame push).
void renderScreen() {}

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
