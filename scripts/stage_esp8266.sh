#!/usr/bin/env bash
# Assembles the ESP8266 sketch in one folder: the sketch itself plus the plain-C++ sources it shares with the ESP32
# build (arduino-cli compiles a copy of the sketch folder, so files elsewhere in the repo cannot be included by path).
#
#   scripts/stage_esp8266.sh [out_dir]      default: build/esp8266-sketch/skywatch_esp8266
#
# Build and flash from the staged folder:
#   arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 build/esp8266-sketch/skywatch_esp8266
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-build/esp8266-sketch/skywatch_esp8266}"
SHARED=(flights.cpp flights.h config.cpp config.h airline.cpp airline.h livery.h timing_util.h version.h version_util.h)
rm -rf "$OUT" && mkdir -p "$OUT"
cp firmware/skywatch_esp8266/skywatch_esp8266.ino "$OUT/"
for f in "${SHARED[@]}"; do cp "firmware/jf_flight_display/$f" "$OUT/$f"; done
echo "$OUT"
