// Host-side shim of the ESP32 WiFi library for the unit tests (issue #49,
// step 6 of 11 of the refactoring plan in #42).
//
// display.cpp references WiFi.SSID() in showWifiStatus(). On the host build
// the WiFi behavior is NOT emulated - the tests exercise the Recorder
// streaming API, not the WiFi state.
#pragma once

#include <Arduino.h>  // String

class WiFiClass {
 public:
  String SSID() { return String(); }
};

extern WiFiClass WiFi;
