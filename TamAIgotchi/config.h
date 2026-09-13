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

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define I2S_DIN     5     // DATA IN
#define I2S_WS      18     // LRCLK
#define I2S_SCK     16     // BCLK

#define BUTTON_PIN  3     // Button → GND (INPUT_PULLUP)
#define LED_PIN     12

#define WIFI_CONFIG_BUTTON_PIN 9   // Hold 5 s → reset saved WiFi settings & start the setup AP (Button → GND, INPUT_PULLUP)
#define RESERVE_BUTTON_PIN 11      // Reserved, unused for now (Button → GND, INPUT_PULLUP)
