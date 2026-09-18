// Recording LED (issue #47, step 4 of 11 of the refactoring plan in #42
// - 1:1 wrap of the raw digitalWrite(LED_PIN, ...) calls, behavior
// unchanged).
#include "led.h"
#include <Arduino.h>  // digitalWrite, HIGH, LOW

Led::Led(int pin) : pin_(pin), on_(false) {}

// LED on: digitalWrite(pin_, HIGH) + track the logical state.
void Led::on() {
  digitalWrite(pin_, HIGH);
  on_ = true;
}

// LED off: digitalWrite(pin_, LOW) + track the logical state.
void Led::off() {
  digitalWrite(pin_, LOW);
  on_ = false;
}

bool Led::isOn() const { return on_; }
