// Hand-drawn pixel-art alien frames for the idle animation (issue #16).
// 24x22 px, 1-bit, MSB-first row-major (Adafruit_GFX drawBitmap format),
// 66 bytes per frame, stored in flash (PROGMEM-equivalent on ESP32).
#pragma once
#include <stdint.h>

#define ALIEN_SPRITE_W 24
#define ALIEN_SPRITE_H 22

static const uint8_t alienFrameStand[66] = {
  0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x0F, 0x0F, 0x00,
  0x1F, 0xFF, 0x80, 0x3F, 0xFF, 0xC0, 0x7F, 0xFF, 0xE0, 0xFF, 0xFF, 0xF0,
  0xFF, 0xFF, 0xF0, 0xBF, 0xFF, 0xD0, 0xBF, 0xFF, 0xD0, 0xFF, 0xFF, 0xF0,
  0x7F, 0xFF, 0xE0, 0x3F, 0x8F, 0xC0, 0x1F, 0xFF, 0x80, 0x0F, 0xFF, 0x00,
  0x07, 0xFE, 0x00, 0x03, 0xFC, 0x00, 0x03, 0x0C, 0x00, 0x03, 0x0C, 0x00,
  0x07, 0x38, 0x00, 0x07, 0x38, 0x00,
};

static const uint8_t alienFrameWaveUp[66] = {
  0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x0F, 0x0F, 0x00,
  0x1F, 0xFF, 0x80, 0x3F, 0xFF, 0xC0, 0x7F, 0xFF, 0xE3, 0xFF, 0xFF, 0xF1,
  0xFF, 0xFF, 0xF2, 0xBF, 0xFF, 0xD4, 0xBF, 0xFF, 0xD8, 0xFF, 0xFF, 0xF0,
  0x7F, 0xFF, 0xE0, 0x3F, 0x8F, 0xC0, 0x1F, 0xFF, 0x80, 0x0F, 0xFF, 0x00,
  0x07, 0xFE, 0x00, 0x03, 0xFC, 0x00, 0x03, 0x0C, 0x00, 0x03, 0x0C, 0x00,
  0x07, 0x38, 0x00, 0x07, 0x38, 0x00,
};

