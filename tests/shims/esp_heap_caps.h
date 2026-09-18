// Host-side shim of esp_heap_caps.h for the unit tests (issue #49, step 6
// of 11 of the refactoring plan in #42).
//
// recorder.cpp includes <esp_heap_caps.h> for the PSRAM buffer allocation
// (heap_caps_malloc / heap_caps_get_free_size). On the host build this
// provides the two functions backed by the plain C heap (malloc / free),
// plus a test hook to control the reported free size so the
// initRecBuffer() sizing logic is testable.
#pragma once

#include <cstddef>
#include <cstdlib>

// Free-bytes report that initRecBuffer() reads. Tests set it with
// host_set_heap_free(). Defaults to 0 (no free memory -> cap = 0, the
// allocation-failure path), so a test that wants a successful allocation
// must set it explicitly.
extern size_t host_heap_free_bytes;
inline size_t host_heap_free_bytes_get() { return host_heap_free_bytes; }
inline void host_set_heap_free(size_t bytes) { host_heap_free_bytes = bytes; }

inline size_t heap_caps_get_free_size(unsigned long caps) {
  (void)caps;
  return host_heap_free_bytes;
}

// The MALLOC_CAP_* flags the firmware passes (the shim ignores them - the
// host heap is the "PSRAM" for test purposes).
#define MALLOC_CAP_SPIRAM 0x01
#define MALLOC_CAP_8BIT   0x02

inline void* heap_caps_malloc(size_t size, unsigned long caps) {
  (void)caps;
  return std::malloc(size);
}

inline void heap_caps_free(void* ptr) { std::free(ptr); }
