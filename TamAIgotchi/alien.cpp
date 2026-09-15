// Idle-alien animation (issue #16) — the state machine + rendering (extracted
// from TamAIgotchi.ino as step 4 of the refactoring proposed in issue #18).
#include "alien.h"
#include "config.h"   // ALIEN_* timings + ALIEN_BUBBLE_TEXT
#include "recorder.h" // Recorder (rec_buf + recState)
#include <Arduino.h>  // millis(), Serial, F()
#include <string.h>   // strlen (speech-bubble width from ALIEN_BUBBLE_TEXT)
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include <ESPWifiConfig.h>     // for the shared `wifiConfig` object (canAnimate)

// Shared objects + state declared in TamAIgotchi.ino (the sketch entry
// point); referenced here instead of passed through every call.
extern Adafruit_SSD1306 display;
extern ESPWifiConfig wifiConfig;
extern Recorder recorder;      // recorder.rec_buf (canAnimate)
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
  return (recorder.rec_buf != NULL) &&
         (wifiConfig.ESP_mode != AP_MODE) &&
         wifiConfig.wifi_connected;
}

// Draw one alien sprite at (x, y) using the Adafruit_GFX 1-bit format
// (MSB-first row-major, as in alien.h).
void AlienAnimation::renderAlien(int frame, int x, int y) {
  display.drawBitmap(x, y, alienFrameData[frame], ALIEN_SPRITE_W, ALIEN_SPRITE_H, WHITE);
}

// Render the current animation scene. The alien (24x22 px) is anchored at
// (4, 42): it spans x 4..27 (< 64) and y 42..63 (>= 32), so it stays in the
// lower-left quadrant. The speech bubble sits above the alien's head at
// (2, 24) with its width derived from the configured text (clamped so it
// never crosses the vertical middle at x = 64); y 24..39 (above the middle).
void AlienAnimation::renderAlienScene() {
  display.clearDisplay();
  if (alienPhase == 0 || alienPhase == 2) {
    // Speech-bubble phase: standing still + bubble with the configured text.
    // Font 1 advances 6 px per char (5 px glyph + 1 px gap); add 2 px
    // padding on each side, clamp to 60 px wide so the bubble stays left
    // of the vertical middle even with a longer ALIEN_BUBBLE_TEXT.
    int bubbleW = strlen(ALIEN_BUBBLE_TEXT) * 6 + 4;
    if (bubbleW > 60) bubbleW = 60;
    display.drawRect(2, 24, bubbleW, 16, WHITE);
    display.setCursor(4, 28);
    display.print(ALIEN_BUBBLE_TEXT);
    renderAlien(ALIEN_FRAME_STAND, 4, 42);
  } else if (alienPhase == 1 || alienPhase == 3) {
    // Jump & wave phase: alternating jump / wave frames (swap in alienUpdate()).
    renderAlien(alienFrame, 4, 42);
  } else {
    // Standing-still phase: just the alien, no animation.
    renderAlien(ALIEN_FRAME_STAND, 4, 42);
  }
  display.display();
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
