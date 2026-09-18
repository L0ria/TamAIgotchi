// OLED display helpers (extracted from TamAIgotchi.ino as step 5 of the
// refactoring proposed in issue #18).
#include "display.h"
#include <Arduino.h>  // Serial, String, F(), pinMode, digitalWrite, delay, ESP.restart()
#include <Adafruit_SSD1306.h>  // for the shared `display` object
#include <ESPWifiConfig.h>     // for the shared `wifiConfig` object
#include "alien.h"      // AlienAnimation (markActivity re-arms the idle timer)
#include "recorder.h"   // Recorder (startRecording() uses the streaming API, issue #49)
#include "statusbar.h"  // statusBar (issue #46, step 3)
#include "messages.h"   // MSG_* user-facing display strings (issue #36, step 6)
#include "bubble.h"      // bubble.clear() / bubble.render() (issue #34, step 4)
#include "led.h"       // led.on() (issue #47, step 4)
#include "display.h"     // renderScreen() (issue #35, step 5)

// Shared objects + state (defined in hardware.h / owned by the sketch);
// referenced here instead of passed through every call - the same pattern
// as text_utils.cpp / alien.cpp / recorder.cpp.
extern Adafruit_SSD1306 display;
extern ESPWifiConfig wifiConfig;
extern Recorder recorder;
extern RecState recState;
extern Led led;        // recording LED (issue #47, step 4)
extern AlienAnimation alien;

// THE single render pass (issue #35, step 5 of 6 of the UI restructure in
// #29): one clearDisplay() + one display() per frame.
//   status bar (top 16 px) -> alien sprite (always present) -> bubble.
// The status bar draws its two stored lines itself (statusBar.draw()
// keeps its "store + draw region" job and then calls renderScreen());
// the alien and the bubble keep their own current-content state, so
// renderScreen() just composes them.
void renderScreen() {
  display.clearDisplay();
  statusBar.draw();     // redraw the status bar's two lines (no panel push)
  alien.drawSprite();    // stand frame / current animation frame (always)
  // Bubble: while the idle animation runs, the animation owns the bubble
  // content (bubble.renderText() - the line table is untouched, so the
  // response survives the animation, issue #35 step 5 follow-up, Q4);
  // otherwise the stored table window is drawn as usual.
  if (alien.state() == ANIM_ACTIVE) {
    alien.renderBubble();
  } else {
    bubble.render();
  }
  display.display();
}

// Show the current WiFi situation on the display (and Serial).
//  - AP mode:    show the access point name + IP so it can be configured
//  - connected:  show the IP address assigned by the router
//  - otherwise:  show that it is still trying to connect
void showWifiStatus() {
  // Status bar (issue #32, step 2 of the UI restructure in #29): the top
  // two lines carry the WiFi state; the full details still go to Serial.
  if (wifiConfig.ESP_mode == AP_MODE) {
    // AP mode (issue #29 Q2): line 1 = AP name, line 2 = setup address.
    // No "No WiFi - join AP" line. The full setup URL goes to Serial.
    statusBar.show(wifiConfig.get_AP_name(),
                 String(MSG_AP_IP_PREFIX) + WIFI_SETUP_PORT);
    Serial.print(F("AP name: "));
    Serial.println(wifiConfig.get_AP_name());
    Serial.print(F("Setup URL: http://"));
    Serial.print(wifiConfig.ESP_IP.toString());
    Serial.print(F(":"));
    Serial.println(WIFI_SETUP_PORT);
  } else if (wifiConfig.wifi_connected) {
    // STA (issue #29 Q3): line 1 = "WiFi: <SSID>" (SSID truncated to 16
    // chars, no ellipsis - 21 - 5), line 2 = "IP: x.x.x.x".
    statusBar.show(String(MSG_WIFI_PREFIX) + WiFi.SSID().substring(0, 16),
                 String(MSG_IP_PREFIX) + wifiConfig.ESP_IP.toString());
    Serial.print(F("Connected to "));
    Serial.print(WiFi.SSID());
    Serial.print(F(" IP: "));
    Serial.println(wifiConfig.ESP_IP.toString());
  } else {
    statusBar.show(MSG_WIFI_CONNECTING);
  }
  alien.markActivity(); // issue #16: showWifiStatus() is always the result of
                       // a button press (or boot) - re-arm the idle timer
}

// Escape hatch: wipe the stored WiFi (and web) credentials and reboot.
// With no saved SSID the library drops into AP mode, so the device
// comes back up serving the setup page again. Used by the 5 s
// long-press of the WiFi-config button when a wrong password was saved
// and the device would otherwise keep retrying forever.
// resetAllSettings() covers all registered settings (built-in + the LocalAI
// URL/key user slots), so the config.h defaults come back after the reboot.
void resetWifiSettingsAndRestart() {
  wifiConfig.resetAllSettings(); // public library helper (v2.3.0), all settings
  Serial.println(F("WiFi settings reset. Rebooting into setup AP mode..."));
  statusBar.show(MSG_WIFI_RESET, MSG_WIFI_REBOOT);
  delay(300);
  ESP.restart();
}

// Start a new recording take (dedupes the ~15-line block that used to be
// copied verbatim in the IDLE and RESPONSE branches of loop()).
bool startRecording() {
  D_TDLN(F("button pressed (hold to record)"));
  if (!recorder.bufferAllocated()) {
    // Recording buffer allocation failed at boot: keep the error
    // visible, do not start a take.
    statusBar.error(MSG_REC_BUF_FAIL, MSG_REBOOT_DEVICE);
    return false;
  }
  if (wifiConfig.ESP_mode != AP_MODE && wifiConfig.wifi_connected) {
    // Connected to a known network: start recording into the buffer.
    // The bubble may still hold the previous response (startRecording() is
    // reached from the RESPONSE state via the main button, issue #34):
    // empty it and re-draw the (empty) bubble so no stale text lingers
    // under the recording status.
    bubble.clear();
    showWifiStatus(); // status lines + renderScreen() (issue #35, step 5)
    recorder.beginStreaming();
    recState = RECORDING;
    led.on();  // recording LED (issue #47, step 4)
    // Status bar (issue #33, step 3 of the UI restructure in #29): line 1
    // = "Recording (max 10 s)" (20 chars, fits the 21-char limit); line 2
    // counts up the elapsed seconds from the RECORDING branch of loop()
    // (throttled to once per whole second).
    statusBar.show(MSG_RECORDING);
    D_TDLN(F("recording start (hold button, max 10 s)"));
    return true;
  }
  // Not connected: keep showing the access point / connection status.
  showWifiStatus();
  return false;
}
