// Shared hardware objects + hardware bring-up, owned by the Hardware class
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// This file is the ONLY definition of the six shared library objects
// (display / i2s / wifiConfig / openai / chat / audio): they are the
// private members `panel_` / `i2s_` / `wifi_` / `openai_` / `chat_` /
// `audio_` of the `hw` instance (hardware.h). The old `inline
// hardwareInit()` is `Hardware::init()` - the body is unchanged
// (pinMode -> OLED begin -> I2S begin, the bring-up order of the
// standing rules).
#include "hardware.h"  // the class + the library headers (the six member
                       // types; the library headers come with the header -
                       // do NOT re-include them here: ESPWifiConfig.h has
                       // no include guard, a second include in the same TU
                       // is a redefinition error)
#include "messages.h"        // MSG_I2S_* (I2S bring-up status strings, issue #36, step 6)
#include "statusbar.h"       // statusBar (hardwareInit() progress + I2S error, issue #46)

// ---------------------------------------------------------------------------
// Hardware constructor (issue #52, step 9): the six shared objects, in
// declaration order (the C++ rule: members initialize in declaration order
// regardless of the initializer list):
//
//   panel_   - the SSD1306 OLED (I2C, address set in init(), the old
//              `display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1)`)
//   i2s_     - the I2S mic bus (the old `i2s` global)
//   wifi_    - the ESP-Wifi-Config object (the old `wifiConfig` global;
//              WiFi credentials are stored by the library in flash/EEPROM,
//              no hardcoded SSID)
//   openai_  - the LocalAI-ESP32 OpenAI client, built with EMPTY url/key:
//              the real endpoint + key are user-defined settings of the
//              ESP-Wifi-Config library (v2.3.0+), registered in setup()
//              before initialize() and read back via getSetting(); the
//              client is (re)built from those values by setOpenAI() after
//              wifi_.initialize() (the library reboots after a save on the
//              setup page, so the values read there are always up to date)
//   chat_    - the chat-completion client (references openai_)
//   audio_   - the audio-transcription client (references openai_)
// ---------------------------------------------------------------------------
Hardware::Hardware()
    : panel_(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      wifi_(WIFI_AP_NAME, WIFI_SETUP_PORT, -1, false, "", "", true),
      openai_("", ""),
      chat_(openai_),
      audio_(openai_) {}

// Hardware bring-up (was hardwareInit(), step 5 of the refactoring, issue
// #18): the button/LED pin modes, the OLED init and the I2S init. The body
// is unchanged by step 9 (issue #52) - only the object names moved into
// the class (display -> panel_, i2s -> i2s_).
bool Hardware::init() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_CONFIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SCROLL_UP_PIN, INPUT_PULLUP);
  D_TDLN(F("pin setup done (BUTTON_PIN, LED_PIN, WIFI_CONFIG_BUTTON_PIN, SCROLL_UP_PIN)"));

  /* setup display */
  if (!panel_.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  D_TDLN(F("OLED display initialized (SSD1306 @ 0x3C)"));
  panel_.setTextSize(1);
  panel_.setTextColor(WHITE);
  panel_.clearDisplay();

  /* setup i2s */
  statusBar.show(MSG_I2S_INIT);
  i2s_.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);
  if (!i2s_.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    statusBar.error(MSG_I2S_FAIL, MSG_REBOOT_DEVICE);
    return false;
  }
  statusBar.show(MSG_I2S_READY);
  D_TDLN(F("I2S bus initialized (16 kHz, 32-bit, mono, slot left)"));
  return true;
}

// The six shared objects (the accessors replace the old globals).
Adafruit_SSD1306& Hardware::panel() { return panel_; }
I2SClass& Hardware::i2s() { return i2s_; }
ESPWifiConfig& Hardware::wifi() { return wifi_; }
OpenAI& Hardware::openai() { return openai_; }
OpenAI_ChatCompletion& Hardware::chat() { return chat_; }
OpenAI_AudioTranscription& Hardware::audio() { return audio_; }

// (Re)build the OpenAI client from the stored LocalAI settings (was
// `openai = OpenAI(key, url)` in setup()). The chat / audio clients hold a
// reference to the same `openai_` object, so they pick up the new
// endpoint/key automatically - no rewiring (the old globals had the same
// property: chat(openai) / audio(openai) referenced the client in place).
void Hardware::setOpenAI(const char* url, const char* key) {
  openai_ = OpenAI(key, url);
}
