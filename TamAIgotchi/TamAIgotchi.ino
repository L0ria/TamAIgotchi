#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <ESPWifiConfig.h>
#include "ESP_I2S.h"
#include "esp_heap_caps.h"  // heap_caps_malloc / heap_caps_get_free_size (PSRAM recording buffer)
#include <OpenAI.h>
#include "config.h"
#include "alien.h"
#include "text_utils.h"
#include <string.h>  // strlen (speech-bubble width from ALIEN_BUBBLE_TEXT)

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

// State machine (issue #9 + issue #13):
//   IDLE      - waiting for a debounced button press
//   RECORDING - button held: streaming I2S audio into the preallocated
//               PSRAM buffer; LED on; stops on release / buffer full / cap
//   SENDING   - patching the WAV header + uploading to LocalAI for
//               transcription, then the LLM call (blocking)
//   RESPONSE  - the LLM reply is shown as a scrollable window (issue #13):
//               GPIO9 short = scroll down, GPIO11 short = scroll up,
//               GPIO11 hold 5 s = back to IDLE, main button = new recording
enum RecState { IDLE, RECORDING, SENDING, RESPONSE };
RecState recState = IDLE;

// ---------------------------------------------------------------------------
// Idle animation (issue #16): a small pixel-art alien that entertains the
// screen after ALIEN_IDLE_TIMEOUT_MS without any button press (see config.h
// for all timings). The animation also returns ALIEN_RESPONSE_TIMEOUT_MS
// after the LLM response has been shown without a button press.
//   ANIM_IDLE   - waiting for inactivity (IDLE or RESPONSE state)
//   ANIM_ACTIVE - running the 70 s loop (bubble / wave / stand phases)
enum AlienState { ANIM_IDLE, ANIM_ACTIVE };
AlienState alienState = ANIM_IDLE;
unsigned long alienStateStart = 0;   // millis() when the current phase started
unsigned long alienActivityMs = 0;   // millis() of the last button press
int alienPhase = 0;                  // 0=bubble, 1=wave, 2=bubble, 3=wave, 4=stand
int alienFrame = 0;                  // current sprite frame index (0..3)
unsigned long alienFrameMs = 0;      // millis() of the last frame swap

// Scrollable response view (issue #13): the reply is word-wrapped into this
// static table and rendered as a RESPONSE_VISIBLE_LINES window below a
// "Response: x/y" header. ~350 B of static RAM in total.
char respLines[RESPONSE_MAX_LINES][RESPONSE_CHARS_PER_LINE + 1];
int  respLineCount = 0;  // number of wrapped lines actually in use
int  scrollOffset  = 0;  // index of the first visible line (0 .. max(0, count - RESPONSE_VISIBLE_LINES))

// Preallocated recording buffer (44-byte WAV header + PCM audio) in PSRAM.
// Allocated once in setup(), reused by every recording, never freed.
uint8_t *rec_buf = NULL;
size_t rec_buf_bytes = 0;   // PCM capacity in bytes (without the 44-byte header)
size_t rec_pos = 0;         // bytes of PCM recorded in the current take
unsigned long rec_start = 0;

// Button press tracking (shared by the main + scroll buttons):
//   pin        - the GPIO this button is wired to (set in setup())
//   lastState  - last (debounced) level read
//   pressStart - millis() when the current level was first seen
//   longFired  - the 5 s long-press action already ran for this press
//   acted      - the short-press action already ran for this press
struct BtnState {
  int pin = -1;
  int lastState = HIGH;
  unsigned long pressStart = 0;
  bool longFired = false;
  bool acted = false;
};
BtnState mainBtn, scrollUpBtn, scrollDownBtn;

// Debounce: returns true once the button has been stably LOW for
// BUTTON_DEBOUNCE_MS (i.e. after the press edge settles).
bool buttonPressed(BtnState& b) {
  int reading = digitalRead(b.pin);
  if (reading != b.lastState) {
    b.lastState = reading;
    b.pressStart = millis();
  }
  return (b.lastState == LOW) && (millis() - b.pressStart) > BUTTON_DEBOUNCE_MS;
}
// 5 s long-press: true while held past the threshold and not yet fired.
bool buttonLongPress(BtnState& b) {
  return (b.lastState == LOW) && !b.longFired &&
         (millis() - b.pressStart) >= BUTTON_LONG_PRESS_MS;
}

