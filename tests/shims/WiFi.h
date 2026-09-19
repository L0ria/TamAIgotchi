// Host-side shim of the ESP32 WiFi library for the unit tests (issue #49,
// step 6 of 11 of the refactoring plan in #42).
//
// display.cpp references WiFi.SSID() in showWifiStatus(). On the host build
// the WiFi behavior is NOT emulated - the tests exercise the Recorder
// streaming API, not the WiFi state.
#pragma once

#include <Arduino.h>  // String

// The connected network's SSID. On the host build the WiFi behavior is NOT
// emulated; this returns the value the tests set with setSSID() so
// Display::showWifiStatus()'s STA branch is testable.
extern String host_wifi_ssid;
inline void host_set_wifi_ssid(const char* ssid) { host_wifi_ssid = String(ssid ? ssid : ""); }

class WiFiClass {
 public:
  String SSID() { return host_wifi_ssid; }
};

extern WiFiClass WiFi;
