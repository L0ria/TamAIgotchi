// Shared hardware objects + hardware init (extracted from TamAIgotchi.ino
// as step 5 of the refactoring proposed in issue #18).
//
// Collects the objects that were global in the sketch and are shared by
// most of the modules (display, i2s, wifiConfig, openai/chat/audio) plus
// the hardware bring-up (pinMode + OLED begin + I2S begin) in
// hardwareInit().
//
// This header is included by the sketch (TamAIgotchi.ino) only: the
// objects are defined here (one definition for the whole program). The
// .cpp modules reference them via extern, the same pattern as steps 1-4
// (see text_utils.cpp / alien.cpp / recorder.cpp).
#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESPWifiConfig.h>
#include "ESP_I2S.h"
#include <OpenAI.h>
#include "config.h"      // SCREEN_*, WIFI_AP_NAME, WIFI_SETUP_PORT, I2S_*, button pins, D_T*()
#include "messages.h"  // MSG_I2S_* (I2S bring-up status strings, issue #36, step 6)
#include "statusbar.h"  // statusShow() / statusError() (hardwareInit() progress + I2S error)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
I2SClass i2s;

// WiFi credentials are stored by the ESP-Wifi-Config library (flash/EEPROM)
// instead of being hardcoded. When no known network is reachable the device
// drops into AP mode and serves a web setup page to configure the WiFi.
ESPWifiConfig wifiConfig(WIFI_AP_NAME, WIFI_SETUP_PORT, -1, false, "", "", true);

// The LocalAI endpoint + API key are user-defined settings of this same
// library (v2.3.0+): they are registered in setup() before initialize(),
// stored in the library's flash slots, editable on the setup page (Custom
// tab) and read back via getSetting(). The constants in config.h only act
// as initial defaults (first boot / after a full reset).
//
// The OpenAI client is (re)built in setup() from the stored settings after
// wifiConfig.initialize() (the library reboots the device after a save on
// the setup page, so the values read there are always up to date).
OpenAI openai("", "");
OpenAI_ChatCompletion chat(openai);
OpenAI_AudioTranscription audio(openai);

// Hardware bring-up (moved from setup(), step 5 of the refactoring, issue
// #18): the button/LED pin modes, the OLED init and the I2S init.
// Returns false if the I2S bus failed to initialize (the error is already
// on the display); the OLED init does not return on failure (for(;;), as
// before). Call once at the start of setup(), before wifiConfig.initialize().
inline bool hardwareInit() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_CONFIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SCROLL_UP_PIN, INPUT_PULLUP);
  D_TDLN(F("pin setup done (BUTTON_PIN, LED_PIN, WIFI_CONFIG_BUTTON_PIN, SCROLL_UP_PIN)"));

  /* setup display */
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  D_TDLN(F("OLED display initialized (SSD1306 @ 0x3C)"));
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();

  /* setup i2s */
  statusShow(MSG_I2S_INIT);
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    statusError(MSG_I2S_FAIL, MSG_REBOOT_DEVICE);
    return false;
  }
  statusShow(MSG_I2S_READY);
  D_TDLN(F("I2S bus initialized (16 kHz, 32-bit, mono, slot left)"));
  return true;
}
