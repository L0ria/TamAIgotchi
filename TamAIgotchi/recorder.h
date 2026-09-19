// Recording buffer + transcription + LLM text generation (extracted from
// TamAIgotchi.ino as step 3 of the refactoring proposed in issue #18;
// encapsulated in step 6 of 11 of the refactoring plan in #42, issue #49).
//
// Owns the preallocated PSRAM recording buffer (44-byte WAV header + PCM)
// and the SENDING/RESPONSE flow:
//   initRecBuffer()  - allocate the buffer once (PSRAM) + write the WAV header
//   beginStreaming() - start a take: reset the position + start time
//   pcmDestination() - the I2S read target for the next chunk
//   noteChunk(n)     - advance the position after a chunk was read
//   sendRecording()  - patch header + transcribe + LLM call (blocking)
//   textGeneration() - the LLM call + store the reply into the response table
//
// The buffer state (rec_buf / rec_buf_bytes / rec_pos / rec_start) is
// PRIVATE (issue #49): loop() and display.cpp no longer touch it - they
// use the streaming API above instead.
//
// The `chat` / `audio` OpenAI clients are constructor-injected references
// (issue #52, step 9 of 11 of the refactoring plan in #42: the externs in
// recorder.cpp are gone - the same pattern as the Display class, issue #51,
// step 8). alien.markActivity() is the shared `alien` object (TamAIgotchi.ino,
// referenced via extern - the shared-object pattern). Status + bubble
// rendering goes through statusbar.{h,cpp} (statusBar.show / statusBar.error,
// issue #46, step 3) and bubble.{h,cpp} (bubble.setText/bubble.render - the
// prompt text, issue #33, and the response text, issue #34, step 4), which
// use the shared panel (hw.panel(), issue #52, step 9).
#pragma once
#include <cstddef>  // size_t
#include <cstdint>  // uint8_t
#include "config.h"  // MAX_REC_SECONDS, REC_SAFETY_MARGIN_KB

class String;  // forward declaration (complete type via <Arduino.h> in the .cpp)
class OpenAI_ChatCompletion;       // forward declaration (complete type via <OpenAI.h> in the .cpp)
class OpenAI_AudioTranscription;   // forward declaration (complete type via <OpenAI.h> in the .cpp)

// App state machine (issue #9 + #13). Owned by the sketch (TamAIgotchi.ino)
// and shared with the recorder via extern; declared here so both TUs agree.
enum RecState { IDLE, RECORDING, SENDING, RESPONSE };

class Recorder {
 public:
  // issue #52, step 9 of 11 of the refactoring plan in #42: the OpenAI
  // clients are constructor-injected references (the `extern
  // OpenAI_ChatCompletion` / `extern OpenAI_AudioTranscription` in
  // recorder.cpp are gone - the same pattern as the Display class,
  // issue #51, step 8). The clients are sketch-lifetime (members of the
  // shared `hw` object, hardware.cpp), so the references are valid for
  // the whole program. Defined in recorder.cpp, where the types are
  // complete.
  Recorder(OpenAI_ChatCompletion& chat, OpenAI_AudioTranscription& audio);

  // Allocate the recording buffer once (PSRAM) + write the 44-byte PCM WAV
  // header with placeholder sizes. Returns true on success; on failure the
  // error is shown on the display (no fallback to the old fixed 5 s take).
  bool initRecBuffer();
  bool bufferAllocated() const;  // rec_buf != NULL

  // --- take streaming (the RECORDING branch of loop()) ---------------------
  // The buffer + take state below is private (issue #49); these are the
  // only ways loop() / display.cpp touch it.

  // Start a new take: reset the recorded position to 0 and stamp the start
  // time (millis()). What Display::startRecording() (display.cpp) used to do with
  // `rec_pos = 0; rec_start = millis();` - the WiFi/buffer checks stay in
  // Display::startRecording().
  void beginStreaming();

  // The I2S read target for the next chunk: rec_buf + 44 + rec_pos (the
  // 44-byte WAV header is skipped; the I2S object stays in the sketch
  // until step 9, so the class does not take an i2s dependency).
  uint8_t* pcmDestination();

  // Advance the recorded position after `n` bytes were read into
  // pcmDestination(). What loop() used to do with `rec_pos += n;`.
  void noteChunk(size_t n);

  // Stop conditions + status (read-only views of the take state):
  bool isBufferFull() const;    // rec_pos >= rec_buf_bytes
  unsigned long elapsedMs() const;  // millis() - rec_start
  size_t recordedBytes() const;    // rec_pos (the debug prints + sendRecording())

  // --- SENDING / RESPONSE flow ---------------------------------------------

  // SENDING state: patch the header, transcribe the take, run the LLM call.
  // Returns true if a transcription was produced (false = error already shown).
  bool sendRecording();

  // The LLM call: store the reply in the response table + switch to RESPONSE.
  void textGeneration(const String& prompt);

 private:
  // The OpenAI clients (constructor-injected references, issue #52, step 9
  // of 11 of the refactoring plan in #42 - the externs in recorder.cpp are
  // gone).
  OpenAI_ChatCompletion& chat_;
  OpenAI_AudioTranscription& audio_;

  // The preallocated buffer + take state (private since issue #49; the
  // streaming API above is the only access from outside this class).
  uint8_t *rec_buf = NULL;
  size_t rec_buf_bytes = 0;   // PCM capacity in bytes (without the 44-byte header)
  size_t rec_pos = 0;         // bytes of PCM recorded in the current take
  unsigned long rec_start = 0;

  // Fill in the two size fields of the preallocated WAV header for the
  // current take (both little-endian uint32_t).
  void patchWavHeader(size_t pcm_bytes);
};
