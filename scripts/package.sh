#!/usr/bin/env bash
# Build the release files into dist/, one pair per hardware model listed in firmware/boards.json.
# Always built from a CLEAN copy of the firmware with the secrets.h.example defaults (so real credentials
# can never be embedded) and the release VERSION stamped into version.h of that copy only.
#
#   skywatch-cyd-<V>-<board>-merged.bin    full flash image: flash at 0x0 (also erases saved settings)
#   skywatch-cyd-<V>-<board>-firmware.bin  app only: flash at 0x10000 (keeps saved settings)
# An ESP8266 board ("sketch": "esp8266" in boards.json) has one image at 0x0: both files are that same image.
#
# Only boards whose firmware/boards.json "release" is not false are built (default: all). Env BOARDS="cyd amoled18"
# restricts the run (used for quick local checks).
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${VERSION:-0.0.0-dev}"
rm -rf dist build/pkg && mkdir -p dist build/pkg
cp -R firmware/jf_flight_display build/pkg/jf_flight_display
cp build/pkg/jf_flight_display/secrets.h.example build/pkg/jf_flight_display/secrets.h
printf '#pragma once\n#define SKYWATCH_VERSION "%s"\n' "$VERSION" > build/pkg/jf_flight_display/version.h
python3 - "${BOARDS:-}" <<'PY' > build/pkg/boards.tsv
import json, sys
want = set(sys.argv[1].split())
for b in json.load(open("firmware/boards.json"))["boards"]:
    if b.get("release", True) and (not want or b["id"] in want):
        print("\t".join([b["id"], b["fqbn"], b["flags"] or "-", b.get("sketch", "esp32")]))
PY
[ -s build/pkg/boards.tsv ] || { echo "no boards selected" >&2; exit 1; }
while IFS=$'\t' read -r id fqbn flags sketch; do
  [ "$flags" = "-" ] && flags=""
  echo "building $id"
  if [ "$sketch" = esp8266 ]; then
    # the ESP8266 sketch plus the code it shares with the ESP32 build, staged into one folder; the build is a
    # single image flashed at 0x0, published under both names (install and update are the same file)
    S=build/pkg/esp8266/skywatch_esp8266
    bash scripts/stage_esp8266.sh "$S" >/dev/null
    printf '#pragma once\n#define SKYWATCH_VERSION "%s"\n' "$VERSION" > "$S/version.h"
    arduino-cli compile --fqbn "$fqbn" ${flags:+--build-property "compiler.cpp.extra_flags=$flags"} \
      "$S" --build-path "build/pkg/out-$id" --export-binaries >/dev/null
    cp "build/pkg/out-$id/skywatch_esp8266.ino.bin" "dist/skywatch-cyd-${VERSION}-${id}-merged.bin"
    cp "build/pkg/out-$id/skywatch_esp8266.ino.bin" "dist/skywatch-cyd-${VERSION}-${id}-firmware.bin"
  else
    arduino-cli compile --fqbn "$fqbn" ${flags:+--build-property "compiler.cpp.extra_flags=$flags"} \
      build/pkg/jf_flight_display --build-path "build/pkg/out-$id" --export-binaries >/dev/null
    cp "build/pkg/out-$id/jf_flight_display.ino.merged.bin" "dist/skywatch-cyd-${VERSION}-${id}-merged.bin"
    cp "build/pkg/out-$id/jf_flight_display.ino.bin" "dist/skywatch-cyd-${VERSION}-${id}-firmware.bin"
  fi
done < build/pkg/boards.tsv
