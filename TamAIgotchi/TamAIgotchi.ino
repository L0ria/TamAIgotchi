// TamAIgotchi - ESP32 based client for LocalAI (app shell).
//
// Step 5 of the refactoring proposed in issue #18: this file now only
// contains setup() + loop() (plus the state-machine globals the modules
// share via extern). Everything else lives in dedicated modules:
//   hardware.h   - shared objects (display, i2s, wifiConfig, openai/chat/audio)
//                  + hardwareInit() (pinMode + OLED + I2S bring-up)
//   display.h    - displayMgr (the Display class: render() / showWifiStatus() /
//                  resetWifiSettingsAndRestart() / startRecording(), issue #51, step 8)
//   recorder.h   - PSRAM recording buffer + SENDING/RESPONSE flow
//   alien.h      - the idle-alien animation (issue #16)
//   buttons.h    - the debounced buttons (step 2)
//   text_utils.h - wrapText() (step 1; the display-role helpers were
//                  removed during the UI restructure in #29, issues #33/#36)
//   statusbar.h  - statusBar (show / error / clear / draw, step 3,
//                  issue #46: the top two lines are the status bar)
#include "hardware.h"   // shared hardware objects + hardwareInit()
#include "display.h"    // displayMgr (the Display class, issue #51, step 8)
#include "buttons.h"    // Button instances
#include "recorder.h"   // Recorder + RecState
#include "alien.h"      // AlienAnimation
#include "statusbar.h"  // statusBar (issue #46, step 3)
#include "messages.h"   // MSG_* user-facing display strings (issue #36, step 6)
#include "bubble.h"     // bubble (scroll / jump / render, issue #34, step 4)
#include "led.h"      // led (recording LED, issue #47, step 4)

// State machine (issue #9 + issue #13):
//   IDLE      - waiting for a debounced button press
//   RECORDING - button held: streaming I2S audio into the preallocated
//               PSRAM buffer; LED on; stops on release / buffer full / cap
//   SENDING   - patching the WAV header + uploading to LocalAI for
//               transcription, then the LLM call (blocking)
//   RESPONSE  - the LLM reply is shown in the speech bubble (issue #34,
//               step 4 of the UI restructure in #29): GPIO9 short = scroll
//               down, GPIO11 short = scroll up, double-press of the same
//               button = jump to start/end (option A, #29 Q7), GPIO11 hold
//               5 s = back to IDLE, main button = new recording
RecState recState = IDLE;  // app state machine (issue #9 + #13); shared with the recorder (recorder.h)

// ---------------------------------------------------------------------------
// Idle animation (issue #16): a small pixel-art alien that entertains the
// screen after ALIEN_IDLE_TIMEOUT_MS without any button press (see config.h
// for all timings). The animation also returns ALIEN_RESPONSE_TIMEOUT_MS
// after the LLM response has been shown without a button press.
//   ANIM_IDLE   - waiting for inactivity (IDLE or RESPONSE state)
//   ANIM_ACTIVE - running the 70 s loop (bubble / wave / stand phases)
// Buttons (step 2 of the refactoring, issue #18): one Button instance per
// physical button. update() is called once per loop() pass for every
// button before reading isPressed() / isLongPressed() (see buttons.h).
Button mainBtn(BUTTON_PIN);
Button scrollUpBtn(SCROLL_UP_PIN);
Button scrollDownBtn(WIFI_CONFIG_BUTTON_PIN);

// Recorder (step 3 of the refactoring, issue #18): owns the preallocated
// PSRAM recording buffer + the SENDING/RESPONSE flow (recorder.initRecBuffer /
// recorder.sendRecording / textGeneration). See recorder.h.
Recorder recorder;

// Alien (step 4 of the refactoring, issue #18): owns the idle-animation state
// machine + rendering (markActivity() on button events, update() each pass).
// See alien.h.
AlienAnimation alien;

// Bubble (step 2 of 11 of the refactoring in #42, issue #45): the shared
// speech-bubble object (the codebase's existing shared-object pattern -
// the instance lives in the sketch, the modules reference it via the
// extern in bubble.h, same as `display`).
Bubble bubble;

