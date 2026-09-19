// Host-side shim of the Adafruit GFX base class for the unit tests
// (issue #52, step 9 of 11 of the refactoring plan in #42).
//
// hardware.h includes <Adafruit_GFX.h> (the real Adafruit_SSD1306 header
// pulls it in as its base class). The host fake (Adafruit_SSD1306.h) does
// NOT inherit from it, so this stub only needs to exist and be includable.
#pragma once
