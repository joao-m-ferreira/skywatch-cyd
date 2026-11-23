#!/usr/bin/env bash
# Build the Go service and compile the firmware for the ESP32 (the CYD family) and, as a compile check
# that keeps the port from rotting, for the ESP32-S3 Waveshare AMOLED 1.8 board (simulator-tested only).
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f firmware/jf_flight_display/secrets.h ] || cp firmware/jf_flight_display/secrets.h.example firmware/jf_flight_display/secrets.h
mkdir -p build
(cd service && go build -o ../build/flights-service .)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app firmware/jf_flight_display --build-path build/firmware
# ESP32-S3 (Waveshare ESP32-S3-Touch-AMOLED-1.8): 16 MB flash, octal PSRAM, serial over native USB.
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc \
  firmware/jf_flight_display --build-path build/firmware-s3 >/dev/null
# LilyGO TTGO T-Display, 135x240 (compile check)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-property "compiler.cpp.extra_flags=-DSKYWATCH_PANEL=7" \
  firmware/jf_flight_display --build-path build/firmware-tdisplay >/dev/null
# M5StickC, 160x80 (compile check)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-property "compiler.cpp.extra_flags=-DSKYWATCH_PANEL=8" \
  firmware/jf_flight_display --build-path build/firmware-m5stickc >/dev/null
# ESP8266 with a 0.96" OLED: the sketch plus the code it shares with the ESP32 build (compile check)
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 "$(bash scripts/stage_esp8266.sh)" --build-path build/firmware-esp8266 >/dev/null
# 0.96" SSD1306 OLED, text only (compile check)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-property "compiler.cpp.extra_flags=-DSKYWATCH_PANEL=6" \
  firmware/jf_flight_display --build-path build/firmware-oled >/dev/null
