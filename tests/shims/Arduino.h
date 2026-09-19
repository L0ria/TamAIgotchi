// Host-side Arduino API shim for the unit tests (issue #44, step 1 of 11
// of the refactoring plan in #42).
//
// The firmware modules under test do `#include <Arduino.h>`; on the host
// build this file provides just enough of the Arduino API for them to
// compile and run under g++ (no ESP32 toolchain):
//
//   - String   (small class over std::string)
//   - millis() (deterministic: tests drive it via host_set_millis())
//   - digitalRead() / digitalWrite() (backed by the host_pin_level[] table)
//   - Serial   (print/println -> stdout AND a capture buffer tests can assert on)
//   - F(x), HIGH, LOW, PROGMEM
//
// It is deliberately NOT a general Arduino emulation: only what the modules
// listed in run_tests.sh actually use is provided. Later steps (2-10) add
// shims for the other libraries they need (ESPWifiConfig.h, OpenAI.h,
// ESP_I2S.h, WiFi.h, ...) to tests/shims/.
//
// NOTE: the file must stay named `Arduino.h` with a capital A - the modules
// include it as <Arduino.h> and Linux is case-sensitive.
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <string>

// ---------------------------------------------------------------------------
// Constants + string literal macro
// ---------------------------------------------------------------------------
#define HIGH 1
#define LOW  0
// Pin modes (hardware.cpp's pinMode() calls; the host shim ignores them).
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define PROGMEM
#define F(x) x

// ---------------------------------------------------------------------------
// Deterministic time: millis() reads host_now_ms; tests advance it with
// host_set_millis() so the Button debounce / long-press logic is testable.
// (Defined in tests/test_main.cpp, the harness translation unit.)
// ---------------------------------------------------------------------------
extern unsigned long host_now_ms;
inline unsigned long millis() { return host_now_ms; }
inline void host_set_millis(unsigned long ms) { host_now_ms = ms; }

// ---------------------------------------------------------------------------
// Deterministic GPIO: digitalRead() / digitalWrite() read/write a pin table
// that defaults to HIGH (released / pull-up). Tests drive a pin with
// host_set_pin(). (Defined in tests/test_main.cpp.)
// ---------------------------------------------------------------------------
extern int host_pin_level[64];
inline int digitalRead(int pin) {
  return (pin >= 0 && pin < 64) ? host_pin_level[pin] : HIGH;
}
inline void digitalWrite(int pin, int level) {
  if (pin >= 0 && pin < 64) host_pin_level[pin] = level;
}
inline void pinMode(int pin, int mode) { (void)pin; (void)mode; }  // no-op on host
// Test hook: drive a pin to a level (defined in tests/test_main.cpp).
void host_set_pin(int pin, int level);

// delay(): the firmware uses it in Display::resetWifiSettingsAndRestart()
// (a 300 ms settle before the reboot). On the host it is a no-op (the
// reboot itself is counted by the ESP shim's restarted() hook).
inline void delay(unsigned long ms) { (void)ms; }

