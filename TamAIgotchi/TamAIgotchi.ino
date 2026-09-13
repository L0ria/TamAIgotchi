#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESPWifiConfig.h>
#include "ESP_I2S.h"
#include "esp_heap_caps.h"  // heap_caps_malloc / heap_caps_get_free_size (PSRAM recording buffer)
#include <OpenAI.h>
#include "config.h"

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

uint32_t lastButtonState = HIGH;
uint32_t lastDebounce = 0;
// Hold-to-record state machine (issue #9):
//   IDLE      - waiting for a debounced button press
//   RECORDING - button held: streaming I2S audio into the preallocated
//               PSRAM buffer; LED on; stops on release / buffer full / cap
//   SENDING   - patching the WAV header + uploading to LocalAI for
//               transcription, then the LLM call (blocking)
enum RecState { IDLE, RECORDING, SENDING };
RecState recState = IDLE;

// Preallocated recording buffer (44-byte WAV header + PCM audio) in PSRAM.
// Allocated once in setup(), reused by every recording, never freed.
uint8_t *rec_buf = NULL;
size_t rec_buf_bytes = 0;   // PCM capacity in bytes (without the 44-byte header)
size_t rec_pos = 0;         // bytes of PCM recorded in the current take
unsigned long rec_start = 0;

// WiFi-config button (WIFI_CONFIG_BUTTON_PIN): holding it for a while
// wipes the stored WiFi credentials so the device reboots into AP mode.
bool wifiBtnHeld = false;
unsigned long wifiBtnPressStart = 0;
const unsigned long WIFI_BTN_LONG_PRESS_MS = 5000; // 5 s hold → reset WiFi settings

void combinedOutput(int x, int y, char* line, bool clrscr) {
  if(clrscr) {
    display.clearDisplay();
  }
  Serial.println(line);
  display.setCursor(x, y);
  display.println(line);
  display.display();
}

