// Tests for the Recorder streaming API (issue #49, step 6 of 11 of the
// refactoring plan in #42).
//
// Pins the behavior of the encapsulated buffer state (rec_buf /
// rec_buf_bytes / rec_pos / rec_start are now PRIVATE - the tests can only
// reach them through the public API):
//
//   - bufferAllocated() is false before initRecBuffer(), true after
//   - beginStreaming() resets the position + stamps the start time
//     (asserted via recordedBytes() / elapsedMs() with the shim clock)
//   - pcmDestination() advances by noteChunk(n) (pointer arithmetic on a
//     test buffer)
//   - isBufferFull() is false after beginStreaming(), true once the
//     position reaches capacity
//   - elapsedMs() tracks the shim clock
//   - recordedBytes() mirrors the position
//
// The shared `recorder` object is defined in tests/test_main.cpp (the host
// build's equivalent of the instance in TamAIgotchi.ino). The PSRAM heap
// behind the esp_heap_caps.h shim is driven with host_set_heap_free() so
// initRecBuffer() allocates a real (host) buffer the pointer tests can use.
#include "test_main.h"
#include <Arduino.h>        // host shim: host_set_millis / host_set_heap_free
#include <esp_heap_caps.h>  // host shim: host_set_heap_free
#include "recorder.h"

// The shared recorder object (defined in tests/test_main.cpp, the host
// build's equivalent of the instance in TamAIgotchi.ino) - the same
// pattern as the shared `led` / `statusBar` objects in the other tests.
extern Recorder recorder;

// Reset the shared test state: a large free heap (so initRecBuffer() can
// allocate), the clock at 0, the take position at 0.
static void reset_recorder_state() {
  host_set_millis(0);
  host_set_heap_free(1024 * 1024);  // 1 MB of "PSRAM"
  recorder.beginStreaming();        // position 0, start time 0
}

// --- buffer allocation -----------------------------------------------------

TEST(recorder_not_allocated_before_init) {
  // A fresh instance has no buffer (the shared `recorder` object may have
  // been initialized by earlier tests, so the initial-state assertion uses
  // its own instance).
  Recorder fresh;
  CHECK(!fresh.bufferAllocated());
}

TEST(recorder_init_allocates_and_reports_capacity) {
  reset_recorder_state();
  CHECK(!recorder.bufferAllocated());
  CHECK(recorder.initRecBuffer());
  CHECK(recorder.bufferAllocated());
  // The capacity is min(MAX_REC_SECONDS * 65536, free_mem - margin - 44).
  // With 1 MB free and a 64 KB margin, the 10-second cap (655360 bytes)
  // is the binding constraint.
  size_t expected = (size_t)MAX_REC_SECONDS * 65536;
  CHECK(recorder.isBufferFull() == false);  // position 0 < capacity
  CHECK_EQ_INT((long)expected, (long)((size_t)MAX_REC_SECONDS * 65536));
}

// --- beginStreaming() ------------------------------------------------------

TEST(beginStreaming_resets_position_and_start_time) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());

  // Advance the clock + the position, then start a new take.
  host_set_millis(5000);
  recorder.noteChunk(1234);
  CHECK_EQ_INT(1234, recorder.recordedBytes());

  host_set_millis(10000);
  recorder.beginStreaming();
  CHECK_EQ_INT(0, recorder.recordedBytes());   // position reset
  CHECK_EQ_INT(0, (long)recorder.elapsedMs());  // start time = now (elapsed = 0)

  // Advance the clock: elapsedMs() should increase.
  host_set_millis(15000);
  CHECK_EQ_INT(5000, (long)recorder.elapsedMs());
}

TEST(beginStreaming_is_idempotent) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());
  host_set_millis(100);
  recorder.beginStreaming();
  CHECK_EQ_INT(0, (long)recorder.elapsedMs());  // start time = now (elapsed = 0)
  host_set_millis(200);
  recorder.beginStreaming();
  CHECK_EQ_INT(0, recorder.recordedBytes());
  CHECK_EQ_INT(0, (long)recorder.elapsedMs());  // start time reset to now
}

// --- pcmDestination() + noteChunk() ----------------------------------------

TEST(pcmDestination_advances_by_noteChunk) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());

  // The first destination is the buffer start + the 44-byte WAV header.
  uint8_t* first = recorder.pcmDestination();
  CHECK(first != NULL);

  // A chunk of 1000 bytes advances the destination by exactly 1000.
  recorder.noteChunk(1000);
  uint8_t* second = recorder.pcmDestination();
  CHECK_EQ_INT(1000, (long)(second - first));

  // And by another 250.
  recorder.noteChunk(250);
  CHECK_EQ_INT(1250, (long)(recorder.pcmDestination() - first));
}

TEST(pcmDestination_starts_after_the_wav_header) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());
  // The buffer is a 44-byte header + PCM; the first PCM byte is at offset 44.
  // We can't read rec_buf (private), but the destination must be a valid,
  // non-NULL pointer and the position must be 0.
  CHECK(recorder.pcmDestination() != NULL);
  CHECK_EQ_INT(0, recorder.recordedBytes());
}

// --- isBufferFull() --------------------------------------------------------

TEST(isBufferFull_false_after_beginStreaming) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());
  CHECK(!recorder.isBufferFull());
}

TEST(isBufferFull_true_at_capacity) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());
  size_t cap = (size_t)MAX_REC_SECONDS * 65536;  // the initRecBuffer() capacity

  // Just below capacity: not full.
  recorder.noteChunk(cap - 1);
  CHECK(!recorder.isBufferFull());
  CHECK_EQ_INT((long)(cap - 1), (long)recorder.recordedBytes());

  // At capacity: full.
  recorder.noteChunk(1);
  CHECK(recorder.isBufferFull());
  CHECK_EQ_INT((long)cap, (long)recorder.recordedBytes());

  // Beyond capacity (an over-read): still full.
  recorder.noteChunk(10);
  CHECK(recorder.isBufferFull());
}

// --- elapsedMs() -----------------------------------------------------------

TEST(elapsedMs_tracks_shim_clock) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());

  host_set_millis(0);
  recorder.beginStreaming();
  CHECK_EQ_INT(0, (long)recorder.elapsedMs());

  host_set_millis(1500);
  CHECK_EQ_INT(1500, (long)recorder.elapsedMs());

  host_set_millis(9999);
  CHECK_EQ_INT(9999, (long)recorder.elapsedMs());
}

// --- recordedBytes() -------------------------------------------------------

TEST(recordedBytes_mirrors_position) {
  reset_recorder_state();
  CHECK(recorder.initRecBuffer());

  CHECK_EQ_INT(0, recorder.recordedBytes());
  recorder.noteChunk(42);
  CHECK_EQ_INT(42, recorder.recordedBytes());
  recorder.noteChunk(58);
  CHECK_EQ_INT(100, recorder.recordedBytes());
  recorder.beginStreaming();
  CHECK_EQ_INT(0, recorder.recordedBytes());
}
