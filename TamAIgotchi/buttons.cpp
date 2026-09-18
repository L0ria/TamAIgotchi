// Debounced button with long-press support (extracted from TamAIgotchi.ino
// as step 2 of the refactoring proposed in issue #18).
#include "buttons.h"
#include <Arduino.h>  // digitalRead, HIGH, LOW, millis()

Button::Button(int pin)
  : pin(pin),
    lastState(HIGH),
    pressStart(0),
    longFired(false),
    acted(false) {}

// Debounce edge-detect: once the button is stably HIGH again, clear the
// one-shot flags so the next press can act (and the long-press can fire
// again). Call once per loop() pass for every button, before reading
// isPressed() / isLongPressed().
void Button::update() {
  int reading = digitalRead(pin);
  if (reading != lastState) {
    lastState = reading;
    pressStart = millis();
  }
  if (lastState == HIGH) {
    acted = false;
    longFired = false;
  }
}

// Debounced press: true once the button has been stably LOW for at least
// BUTTON_DEBOUNCE_MS (a press at exactly the threshold fires - the >= is
// consistent with isLongPressed(), step 5 / #48 finding #5), and only for
// the first loop() pass of that press (one action per press).
bool Button::isPressed() {
  if (acted) return false;
  if ((lastState == LOW) && (millis() - pressStart) >= BUTTON_DEBOUNCE_MS) {
    acted = true;
    return true;
  }
  return false;
}

// Long-press: true while held past BUTTON_LONG_PRESS_MS and only for the
// first loop() pass past the threshold (one action per press).
// Note: deliberately NOT guarded by `acted` - like the old
// buttonLongPress(), a single hold may fire the short-press action first
// (e.g. one scroll step) AND the long-press action 5 s later (e.g. exit
// the response view / reset the WiFi settings).
bool Button::isLongPressed() {
  if (longFired) return false;
  if ((lastState == LOW) &&
      (millis() - pressStart) >= BUTTON_LONG_PRESS_MS) {
    longFired = true;
    return true;
  }
  return false;
}

// Is the button currently held? True while the (debounced) level is LOW.
// Unlike isPressed() / isLongPressed() this is NOT one-shot and is NOT
// guarded by the acted/longFired flags - it tracks the raw debounced level
// so the RECORDING branch (TamAIgotchi.ino) can stop on release without a
// second raw digitalRead(BUTTON_PIN) path (step 5, #48, finding #2).
bool Button::isHeld() const { return lastState == LOW; }

// Re-arm the debounce: forget the current press so the next one acts.
// (Mirrors the old `mainBtn.lastState = HIGH; mainBtn.acted = false;`
// re-arm done after the SENDING state.)
void Button::reset() {
  lastState = HIGH;
  pressStart = millis();
  acted = false;
  longFired = false;
}
