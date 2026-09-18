#!/usr/bin/env bash
# Host test runner for the TamAIgotchi firmware modules (issue #44, step 1
# of 11 of the refactoring plan in #42).
#
# Compiles the pure/deterministic modules with g++ (no ESP32 toolchain)
# against the Arduino shims in tests/shims/, links them with the test
# harness, and runs the resulting binary. Non-zero exit on any failure.
#
# Usage: bash tests/run_tests.sh
#
# Later steps (2-10) add their modules to MODULES and their test files to
# TESTS below. The shim set in tests/shims/ grows the same way (e.g.
# ESPWifiConfig.h in step 7, OpenAI.h / ESP_I2S.h / WiFi.h in step 10).
set -euo pipefail

cd "$(dirname "$0")/.."   # repo root

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -I tests/shims -I TamAIgotchi"
OUT=tests/build/test_bin
mkdir -p tests/build

# Modules under test (the firmware .cpp files, compiled for the host).
MODULES=(
  TamAIgotchi/text_utils.cpp
  TamAIgotchi/buttons.cpp
  TamAIgotchi/bubble.cpp
  TamAIgotchi/statusbar.cpp
)

# Test files (one per module, plus the harness).
TESTS=(
  tests/test_main.cpp
  tests/test_text_utils.cpp
  tests/test_buttons.cpp
  tests/test_bubble.cpp
  tests/test_statusbar.cpp
)

echo "[run_tests] compiling: ${MODULES[*]}"
echo "[run_tests] tests:     ${TESTS[*]}"
"$CXX" $CXXFLAGS "${TESTS[@]}" "${MODULES[@]}" -o "$OUT"

echo "[run_tests] running:   $OUT"
"$OUT"
