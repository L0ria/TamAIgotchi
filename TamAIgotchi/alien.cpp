// Idle-alien animation (issue #16) — the state machine + rendering (extracted
// from TamAIgotchi.ino as step 4 of the refactoring proposed in issue #18).
#include "alien.h"
#include "config.h"   // ALIEN_* timings
#include "messages.h" // MSG_ALIEN_BUBBLE (idle bubble text, issue #36, step 6)
#include "recorder.h" // Recorder (rec_buf + recState)
#include <Arduino.h>  // millis(), Serial, F()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include <ESPWifiConfig.h>     // for the shared `wifiConfig` object (canAnimate)
#include "bubble.h"     // bubble.renderText() (issue #35, step 5 follow-up)
#include "display.h"    // renderScreen() (issue #35, step 5: the single pass)

// Shared objects + state declared in TamAIgotchi.ino (the sketch entry
// point); referenced here instead of passed through every call.
extern Adafruit_SSD1306 display;
extern ESPWifiConfig wifiConfig;
extern Recorder recorder;      // recorder.bufferAllocated() (canAnimate, issue #49)
extern RecState recState;     // app state (response timeout in update)

// Any button press is activity: it stops the animation and restarts the
// inactivity timers (both the idle start and the response auto-return).
void AlienAnimation::markActivity() {
  alienActivityMs = millis();
  if (alienState == ANIM_ACTIVE) {
    alienState = ANIM_IDLE;
    alienPhase = 0;
  }
}

// The animation only runs while the device is fully usable: STA mode,
// connected, and the recording buffer allocated (see issue #16 answers).
bool AlienAnimation::canAnimate() const {
  return recorder.bufferAllocated() &&
         (wifiConfig.ESP_mode != AP_MODE) &&
         wifiConfig.wifi_connected;
}

// Draw one alien sprite at (x, y) using the Adafruit_GFX 1-bit format
// (MSB-first row-major, as in alien.h).
void AlienAnimation::renderAlien(int frame, int x, int y) {
  display.drawBitmap(x, y, alienFrameData[frame], ALIEN_SPRITE_W, ALIEN_SPRITE_H, WHITE);
}

// Draw the alien sprite at its anchor (4, 42) into the current frame
// (issue #35, step 5 of 6 of the UI restructure in #29): the alien (24x22
// px) spans x 4..27 and y 42..63, so it stays in the lower-left quadrant,
// to the left of the bubble (x 30..126, y 18..62). The alien is present in
// EVERY app state: the stand frame when the animation is not running, the
// current animation frame while it is (drawSprite() is called by
// renderScreen() on every frame).
void AlienAnimation::drawSprite() {
  int frame = (alienState == ANIM_ACTIVE) ? alienFrame : ALIEN_FRAME_STAND;
  renderAlien(frame, 4, 42);
}

// Draw the animation's bubble content into the current frame WITHOUT
// touching the bubble's line table (issue #35 step 5 follow-up, Q4: the
// stored response text must survive the animation): the bubble phases
// (0 / 2) show MSG_ALIEN_BUBBLE (issue #29 Q8: the bubble is always the
// same size - no small bubble, no popping rectangle), the wave phases
// (1 / 3) and the stand phase (4) show an empty bubble. Called by
// renderScreen() (display.cpp) while the animation is running.
void AlienAnimation::renderBubble() {
  if (alienPhase == 0 || alienPhase == 2) {
    bubble.renderText(MSG_ALIEN_BUBBLE);
  } else {
    bubble.renderText(NULL);
  }
}

// Render the current animation scene (issue #35, step 5 of 6 of the UI
// restructure in #29): the animation no longer draws its own frame and no
// longer touches the bubble's line table - it only pushes one full frame
// through the single render pass renderScreen() (the sprite frame is drawn
// by drawSprite(), the bubble content by renderBubble(), both from
// renderScreen()).
void AlienAnimation::renderAlienScene() {
  renderScreen();
}

void AlienAnimation::start() {
  alienState = ANIM_ACTIVE;
  alienPhase = 0;
  alienStateStart = millis();
  alienFrame = ALIEN_FRAME_STAND;
  renderAlienScene();
  D_TDLN(F("alien animation: started (idle timeout without button press)"));
}

// Advance the idle animation each loop() pass (issue #16). Starts it after
// ALIEN_IDLE_TIMEOUT_MS of inactivity (IDLE state) or ALIEN_RESPONSE_TIMEOUT_MS
// of the response being shown (RESPONSE state); any button press (markActivity)
// stops it. Then advances the 70 s loop (bubble / wave / stand phases),
// swapping the sprite frame every ALIEN_FRAME_MS during the wave phases.
void AlienAnimation::update() {
  if (canAnimate() && alienState == ANIM_IDLE) {
    unsigned long now = millis();
    unsigned long timeout = (recState == RESPONSE)
                          ? ALIEN_RESPONSE_TIMEOUT_MS
                          : ALIEN_IDLE_TIMEOUT_MS;
    if ((now - alienActivityMs) >= timeout) {
      start();
      return;
    }
  }
  if (alienState != ANIM_ACTIVE) return;
  unsigned long now = millis();
  unsigned long t = now - alienStateStart;

  if (alienPhase == 0 || alienPhase == 2) {
    if (t >= ALIEN_BUBBLE_MS) { alienPhase++; alienStateStart = now; renderAlienScene(); }
    return;
  }
  if (alienPhase == 1 || alienPhase == 3) {
    // Wave phase: cycle JUMP (body up) -> WAVEUP (arm up) -> WAVEDN (arm
    // down) every ALIEN_FRAME_MS for a continuous jump-and-wave motion.
    if (t >= ALIEN_WAVE_MS) {
      alienPhase++;
      alienStateStart = now;
      alienFrame = ALIEN_FRAME_STAND;
      renderAlienScene();
      return;
    }
    if (now - alienFrameMs >= ALIEN_FRAME_MS) {
      alienFrameMs = now;
      alienFrame = (alienFrame == ALIEN_FRAME_JUMP)   ? ALIEN_FRAME_WAVE_UP
                 : (alienFrame == ALIEN_FRAME_WAVE_UP) ? ALIEN_FRAME_WAVE_DOWN
                 : ALIEN_FRAME_JUMP;
      renderAlienScene();
    }
    return;
  }
  // Phase 4: standing still for ALIEN_STAND_MS, then the loop restarts.
  if (t >= ALIEN_STAND_MS) {
    alienPhase = 0;
    alienStateStart = now;
    renderAlienScene();
  }
}
