// Host-side shim of the Arduino TwoWire (Wire) API for the unit tests
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// hardware.cpp (the Hardware class, compiled for the host build since
// step 9) references `&Wire` when constructing the SSD1306 panel member.
// The real driver takes a TwoWire* and only uses it in begin(); the host
// build never calls begin() (the bring-up is device-only), so this stub
// just needs to exist and be addressable.
#pragma once

class TwoWire {
 public:
  TwoWire() = default;
  void begin() {}
  void begin(int address) { (void)address; }
};

extern TwoWire Wire;
