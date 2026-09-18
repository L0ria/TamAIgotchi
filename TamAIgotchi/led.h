// Recording LED (wrapped in the Led class in step 4 of 11 of the
// refactoring plan in #42, issue #47).
//
// Owns the recording LED (LED_PIN, config.h): the raw digitalWrite()
// calls that used to live in display.cpp::startRecording() (on) and the
// RECORDING branch of TamAIgotchi.ino (off) now go through this class,
// and the logical state (on/off) is visible in the class model.
//
//   led.on()     - LED on (digitalWrite(pin_, HIGH))
//   led.off()    - LED off (digitalWrite(pin_, LOW))
//   led.isOn()   - the logical state (tracks the last on()/off() call;
//                  tests can assert it without touching hardware)
//
// on() / off() are idempotent in the state sense: calling the same state
// twice leaves the logical state unchanged (on() always writes HIGH,
// off() always writes LOW - 1:1 with the raw digitalWrite() calls).
//
// The shared `led` object is referenced via extern, the same pattern as
// the other modules (bubble / statusBar / display).
#pragma once

class Led {
 public:
  explicit Led(int pin);

  void on();    // LED on (idempotent)
  void off();   // LED off (idempotent)
  bool isOn() const;  // the logical state (false after construction)

 private:
  int pin_;
  bool on_ = false;
};

// The shared recording-LED object (the codebase's existing shared-object
// pattern; the instance is defined in TamAIgotchi.ino next to the other
// shared objects, the same pattern as the shared `bubble` / `statusBar`
// objects).
extern Led led;
