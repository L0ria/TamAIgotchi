// Host-side shim of the ESP-Wifi-Config library for the unit tests
// (issue #49, step 6 of 11 of the refactoring plan in #42).
//
// recorder.cpp does not use wifiConfig directly, but it includes
// <ESPWifiConfig.h> (via recorder.cpp's own include list) for the
// OPENAI_AUDIO_INPUT_FORMAT_WAV constant - wait, that is OpenAI.h.
//
// What this shim actually provides: the ESPWifiConfig class + the
// STA_MODE / AP_MODE enum values that display.cpp references. On the
// host build the WiFi behavior is NOT emulated - the tests exercise the
// Recorder streaming API, not the WiFi state.
#pragma once

#include <Arduino.h>  // String
#include <cstdint>

enum WiFiMode_t { STA_MODE, AP_MODE, STA_AP_MODE };

class ESPWifiConfig {
 public:
  ESPWifiConfig(const char* ap, int port, int led, bool autoReboot,
                const char* ssid, const char* pass, bool enableWeb)
      : ESP_mode(STA_MODE), wifi_connected(false) {
    (void)ap; (void)port; (void)led; (void)autoReboot; (void)ssid; (void)pass; (void)enableWeb;
  }
  WiFiMode_t ESP_mode;
  bool wifi_connected;
  String ESP_IP;
  String get_AP_name() { return String(); }
  void resetAllSettings() {}
};
