// LocalAI endpoint + API key: initial defaults only.
// They are registered as user settings of the ESP-Wifi-Config library
// (LOCALAI_URL / LOCALAI_KEY) and can be changed at runtime on the web
// setup page (Custom tab) - the stored values take precedence over these.
const char* api_url = "http://192\.168\.1\.5:8080/v1/";
const char* api_key = "sk1234567890";

// Base name of the access point broadcast while the device waits to be
// configured. The full AP name is "<name>_<mac>", e.g. "TamAIgotchi_12345678".
#define WIFI_AP_NAME "TamAIgotchi"
// Port of the ESP-Wifi-Config setup web page (http://192.168.1.1:<port>).
#define WIFI_SETUP_PORT 8080

// ---------------------------------------------------------------------------
// Serial debug output: OFF by default.
// Enable by uncommenting the line below (or via a build flag: -DDEBUG) to get
// a detailed serial trace of every step (boot, WiFi mode, I2S, recording,
// transcription, prompt, response, button events). All D_T*() calls compile
// away to nothing when DEBUG is not defined.
//#define DEBUG

#ifdef DEBUG
#define D_TD(x)    Serial.print(F("[DEBUG] ")); Serial.print(x)
#define D_TDDEC(x) Serial.print(F("[DEBUG] ")); Serial.print(x, DEC)
#define D_TDLN(x)  Serial.print(F("[DEBUG] ")); Serial.println(x)
#else
#define D_TD(x)
#define D_TDDEC(x)
#define D_TDLN(x)
#endif

// ---------------------------------------------------------------------------
// Hold-to-record (issue #9): the button is held for as long as audio is
// recorded; releasing it sends the recording to LocalAI for transcription.
// The recording buffer is preallocated once in setup() (PSRAM) and reused
// for every recording. These constants are static: the HW configuration is
// fixed (16 kHz / 32-bit / mono = 65536 bytes per second of audio).
//
//   MAX_REC_SECONDS  - hard cap on the recording length. 10 s = 640 KB,
//                      comfortably inside the LocalAI 20 s socket timeout
//                      and any whisper model's comfort zone. The display
//                      shows "max 10 s" while recording.
//   REC_SAFETY_MARGIN_KB - free PSRAM to keep clear (for the HTTP upload
//                      buffer in LocalAI-ESP32) when sizing the buffer.
//   REC_CHUNK_BYTES  - bytes read per i2s.readBytes() call (~97 ms of audio).
//                      Bounding each read keeps loop() responsive and makes
//                      the button-release latency at most one chunk (~100 ms).
#define MAX_REC_SECONDS 10
#define REC_SAFETY_MARGIN_KB 64
#define REC_CHUNK_BYTES 1600

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define I2S_DIN     5     // DATA IN
#define I2S_WS      18     // LRCLK
#define I2S_SCK     16     // BCLK

#define BUTTON_PIN  3     // Button → GND (INPUT_PULLUP)
#define LED_PIN     12

// The two side buttons (issue #13): a short press scrolls the response
// view, a 5 s hold is an escape hatch. Both are wired to GND (INPUT_PULLUP).
#define WIFI_CONFIG_BUTTON_PIN 9   // short: scroll response down · hold 5 s → reset saved WiFi settings & start the setup AP (existing behavior)
#define SCROLL_DOWN_PIN  9   // same physical button as WIFI_CONFIG_BUTTON_PIN (alias, issue #13)
#define SCROLL_UP_PIN 11     // short: scroll response up (was RESERVE_BUTTON_PIN) · hold 5 s → exit the response view

// Scrollable response view (issue #13): the LLM reply is word-wrapped into
// a static line table and shown as a RESPONSE_VISIBLE_LINES window below a
// "Response: x/y" header (x = first visible line, y = total lines).
// 16 lines x 22 bytes ≈ 350 B of static RAM — a generous upper bound for a
// 40-token answer (~330 chars at most).
#define RESPONSE_CHARS_PER_LINE 21  // 128 px / 6 px per char (font size 1)
#define RESPONSE_VISIBLE_LINES 6   // 8 display lines: 1 header + 1 blank separator + 6 response lines
#define RESPONSE_MAX_LINES 16      // capacity of the static line table

// Button timing (all buttons): 50 ms debounce, 5 s long-press threshold.
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 5000

// ---------------------------------------------------------------------------
// Idle animation (issue #16): a small pixel-art alien (24x22 px, 4 hand-
// drawn frames in alien.h, ~264 B of flash, 0 B of RAM) entertains the
// screen after ALIEN_IDLE_TIMEOUT_MS without any button press. It stays in
// the lower-left quadrant (x < 64, y >= 32, well under a quarter of the
// screen). Loop (70 s cycle): bubble 4 s -> jump & wave 16 s -> bubble 4 s
// -> jump & wave 16 s -> stand still 30 s. Any button press stops it.
//   ALIEN_IDLE_TIMEOUT_MS     - start the animation after this much inactivity
//   ALIEN_RESPONSE_TIMEOUT_MS - after the response has been shown this long
//                               (and no button press), go back to the animation
//   ALIEN_BUBBLE_MS           - duration of the speech-bubble phase ("hello")
//   ALIEN_WAVE_MS             - duration of the jump & wave phase
//   ALIEN_STAND_MS            - duration of the standing-still phase
//   ALIEN_FRAME_MS            - sprite frame swap interval during the wave phase
//   ALIEN_BUBBLE_TEXT         - text shown in the speech bubble (configurable)
#define ALIEN_IDLE_TIMEOUT_MS     60000UL
#define ALIEN_RESPONSE_TIMEOUT_MS 60000UL
#define ALIEN_BUBBLE_MS           4000UL
#define ALIEN_WAVE_MS             16000UL
#define ALIEN_STAND_MS            30000UL
#define ALIEN_FRAME_MS            250UL
#define ALIEN_BUBBLE_TEXT "hello"