// Show an error on the OLED (title line 1, wrapped detail lines 2-4) and
// mirror the full message to Serial. The detail text is wrapped at word
// boundaries to fit the 128 px display (21 chars/line at font size 1).
// The screen stays until the next button press (the button flow re-shows
// the WiFi status first).
void displayError(const String& title, const String& detail) {
  Serial.print(F("ERROR: "));
  Serial.println(title);
  if (detail.length()) {
    Serial.print(F("       "));
    Serial.println(detail);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(title);

  String word;
  String line;
  int y = 16;
  const int charsPerLine = 21; // 128 px / 6 px per char
  const int maxLines = 3;
  auto flushLine = [&]() {
    if (line.length() && y < SCREEN_HEIGHT) {
      display.setCursor(0, y);
      display.println(line);
      y += 16;
      line = "";
    }
  };
  for (unsigned int i = 0; i <= detail.length() && y < SCREEN_HEIGHT; i++) {
    char c = detail.charAt(i);
    if (c == ' ' || c == '\n' || c == '\t') {
      if (word.length()) {
        if (line.length() + word.length() > charsPerLine) flushLine();
        if (line.length()) line += " ";
        line += word;
        word = "";
      }
      if (c == '\n') flushLine();
    } else {
      word += c;
    }
  }
  if (word.length()) {
    if (line.length() + word.length() > charsPerLine) flushLine();
    if (line.length()) line += " ";
    line += word;
  }
  flushLine();
  display.display();
}

// Show the current WiFi situation on the display (and Serial).
//  - AP mode:    show the access point name + IP so it can be configured
//  - connected:  show the IP address assigned by the router
//  - otherwise:  show that it is still trying to connect
void showWifiStatus() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (wifiConfig.ESP_mode == AP_MODE) {
    display.println(F("No WiFi connected"));
    display.println(F("Join AP:"));
    display.println(wifiConfig.get_AP_name());
    display.print(F("IP: "));
    display.println(wifiConfig.ESP_IP.toString());
    display.print(F("Port: "));
    display.println(WIFI_SETUP_PORT);
    Serial.print(F("AP name: "));
    Serial.println(wifiConfig.get_AP_name());
    Serial.print(F("Setup URL: http://"));
    Serial.print(wifiConfig.ESP_IP.toString());
    Serial.print(F(":"));
    Serial.println(WIFI_SETUP_PORT);
  } else if (wifiConfig.wifi_connected) {
    display.println(F("WiFi connected"));
    display.print(F("SSID: "));
    display.println(WiFi.SSID());
    display.print(F("IP: "));
    display.println(wifiConfig.ESP_IP.toString());
    Serial.print(F("Connected to "));
    Serial.print(WiFi.SSID());
    Serial.print(F(" IP: "));
    Serial.println(wifiConfig.ESP_IP.toString());
  } else {
    display.println(F("Connecting to WiFi..."));
    Serial.println(F("Connecting to WiFi..."));
  }
  display.display();
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
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("WiFi settings reset."));
  display.println(F("Rebooting to setup..."));
  display.display();
  delay(300);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Hold-to-record (issue #9): helpers
// ---------------------------------------------------------------------------

void textGeneration(String prompt); // forward declaration (defined below)

// Allocate the recording buffer once (PSRAM) and write the 44-byte PCM WAV
// header with placeholder sizes (patched per take by patchWavHeader()).
// The buffer is reused by every recording and never freed.
// Returns true on success; on failure the error is shown on the display
// (no fallback to the old fixed 5 s recording).
bool initRecBuffer() {
  // Steady-state free memory after WiFi + web server + I2S are up.
  size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  bool have_psram = (free_mem > 0);
  if (!have_psram) {
    free_mem = esp_get_free_heap_size(); // fallback: internal heap
  }

  // 16 kHz * 32-bit * mono = 65536 bytes of PCM per second (fixed HW config).
  size_t cap = (size_t)MAX_REC_SECONDS * 65536;
  size_t margin = (size_t)REC_SAFETY_MARGIN_KB * 1024;
  if (free_mem > margin + 44) {
    size_t avail = free_mem - margin - 44;
    if (avail < cap) cap = avail;
  } else {
    cap = 0;
  }

  D_TD(F("rec buffer: free_mem="));
  D_TDDEC(free_mem);
  D_TD(F(" psram="));
  D_TDLN(have_psram ? "yes" : "no");

  if (cap == 0) {
    displayError(F("Record buffer alloc failed"),
                F("Not enough free memory for the recording buffer."));
    return false;
  }

  rec_buf = (uint8_t *)heap_caps_malloc(cap + 44,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (rec_buf == NULL && !have_psram) {
    rec_buf = (uint8_t *)malloc(cap + 44);
  }
  if (rec_buf == NULL) {
    displayError(F("Record buffer alloc failed"),
                F("heap_caps_malloc failed for the recording buffer."));
    return false;
  }

  rec_buf_bytes = cap;

  // 44-byte PCM WAV header (16 kHz, 32-bit, mono) with placeholder sizes.
  // Layout (little-endian): "RIFF" | riff_size | "WAVE" | "fmt " | 16 | 1 |
  //   1 | 16000 | 64000 | 4 | 32 | "data" | data_size
  const uint8_t hdr[44] = {
    'R', 'I', 'F', 'F', 0, 0, 0, 0,          // riff_size patched per take
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ', 16, 0, 0, 0,         // fmt subchunk size = 16
    1, 0,                                    // audio format: PCM
    1, 0,                                    // channels: mono
    0x00, 0x3E, 0x00, 0x00,                  // sample rate: 16000
    0x00, 0xFC, 0x00, 0x00,                  // byte rate: 64000
    4, 0,                                    // block align
    32, 0,                                   // bits per sample
    'd', 'a', 't', 'a', 0, 0, 0, 0           // data_size patched per take
  };
  memcpy(rec_buf, hdr, 44);

  D_TD(F("rec buffer: "));
  D_TDDEC(cap);
  D_TD(F(" bytes PCM = "));
  D_TDDEC(cap / 65536);
  D_TDLN(F(" s max"));
  return true;
}

// Fill in the two size fields of the preallocated WAV header for the
// current take (both little-endian uint32_t):
//   offset 4  -> RIFF chunk size = rec_pos + 36
//   offset 40 -> data chunk size = rec_pos
void patchWavHeader(size_t pcm_bytes) {
  uint32_t riff_size = pcm_bytes + 36;
  rec_buf[4]  = (uint8_t)(riff_size & 0xFF);
  rec_buf[5]  = (uint8_t)((riff_size >> 8) & 0xFF);
  rec_buf[6]  = (uint8_t)((riff_size >> 16) & 0xFF);
  rec_buf[7]  = (uint8_t)((riff_size >> 24) & 0xFF);
  rec_buf[40] = (uint8_t)(pcm_bytes & 0xFF);
  rec_buf[41] = (uint8_t)((pcm_bytes >> 8) & 0xFF);
  rec_buf[42] = (uint8_t)((pcm_bytes >> 16) & 0xFF);
  rec_buf[43] = (uint8_t)((pcm_bytes >> 24) & 0xFF);
}

// SENDING state: patch the header, transcribe the take, run the LLM call.
// Returns true if a transcription was produced (false = error already shown).
bool sendRecording() {
  patchWavHeader(rec_pos);
  D_TD(F("sending "));
  D_TDDEC(rec_pos);
  D_TDLN(F(" bytes of PCM ("));
  D_TDDEC((unsigned)(rec_pos / 65536));
  D_TDLN(F(" s of audio)"));

  combinedOutput(0, 0, "Sending audio", true);
  String transcription = audio.file(rec_buf, 44 + rec_pos, OPENAI_AUDIO_INPUT_FORMAT_WAV);
  log_d(transcription);
  D_TD(F("transcription length: "));
  D_TDLN(transcription.length());

  // The library swallows HTTP failures (unreachable host, server error,
  // model not installed) and just returns an empty string. Make that
  // visible instead of sending an empty prompt to the LLM.
  transcription.trim();
  if (transcription.length() == 0) {
    displayError(F("Transcription failed"),
                F("LocalAI unreachable or returned an error. Check LOCALAI_URL in the setup page (Custom tab)."));
    return false;
  }

  textGeneration(transcription);
  return true;
}

void textGeneration(String prompt) {
  char cprompt[prompt.length() + 1];
  memcpy(cprompt, prompt.c_str(), prompt.length() + 1);
  D_TD(F("prompt length: "));
  D_TDLN(prompt.length());
  combinedOutput(0, 0, "Sending prompt", true);
  combinedOutput(0, 16, cprompt, false);

  OpenAI_StringResponse result = chat.message(prompt);
  Serial.printf("Received message. Tokens: %u\n", result.tokens());
  D_TD(F("response length: "));
  D_TDLN(String(result.getAt(0)).length());

  // Check the error FIRST: on failure the library returns an empty
  // response plus the server's error text (e.g. "The model 'gpt-4' does
  // not exist"), which we must not swallow into a blank display.
  if (result.error()) {
    displayError(F("LLM error"), String(result.error()));
    return;
  }

  String response = result.getAt(0);
  response.trim();
  response.replace("\n", " ");
  log_d(response);

  if (response.length() == 0) {
    // HTTP 200 but no content (e.g. an unexpected response shape).
    displayError(F("Empty response"),
                F("LocalAI returned no text. Check the model and its settings."));
    return;
  }

  char cresponse[response.length() + 1];
  memcpy(cresponse, response.c_str(), response.length() + 1);
  combinedOutput(0, 0, "Response: ", true);
  combinedOutput(0, 16, cresponse, false);
}

void setup() {
  Serial.begin(115200);
  D_TDLN(F("setup() start"));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_CONFIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESERVE_BUTTON_PIN, INPUT_PULLUP);
  D_TDLN(F("pin setup done (BUTTON_PIN, LED_PIN, WIFI_CONFIG_BUTTON_PIN, RESERVE_BUTTON_PIN)"));

  // Register the LocalAI user settings BEFORE initialize() (library API
  // requirement). The config.h values are the initial defaults.
  if (wifiConfig.addSetting("LOCALAI_URL", api_url) < 0)
    Serial.println(F("WARNING: could not register LOCALAI_URL setting"));
  if (wifiConfig.addSetting("LOCALAI_KEY", api_key) < 0)
    Serial.println(F("WARNING: could not register LOCALAI_KEY setting"));
  D_TDLN(F("LocalAI settings registered (LOCALAI_URL, LOCALAI_KEY)"));

/* setup display*/
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  D_TDLN(F("OLED display initialized (SSD1306 @ 0x3C)"));
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();

/* connect to WiFi (or start the setup access point) */
  combinedOutput(0, 0, "Connecting to WiFi", true);
  if (wifiConfig.initialize() == AP_MODE) {
    // No known network was reachable: the device is broadcasting an access
    // point. Keep the setup web server running so the WiFi can be configured.
    wifiConfig.Start_HTTP_Server(0);
  }

  D_TD(F("WiFi mode after initialize(): "));
  D_TDLN(wifiConfig.ESP_mode == AP_MODE ? "AP (setup page)" : "STA");

/* setup i2s */  
  combinedOutput(0, 0, "Initializing I2S bus...", true);
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    combinedOutput(0, 16, "Failed to initialize I2S bus!", false);
    return;
  }
  combinedOutput(0, 16, "I2S bus initialized.", false);
  D_TDLN(F("I2S bus initialized (16 kHz, 32-bit, mono, slot left)"));

/* allocate the hold-to-record buffer once (PSRAM), before the OpenAI client
   so the upload buffer is sized with the recording buffer already in place */
  if (!initRecBuffer()) {
    // No fallback to the old fixed 5 s recording: the error is already on
    // the display. Recording stays disabled until the device is rebooted
    // with enough free memory.
    recState = IDLE;
  }

/* setup openai (endpoint + key from the stored settings: Custom tab of the
   setup page, config.h defaults on first boot / after a full reset) */
  String localaiUrl = wifiConfig.getSetting("LOCALAI_URL");
  String localaiKey = wifiConfig.getSetting("LOCALAI_KEY");
  if (localaiUrl.length() == 0) localaiUrl = api_url;
  if (localaiKey.length() == 0) localaiKey = api_key;
  openai = OpenAI(localaiKey.c_str(), localaiUrl.c_str());
  Serial.print(F("LocalAI endpoint: "));
  Serial.println(localaiUrl);
  D_TD(F("LocalAI endpoint resolved: "));
  D_TDLN(localaiUrl);

  chat.setModel("gpt-4");           //Model to use for completion. Default is gpt-3.5-turbo
  D_TDLN(F("chat model: gpt-4, max_tokens: 40, temperature: 0.2"));
  chat.setSystem("You are communicating through a small display, keep answers as short as possible");      //Description of the required assistant
  chat.setMaxTokens(40);            //The maximum number of tokens to generate in the completion.
  chat.setTemperature(0.2);         //float between 0 and 2. Higher value gives more random results.
  chat.setStop("\r");               //Up to 4 sequences where the API will stop generating further tokens.
  chat.setPresencePenalty(0);       //float between -2.0 and 2.0. Positive values increase the model's likelihood to talk about new topics.
  chat.setFrequencyPenalty(0);      //float between -2.0 and 2.0. Positive values decrease the model's likelihood to repeat the same line verbatim.
  chat.setUser("OpenAI-ESP32");     //A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.

  audio.setTemperature(0.1);
  audio.setLanguage("en");

/* show the final WiFi status (access point name + IP, or the assigned IP) */
  showWifiStatus();
  D_TDLN(F("setup() done"));
}

