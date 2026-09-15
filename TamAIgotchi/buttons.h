// Debounced button with long-press support (extracted from TamAIgotchi.ino
// as step 2 of the refactoring proposed in issue #18).
//
// Replaces the old `BtnState` struct + the `buttonPressed()` /
// `buttonLongPress()` helpers + the hand-rolled GPIO9 pre-handling in
// loop(). One instance per physical button; the sketch owns the instances
// and calls update() once per loop() pass for every button before reading
// isPressed() / isLongPressed().
//
//   Button(int pin)     - constructor (pin must already be INPUT_PULLUP)
//   update()            - debounce edge-detect; call once per loop() pass
//   isPressed()         - true for exactly one loop() pass per debounced press
//   isLongPressed()     - true for exactly one loop() pass per >=5 s hold
//   reset()             - re-arm the debounce (forget the current press)
#pragma once
#include "config.h"  // BUTTON_DEBOUNCE_MS, BUTTON_LONG_PRESS_MS

class Button {
 public:
  explicit Button(int pin);

  void update();      // debounce edge-detect (call once per loop() for every button)
  bool isPressed();   // true for exactly one loop() pass per press (debounced)
  bool isLongPressed();  // true for exactly one loop() pass per >= BUTTON_LONG_PRESS_MS hold
  void reset();       // re-arm: forget the current press (used after SENDING)

  int pin;            // the GPIO this button is wired to

 private:
  int lastState;            // last (debounced) level seen
  unsigned long pressStart; // millis() when the current level was first seen
  bool longFired;           // the long-press action already ran for this press
  bool acted;               // the short-press action already ran for this press
};
