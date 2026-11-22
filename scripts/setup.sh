#!/usr/bin/env bash
# Install firmware toolchain deps and prepare local config. Idempotent.
set -euo pipefail
cd "$(dirname "$0")/.."
command -v arduino-cli >/dev/null || { echo "arduino-cli is required" >&2; exit 1; }
command -v go >/dev/null || { echo "go is required" >&2; exit 1; }
arduino-cli core update-index
arduino-cli core list | grep -q '^esp32:esp32 ' || arduino-cli core install esp32:esp32
arduino-cli lib install "LovyanGFX" "ArduinoJson" "TJpg_Decoder"
if [ ! -f firmware/jf_flight_display/secrets.h ]; then
  cp firmware/jf_flight_display/secrets.h.example firmware/jf_flight_display/secrets.h
  echo "created firmware/jf_flight_display/secrets.h from the example - edit it before flashing"
fi
(cd service && go mod download)