// ---------------------------------------------------------------------------
// Idle animation (issue #16): helpers
// ---------------------------------------------------------------------------

// Any button press is activity: it stops the animation and restarts the
// inactivity timers (both the idle start and the response auto-return).
void markAlienActivity() {
  alienActivityMs = millis();
  if (alienState == ANIM_ACTIVE) {
    alienState = ANIM_IDLE;
    alienPhase = 0;
  }
}

// The animation only runs while the device is fully usable: STA mode,
// connected, and the recording buffer allocated (see issue #16 answers).
bool alienCanAnimate() {
  return (rec_buf != NULL) &&
         (wifiConfig.ESP_mode != AP_MODE) &&
         wifiConfig.wifi_connected;
}

// Draw one alien sprite at (x, y) using the Adafruit_GFX 1-bit format
// (MSB-first row-major, as in alien.h).
void renderAlien(int frame, int x, int y) {
  display.drawBitmap(x, y, alienFrameData[frame], ALIEN_SPRITE_W, ALIEN_SPRITE_H, WHITE);
}

// Render the current animation scene. The alien (24x22 px) is anchored at
// (4, 42): it spans x 4..27 (< 64) and y 42..63 (>= 32), so it stays in the
// lower-left quadrant. The speech bubble sits above the alien's head at
// (2, 24) with its width derived from the configured text (clamped so it
// never crosses the vertical middle at x = 64); y 24..39 (above the middle).
void renderAlienScene() {
  display.clearDisplay();
  if (alienPhase == 0 || alienPhase == 2) {
    // Speech-bubble phase: standing still + bubble with the configured text.
    // Font 1 advances 6 px per char (5 px glyph + 1 px gap); add 2 px
    // padding on each side, clamp to 60 px wide so the bubble stays left
    // of the vertical middle even with a longer ALIEN_BUBBLE_TEXT.
    int bubbleW = strlen(ALIEN_BUBBLE_TEXT) * 6 + 4;
    if (bubbleW > 60) bubbleW = 60;
    display.drawRect(2, 24, bubbleW, 16, WHITE);
    display.setCursor(4, 28);
    display.print(ALIEN_BUBBLE_TEXT);
    renderAlien(ALIEN_FRAME_STAND, 4, 42);
  } else if (alienPhase == 1 || alienPhase == 3) {
    // Jump & wave phase: alternating jump / wave frames (swap in alienUpdate()).
    renderAlien(alienFrame, 4, 42);
  } else {
    // Standing-still phase: just the alien, no animation.
    renderAlien(ALIEN_FRAME_STAND, 4, 42);
  }
  display.display();
}

