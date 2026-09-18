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

// ============================================================================
// Step 5 (issue #48) — Button fixes + isHeld().
//
// These pin the behavior changed / added in step 5:
//   - Finding #4 (release re-arm): a press that is never consumed (the
//     one-shot flag left set) must still fire on the NEXT press after a
//     release. If the current update() re-arms correctly the test passes
//     and pins the behavior; if it did not, the fix makes it pass.
//   - Finding #5 (consistent threshold): isPressed() now uses >= so a press
//     at exactly BUTTON_DEBOUNCE_MS (50 ms) fires (was false before the fix);
//     isLongPressed() already used >= (a press at exactly 5000 ms fires) — pin it.
//   - isHeld(): false when HIGH, true once stably LOW, false again after release.
//
// (The Finding #4 regression test is the one the #42 audit flagged as a
// latent "stale one-shot flag" bug. It is written first, per the issue.)
// ============================================================================

// --- Finding #4: release re-arm (the stale-flag scenario) -------------------

// A press whose isPressed() is NEVER consumed (the one-shot flag left set),
// then release, then a fresh press: the next press MUST fire. This is the
// regression the #42 audit flagged (findings #4/#5) as a potentially
// swallowed press.
TEST(step5_finding4_unconsumed_press_release_repress_fires) {
  reset_hw();
  Button b(PIN);
  b.update();  // t=0, pin HIGH

  // First press — held past the debounce window, but isPressed() is NEVER
  // called, so the acted flag is left unset for this press.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();  // sees the press edge, pressStart = 100
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 10);  // held past the window
  b.update();  // (isPressed() deliberately NOT read here)

  // Release the button: update() must clear the one-shot flags on the
  // HIGH edge so the next press can act.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200);
  host_set_pin(PIN, HIGH);
  b.update();  // sees the release, re-arms

  // Fresh press: it must fire (not be swallowed by a stale flag).
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 300);
  host_set_pin(PIN, LOW);
  b.update();  // press edge
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 300 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());          // the re-press fires
  CHECK(!b.isPressed());         // and it is still one-shot
}

// The normal path, pinned for contrast: press -> consume -> release ->
// re-press -> fires again. (Covers the consumed-flag re-arm.)
TEST(step5_finding4_consumed_press_release_repress_fires) {
  reset_hw();
  Button b(PIN);
  b.update();

  // First press, consumed (isPressed() read).
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
  CHECK(!b.isPressed());

  // Release.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 200);
  host_set_pin(PIN, HIGH);
  b.update();

  // Re-press fires again.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 300);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 300 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());
  CHECK(!b.isPressed());
}

// --- Finding #5: consistent threshold (>=) ----------------------------------

// At exactly BUTTON_DEBOUNCE_MS (50 ms) isPressed() is now true (was false
// before the fix, when the comparison was >).
TEST(step5_finding5_isPressed_fires_at_exactly_debounce_ms) {
  reset_hw();
  Button b(PIN);
  b.update();

  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();  // press edge, pressStart = 100

  // One tick BEFORE the threshold: not yet (50 ms - 1 = 49 ms).
  host_set_millis(100 + BUTTON_DEBOUNCE_MS - 1);
  b.update();
  CHECK(!b.isPressed());

  // Exactly AT the threshold: fires (>= semantics).
  host_set_millis(100 + BUTTON_DEBOUNCE_MS);
  b.update();
  CHECK(b.isPressed());
}

// At exactly BUTTON_LONG_PRESS_MS (5000 ms) isLongPressed() is true. It
// already used >= before step 5 — pin it so the two thresholds stay
// consistent.
TEST(step5_finding5_isLongPressed_fires_at_exactly_5000ms) {
  reset_hw();
  Button b(PIN);
  b.update();

  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();  // press edge, pressStart = 100

  // One tick before the long-press threshold: not yet.
  host_set_millis(100 + BUTTON_LONG_PRESS_MS - 1);
  b.update();
  CHECK(!b.isLongPressed());

  // Exactly at the threshold: fires (>= semantics).
  host_set_millis(100 + BUTTON_LONG_PRESS_MS);
  b.update();
  CHECK(b.isLongPressed());
  CHECK(!b.isLongPressed());  // one-shot
}

// --- isHeld(): tracks the raw debounced level -------------------------------

TEST(step5_isHeld_false_when_high) {
  reset_hw();
  Button b(PIN);
  b.update();  // pin HIGH
  CHECK(!b.isHeld());
}

TEST(step5_isHeld_true_once_stably_low) {
  reset_hw();
  Button b(PIN);
  b.update();  // pin HIGH

  // Press the button (pin LOW). isHeld() is true as soon as the debounced
  // level is LOW — it is NOT one-shot and NOT gated on the debounce window.
  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();  // sees the LOW edge, lastState = LOW
  CHECK(b.isHeld());

  // Still held: isHeld() stays true (no one-shot flag to clear it).
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 500);
  b.update();
  CHECK(b.isHeld());
}

TEST(step5_isHeld_false_again_after_release) {
  reset_hw();
  Button b(PIN);
  b.update();

  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();
  CHECK(b.isHeld());

  // Release: the debounced level is HIGH again, isHeld() is false.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 500);
  host_set_pin(PIN, HIGH);
  b.update();
  CHECK(!b.isHeld());
}

// isHeld() is independent of the one-shot flags: it stays true even after
// isPressed() has fired and been consumed (the RECORDING branch relies on
// this to keep recording while the button is held, then stop on release).
TEST(step5_isHeld_independent_of_oneshot_flags) {
  reset_hw();
  Button b(PIN);
  b.update();

  host_set_millis(100);
  host_set_pin(PIN, LOW);
  b.update();
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1);
  b.update();
  CHECK(b.isPressed());   // consume the short press
  CHECK(!b.isPressed());  // one-shot now cleared

  // Even though isPressed() is exhausted, the button is still held.
  CHECK(b.isHeld());

  // Release: isHeld() drops.
  host_set_millis(100 + BUTTON_DEBOUNCE_MS + 1000);
  host_set_pin(PIN, HIGH);
  b.update();
  CHECK(!b.isHeld());
}
