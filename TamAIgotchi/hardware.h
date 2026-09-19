// Shared hardware objects + hardware bring-up, owned by the Hardware class
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// The six objects that used to be defined in this header (display, i2s,
// wifiConfig, openai, chat, audio - finding #9 in the #42 audit: library
// objects defined in a header) now live as PRIVATE members of the
// Hardware class (hardware.cpp). The header only declares the class +
// the shared `hw` instance (the codebase's shared-object pattern: the
// instance is defined in TamAIgotchi.ino, the modules reach the objects
// through the accessors below or through constructor-injected references).
//
//   hw.panel()  - the SSD1306 OLED (was the `display` global)
//   hw.i2s()    - the I2S mic bus (was the `i2s` global)
//   hw.wifi()   - the ESP-Wifi-Config object (was the `wifiConfig` global)
//   hw.openai() - the LocalAI-ESP32 OpenAI client (was the `openai` global)
//   hw.chat()   - the chat-completion client (was the `chat` global)
//   hw.audio()  - the audio-transcription client (was the `audio` global)
//   hw.init()   - the hardware bring-up (was hardwareInit(), issue #18
//                 step 5): pinMode -> OLED begin -> I2S begin, unchanged
//   hw.setOpenAI(url, key) - (re)build the OpenAI client from the stored
//                 settings (was `openai = OpenAI(...)` in setup())
//
// The module-to-module `extern` web is gone with this step: Recorder /
// AlienAnimation / StatusBar / Bubble take their dependencies by
// constructor-injected references (the same pattern as the Display class,
// issue #51, step 8).
#pragma once

#include "config.h"  // SCREEN_*, WIFI_AP_NAME, WIFI_SETUP_PORT, I2S_*, button pins, D_T*()

// The library headers (the same set the old hardware.h included - the
// class members below are complete types, so the header must provide them;
// on the host build the tests/shims/ versions stand in for the real
// libraries).
#include <Wire.h>            // Wire (the SSD1306 I2C bus)
#include <Adafruit_GFX.h>    // Adafruit_GFX base (via Adafruit_SSD1306)
#include <Adafruit_SSD1306.h>  // Adafruit_SSD1306 (panel_)
#include <WiFi.h>            // WiFi (ESPWifiConfig dependency)
#include <ESPWifiConfig.h>   // ESPWifiConfig (wifi_)
#include "ESP_I2S.h"         // I2SClass (i2s_)
#include <OpenAI.h>          // OpenAI / OpenAI_ChatCompletion / OpenAI_AudioTranscription

// The hardware: owns the six shared library objects + the bring-up
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// Construction order matters: the members are initialized in declaration
// order (hardware.cpp), so `chat_` / `audio_` are always constructed
// AFTER `openai_` and hold a valid reference to it - the same wiring the
// old globals had (chat(openai) / audio(openai)).
class Hardware {
 public:
  Hardware();

  // Hardware bring-up (was hardwareInit(), issue #18 step 5): the
  // button/LED pin modes, the OLED init and the I2S init. Returns false if
  // the I2S bus failed to initialize (the error is already on the display);
  // the OLED init does not return on failure (for(;;), as before). Call
  // once at the start of setup(), before hw.wifi().initialize().
  bool init();

  // The six shared objects (the accessors replace the old globals).
  Adafruit_SSD1306& panel();   // was `display`
  I2SClass& i2s();             // was `i2s`
  ESPWifiConfig& wifi();       // was `wifiConfig`
  OpenAI& openai();            // was `openai`
  OpenAI_ChatCompletion& chat();  // was `chat`
  OpenAI_AudioTranscription& audio();  // was `audio`

  // (Re)build the OpenAI client from the stored LocalAI settings (was
  // `openai = OpenAI(key, url)` in setup()). The chat / audio clients keep
  // their reference to the same `openai_` object, so no rewiring is needed.
  void setOpenAI(const char* url, const char* key);

 private:
  // The six shared library objects (declaration order = initialization
  // order in the hardware.cpp constructor: openai_ before chat_ / audio_).
  Adafruit_SSD1306 panel_;
  I2SClass i2s_;
  ESPWifiConfig wifi_;
  OpenAI openai_;
  OpenAI_ChatCompletion chat_;
  OpenAI_AudioTranscription audio_;
};

// The shared hardware object (the codebase's shared-object pattern; the
// instance is defined in TamAIgotchi.ino next to the other shared objects
// - constructed FIRST, since the other modules reference its members).
extern Hardware hw;
