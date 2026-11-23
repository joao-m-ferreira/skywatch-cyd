#!/usr/bin/env bash
# End-to-end smoke test of the REAL firmware in the esp32sim emulator (https://github.com/mikroverk/esp32sim,
# MIT): boots the unmodified binary on the "cyd" (ESP32-2432S028) board model, joins a virtual WiFi network,
# and checks the console: it must fetch aircraft over HTTPS, draw, stay within time and memory budgets and
# never panic or reboot. No hardware needed. Needs the internet (adsb.fi and friends are reached for real).
#
#   scripts/emu-smoke.sh [seconds] [landscape|portrait|s3]
#       default 60 emulated seconds (about 10 s of wall time); "portrait" builds with rotation 0 (the
#       stacked layout on the CYD's 240x320); "s3" builds for the ESP32-S3 and runs the Waveshare
#       ESP32-S3-Touch-AMOLED-1.8 (V2) board model. That board is TESTED IN THE SIMULATOR ONLY.
#
# Writes build/emu/run.log and build/emu/tft.png (the emulated screen, flipped upright). Real credentials are
# never used: the firmware is built from a temp copy with a placeholder WiFi network.
#
# Limits of the emulator (early classic-ESP32 support): the screen capture is NOT pixel-faithful (some large
# LovyanGFX fills are missing and the panel polarity differs from the real board), so this checks the firmware's
# logic, timing and memory, not how the screen looks. The 3D renders are checked by scripts/test.sh instead.
# In "s3" mode the emulated AMOLED panel only applies full-window writes: its model expects the QSPI command
# header as one transfer (as Arduino_GFX sends it), while LovyanGFX sends it in pieces, so windowed drawing is
# not shown. Boot, WiFi, fetching, draw time and memory are still checked; the display path is not.
set -euo pipefail
cd "$(dirname "$0")/.."
SECS="${1:-60}"
MODE="${2:-landscape}"
VER=0.4.0
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) PLAT=macos-arm64;  SHA=d9755dd8ab5ae29dba05df77162428ea9d67512d05db77330c1ad87ddac3a6ef ;;
  Linux-x86_64) PLAT=linux-x86_64; SHA=9c8a963cc2945c127e96726104ff48129b2122032ddc532726ffdbd400ad6849 ;;
  *) echo "no esp32sim release for $(uname -s)-$(uname -m)" >&2; exit 2 ;;
esac
command -v arduino-cli >/dev/null || { echo "arduino-cli is required (scripts/setup.sh)" >&2; exit 2; }
D=build/emu
mkdir -p "$D"

# 1. The emulator, pinned by SHA-256 (downloaded once, then cached).
TAR="esp32sim-$VER-$PLAT.tar.gz"
sha_ok() { echo "$2  $1" | (shasum -a 256 -c >/dev/null 2>&1 || sha256sum -c >/dev/null 2>&1); }
if [ ! -x "$D/esp32sim-$VER-$PLAT/esp32sim" ]; then
  curl -fsSL --retry 3 "https://github.com/mikroverk/esp32sim/releases/download/v$VER/$TAR" -o "$D/$TAR"
  sha_ok "$D/$TAR" "$SHA" || { echo "$TAR: SHA-256 mismatch" >&2; rm -f "$D/$TAR"; exit 1; }
  tar xzf "$D/$TAR" -C "$D"
  xattr -d com.apple.quarantine "$D/esp32sim-$VER-$PLAT/esp32sim" 2>/dev/null || true
fi
SIM="$D/esp32sim-$VER-$PLAT/esp32sim"

# 2. The chip's mask ROM (Espressif, pinned by the emulator's own fetch script).
sh "$D/esp32sim-$VER-$PLAT/tools/fetch-rom-elfs.sh" "$D/roms" >/dev/null