// Advance the 70 s loop (bubble 4 s -> wave 16 s -> bubble 4 s -> wave 16 s
// -> stand 30 s -> repeat), swap the sprite frame every ALIEN_FRAME_MS
// during the wave phases, and re-render on every change.
void alienUpdate() {
  if (alienState != ANIM_ACTIVE) return;
  unsigned long now = millis();
  unsigned long t = now - alienStateStart;

  if (alienPhase == 0 || alienPhase == 2) {
    if (t >= ALIEN_BUBBLE_MS) { alienPhase++; alienStateStart = now; renderAlienScene(); }
    return;
  }
  if (alienPhase == 1 || alienPhase == 3) {
    // Wave phase: cycle JUMP (body up) -> WAVEUP (arm up) -> WAVEDN (arm
    // down) every ALIEN_FRAME_MS for a continuous jump-and-wave motion.
    if (t >= ALIEN_WAVE_MS) {
      alienPhase++;
      alienStateStart = now;
      alienFrame = ALIEN_FRAME_STAND;
      renderAlienScene();
      return;
    }
    if (now - alienFrameMs >= ALIEN_FRAME_MS) {
      alienFrameMs = now;
      alienFrame = (alienFrame == ALIEN_FRAME_JUMP)   ? ALIEN_FRAME_WAVE_UP
                 : (alienFrame == ALIEN_FRAME_WAVE_UP) ? ALIEN_FRAME_WAVE_DOWN
                 : ALIEN_FRAME_JUMP;
      renderAlienScene();
    }
    return;
  }
  // Phase 4: standing still for ALIEN_STAND_MS, then the loop restarts.
  if (t >= ALIEN_STAND_MS) {
    alienPhase = 0;
    alienStateStart = now;
    renderAlienScene();
  }
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
  markAlienActivity(); // issue #16: showWifiStatus() is always the result of
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

// Render the current response window (issue #13). The default font is
// 6x8 px, so the 128x64 screen holds 21 chars x 8 lines. Line 0 is the
// "Response: x/y" header (x = first visible line, y = total lines); line 1
// is a blank separator; the next RESPONSE_VISIBLE_LINES lines are the
// window starting at scrollOffset. Lines are printed consecutively
// (println auto-advances 8 px), matching showWifiStatus().
void renderResponseWindow() {
  int total = respLineCount;
  int maxOffset = (total > RESPONSE_VISIBLE_LINES) ? total - RESPONSE_VISIBLE_LINES : 0;
  if (scrollOffset < 0) scrollOffset = 0;
  if (scrollOffset > maxOffset) scrollOffset = maxOffset;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Response: "));
  display.print(scrollOffset + 1);
  display.print('/');
  display.println(total); // newline -> next line (y=8)
  display.println();      // blank separator line (y=16)

  for (int i = 0; i < RESPONSE_VISIBLE_LINES; i++) {
    int idx = scrollOffset + i;
    if (idx >= total) break;
    display.println(respLines[idx]); // auto-advances 8 px per line
  }
  display.display();
}

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

  // Store the reply in the static line table and switch to the scrollable
  // RESPONSE view (issue #13) instead of dumping the raw text on the screen
  // (which clipped everything below y=64).
  respLineCount = wrapText(response, respLines, RESPONSE_MAX_LINES);
  scrollOffset = 0;
  renderResponseWindow();
  recState = RESPONSE;
  // Issue #16: re-arm the inactivity timer so the animation returns
  // ALIEN_RESPONSE_TIMEOUT_MS after the response has been shown without a
  // button press (driven from loop() via markAlienActivity()).
  markAlienActivity();
  D_TDLN(F("response ready (scroll: GPIO9 down / GPIO11 up, hold GPIO11 5 s to exit)"));
}