// ---------------------------------------------------------------------------
// String: the subset of the Arduino String API the firmware modules use.
// Arduino semantics kept where they differ from std::string, notably
// substring(from, to) where `to` is INCLUSIVE.
// ---------------------------------------------------------------------------
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String(const String&) = default;
  // Arduino: String(int) / String(unsigned) / String(size_t) - the decimal
  // representation (used by String(bubble.lineCount()), String(recSeconds),
  // String(result.tokens()), ...).
  String(int v) { s_ = std::to_string(v); }
  String(unsigned v) { s_ = std::to_string(v); }
  String(long v) { s_ = std::to_string(v); }
  String(unsigned long v) { s_ = std::to_string(v); }  // covers size_t on 64-bit
  String& operator=(const String&) = default;
  String& operator=(const char* s) { s_ = s ? s : ""; return *this; }

  bool empty() const { return s_.empty(); }
  size_t length() const { return s_.size(); }
  char charAt(size_t i) const { return (i < s_.size()) ? s_[i] : '\0'; }
  // Arduino: String::getAt(i) - the single char at index i (empty past end).
  String getAt(size_t i) const {
    if (i >= s_.size()) return String();
    char c = s_[i];
    return String(&c);
  }
  const char* c_str() const { return s_.c_str(); }
  // Arduino: String::toString() - a copy of this string.
  String toString() const { return *this; }

  // Arduino: substring(from, to) - `to` is EXCLUSIVE (the ESP32 core
  // copies `to - from` chars: WString.cpp substring() -> copy(left,
  // right - left)), clamped to the string length.
  String substring(size_t from, size_t to) const {
    if (from >= s_.size()) return String();
    size_t last = (to > s_.size()) ? s_.size() : to;
    if (last <= from) return String();
    return String(s_.substr(from, last - from).c_str());
  }

  // Arduino: toCharArray(buf, len) copies at most len-1 chars + NUL.
  void toCharArray(char* buf, size_t len) const {
    if (!buf || len == 0) return;
    size_t n = (s_.size() < len - 1) ? s_.size() : len - 1;
    s_.copy(buf, n, 0);
    buf[n] = '\0';
  }

  String& operator+=(char c) { s_ += c; return *this; }
  String& operator+=(const char* s) { if (s) s_ += s; return *this; }
  String& operator+=(const String& s) { s_ += s.s_; return *this; }

  String operator+(const String& o) const { return String((s_ + o.s_).c_str()); }
  String operator+(const char* o) const { return String((s_ + (o ? o : "")).c_str()); }
  String operator+(char o) const { std::string r = s_; r += o; return String(r.c_str()); }

  String trim() const {
    size_t b = s_.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return String();
    size_t e = s_.find_last_not_of(" \t\r\n");
    return String(s_.substr(b, e - b + 1).c_str());
  }

  String& replace(const String& target, const String& replacement) {
    if (target.empty()) return *this;
    std::string out;
    size_t offset = 0;
    while (offset < s_.size()) {
      size_t pos = s_.find(target.s_, offset);
      if (pos == std::string::npos) { out += s_.substr(offset); break; }
      out.append(s_, offset, pos - offset);
      out += replacement.s_;
      offset = pos + target.s_.size();
    }
    s_ = out;
    return *this;
  }
  String& replace(const char* target, const char* replacement) {
    return replace(String(target), String(replacement));
  }

  bool equals(const String& o) const { return s_ == o.s_; }
  bool equals(const char* o) const { return s_ == (o ? o : ""); }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == (o ? o : ""); }
  bool operator!=(const String& o) const { return !(*this == o); }
  bool operator!=(const char* o) const { return !(*this == o); }

 private:
  std::string s_;
};

// ---------------------------------------------------------------------------
// Serial: print/println go to stdout (so a human running the tests sees the
// same trace as the firmware) AND into a capture buffer tests can assert on.
// (Defined in tests/test_main.cpp.)
// ---------------------------------------------------------------------------
class SerialClass {
 public:
  void begin(unsigned long baud) { (void)baud; }
  void end() {}

  size_t print(const char* s) { return write(s ? s : ""); }
  size_t print(const String& s) { return write(s.c_str()); }
  size_t print(char c) { return write(std::string(1, c)); }
  size_t print(int v) { char b[24]; std::snprintf(b, sizeof b, "%d", v); return write(b); }
  size_t print(unsigned long v) { char b[24]; std::snprintf(b, sizeof b, "%lu", v); return write(b); }

  size_t println() { return write("\n"); }
  size_t println(const char* s) { return print(s) + write("\n"); }
  size_t println(const String& s) { return print(s) + write("\n"); }
  size_t println(char c) { return print(c) + write("\n"); }
  size_t println(int v) { return print(v) + write("\n"); }
  size_t println(unsigned long v) { char b[24]; std::snprintf(b, sizeof b, "%lu", v); return write(b) + write("\n"); }

  // printf: format + (up to 3) args, mirrored to the capture buffer. The
  // firmware uses it for the token-count log line.
  size_t printf(const char* fmt, ...) {
    char b[128];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    return write(n < (int)sizeof b ? b : "");
  }

  // Test hooks
  void clearCapture() { captured_.clear(); }
  const std::string& captured() const { return captured_; }

 private:
  size_t write(const std::string& s) {
    captured_ += s;
    std::fputs(s.c_str(), stdout);
    return s.size();
  }
  std::string captured_;
};
// Free operator+ for (const char* + String): the "Response 1/" counter builds
// a string-literal + String(n). (String + const char* is a member.)
inline String operator+(const char* a, const String& b) {
  return String((std::string(a ? a : "") + b.c_str()).c_str());
}

extern SerialClass Serial;

// ---------------------------------------------------------------------------
// ESP: the subset recorder.cpp / display.cpp use (esp_get_free_heap_size in
// the PSRAM-less fallback of initRecBuffer(), ESP.restart() in the reset
// escape hatch). host_set_free_heap() drives the reported free heap.
// (Defined in tests/test_main.cpp.)
// ---------------------------------------------------------------------------
extern size_t host_free_heap_bytes;
inline void host_set_free_heap(size_t bytes) { host_free_heap_bytes = bytes; }
inline size_t esp_get_free_heap_size() { return host_free_heap_bytes; }

class EspClass {
 public:
  void restart() { restarted_++; }
  int restarted() const { return restarted_; }  // test hook
 private:
  int restarted_ = 0;
};
extern EspClass ESP;
