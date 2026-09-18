// Tests for the Led class (issue #47, step 4 of 11 of the refactoring
// plan in #42).
//
// Pins the behavior of the 1:1 class wrap of the raw
// digitalWrite(LED_PIN, ...) calls:
//   - on() sets the pin HIGH (asserted via the shim pin table) and
//     isOn() is true
//   - off() sets the pin LOW and isOn() is false
//   - on() is idempotent (state stays on, no crash)
//   - off() is idempotent (state stays off, no crash)
//   - the constructor starts in the off state
//
// The shared `led` object is defined in tests/test_main.cpp (the host
// build's equivalent of the instance in TamAIgotchi.ino); the pin table
// behind the shim's digitalWrite() is what the on/off assertions read.
#include "test_main.h"
#include <Arduino.h>  // host shim: host_set_pin / HIGH / LOW

#include "config.h"  // LED_PIN
#include "led.h"

// Reset the shared test state (pin + LED logical state) to a known
// baseline.
static void reset_hw() {
  host_set_pin(LED_PIN, HIGH);  // released / pull-up default
  led.off();                    // logical state: off
}

// --- initial state ---------------------------------------------------------

TEST(led_starts_off) {
  // A fresh instance starts in the off state (the shared `led` object has
  // already been driven by earlier tests, so the initial-state assertion
  // uses its own instance on the same pin).
  Led fresh(LED_PIN);
  CHECK(!fresh.isOn());
}

// --- on --------------------------------------------------------------------

TEST(led_on_sets_pin_high_and_isOn_true) {
  reset_hw();
  led.on();
  CHECK_EQ_INT(digitalRead(LED_PIN), HIGH);  // the shim pin table
  CHECK(led.isOn());
}

TEST(led_on_is_idempotent) {
  reset_hw();
  led.on();
  led.on();  // second on(): no crash, state stays on
  CHECK(led.isOn());
  CHECK_EQ_INT(digitalRead(LED_PIN), HIGH);
}

// --- off -------------------------------------------------------------------

TEST(led_off_sets_pin_low_and_isOn_false) {
  reset_hw();
  led.on();
  CHECK(led.isOn());
  led.off();
  CHECK_EQ_INT(digitalRead(LED_PIN), LOW);  // the shim pin table
  CHECK(!led.isOn());
}

TEST(led_off_is_idempotent) {
  reset_hw();
  led.off();
  led.off();  // second off(): no crash, state stays off
  CHECK(!led.isOn());
}

// --- full on/off cycle -----------------------------------------------------

TEST(led_on_off_on_cycle) {
  reset_hw();
  led.on();
  CHECK(led.isOn());
  CHECK_EQ_INT(digitalRead(LED_PIN), HIGH);
  led.off();
  CHECK(!led.isOn());
  CHECK_EQ_INT(digitalRead(LED_PIN), LOW);
  led.on();
  CHECK(led.isOn());
  CHECK_EQ_INT(digitalRead(LED_PIN), HIGH);
}
