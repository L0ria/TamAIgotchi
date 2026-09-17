// User-facing display strings (issue #36, step 6 of 6 of the UI restructure
// in #29): every string the user can see on the OLED, in one place, so all
// on-screen text can be reviewed here. config.h keeps only numbers / pins /
// timings.
//
// Status-bar strings must fit STATUS_CHARS_PER_LINE chars x 2 lines
// (21 x 2 at font size 1) - the 21-char status rule (issue #29, pitfall 10):
// longer text goes to the bubble or Serial, never the SSD1306 driver
// clipping. All the constants below already fit (the three that used to
// overflow - "Initializing I2S bus...", "Failed to initialize I2S bus!",
// "Record buffer alloc failed" - were shortened when the status bar was
// introduced in step 2, issue #32).
//
// The dynamic parts of the mixed lines (the SSIDs / IPs / counters) are
// concatenated at the call sites; only the literal parts live here.
#pragma once

// --- WiFi / boot -----------------------------------------------------------
#define MSG_WIFI_CONNECTING   "Connecting to WiFi..."   // 21 chars (setup + connecting state)
#define MSG_WIFI_PREFIX       "WiFi: "                  // + SSID (truncated to 16 chars at the call site)
#define MSG_IP_PREFIX         "IP: "                    // + the assigned IP
#define MSG_AP_IP_PREFIX      "192.168.4.1:"            // + WIFI_SETUP_PORT (AP mode setup address)
#define MSG_WIFI_RESET        "WiFi settings reset."    // 5 s hold, GPIO9
#define MSG_WIFI_REBOOT       "Rebooting to setup..."

// --- I2S bring-up (hardwareInit) -------------------------------------------
#define MSG_I2S_INIT          "Initializing I2S..."     // 18 chars (was 25 - overflowed before step 2)
#define MSG_I2S_READY         "I2S bus initialized."
#define MSG_I2S_FAIL          "I2S init failed"         // error title
#define MSG_REBOOT_DEVICE     "Reboot the device."      // error detail (I2S / rec-buffer failures)

// --- Recording --------------------------------------------------------------
#define MSG_RECORDING         "Recording (max 10 s)"    // 20 chars, status line 1
#define MSG_REC_BUF_FAIL      "Rec buf alloc failed"    // error title
#define MSG_NOT_ENOUGH_MEM    "Not enough memory"       // error detail
#define MSG_REC_BUF_MALLOC_FAIL "rec buf malloc failed" // error detail

// --- Send flow (transcription + LLM) ----------------------------------------
#define MSG_SENDING_AUDIO     "Sending audio..."
#define MSG_SENDING_PROMPT    "Sending prompt..."
#define MSG_TRANSCRIBE_FAIL   "Transcription failed"    // error title
#define MSG_CHECK_URL         "Check LOCALAI_URL"       // error detail
#define MSG_LLM_ERROR         "LLM error"               // error title
#define MSG_EMPTY_RESPONSE    "Empty response"          // error title
#define MSG_CHECK_MODEL       "Check model settings"    // error detail

// --- Response view ------------------------------------------------------------
#define MSG_RESPONSE_PREFIX   "Response "               // + "<first visible line>/<total lines>"

// --- Idle animation -----------------------------------------------------------
#define MSG_ALIEN_BUBBLE      "hello"                   // the idle alien's speech bubble (was ALIEN_BUBBLE_TEXT in config.h)
