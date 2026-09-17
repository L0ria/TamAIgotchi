// Recording buffer + transcription + LLM text generation (extracted from
// TamAIgotchi.ino as step 3 of the refactoring proposed in issue #18).
#include "recorder.h"
#include "alien.h"   // AlienAnimation (markActivity on response ready)
#include <Arduino.h>  // String, Serial, F(), memcpy
#include <esp_heap_caps.h>  // heap_caps_malloc / heap_caps_get_free_size (PSRAM recording buffer)
#include <OpenAI.h>  // OpenAI_ChatCompletion / OpenAI_AudioTranscription / OpenAI_StringResponse / log_d
#include "statusbar.h"   // statusShow() / statusError() (issue #33, step 3)
#include "bubble.h"      // bubbleSetText() (prompt in the bubble, issue #33)
#include "messages.h"    // MSG_* user-facing display strings (issue #36, step 6)
#include "display.h"     // renderScreen() (issue #35, step 5: the single pass)

// Shared objects + helpers declared in TamAIgotchi.ino (the sketch entry
// point); referenced here instead of passed through every call.
extern RecState recState;
extern AlienAnimation alien;  // markActivity() on response ready
extern OpenAI_ChatCompletion chat;
extern OpenAI_AudioTranscription audio;

bool Recorder::bufferAllocated() const {
  return rec_buf != NULL;
}

// Allocate the recording buffer once (PSRAM) and write the 44-byte PCM WAV
// header with placeholder sizes (patched per take by patchWavHeader()).
// The buffer is reused by every recording and never freed.
// Returns true on success; on failure the error is shown on the display
// (no fallback to the old fixed 5 s recording).
bool Recorder::initRecBuffer() {
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
    // Status bar (issue #33, step 3 of the UI restructure in #29): the
    // full detail goes to Serial (the status bar fits 21 chars/line).
    Serial.println(F("Record buffer alloc failed: Not enough free memory for the recording buffer."));
    statusError(MSG_REC_BUF_FAIL, MSG_NOT_ENOUGH_MEM);
    return false;
  }

  rec_buf = (uint8_t *)heap_caps_malloc(cap + 44,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (rec_buf == NULL && !have_psram) {
    rec_buf = (uint8_t *)malloc(cap + 44);
  }
  if (rec_buf == NULL) {
    // Status bar (issue #33): full detail to Serial (21 chars/line limit).
    Serial.println(F("Record buffer alloc failed: heap_caps_malloc failed for the recording buffer."));
    statusError(MSG_REC_BUF_FAIL, MSG_REC_BUF_MALLOC_FAIL);
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
void Recorder::patchWavHeader(size_t pcm_bytes) {
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
bool Recorder::sendRecording() {
  patchWavHeader(rec_pos);
  D_TD(F("sending "));
  D_TDDEC(rec_pos);
  D_TDLN(F(" bytes of PCM ("));
  D_TDDEC((unsigned)(rec_pos / 65536));
  D_TDLN(F(" s of audio)"));

  // Status bar (issue #33, step 3 of the UI restructure in #29): the
  // "Sending audio" screen (full clear + 1 line) becomes a status line;
  // the byte count already went to Serial above.
  statusShow(MSG_SENDING_AUDIO);
  String transcription = audio.file(rec_buf, 44 + rec_pos, OPENAI_AUDIO_INPUT_FORMAT_WAV);
  log_d(transcription);
  D_TD(F("transcription length: "));
  D_TDLN(transcription.length());

  // The library swallows HTTP failures (unreachable host, server error,
  // model not installed) and just returns an empty string. Make that
  // visible instead of sending an empty prompt to the LLM.
  transcription.trim();
  if (transcription.length() == 0) {
    // Status bar (issue #33): 2-line form; the full detail goes to Serial
    // (the status bar fits 21 chars/line).
    Serial.println(F("Transcription failed: LocalAI unreachable or returned an error. Check LOCALAI_URL in the setup page (Custom tab)."));
    statusError(MSG_TRANSCRIBE_FAIL, MSG_CHECK_URL);
    return false;
  }

  textGeneration(transcription);
  return true;
}

void Recorder::textGeneration(const String& prompt) {
  D_TD(F("prompt length: "));
  D_TDLN(prompt.length());
  // Status bar (issue #33, step 3 of the UI restructure in #29): the
  // "Sending prompt" line becomes a status line, and the prompt text
  // (content, #29 section 4 row 10) goes into the speech bubble (step 1
  // module). renderScreen() pushes the full frame (issue #35, step 5).
  statusShow(MSG_SENDING_PROMPT);
  bubbleSetText(prompt);
  renderScreen();

  OpenAI_StringResponse result = chat.message(prompt);
  Serial.printf("Received message. Tokens: %u\n", result.tokens());
  D_TD(F("response length: "));
  D_TDLN(String(result.getAt(0)).length());

  // Check the error FIRST: on failure the library returns an empty
  // response plus the server's error text (e.g. "The model 'gpt-4' does
  // not exist"), which we must not swallow into a blank display.
  if (result.error()) {
    // Status bar (issue #33): statusError() truncates the detail to 21
    // chars on screen; the full server text always goes to Serial.
    statusError(MSG_LLM_ERROR, String(result.error()));
    return;
  }

  String response = result.getAt(0);
  response.trim();
  response.replace("\n", " ");
  log_d(response);

  if (response.length() == 0) {
    // HTTP 200 but no content (e.g. an unexpected response shape).
    // Status bar (issue #33): 2-line form; full detail to Serial.
    Serial.println(F("Empty response: LocalAI returned no text. Check the model and its settings."));
    statusError(MSG_EMPTY_RESPONSE, MSG_CHECK_MODEL);
    return;
  }

  // Store the reply in the speech bubble and switch to the RESPONSE state
  // (issue #34, step 4 of 6 of the UI restructure in #29 - the bubble
  // owns the text table from now on; the old response table + window
  // renderer are gone). The response is content, so it
  // lives in the bubble; then the "Response 1/N" counter goes on status
  // line 1 (issue #29 Q6). One full frame through renderScreen()
  // (issue #35, step 5).
  bubbleSetText(response);
  renderScreen();
  statusShow(MSG_RESPONSE_PREFIX "1/" + String(bubbleLineCount()));
  recState = RESPONSE;
  // Issue #16: re-arm the inactivity timer so the animation returns
  // ALIEN_RESPONSE_TIMEOUT_MS after the response has been shown without a
  // button press (driven from loop() via alien.markActivity()).
  alien.markActivity();
  D_TDLN(F("response ready (scroll: GPIO9 down / GPIO11 up, hold GPIO11 5 s to exit)"));
}