# 3. The firmware, built from a temp copy with a placeholder WiFi network (never your secrets.h).
rm -rf "$D/fw" && mkdir -p "$D/fw"
cp -R firmware/jf_flight_display "$D/fw/jf_flight_display"
cat > "$D/fw/jf_flight_display/secrets.h" <<'EOF'
#pragma once
#define WIFI_SSID "emu-ap"
#define WIFI_PASSWORD "emu-password"
#define HOME_LAT 38.7223
#define HOME_LON -9.1393
#define RADIUS_KM 40
#define PLANESPOTTERS_CONTACT "emulator@example.invalid"
EOF
printf '#pragma once\n#define SKYWATCH_VERSION "emu"\n' > "$D/fw/jf_flight_display/version.h"
[ "$MODE" = portrait ] && sed -i.bak 's/c->rotation = 1;/c->rotation = 0;/' "$D/fw/jf_flight_display/config.cpp"
if [ "$MODE" = s3 ]; then
  FQBN=esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB  # serial on UART0 for the emulator
else
  FQBN=esp32:esp32:esp32:PartitionScheme=huge_app
fi
arduino-cli compile --fqbn "$FQBN" "$D/fw/jf_flight_display" \
  --build-path "$D/out" --export-binaries >/dev/null
F="$D/out/jf_flight_display.ino"

# 4. Run it.
set +e
if [ "$MODE" = s3 ]; then
  CHIPARGS=(--chip s3 --board waveshare-amoled18-v2 --flash-mb 16 --psram-mb 8 --rom "$D/roms/esp32s3_rev0_rom.elf")
else
  CHIPARGS=(--chip esp32 --board cyd --rom "$D/roms/esp32_rev300_rom.elf")
fi
timeout $((SECS * 3 + 60)) "$SIM" "${CHIPARGS[@]}" --boot rom \
  --console uart0 --bootloader "$F.bootloader.bin" --ptable "$F.partitions.bin" --app "$F.bin" --elf "$F.elf" \
  --wifi ssid=emu-ap,psk=emu-password --tft-png "$D/tft-raw.png" --max-seconds "$SECS" --no-dump \
  > "$D/run.log" 2>&1
rc=$?
set -e
[ "$rc" -eq 0 ] || { echo "FAIL: esp32sim exited with $rc (see $D/run.log)" >&2; exit 1; }
python3 - "$D" "$MODE" <<'EOF' 2>/dev/null || true
import sys
try:
    from PIL import Image, ImageOps
    d = sys.argv[1]
    im = Image.open(f"{d}/tft-raw.png").convert("RGB")
    (im if len(sys.argv) > 2 and sys.argv[2] == "s3" else ImageOps.flip(im)).save(f"{d}/tft.png")
except Exception:
    pass
EOF

# 5. Checks on what the firmware did.
log="$D/run.log"
fail=0
check() { if ! eval "$2"; then echo "FAIL: $1" >&2; fail=1; else echo "ok:   $1"; fi; }
check "boots and prints its banner"            "grep -q 'skywatch-cyd .*type .help. for settings' $log"
check "joins the WiFi network"                 "grep -q 'state Associated' $log"
check "fetches aircraft over HTTPS"            "grep -Eq 'fetched [0-9]+ aircraft' $log"
check "draws a plate"                          "grep -Eq 'drawn in [0-9]+ ms' $log"
check "no panic, abort or restart"             "! grep -Eqi 'Guru Meditation|abort\\(\\)|panic|restarting:|stack overflow|assert failed' $log"
check "no second boot (rst:0x1 only once)"     "[ \$(grep -c 'rst:0x' $log) -le 1 ]"
slowest=$(grep -Eo 'drawn in [0-9]+ ms' "$log" | awk '{print $3}' | sort -n | tail -1)
minheap=$(grep -Eo 'free heap [0-9]+' "$log" | awk '{print $3}' | sort -n | head -1)
echo "info: slowest draw ${slowest:-?} ms, lowest free heap ${minheap:-?} bytes, last drawn frame in $D/tft.png"
check "every draw under 400 ms (emulated clock)" "[ -n '${slowest}' ] && [ '${slowest}' -lt 400 ]"
check "free heap never under 40 KB"            "[ -n '${minheap}' ] && [ '${minheap}' -ge 40000 ]"
grep -E 'JSON: |HTTP -|fetch failed' "$log" | sort | uniq -c | sed 's/^/warn: /' || true
[ "$fail" -eq 0 ] && echo "emulator smoke test passed" || { echo "emulator smoke test FAILED (see $log)" >&2; exit 1; }
