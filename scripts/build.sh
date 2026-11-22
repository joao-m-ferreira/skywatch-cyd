#!/usr/bin/env bash
# Build the Go service and compile the firmware for the ESP32.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f firmware/jf_flight_display/secrets.h ] || cp firmware/jf_flight_display/secrets.h.example firmware/jf_flight_display/secrets.h
mkdir -p build
(cd service && go build -o ../build/flights-service .)
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/jf_flight_display --build-path build/firmware