static const uint8_t alienFrameJump[66] = {
  0x08, 0x01, 0x00, 0x0F, 0x0F, 0x00, 0x1F, 0xFF, 0x80, 0x3F, 0xFF, 0xC0,
  0x7F, 0xFF, 0xE0, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xF0, 0xBF, 0xFF, 0xD0,
  0xBF, 0xFF, 0xD0, 0xFF, 0xFF, 0xF2, 0x7F, 0xFF, 0xEE, 0x3F, 0x8F, 0xD0,
  0x1F, 0xFF, 0xA0, 0x0F, 0xFF, 0x40, 0x07, 0xFE, 0x80, 0x03, 0xFC, 0x00,
  0x03, 0x0C, 0x00, 0x03, 0x0C, 0x00, 0x07, 0x38, 0x00, 0x07, 0x38, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t alienFrameWaveDown[66] = {
  0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x08, 0x01, 0x00, 0x0F, 0x0F, 0x00,
  0x1F, 0xFF, 0x80, 0x3F, 0xFF, 0xC0, 0x7F, 0xFF, 0xE0, 0xFF, 0xFF, 0xF0,
  0xFF, 0xFF, 0xF0, 0xBF, 0xFF, 0xD0, 0xBF, 0xFF, 0xD0, 0xFF, 0xFF, 0xF0,
  0x7F, 0xFF, 0xE0, 0x3F, 0x8F, 0xC0, 0x1F, 0xFF, 0x80, 0x0F, 0xFF, 0x40,
  0x07, 0xFE, 0x20, 0x03, 0xFC, 0x18, 0x03, 0x0C, 0x08, 0x03, 0x0C, 0x00,
  0x07, 0x38, 0x00, 0x07, 0x38, 0x00,
};

#define ALIEN_FRAME_STAND  0
#define ALIEN_FRAME_WAVE_UP 1
#define ALIEN_FRAME_JUMP   2
#define ALIEN_FRAME_WAVE_DOWN 3

static const uint8_t* const alienFrameData[4] = {
  alienFrameStand, alienFrameWaveUp, alienFrameJump, alienFrameWaveDown
};

// ---------------------------------------------------------------------------
// Idle-alien animation (issue #16) — the state machine + rendering (extracted
// from TamAIgotchi.ino as step 4 of the refactoring proposed in issue #18).
//
// The AlienAnimation class owns the 6 animation state variables and the
// markActivity() / canAnimate() / start() / update() / drawSprite() /
// renderAlienScene() helpers. The sketch (TamAIgotchi.ino) owns the instance
// and calls:
//   alien.markActivity()  - on every button press (stops the animation +
//                           restarts the inactivity timers)
//   alien.update()        - once per loop() pass (starts on idle timeout +
//                           advances the 70 s loop)
//
// The shared `display` object + the recorder (for canAnimate(): the
// recording buffer + the app state, for the response timeout) are
// referenced via extern (see alien.cpp).
// ---------------------------------------------------------------------------

// Animation state (moved here from TamAIgotchi.ino).
enum AlienState { ANIM_IDLE, ANIM_ACTIVE };

class AlienAnimation {
 public:
  // Any button press is activity: it stops the animation and restarts the
  // inactivity timers (both the idle start and the response auto-return).
  void markActivity();

  // The animation only runs while the device is fully usable: STA mode,
  // connected, and the recording buffer allocated (see issue #16 answers).
  bool canAnimate() const;

  // Starts the animation (call when the idle timeout elapses).
  void start();

  // Advance the 70 s loop (bubble / wave / stand phases) + swap the sprite
  // frame during the wave phases. Call once per loop() pass.
  void update();

  // The current animation state (ANIM_IDLE / ANIM_ACTIVE).
  AlienState state() const { return alienState; }

  // Draw the alien sprite at its anchor (x 4..27, y 42..63) into the current
  // frame: the stand frame when the animation is not running, the current
  // animation frame while it is (issue #35, step 5 of 6 of the UI restructure
  // in #29: the alien is present in EVERY app state). Called by
  // renderScreen() (display.cpp) on every frame - no clearDisplay() /
  // display() of its own.
  void drawSprite();

  // Draw the animation's bubble content into the current frame WITHOUT
  // touching the bubble's line table (issue #35 step 5 follow-up, Q4: the
  // stored response text must survive the animation): ALIEN_BUBBLE_TEXT in
  // the bubble phases (0 / 2), an empty bubble in the wave / stand phases
  // (1 / 3 / 4). Called by renderScreen() (display.cpp) while the
  // animation is running - no clearDisplay() / display() of its own.
  void renderBubble();

 private:
  // Draw one alien sprite at (x, y) using the Adafruit_GFX 1-bit format.
  void renderAlien(int frame, int x, int y);
  // Render the current animation scene: push one full frame through
  // renderScreen() (issue #35, step 5) - the sprite frame is drawn by
  // drawSprite() and the bubble content by renderBubble(), both from
  // renderScreen(), WITHOUT touching the bubble's line table (Q4).
  void renderAlienScene();

  // The 6 animation state variables (moved here from the .ino globals).
  AlienState alienState = ANIM_IDLE;
  unsigned long alienStateStart = 0;   // millis() when the current phase started
  unsigned long alienActivityMs = 0;   // millis() of the last button press
  int alienPhase = 0;                  // 0=bubble, 1=wave, 2=bubble, 3=wave, 4=stand
  int alienFrame = 0;                  // current sprite frame index (0..3)
  unsigned long alienFrameMs = 0;      // millis() of the last frame swap
};
