#!/usr/bin/env bash
# Smoke-test the release files in the given dir (default dist): every hardware model in
# firmware/boards.json that has files there is checked with its own flash layout.
set -euo pipefail
cd "$(dirname "$0")/.."
D="${1:-dist}"
magic() { dd if="$1" bs=1 skip="$2" count=1 2>/dev/null | xxd -p; }
checked=0
while IFS=$'\t' read -r id boot flash; do
  fw=$(ls "$D"/skywatch-cyd-*-"$id"-firmware.bin 2>/dev/null | head -n 1 || true)
  mg=$(ls "$D"/skywatch-cyd-*-"$id"-merged.bin 2>/dev/null | head -n 1 || true)
  [ -n "$fw" ] || [ -n "$mg" ] || continue
  [ -n "$fw" ] && [ -n "$mg" ] || { echo "$id: need both the firmware and the merged image in $D" >&2; exit 1; }
  [ "$(magic "$fw" 0)" = "e9" ] || { echo "$fw lacks the ESP image magic 0xE9" >&2; exit 1; }
  [ "$(wc -c <"$fw")" -gt 100000 ] || { echo "$fw suspiciously small" >&2; exit 1; }
  [ "$(wc -c <"$mg")" -eq "$flash" ] || { echo "$mg is not a $flash byte flash image" >&2; exit 1; }
  [ "$(magic "$mg" "$boot")" = "e9" ] || { echo "$mg: no bootloader at $boot" >&2; exit 1; }
  [ "$(magic "$mg" 65536)" = "e9" ] || { echo "$mg: no application at 0x10000" >&2; exit 1; }
  # the application inside the merged image is the firmware file, byte for byte
  size=$(wc -c <"$fw")
  cmp <(dd if="$mg" bs=65536 skip=1 2>/dev/null | head -c "$size") "$fw" >/dev/null \
    || { echo "$mg does not contain $fw at 0x10000" >&2; exit 1; }
  # the version is stamped into the binary
  ver=$(basename "$fw" | sed -E "s/^skywatch-cyd-(.*)-$id-firmware\\.bin\$/\\1/")
  grep -aq "$ver" "$fw" || { echo "version $ver is not stamped in $fw" >&2; exit 1; }
  echo "package verified: $id ($ver)"
  checked=$((checked + 1))
done < <(python3 -c '
import json
for b in json.load(open("firmware/boards.json"))["boards"]:
    print("\t".join([b["id"], str(b["bootloaderOffset"]), str(b["flashBytes"])]))
')
[ "$checked" -gt 0 ] || { echo "no known board files in $D" >&2; exit 1; }
