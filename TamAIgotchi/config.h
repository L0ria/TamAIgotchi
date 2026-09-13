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

#define WIFI_CONFIG_BUTTON_PIN 9   // Hold 5 s → reset saved WiFi settings & start the setup AP (Button → GND, INPUT_PULLUP)
#define RESERVE_BUTTON_PIN 11      // Reserved, unused for now (Button → GND, INPUT_PULLUP)
