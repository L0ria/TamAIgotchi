// Recording buffer + transcription + LLM text generation (extracted from
// TamAIgotchi.ino as step 3 of the refactoring proposed in issue #18).
//
// Owns the preallocated PSRAM recording buffer (44-byte WAV header + PCM)
// and the SENDING/RESPONSE flow:
//   initRecBuffer()  - allocate the buffer once (PSRAM) + write the WAV header
//   sendRecording()  - patch header + transcribe + LLM call (blocking)
//   textGeneration() - the LLM call + store the reply into the response table
//
// The `chat` / `audio` OpenAI clients, the scrollable-response table
// (respLines[] / respLineCount / scrollOffset), renderResponseWindow() and
// markAlienActivity() are declared in TamAIgotchi.ino and referenced via
// extern (see recorder.cpp). The shared `display` object is used by the
// text_utils helpers these functions call.
#pragma once
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t
#include "config.h"  // MAX_REC_SECONDS, REC_SAFETY_MARGIN_KB, RESPONSE_*

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)

// App state machine (issue #9 + #13). Owned by the sketch (TamAIgotchi.ino)
// and shared with the recorder via extern; declared here so both TUs agree.
enum RecState { IDLE, RECORDING, SENDING, RESPONSE };

class Recorder {
 public:
  // Allocate the recording buffer once (PSRAM) + write the 44-byte PCM WAV
  // header with placeholder sizes. Returns true on success; on failure the
  // error is shown on the display (no fallback to the old fixed 5 s take).
  bool initRecBuffer();
  bool bufferAllocated() const;  // rec_buf != NULL

  // SENDING state: patch the header, transcribe the take, run the LLM call.
  // Returns true if a transcription was produced (false = error already shown).
  bool sendRecording();

  // The LLM call: store the reply in the response table + switch to RESPONSE.
  void textGeneration(const String& prompt);

  // The preallocated buffer + take position are read/written by the RECORDING
  // branch of loop() (I2S streaming + stop conditions), so they stay public
  // (a faithful move of the old globals).
  uint8_t *rec_buf = NULL;
  size_t rec_buf_bytes = 0;   // PCM capacity in bytes (without the 44-byte header)
  size_t rec_pos = 0;         // bytes of PCM recorded in the current take
  unsigned long rec_start = 0;

 private:
  // Fill in the two size fields of the preallocated WAV header for the
  // current take (both little-endian uint32_t).
  void patchWavHeader(size_t pcm_bytes);
};
