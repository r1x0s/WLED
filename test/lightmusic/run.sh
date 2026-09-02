#!/usr/bin/env bash
# Host tests for the WLED-LightMusic header-only modules (wled00/lightmusic_*.h).
# Each test is compiled with both C++11 (the ESP32 firmware standard, gnu++11) and C++17,
# with warnings as errors, and executed on the host. No Arduino toolchain required.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out="${LIGHTMUSIC_TEST_OUT:-${TMPDIR:-/tmp}/lightmusic-host-tests}"
mkdir -p "$out"
cxx="${CXX:-g++}"

tests=(ap_config wifi_priority net_utils sync_heartbeat node_registry)
status=0
for t in "${tests[@]}"; do
  for std in c++11 c++17; do
    bin="$out/${t}_${std}"
    if ! "$cxx" -std="$std" -Wall -Wextra -pedantic -Werror "$here/${t}_test.cpp" -o "$bin"; then
      echo "COMPILE FAILED: $t ($std)"; status=1; continue
    fi
    if ! "$bin"; then status=1; fi
  done
done

if [ "$status" -ne 0 ]; then echo "lightmusic host tests: FAILED"; exit 1; fi
echo "lightmusic host tests: all passed"
