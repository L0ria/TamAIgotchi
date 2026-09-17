// Baseline tests for Button (issue #44, step 1 of 11 of the refactoring
// plan in #42).
//
// These pin the CURRENT debounce / long-press / re-arm semantics so steps
// 2-10 (and in particular step 5, #48, which deliberately changes two of
// them) cannot silently regress. The tests drive time via host_set_millis()
// and the pin via host_set_pin(), so they are fully deterministic.
//
// Button timing (config.h): BUTTON_DEBOUNCE_MS = 50, BUTTON_LONG_PRESS_MS = 5000.
#include "test_main.h"
#include <Arduino.h>  // host shim: host_set_millis/host_set_pin/HIGH/LOW
#include "buttons.h"

static const int PIN = BUTTON_PIN;  // config.h (the main button)

// Reset the shared test state (pin + clock) to a known baseline.
static void reset_hw() {
  host_set_millis(0);
  host_set_pin(PIN, HIGH);
}

TEST(button_press_fires_isPressed_exactly_once_after_debounce) {
  reset_hw();
  Button b(PIN);
  b.update();  // t=0, pin HIGH

  // Press the button.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();  // sees the press edge, records pressStart = 100

  // Before the debounce window has elapsed: no press yet.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS - 1);
  b.update();
  CHECK(!b.isPressed());

  // After the debounce window: the press fires exactly once.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
  CHECK(!b.isPressed());  // one-shot: second read in the same pass is false
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 100);
  b.update();
  CHECK(!b.isPressed());  // and it does not fire again while still held
}

TEST(button_sub_debounce_blip_does_not_fire) {
  reset_hw();
  Button b(PIN);
  b.update();

  // A blip shorter than the debounce window: pin goes LOW and back HIGH
  // before BUTTON_DEBOUNCE_MS elapses.
  host_set_millis(10);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(30);  // still < 50 ms
  host_set_pin(PIN, HIGH);
  b.update();  // sees the release, re-arms
  host_set_millis(200);
  b.update();
  CHECK(!b.isPressed());  // the blip never fired
}

TEST(button_release_rearms_second_press_fires) {
  reset_hw();
  Button b(PIN);
  b.update();

  // First press.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
  CHECK(!b.isPressed());

  // Release.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 100);
  host_set_pin(PIN, HIGH);
  b.update();  // sees the release, clears acted/longFired

  // Second press fires again.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
  CHECK(!b.isPressed());
}

TEST(button_long_press_fires_once_at_5000ms) {
  reset_hw();
  Button b(PIN);
  b.update();

  // Hold the button.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();

  // The short-press fires first (at the debounce window).
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());

  // Before the long-press threshold: not fired yet.
  host_set_millis(100 + BUTTON_LONG_PRESS_MS - 1);
  b.update();
  CHECK(!b.isLongPressed());

  // At the threshold: fires exactly once.
  host_set_millis(100 + BUTTON_LONG_PRESS_MS);
  b.update();
  CHECK(b.isLongPressed());
  CHECK(!b.isLongPressed());  // one-shot

  // And it does not fire again while still held.
  host_set_millis(100 + BUTTON_LONG_PRESS_MS + 1000);
  b.update();
  CHECK(!b.isLongPressed());
}

TEST(button_reset_rearms) {
  reset_hw();
  Button b(PIN);
  b.update();

  // Press and fire.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());

  // reset() forgets the current press: even though the pin is still LOW,
  // the next isPressed() must not fire until a fresh press edge.
  b.reset();
  CHECK(!b.isPressed());
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 100);
  b.update();
  CHECK(!b.isPressed());  // still no fire (no new edge since reset)

  // A fresh press edge fires again.
  host_set_pin(PIN, HIGH);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
}
