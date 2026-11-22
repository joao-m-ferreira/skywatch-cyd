#!/usr/bin/env bash
# Smoke-test the release files in the given dir (default dist).
set -euo pipefail
cd "$(dirname "$0")/.."
D="${1:-dist}"
magic() { dd if="$1" bs=1 skip="$2" count=1 2>/dev/null | xxd -p; }
fw=$(ls "$D"/skywatch-cyd-*-firmware.bin) || { echo "no firmware in $D" >&2; exit 1; }
[ "$(magic "$fw" 0)" = "e9" ] || { echo "$fw lacks the ESP image magic 0xE9" >&2; exit 1; }
[ "$(wc -c <"$fw")" -gt 100000 ] || { echo "$fw suspiciously small" >&2; exit 1; }
mg=$(ls "$D"/skywatch-cyd-*-merged.bin) || { echo "no merged image in $D" >&2; exit 1; }
[ "$(wc -c <"$mg")" -eq 4194304 ] || { echo "$mg is not a 4 MB flash image" >&2; exit 1; }
[ "$(magic "$mg" 4096)" = "e9" ] || { echo "$mg: no bootloader at 0x1000" >&2; exit 1; }
[ "$(magic "$mg" 65536)" = "e9" ] || { echo "$mg: no application at 0x10000" >&2; exit 1; }
# the application inside the merged image is the firmware file, byte for byte
size=$(wc -c <"$fw")
cmp <(dd if="$mg" bs=65536 skip=1 2>/dev/null | head -c "$size") "$fw" >/dev/null \
  || { echo "$mg does not contain $fw at 0x10000" >&2; exit 1; }
# the version is stamped into the binary
ver=$(basename "$fw" | sed -E 's/^skywatch-cyd-(.*)-firmware\.bin$/\1/')
grep -aq "$ver" "$fw" || { echo "version $ver is not stamped in $fw" >&2; exit 1; }
echo "package verified ($ver)"
