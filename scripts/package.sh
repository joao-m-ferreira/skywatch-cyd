#!/usr/bin/env bash
# Build the release files into dist/. Always built from a CLEAN copy of the firmware with the
# secrets.h.example defaults (so real credentials can never be embedded) and the release VERSION
# stamped into version.h of that copy only.
#
#   skywatch-cyd-<V>-merged.bin    full 4 MB flash image: flash at 0x0 (also erases saved settings)
#   skywatch-cyd-<V>-firmware.bin  app only: flash at 0x10000 (keeps saved settings)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${VERSION:-0.0.0-dev}"
rm -rf dist build/pkg && mkdir -p dist build/pkg
cp -R firmware/jf_flight_display build/pkg/jf_flight_display
cp build/pkg/jf_flight_display/secrets.h.example build/pkg/jf_flight_display/secrets.h
printf '#pragma once\n#define SKYWATCH_VERSION "%s"\n' "$VERSION" > build/pkg/jf_flight_display/version.h
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app build/pkg/jf_flight_display \
  --build-path build/pkg/out --export-binaries >/dev/null
cp build/pkg/out/jf_flight_display.ino.merged.bin "dist/skywatch-cyd-${VERSION}-merged.bin"
cp build/pkg/out/jf_flight_display.ino.bin "dist/skywatch-cyd-${VERSION}-firmware.bin"
