#!/usr/bin/env bash
# Install firmware toolchain deps and prepare local config. Idempotent.
set -euo pipefail
cd "$(dirname "$0")/.."
command -v arduino-cli >/dev/null || { echo "arduino-cli is required" >&2; exit 1; }
command -v go >/dev/null || { echo "go is required" >&2; exit 1; }
arduino-cli core update-index
arduino-cli core list | grep -q '^esp32:esp32 ' || arduino-cli core install esp32:esp32
ESP8266_INDEX=https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index --additional-urls "$ESP8266_INDEX"
arduino-cli core list | grep -q '^esp8266:esp8266 ' || arduino-cli core install esp8266:esp8266 --additional-urls "$ESP8266_INDEX"
arduino-cli lib install "LovyanGFX" "ArduinoJson" "TJpg_Decoder" "U8g2"
if [ ! -f firmware/jf_flight_display/secrets.h ]; then
  cp firmware/jf_flight_display/secrets.h.example firmware/jf_flight_display/secrets.h
  echo "created firmware/jf_flight_display/secrets.h from the example - edit it before flashing"
fi
(cd service && go mod download)
