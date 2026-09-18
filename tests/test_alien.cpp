// Tests for the AlienAnimation class (issue #50, step 7 of 11 of the
// refactoring plan in #42).
//
// Pins the behavior after canAnimate() / update() take their inputs as
// parameters (no more extern Recorder / RecState / ESPWifiConfig):
//
//   - canAnimate(bufferOk, wifiOk) is a pure predicate: true only when both
//     are true (the alien no longer reads the recorder / WiFi library)
//   - update(inResponse, bufferOk, wifiOk) starts the animation after
//     ALIEN_IDLE_TIMEOUT_MS of inactivity (inResponse=false) or
//     ALIEN_RESPONSE_TIMEOUT_MS (inResponse=true), gated by canAnimate()
//     (bufferOk && wifiOk)
//   - markActivity() resets the inactivity timer + stops a running animation
//   - the 70 s phase progression bubble -> wave -> bubble -> wave -> stand
//     -> loop back to bubble (asserted via the phase() test accessor)
//
// The shared `alien` object is defined in tests/test_main.cpp (the host
// build's equivalent of the instance in TamAIgotchi.ino). Time is driven
// with the shim clock (host_set_millis()). In the host build renderScreen()
// is a no-op, so the tests assert on state() / phase(), not on the frame.
#include "test_main.h"
#include <Arduino.h>  // host shim: host_set_millis
#include "config.h"   // ALIEN_* timings
#include "alien.h"    // AlienAnimation

// The shared alien object (defined in tests/test_main.cpp, the host build's
// equivalent of the instance in TamAIgotchi.ino) - the same pattern as the
// shared `led` / `statusBar` / `recorder` objects in the other tests.
extern AlienAnimation alien;

// Reset the shared test state: the clock at 0, the animation stopped (IDLE,
// phase 0), the inactivity timer at 0.
static void reset_alien() {
  host_set_millis(0);
  alien.markActivity();  // -> IDLE, phase 0, alienActivityMs = 0
}

// --- canAnimate() (now a pure function) ------------------------------------

TEST(canAnimate_true_true_is_true) {
  CHECK(alien.canAnimate(true, true));
}
TEST(canAnimate_false_true_is_false) {
  CHECK(!alien.canAnimate(false, true));
}
TEST(canAnimate_true_false_is_false) {
  CHECK(!alien.canAnimate(true, false));
}
TEST(canAnimate_false_false_is_false) {
  CHECK(!alien.canAnimate(false, false));
}

// --- update() start gating --------------------------------------------------

TEST(update_false_starts_after_idle_timeout) {
  reset_alien();
  // Just before ALIEN_IDLE_TIMEOUT_MS of inactivity: not started.
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS - 1);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_IDLE);
  // At ALIEN_IDLE_TIMEOUT_MS: started (bubble phase).
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_ACTIVE);
  CHECK(alien.phase() == 0);
}

TEST(update_true_uses_response_timeout) {
  reset_alien();
  // Just before ALIEN_RESPONSE_TIMEOUT_MS of the response being shown:
  // not started.
  host_set_millis(ALIEN_RESPONSE_TIMEOUT_MS - 1);
  alien.update(true, true, true);
  CHECK(alien.state() == ANIM_IDLE);
  // At ALIEN_RESPONSE_TIMEOUT_MS: started.
  host_set_millis(ALIEN_RESPONSE_TIMEOUT_MS);
  alien.update(true, true, true);
  CHECK(alien.state() == ANIM_ACTIVE);
}

TEST(update_does_not_start_when_buffer_not_allocated) {
  reset_alien();
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, false, true);  // bufferOk = false
  CHECK(alien.state() == ANIM_IDLE);
}

TEST(update_does_not_start_when_wifi_down) {
  reset_alien();
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, false);  // wifiOk = false
  CHECK(alien.state() == ANIM_IDLE);
}

// --- markActivity() ---------------------------------------------------------

TEST(markActivity_resets_inactivity_timer) {
  reset_alien();
  // Advance most of the way to the idle timeout.
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS - 1000);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_IDLE);  // only 59 s elapsed - not started
  // A button press resets the timer (alienActivityMs = now).
  alien.markActivity();
  // Past the ORIGINAL timeout point, but only 1 s since the press: not
  // started.
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_IDLE);
  // It starts ALIEN_IDLE_TIMEOUT_MS after the LAST press.
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS - 1000 + ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_ACTIVE);
}

TEST(markActivity_stops_running_animation) {
  reset_alien();
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_ACTIVE);
  // A button press stops the animation + restarts the inactivity timer.
  alien.markActivity();
  CHECK(alien.state() == ANIM_IDLE);
  CHECK(alien.phase() == 0);
}

// --- 70 s phase progression -------------------------------------------------

TEST(phase_progression_bubble_wave_stand_loop) {
  reset_alien();
  // Start the animation (alienStateStart = T0).
  host_set_millis(ALIEN_IDLE_TIMEOUT_MS);
  alien.update(false, true, true);
  CHECK(alien.state() == ANIM_ACTIVE);
  CHECK(alien.phase() == 0);  // bubble
  const unsigned long T0 = ALIEN_IDLE_TIMEOUT_MS;

  // Phase 0 (bubble, ALIEN_BUBBLE_MS) -> phase 1 (wave).
  host_set_millis(T0 + ALIEN_BUBBLE_MS);
  alien.update(false, true, true);
  CHECK(alien.phase() == 1);

  // Phase 1 (wave, ALIEN_WAVE_MS) -> phase 2 (bubble).
  host_set_millis(T0 + ALIEN_BUBBLE_MS + ALIEN_WAVE_MS);
  alien.update(false, true, true);
  CHECK(alien.phase() == 2);

  // Phase 2 (bubble, ALIEN_BUBBLE_MS) -> phase 3 (wave).
  host_set_millis(T0 + ALIEN_BUBBLE_MS + ALIEN_WAVE_MS + ALIEN_BUBBLE_MS);
  alien.update(false, true, true);
  CHECK(alien.phase() == 3);

  // Phase 3 (wave, ALIEN_WAVE_MS) -> phase 4 (stand).
  host_set_millis(T0 + ALIEN_BUBBLE_MS + ALIEN_WAVE_MS + ALIEN_BUBBLE_MS
                  + ALIEN_WAVE_MS);
  alien.update(false, true, true);
  CHECK(alien.phase() == 4);

  // Phase 4 (stand, ALIEN_STAND_MS) -> loop back to phase 0 (bubble).
  const unsigned long T_END = T0 + ALIEN_BUBBLE_MS + ALIEN_WAVE_MS
                             + ALIEN_BUBBLE_MS + ALIEN_WAVE_MS + ALIEN_STAND_MS;
  CHECK_EQ_INT((long)(T_END - T0), 70000L);  // the 70 s cycle
  host_set_millis(T_END);
  alien.update(false, true, true);
  CHECK(alien.phase() == 0);  // the loop restarted
}
