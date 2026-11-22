#!/usr/bin/env bash
# Build release artifacts into dist/. Always built from a CLEAN copy of the firmware
# using secrets.h.example, so real credentials can never be embedded.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${VERSION:-0.0.0-dev}"
rm -rf dist build/pkg && mkdir -p dist build/pkg
cp -R firmware/jf_flight_display build/pkg/jf_flight_display
cp build/pkg/jf_flight_display/secrets.h.example build/pkg/jf_flight_display/secrets.h
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app build/pkg/jf_flight_display \
  --build-path build/pkg/out --export-binaries >/dev/null
cp build/pkg/out/jf_flight_display.ino.bin "dist/jf-flight-display-${VERSION}-firmware.bin"
(cd service && GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -ldflags "-X main.version=${VERSION}" \
  -o "../dist/flights-service-${VERSION}-linux-amd64" .)