void setup() {
  Serial.begin(115200);
  D_TDLN(F("setup() start"));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_CONFIG_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SCROLL_UP_PIN, INPUT_PULLUP);
  mainBtn.pin = BUTTON_PIN;
  scrollDownBtn.pin = WIFI_CONFIG_BUTTON_PIN;
  scrollUpBtn.pin = SCROLL_UP_PIN;
  D_TDLN(F("pin setup done (BUTTON_PIN, LED_PIN, WIFI_CONFIG_BUTTON_PIN, SCROLL_UP_PIN)"));

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

  // Idle animation (issue #16): starts after ALIEN_IDLE_TIMEOUT_MS without
  // any button press (IDLE state) or after ALIEN_RESPONSE_TIMEOUT_MS of the
  // response being shown (RESPONSE state); any button press stops it.
  if (alienCanAnimate() && alienState == ANIM_IDLE) {
    unsigned long now = millis();
    unsigned long timeout = (recState == RESPONSE)
                          ? ALIEN_RESPONSE_TIMEOUT_MS
                          : ALIEN_IDLE_TIMEOUT_MS;
    if ((now - alienActivityMs) >= timeout) {
      alienState = ANIM_ACTIVE;
      alienPhase = 0;
      alienStateStart = now;
      alienFrame = ALIEN_FRAME_STAND;
      renderAlienScene();
      D_TDLN(F("alien animation: started (idle timeout without button press)"));
    }
  }
  alienUpdate();

  // Release detection: once a button is stably HIGH again, clear its
  // one-shot flags so the next press can act (and the 5 s long-press can
  // fire again). Checked for every button in every state.
  for (BtnState* b : {&mainBtn, &scrollUpBtn, &scrollDownBtn}) {
    if (digitalRead(b->pin) == HIGH && b->lastState == LOW) {
      b->lastState = HIGH;
      b->pressStart = millis();
      b->acted = false;
      b->longFired = false;
    }
  }

  // WiFi-config button (GPIO9): a 5 s long-press wipes the stored WiFi
  // settings and reboots into the setup AP (escape hatch for a wrong
  // password). Checked in every state, before the state machine branches.
  if (digitalRead(WIFI_CONFIG_BUTTON_PIN) == LOW) {
    if (!scrollDownBtn.longFired && scrollDownBtn.lastState == HIGH) {
      D_TDLN(F("WiFi-config button pressed (hold 5 s to reset settings)"));
      scrollDownBtn.lastState = LOW;
      scrollDownBtn.pressStart = millis();
    }
    if (buttonLongPress(scrollDownBtn)) {
      Serial.println(F("WiFi-config button held 5 s - resetting WiFi settings"));
      D_TDLN(F("WiFi-config button long-press: resetting WiFi settings and rebooting"));
      resetWifiSettingsAndRestart(); // does not return (reboots)
    }
  } else {
    scrollDownBtn.lastState = HIGH;
    scrollDownBtn.longFired = false;
    scrollDownBtn.acted = false;
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
    if (buttonPressed(mainBtn) && !mainBtn.acted) {
      mainBtn.acted = true; // one action per press
      // Issue #16: a press is activity - stop the animation (if running) and
      // re-arm the inactivity timer.
      markAlienActivity();
      D_TDLN(F("button pressed (hold to record)"));
      if (rec_buf == NULL) {
        // Recording buffer allocation failed at boot: keep the error
        // visible, do not start a take.
        displayError(F("Record buffer alloc failed"),
                    F("Recording is disabled. Reboot the device."));
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

  if (recState == RESPONSE) {
    // Scrollable response view (issue #13).
    // Issue #16: any button press is activity - it stops the animation
    // (if running) and restarts the auto-return timer.
    // GPIO9 (scroll down): short press = next line; the 5 s long-press
    // (WiFi reset) is already handled above in every state.
    if (buttonPressed(scrollDownBtn) && !scrollDownBtn.acted) {
      scrollDownBtn.acted = true;
      markAlienActivity();
      if (scrollOffset < respLineCount - RESPONSE_VISIBLE_LINES) {
        scrollOffset++;
        D_TD(F("scroll down ")); D_TDLN(scrollOffset + 1);
      }
      // Always refresh: also recovers the screen if the idle animation was
      // running when this press landed (issue #16).
      renderResponseWindow();
    }
    // GPIO11 (scroll up): short press = previous line; 5 s hold = exit
    // the response view back to IDLE.
    if (buttonLongPress(scrollUpBtn)) {
      scrollUpBtn.longFired = true;
      Serial.println(F("Scroll-up button held 5 s - exiting response view"));
      D_TDLN(F("scroll-up button long-press: back to IDLE"));
      recState = IDLE;
      showWifiStatus(); // also marks activity (issue #16)
      mainBtn.lastState = HIGH;
      return;
    }
    if (buttonPressed(scrollUpBtn) && !scrollUpBtn.acted) {
      scrollUpBtn.acted = true;
      markAlienActivity();
      if (scrollOffset > 0) {
        scrollOffset--;
        D_TD(F("scroll up ")); D_TDLN(scrollOffset + 1);
      }
      // Always refresh: also recovers the screen if the idle animation was
      // running when this press landed (issue #16).
      renderResponseWindow();
    }
    // Main button: starts a new recording (same as in IDLE).
    if (buttonPressed(mainBtn) && !mainBtn.acted) {
      mainBtn.acted = true;
      markAlienActivity();
      D_TDLN(F("button pressed (hold to record)"));
      if (rec_buf == NULL) {
        displayError(F("Record buffer alloc failed"),
                    F("Recording is disabled. Reboot the device."));
        return;
      }
      if (wifiConfig.ESP_mode != AP_MODE && wifiConfig.wifi_connected) {
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
        showWifiStatus();
      }
    }
    return;
  }

  // SENDING (blocking: transcription + LLM call). On success textGeneration()
  // already switched us to the RESPONSE view; on any error it stays in
  // SENDING, in which case we fall back to IDLE.
  sendRecording();
  if (recState != RESPONSE) {
    recState = IDLE;
    mainBtn.lastState = HIGH; // re-arm the debounce for the next press
    mainBtn.acted = false;
    markAlienActivity(); // issue #16: re-arm the idle-animation timer
  }
}
