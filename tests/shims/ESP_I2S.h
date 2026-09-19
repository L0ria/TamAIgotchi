// Host-side shim of the ESP_I2S library (I2SClass) for the unit tests
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// hardware.cpp (the Hardware class, compiled for the host build since
// step 9) holds an I2SClass member and calls setPins() / begin() /
// readBytes() from Hardware::init() + loop(). On the host build the
// I2S bus is NOT emulated - the methods are no-ops (the tests exercise
// the class wiring, not the audio path; Hardware::init() is never called
// from the tests, the bring-up is device-only).
#pragma once

#include <Arduino.h>  // size_t

enum {
  I2S_NUM_AUTO = 0,
  I2S_NUM_0,
  I2S_NUM_1
};

enum {
  I2S_MODE_STD,
  I2S_MODE_PDM
};

enum {
  I2S_DATA_BIT_WIDTH_8BIT,
  I2S_DATA_BIT_WIDTH_16BIT,
  I2S_DATA_BIT_WIDTH_24BIT,
  I2S_DATA_BIT_WIDTH_32BIT
};

enum {
  I2S_SLOT_MODE_MONO,
  I2S_SLOT_MODE_STEREO
};

enum {
  I2S_STD_SLOT_LEFT = 1,
  I2S_STD_SLOT_RIGHT = 2,
  I2S_STD_SLOT_BOTH = 3
};

class I2SClass {
 public:
  explicit I2SClass(int port = 0) { (void)port; }

  void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t din = -1, int8_t mclk = -1) {
    (void)bclk; (void)ws; (void)dout; (void)din; (void)mclk;
  }

  bool begin(int mode, unsigned long rate, int bits, int slot_mode, int slot_mask = -1) {
    (void)mode; (void)rate; (void)bits; (void)slot_mode; (void)slot_mask;
    return true;  // the host build never fails the bring-up
  }

  size_t readBytes(char* buffer, size_t size) { (void)buffer; return size; }
};
