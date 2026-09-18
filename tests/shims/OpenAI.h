// Host-side shim of the LocalAI-ESP32 OpenAI library for the unit tests
// (issue #49, step 6 of 11 of the refactoring plan in #42).
//
// recorder.cpp includes <OpenAI.h> for the transcription + LLM call. On
// the host build this provides just enough of the API surface for
// recorder.cpp to compile and link:
//
//   - OpenAI / OpenAI_ChatCompletion / OpenAI_AudioTranscription
//   - OpenAI_StringResponse (tokens / getAt / error)
//   - OPENAI_AUDIO_INPUT_FORMAT_WAV, log_d()
//
// The network behavior is NOT emulated: the methods are no-ops (the tests
// exercise the buffer/streaming API, not the SENDING/RESPONSE flow).
#pragma once

#include <Arduino.h>  // String

#define OPENAI_AUDIO_INPUT_FORMAT_WAV "wav"

// log_d(): the library's debug logger - a no-op on the host.
inline void log_d(const String& s) { (void)s; }

// The LLM response object: tokens() / getAt(i) / error(). (Defined before
// OpenAI_ChatCompletion, which returns it.)
class OpenAI_StringResponse {
 public:
  OpenAI_StringResponse() = default;
  unsigned long tokens() const { return 0; }
  String getAt(size_t i) const { (void)i; return String(); }
  const char* error() const { return nullptr; }
};

class OpenAI {
 public:
  OpenAI(const char* url = "", const char* key = "") { (void)url; (void)key; }
};

// chat.message(prompt) - the LLM call (no-op on the host).
class OpenAI_ChatCompletion {
 public:
  explicit OpenAI_ChatCompletion(OpenAI& o) { (void)o; }
  OpenAI_StringResponse message(const String& prompt) { (void)prompt; return OpenAI_StringResponse(); }
};

// audio.file(buf, len, format) - the transcription upload (no-op on the host).
class OpenAI_AudioTranscription {
 public:
  explicit OpenAI_AudioTranscription(OpenAI& o) { (void)o; }
  String file(const uint8_t* buf, size_t len, const char* format) {
    (void)buf; (void)len; (void)format;
    return String();
  }
};