// Status bar (step 3 of 11 of the refactoring in #42, issue #46): the
// shared status-bar object (same shared-object pattern as `bubble` - the
// instance lives in the sketch, the modules reference it via the extern
// in statusbar.h).
StatusBar statusBar;

// Recording LED (step 4 of 11 of the refactoring in #42, issue #47): the
// shared LED object (same shared-object pattern as `bubble` / `statusBar`
// - the instance lives in the sketch, the modules reference it via the
// extern in led.h).
Led led(LED_PIN);

// Display manager (step 8 of 11 of the refactoring in #42, issue #51): the
// shared display object owning the single render pass + the display-level
// actions (showWifiStatus / resetWifiSettingsAndRestart / startRecording).
// Constructed AFTER the six objects it references (display / statusBar /
// alien / bubble / wifiConfig / recorder / led) - the same shared-object
// pattern as `bubble` / `statusBar` / `led` (the instance lives in the
// sketch, the modules reference it via the extern in display.h).
Display displayMgr(display, statusBar, alien, bubble, wifiConfig, recorder, led);

void setup() {
  Serial.begin(115200);
  D_TDLN(F("setup() start"));

  // Hardware bring-up (step 5 of the refactoring, issue #18): pinMode +
  // OLED init + I2S init, moved to hardwareInit() (hardware.h).
  if (!hardwareInit()) {
    return; // I2S failed to initialize - the error is already on the display
  }
  D_TDLN(F("hardware init done (pins, OLED, I2S)"));

  // Register the LocalAI user settings BEFORE initialize() (library API
  // requirement). The config.h values are the initial defaults.
  if (wifiConfig.addSetting("LOCALAI_URL", api_url) < 0)
    Serial.println(F("WARNING: could not register LOCALAI_URL setting"));
  if (wifiConfig.addSetting("LOCALAI_KEY", api_key) < 0)
    Serial.println(F("WARNING: could not register LOCALAI_KEY setting"));
  D_TDLN(F("LocalAI settings registered (LOCALAI_URL, LOCALAI_KEY)"));

/* connect to WiFi (or start the setup access point) */
  statusBar.show(MSG_WIFI_CONNECTING);
  if (wifiConfig.initialize() == AP_MODE) {
    // No known network was reachable: the device is broadcasting an access
    // point. Keep the setup web server running so the WiFi can be configured.
    wifiConfig.Start_HTTP_Server(0);
  }

  D_TD(F("WiFi mode after initialize(): "));
  D_TDLN(wifiConfig.ESP_mode == AP_MODE ? "AP (setup page)" : "STA");

/* allocate the hold-to-record buffer once (PSRAM), before the OpenAI client
   so the upload buffer is sized with the recording buffer already in place */
  if (!recorder.initRecBuffer()) {
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
  D_TDLN(F("chat model: gpt-4, max_tokens: 200, temperature: 0.2"));
  chat.setSystem("You are communicating through a small display, keep answers as short as possible");      //Description of the required assistant
  chat.setMaxTokens(LLM_MAX_TOKENS); //The maximum number of tokens to generate (issue #35, step 5 of #29: 40 -> 200 via config.h).
  chat.setTemperature(0.2);         //float between 0 and 2. Higher value gives more random results.
  chat.setStop("\r");               //Up to 4 sequences where the API will stop generating further tokens.
  chat.setPresencePenalty(0);       //float between -2.0 and 2.0. Positive values increase the model's likelihood to talk about new topics.
  chat.setFrequencyPenalty(0);      //float between -2.0 and 2.0. Positive values decrease the model's likelihood to repeat the same line verbatim.
  chat.setUser("OpenAI-ESP32");     //A unique identifier representing your end-user, which can help OpenAI to monitor and detect abuse.

  audio.setTemperature(0.1);
  audio.setLanguage("en");

/* show the final WiFi status (access point name + IP, or the assigned IP) */
  displayMgr.showWifiStatus();
  D_TDLN(F("setup() done"));
}

void loop() {
  // Keep the ESP-Wifi-Config machinery running: it (re)connects to a known
  // network and serves the setup page while in AP mode.
  wifiConfig.handle(10000);

  // Idle animation (issue #16): starts after ALIEN_IDLE_TIMEOUT_MS without
  // any button press (IDLE state) or after ALIEN_RESPONSE_TIMEOUT_MS of the
  // response being shown (RESPONSE state); any button press stops it.
  // (step 4, issue #18: the state machine now lives in the AlienAnimation
  // class - see alien.h; update() starts it on the idle timeout + advances
  // the 70 s loop.)
  // issue #50, step 7 of 11 of the refactoring plan in #42: the alien no
  // longer reads the app state / recorder / WiFi library via extern - the
  // caller computes + passes the three inputs (RESPONSE flag, the recording
  // buffer allocated, and the WiFi link up).
  alien.update(recState == RESPONSE,
               recorder.bufferAllocated(),
               (wifiConfig.ESP_mode != AP_MODE) && wifiConfig.wifi_connected);

  // Debounce edge-detect for all buttons (step 2, issue #18): call once
  // per loop() pass for every button, before reading isPressed() /
  // isLongPressed().
  mainBtn.update();
  scrollUpBtn.update();
  scrollDownBtn.update();

  // WiFi-config button (GPIO9): a 5 s long-press wipes the stored WiFi
  // settings and reboots into the setup AP (escape hatch for a wrong
  // password). Checked in every state, before the state machine branches.
  if (scrollDownBtn.isLongPressed()) {
    Serial.println(F("WiFi-config button held 5 s - resetting WiFi settings"));
    D_TDLN(F("WiFi-config button long-press: resetting WiFi settings and rebooting"));
    displayMgr.resetWifiSettingsAndRestart(); // does not return (reboots)
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
    if (mainBtn.isPressed()) {  // one action per press (debounced)
      // Issue #16: a press is activity - stop the animation (if running) and
      // re-arm the inactivity timer.
      alien.markActivity();
      displayMgr.startRecording(); // deduped block (display.h); shows the error / AP status
    }
    return;
  }

  if (recState == RECORDING) {
    // Stream I2S audio into the preallocated buffer (blocking, ~97 ms).
    // The buffer state is private (issue #49, step 6): the I2S read target
    // + the position advance go through the Recorder streaming API.
    size_t n = i2s.readBytes((char *)recorder.pcmDestination(), REC_CHUNK_BYTES);
    recorder.noteChunk(n);

    // Live recording counter (issue #33, step 3 of the UI restructure in
    // #29, Q10): status line 1 = "Recording (max 10 s)", line 2 = elapsed
    // seconds. The I2S read loop is chunked (~97 ms), so the counter
    // refreshes naturally each loop pass - throttled to once per whole
    // second to avoid re-drawing ~10x/s.
    unsigned long recSeconds = recorder.elapsedMs() / 1000UL;
    static unsigned long recSecondsShown = 0;
    if (recSeconds != recSecondsShown) {
      recSecondsShown = recSeconds;
      statusBar.show(MSG_RECORDING, String(recSeconds) + " s");
    }

    // Stop conditions (checked after every chunk):
    bool stop = false;
    if (!mainBtn.isHeld()) {
      D_TDLN(F("button released - stopping recording"));
      stop = true;
    } else if (recorder.isBufferFull()) {
      Serial.println(F("Recording buffer full - stopping"));
      D_TDLN(F("recording buffer full - stopping"));
      stop = true;
    } else if (recorder.elapsedMs() >= (unsigned long)MAX_REC_SECONDS * 1000UL) {
      Serial.println(F("Max recording time reached - stopping"));
      D_TDLN(F("max recording time reached - stopping"));
      stop = true;
    }

    if (!stop) {
      return; // still recording
    }

    // Recording finished: hand over to the send flow.
    led.off();  // recording LED (issue #47, step 4)
    D_TD(F("recorded "));
    D_TDDEC(recorder.recordedBytes());
    D_TDLN(F(" bytes of PCM"));
    recState = SENDING;
  }

  if (recState == RESPONSE) {
    // Scrollable response in the speech bubble (issue #34, step 4 of 6 of
    // the UI restructure in #29): the reply lives in the bubble table;
    // the "Response x/y" counter is status line 1 (issue #29 Q6).
    // Issue #16: any button press is activity - it stops the animation
    // (if running) and restarts the auto-return timer.
    //
    // Double-press jump (issue #29 Q7, option A): the same scroll button
    // pressed twice within DOUBLE_PRESS_MS jumps to the start (up) / end
    // (down) so long answers (~50+ wrapped lines) can be reached without
    // ~45 single presses. A press of the OTHER button resets the pair, so
    // up-down-up is never a double. The 5 s holds keep their meaning.
    static unsigned long lastPressMs  = 0;  // millis() of the last scroll press
    static int           lastPressBtn = -1; // -1 none, 0 = down, 1 = up
    static const unsigned long DOUBLE_PRESS_MS = 500;

    // Re-render the frame (single render pass, issue #35, step 5) +
    // refresh the "Response x/y" status counter. Called after every
    // scroll / jump, and on every press (a press also recovers the screen
    // if the idle animation was running when it landed, issue #16: the
    // bubble still holds the response text, so displayMgr.render() restores
    // it - Q4).
    auto renderResponse = []() {
      displayMgr.render();
      statusBar.show(MSG_RESPONSE_PREFIX + String(bubble.scrollOffset() + 1) + "/"
                 + String(bubble.lineCount()));
    };

    // GPIO9 (scroll down): short press = next line; double press = jump to
    // the end; the 5 s long-press (WiFi reset) is already handled above
    // in every state.
    if (scrollDownBtn.isPressed()) {
      alien.markActivity();
      unsigned long now = millis();
      bool isDouble = (lastPressBtn == 0) && ((now - lastPressMs) < DOUBLE_PRESS_MS);
      if (isDouble) {
        bubble.jumpTo(true);
        D_TDLN(F("double-press down: jump to end"));
      } else {
        bubble.scroll(true);
        D_TD(F("scroll down ")); D_TDLN(bubble.scrollOffset() + 1);
      }
      lastPressMs = now;
      lastPressBtn = 0;
      renderResponse();
    }
    // GPIO11 (scroll up): short press = previous line; double press = jump
    // to the start; 5 s hold = exit the response view back to IDLE.
    if (scrollUpBtn.isLongPressed()) {
      Serial.println(F("Scroll-up button held 5 s - exiting response view"));
      D_TDLN(F("scroll-up button long-press: back to IDLE"));
      bubble.clear(); // Q4: the response is removed when we leave the view
      recState = IDLE;
      displayMgr.showWifiStatus(); // also marks activity (issue #16)
      mainBtn.reset(); // re-arm the main button for the next press
      return;
    }
    if (scrollUpBtn.isPressed()) {
      alien.markActivity();
      unsigned long now = millis();
      bool isDouble = (lastPressBtn == 1) && ((now - lastPressMs) < DOUBLE_PRESS_MS);
      if (isDouble) {
        bubble.jumpTo(false);
        D_TDLN(F("double-press up: jump to start"));
      } else {
        bubble.scroll(false);
        D_TD(F("scroll up ")); D_TDLN(bubble.scrollOffset() + 1);
      }
      lastPressMs = now;
      lastPressBtn = 1;
      renderResponse();
    }
    // Main button: starts a new recording (same as in IDLE).
    if (mainBtn.isPressed()) {
      alien.markActivity();
      displayMgr.startRecording(); // deduped block (display.h); clears the bubble (issue #34)
    }
    return;
  }

  // SENDING (blocking: transcription + LLM call). On success textGeneration()
  // already switched us to the RESPONSE view; on any error it stays in
  // SENDING, in which case we fall back to IDLE.
  recorder.sendRecording();
  if (recState != RESPONSE) {
    recState = IDLE;
    mainBtn.reset(); // re-arm the debounce for the next press
    alien.markActivity(); // issue #16: re-arm the idle-animation timer
  }
}