void loop() {
  // Keep the ESP-Wifi-Config machinery running: it (re)connects to a known
  // network and serves the setup page while in AP mode.
  wifiConfig.handle(10000);

  // WiFi-config button: a 5 s long-press wipes the stored WiFi settings
  // and reboots into the setup AP (escape hatch for a wrong password).
  if (digitalRead(WIFI_CONFIG_BUTTON_PIN) == LOW) {
    if (!wifiBtnHeld) {
      D_TDLN(F("WiFi-config button pressed (hold 5 s to reset settings)"));
      wifiBtnHeld = true;
      wifiBtnPressStart = millis();
    } else if ((millis() - wifiBtnPressStart) >= WIFI_BTN_LONG_PRESS_MS) {
      Serial.println(F("WiFi-config button held 5 s - resetting WiFi settings"));
      D_TDLN(F("WiFi-config button long-press: resetting WiFi settings and rebooting"));
      resetWifiSettingsAndRestart(); // does not return (reboots)
    }
  } else {
    wifiBtnHeld = false;
  }

  // -----------------------------------------------------------------------
  // Hold-to-record state machine (issue #9):
  //   IDLE      - debounced button press starts recording (if WiFi is up and
  //               the recording buffer was allocated)
  //   RECORDING - button held: stream I2S audio into the PSRAM buffer;
  //               stops on release / buffer full / MAX_REC_SECONDS
  //   SENDING   - patch WAV header, transcribe, LLM call (blocking)
  // -----------------------------------------------------------------------

  if (recState == IDLE) {
    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState) {
      lastDebounce = millis();
      lastButtonState = reading;
    }

    if ((millis() - lastDebounce) > 50) {
      if (reading == LOW) { // Button is pushed (low due to pullup)
        D_TDLN(F("button pressed (hold to record)"));
        if (rec_buf == NULL) {
          // Recording buffer allocation failed at boot: keep the error
          // visible, do not start a take.
          displayError(F("Record buffer alloc failed"),
                      F("Recording is disabled. Reboot the device."));
          lastButtonState = HIGH; // re-arm: only react to a fresh press
          return;
        }
        if (wifiConfig.ESP_mode != AP_MODE && wifiConfig.wifi_connected) {
          // Connected to a known network: start recording into the buffer.
          showWifiStatus();
          rec_pos = 0;
          rec_start = millis();
          recState = RECORDING;
          digitalWrite(LED_PIN, HIGH);
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println(F("Recording"));
          display.println(F("max 10 s"));
          display.display();
          D_TDLN(F("recording start (hold button, max 10 s)"));
        } else {
          // Not connected: keep showing the access point / connection status.
          showWifiStatus();
          lastButtonState = HIGH; // re-arm: only react to a fresh press
        }
      }
    }
    return;
  }

  if (recState == RECORDING) {
    // Stream I2S audio into the preallocated buffer (blocking, ~97 ms).
    size_t n = i2s.readBytes((char *)(rec_buf + 44 + rec_pos), REC_CHUNK_BYTES);
    rec_pos += n;

    // Stop conditions (checked after every chunk):
    bool stop = false;
    if (digitalRead(BUTTON_PIN) != LOW) {
      D_TDLN(F("button released - stopping recording"));
      stop = true;
    } else if (rec_pos >= rec_buf_bytes) {
      Serial.println(F("Recording buffer full - stopping"));
      D_TDLN(F("recording buffer full - stopping"));
      stop = true;
    } else if ((millis() - rec_start) >= (unsigned long)MAX_REC_SECONDS * 1000UL) {
      Serial.println(F("Max recording time reached - stopping"));
      D_TDLN(F("max recording time reached - stopping"));
      stop = true;
    }

    if (!stop) {
      return; // still recording
    }

    // Recording finished: hand over to the send flow.
    digitalWrite(LED_PIN, LOW);
    D_TD(F("recorded "));
    D_TDDEC(rec_pos);
    D_TDLN(F(" bytes of PCM"));
    recState = SENDING;
  }

  // SENDING (blocking: transcription + LLM call)
  sendRecording();
  recState = IDLE;
  lastButtonState = HIGH; // re-arm the debounce for the next press
}
