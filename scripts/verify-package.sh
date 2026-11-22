#!/usr/bin/env bash
# Smoke-test the artifacts in the given dir (default dist).
set -euo pipefail
cd "$(dirname "$0")/.."
D="${1:-dist}"
fw=$(ls "$D"/jf-flight-display-*-firmware.bin) || { echo "no firmware in $D" >&2; exit 1; }
[ "$(head -c1 "$fw" | xxd -p)" = "e9" ] || { echo "$fw lacks the ESP image magic 0xE9" >&2; exit 1; }
[ "$(wc -c <"$fw")" -gt 100000 ] || { echo "$fw suspiciously small" >&2; exit 1; }
svc=$(ls "$D"/flights-service-*-linux-amd64)
[ "$(head -c4 "$svc" | xxd -p)" = "7f454c46" ] || { echo "$svc is not an ELF binary" >&2; exit 1; }
echo "package verified"
