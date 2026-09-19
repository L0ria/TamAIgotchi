// OLED display helpers (extracted from TamAIgotchi.ino as step 5 of the
// refactoring proposed in issue #18; wrapped in the Display class in step 8
// of 11 of the refactoring plan in #42, issue #51).
#include "display.h"
#include <Arduino.h>  // Serial, String, F(), pinMode, digitalWrite, delay, ESP.restart()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include <ESPWifiConfig.h>     // for the shared `wifiConfig` object
#include <WiFi.h>              // WiFi.SSID() (showWifiStatus())
#include "alien.h"      // AlienAnimation (markActivity re-arms the idle timer)
#include "recorder.h"   // Recorder (startRecording() uses the streaming API, issue #49)
#include "statusbar.h"  // statusBar (issue #46, step 3)
#include "messages.h"   // MSG_* user-facing display strings (issue #36, step 6)
#include "bubble.h"      // bubble.clear() / bubble.render() (issue #34, step 4)
#include "led.h"       // led.on() (issue #47, step 4)

// ---------------------------------------------------------------------------
// Display constructor (issue #51, step 8 of 11 of the refactoring plan in
// #42): the six dependencies are injected by reference - the free-function
// externs are gone. The objects are all sketch-lifetime (defined in
// hardware.cpp / TamAIgotchi.ino - issue #52, step 9: the panel / WiFi
// objects are members of the shared `hw` Hardware object), so the
// references are valid for the whole program.
// ---------------------------------------------------------------------------
Display::Display(Adafruit_SSD1306& panel, StatusBar& status, AlienAnimation& alien,
                 Bubble& bubble, ESPWifiConfig& wifi, Recorder& rec, Led& led)
    : panel_(panel), status_(status), alien_(alien),
      bubble_(bubble), wifi_(wifi), rec_(rec), led_(led) {}

// THE single render pass (issue #35, step 5 of 6 of the UI restructure in
// #29): one clearDisplay() + one display() per frame.
//   status bar (top 16 px) -> alien sprite (always present) -> bubble.
// The status bar draws its two stored lines itself (status_.draw()
// keeps its "store + draw region" job); the alien and the bubble keep
// their own current-content state, so render() just composes them.
// (issue #51, step 8 of 11 of the refactoring plan in #42: the free
// function renderScreen() is now Display::render().)
void Display::render() {
  panel_.clearDisplay();
  status_.draw();     // redraw the status bar's two lines (no panel push)
  alien_.drawSprite(); // stand frame / current animation frame (always)
  // Bubble: while the idle animation runs, the animation owns the bubble
  // content (bubble.renderText() - the line table is untouched, so the
  // response survives the animation, issue #35 step 5 follow-up, Q4);
  // otherwise the stored table window is drawn as usual.
  if (alien_.state() == ANIM_ACTIVE) {
    alien_.renderBubble();
  } else {
    bubble_.render();
  }
  panel_.display();
}

// Show the current WiFi situation on the display (and Serial).
//  - AP mode:    show the access point name + IP so it can be configured
//  - connected:  show the IP address assigned by the router
//  - otherwise:  show that it is still trying to connect
void Display::showWifiStatus() {
  // Status bar (issue #32, step 2 of the UI restructure in #29): the top
  // two lines carry the WiFi state; the full details still go to Serial.
  if (wifi_.ESP_mode == AP_MODE) {
    // AP mode (issue #29 Q2): line 1 = AP name, line 2 = setup address.
    // No "No WiFi - join AP" line. The full setup URL goes to Serial.
    status_.show(wifi_.get_AP_name(),
                 String(MSG_AP_IP_PREFIX) + String(WIFI_SETUP_PORT));
    Serial.print(F("AP name: "));
    Serial.println(wifi_.get_AP_name());
    Serial.print(F("Setup URL: http://"));
    Serial.print(wifi_.ESP_IP.toString());
    Serial.print(F(":"));
    Serial.println(WIFI_SETUP_PORT);
  } else if (wifi_.wifi_connected) {
    // STA (issue #29 Q3): line 1 = "WiFi: <SSID>" (SSID truncated to 16
    // chars, no ellipsis - 21 - 5), line 2 = "IP: x.x.x.x".
    status_.show(String(MSG_WIFI_PREFIX) + WiFi.SSID().substring(0, 16),
                 String(MSG_IP_PREFIX) + wifi_.ESP_IP.toString());
    Serial.print(F("Connected to "));
    Serial.print(WiFi.SSID());
    Serial.print(F(" IP: "));
    Serial.println(wifi_.ESP_IP.toString());
  } else {
    status_.show(MSG_WIFI_CONNECTING);
  }
  alien_.markActivity(); // issue #16: showWifiStatus() is always the result of
                        // a button press (or boot) - re-arm the idle timer
}

// Escape hatch: wipe the stored WiFi (and web) credentials and reboot.
// With no saved SSID the library drops into AP mode, so the device
// comes back up serving the setup page again. Used by the 5 s
// long-press of the WiFi-config button when a wrong password was saved
// and the device would otherwise keep retrying forever.
// resetAllSettings() covers all registered settings (built-in + the LocalAI
// URL/key user slots), so the config.h defaults come back after the reboot.
// Does not return (ESP.restart()).
void Display::resetWifiSettingsAndRestart() {
  wifi_.resetAllSettings(); // public library helper (v2.3.0), all settings
  Serial.println(F("WiFi settings reset. Rebooting into setup AP mode..."));
  status_.show(MSG_WIFI_RESET, MSG_WIFI_REBOOT);
  delay(300);
  ESP.restart();
}

// Start a new recording take (dedupes the ~15-line block that used to be
// copied verbatim in the IDLE and RESPONSE branches of loop()).
bool Display::startRecording() {
  D_TDLN(F("button pressed (hold to record)"));
  if (!rec_.bufferAllocated()) {
    // Recording buffer allocation failed at boot: keep the error
    // visible, do not start a take.
    status_.error(MSG_REC_BUF_FAIL, MSG_REBOOT_DEVICE);
    return false;
  }
  if (wifi_.ESP_mode != AP_MODE && wifi_.wifi_connected) {
    // Connected to a known network: start recording into the buffer.
    // The bubble may still hold the previous response (startRecording() is
    // reached from the RESPONSE state via the main button, issue #34):
    // empty it and re-draw the (empty) bubble so no stale text lingers
    // under the recording status.
    bubble_.clear();
    showWifiStatus(); // status lines + render() (issue #35, step 5)
    rec_.beginStreaming();
    led_.on();  // recording LED (issue #47, step 4)
    // Status bar (issue #33, step 3 of the UI restructure in #29): line 1
    // = "Recording (max 10 s)" (20 chars, fits the 21-char limit); line 2
    // counts up the elapsed seconds from the RECORDING branch of loop()
    // (throttled to once per whole second).
    status_.show(MSG_RECORDING);
    D_TDLN(F("recording start (hold button, max 10 s)"));
    return true;
  }
  // Not connected: keep showing the access point / connection status.
  showWifiStatus();
  return false;
}
