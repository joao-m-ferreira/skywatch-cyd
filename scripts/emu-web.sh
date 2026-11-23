#!/usr/bin/env bash
# Exercises the board's optional web page and MCP server in the esp32sim emulator (the real firmware, booted
# unmodified): the access token is set with serial commands (the emulator types them at fixed times), then
# the board's port 80 is reached from the host through the emulator's port forwarding.
#
#   scripts/emu-web.sh
#
# Checks: both are refused without the token; wrong tokens are refused and locked out after five; the page and
# JSON need the token; the MCP endpoint speaks JSON-RPC (initialize, tools/list, tools/call, notifications) and
# refuses browser requests; the public Server Card and AI Catalog hold no secret; the settings page saves.
# Needs the emulator from scripts/emu-smoke.sh (it is fetched on first use) and the internet (as emu-smoke.sh).
# Placeholder WiFi credentials only; your secrets.h is never used.
set -euo pipefail
cd "$(dirname "$0")/.."
VER=0.4.0
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) PLAT=macos-arm64 ;;
  Linux-x86_64) PLAT=linux-x86_64 ;;
  *) echo "no esp32sim release for $(uname -s)-$(uname -m)" >&2; exit 2 ;;
esac
SIM="build/emu/esp32sim-$VER-$PLAT/esp32sim"
ROM="build/emu/roms/esp32_rev300_rom.elf"
[ -x "$SIM" ] && [ -f "$ROM" ] || bash scripts/emu-smoke.sh 10 >/dev/null  # downloads and verifies the emulator
D=build/emu-web
PORT=18080
TOKEN=EMUTOKEN0123456789
mkdir -p "$D"

# 1. The firmware, from a temp copy with a placeholder WiFi network.
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
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app "$D/fw/jf_flight_display" \
  --build-path "$D/out" --export-binaries >/dev/null
F="$D/out/jf_flight_display.ino"

# 2. Serial commands typed into the board at emulated times: a fixed token, then both features on.
cat > "$D/actions.txt" <<EOF
25 uart0 set token $TOKEN
26 uart0 set web on
26.5 uart0 set debug on
27 uart0 set mcp on
450 uart0 set webauth on
500 uart0 set mcpauth on
EOF

# Defaults are all on, so 'set web on' and 'set mcp on' above change nothing: they check the commands. The switches
# are turned off one at a time later, by actions at fixed emulated times (the emulator runs far faster than real time).
cat >> "$D/actions.txt" <<EOF
1500 uart0 set webauth off
1700 uart0 set webauth on
2100 uart0 set advertise off
2300 uart0 set web off
2500 uart0 set mcp off
EOF

# 3. Run the emulator in the background, with the board's port 80 published on the host.
"$SIM" --chip esp32 --board cyd --rom "$ROM" --boot rom --console uart0 \
  --bootloader "$F.bootloader.bin" --ptable "$F.partitions.bin" --app "$F.bin" --elf "$F.elf" \
  --wifi ssid=emu-ap,psk=emu-password --hostfwd "tcp:127.0.0.1:$PORT-:80" \
  --script "$D/actions.txt" --max-seconds 6000 --no-dump > "$D/run.log" 2>&1 &
SIMPID=$!
trap 'kill $SIMPID 2>/dev/null || true' EXIT

URL="http://127.0.0.1:$PORT"
echo "waiting for the board's web server (the emulator boots, joins WiFi, takes the serial commands)..."
up=0
for _ in $(seq 1 240); do
  code=$(curl -s -o /dev/null -m 4 -w '%{http_code}' "$URL/" || true)
  { [ "$code" = "200" ] || [ "$code" = "401" ]; } && { up=1; break; }  # open by default; 401 with the login on
  kill -0 $SIMPID 2>/dev/null || { echo "FAIL: the emulator stopped (see $D/run.log)" >&2; exit 1; }
  sleep 2
done
[ "$up" = 1 ] || { echo "FAIL: no web server answered (see $D/run.log)" >&2; exit 1; }

fail=0
check() { if ! eval "$2"; then echo "FAIL: $1" >&2; fail=1; else echo "ok:   $1"; fi; }
# status, headers+body of one request
req() { curl -s -m 15 -i "$@" || true; }
status() { head -1 | awk '{print $2}'; }
RPC() { req -X POST "$URL/mcp" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d "$1"; }
waitfor() {  # waitfor <command that succeeds once it happened>; gives up if the emulator stops
  for _ in $(seq 1 180); do eval "$1" && return 0; kill -0 $SIMPID 2>/dev/null || return 1; sleep 2; done; return 1
}
code() { curl -s -o /dev/null -m 8 -w '%{http_code}' "$@" || true; }
rpc_code() { code -X POST "$URL/mcp" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":13,"method":"ping"}'; }
mcp_up() { [ "$(rpc_code)" = 200 ]; }
mcp_gone() { c="$(rpc_code)"; [ "$c" = 404 ] || [ "$c" = 000 ]; }  # 404, or the port is closed
card_gone() { [ "$(code "$URL/mcp/server-card")" = 404 ] && [ "$(code "$URL/.well-known/ai-catalog.json")" = 404 ]; }
web_gone() { [ "$(code -u "skywatch:$TOKEN" "$URL/api/flights")" = 404 ]; }
port_closed() { [ "$(code "$URL/")" = 000 ]; }
view_open() { [ "$(code "$URL/api/flights")" = 200 ]; }
view_closed() { [ "$(code "$URL/api/flights")" = 401 ]; }
mcp_needs_token() { [ "$(code -X POST "$URL/mcp" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":14,"method":"ping"}')" = 401 ]; }

# header authentication for /mcp is a switch, off by default: first the board answers without a token
r=$(req -X POST "$URL/mcp" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":20,"method":"tools/list"}')
check "MCP answers without a token while header authentication is off (the default)" "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q get_nearby_flights"
r=$(req -X POST "$URL/mcp" -H 'Origin: http://evil.example' -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":21,"method":"ping"}')
check "a browser request is refused even then (403)"                   "echo \"\$r\" | head -1 | grep -q 403"
r=$(req "$URL/mcp/server-card")
check "the Server Card lists no required header while it is off"        "echo \"\$r\" | head -1 | grep -q 200 && ! echo \"\$r\" | grep -q 'Bearer' && ! echo \"\$r\" | grep -q '\"headers\"'"
r=$(req "$URL/api/flights")
check "the plane list's JSON needs no login by default (200)"          "echo \"\$r\" | head -1 | grep -q 200"
r=$(req "$URL/settings")
check "with no web login the settings page opens without a token (200)" "echo \"\$r\" | head -1 | grep -q 200"
check "'set webauth on' (typed at an emulated time) puts the login on the plane list" "waitfor view_closed"
check "'set mcpauth on' (typed at an emulated time) makes /mcp need a token (401)" "waitfor mcp_needs_token"
r=$(req "$URL/mcp/server-card")
check "the Server Card now lists the Authorization and X-API-Key headers" "echo \"\$r\" | grep -q 'Bearer {token}' && echo \"\$r\" | grep -q 'X-API-Key' && ! echo \"\$r\" | grep -q '$TOKEN'"
r=$(req -X POST "$URL/mcp" -H "X-API-Key: $TOKEN" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":22,"method":"ping"}')
check "an X-API-Key header with the token is accepted"                  "echo \"\$r\" | head -1 | grep -q 200"
r=$(req -X POST "$URL/mcp" -H 'X-API-Key: WRONGTOKEN0123456789' -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":23,"method":"ping"}')
check "a wrong X-API-Key is refused (401)"                              "echo \"\$r\" | head -1 | grep -q 401"
# the wrong key above counted once towards the lock-out; a right request resets it
curl -s -o /dev/null -m 15 -X POST "$URL/mcp" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":24,"method":"ping"}' || true


r=$(req "$URL/")
check "the web page needs the token (401 with a Basic challenge)"  "echo \"\$r\" | head -1 | grep -q 401 && echo \"\$r\" | grep -qi 'WWW-Authenticate: Basic'"
r=$(req -u "skywatch:$TOKEN" "$URL/")
check "the web page opens with the token"                           "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q '<title>skywatch</title>'"
r=$(req -u "skywatch:$TOKEN" "$URL/api/flights")
check "/api/flights is JSON with the token"                          "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | tail -1 | python3 -c 'import json,sys; assert isinstance(json.load(sys.stdin), list)'"
r=$(req -u "skywatch:$TOKEN" "$URL/api/status")
check "/api/status reports the firmware, no secret"                   "echo \"\$r\" | grep -q '\"firmware\":\"emu\"' && ! echo \"\$r\" | grep -Eqi 'token|password|ssid'"
r=$(req -u "skywatch:$TOKEN" "$URL/settings")
check "the settings page opens with the token and has the switches"  "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q 'name=mcp' && echo \"\$r\" | grep -q 'name=web'"
check "the settings page never shows the token or WiFi password"      "! echo \"\$r\" | grep -q '$TOKEN' && ! echo \"\$r\" | grep -q 'emu-password'"

r=$(RPC '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"t","version":"1"}}}')
check "MCP initialize answers with the protocol version"             "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q '\"protocolVersion\":\"2025-03-26\"' && echo \"\$r\" | grep -qi 'Content-Type: application/json'"
r=$(RPC '{"jsonrpc":"2.0","method":"notifications/initialized"}')
check "a notification is accepted with 202 and no body"               "echo \"\$r\" | head -1 | grep -q 202"
r=$(RPC '{"jsonrpc":"2.0","id":2,"method":"tools/list"}')
check "MCP tools/list names the read-only tools"                     "echo \"\$r\" | grep -q get_nearby_flights && echo \"\$r\" | grep -q get_current_plane && echo \"\$r\" | grep -q get_status"
r=$(RPC '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_status"}}')
check "MCP get_status reports the firmware and leaks nothing"         "echo \"\$r\" | grep -q 'firmware' && echo \"\$r\" | grep -q 'emu' && ! echo \"\$r\" | grep -Eqi 'emu-password|EMUTOKEN'"
r=$(RPC '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get_nearby_flights","arguments":{"limit":3}}}')
check "MCP get_nearby_flights returns content"                       "echo \"\$r\" | grep -q '\"isError\":false'"
r=$(RPC '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"set_ssid","arguments":{}}}')
check "there is no tool that changes anything (-32602)"               "echo \"\$r\" | grep -q -- '-32602'"
r=$(RPC '{"jsonrpc":"2.0","id":6,"method":"resources/list"}')
check "unknown methods answer -32601"                                 "echo \"\$r\" | grep -q -- '-32601'"
r=$(req -X POST "$URL/mcp" -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":7,"method":"ping"}')
check "MCP without a token is refused (401, Bearer challenge)"        "echo \"\$r\" | head -1 | grep -q 401 && echo \"\$r\" | grep -qi 'WWW-Authenticate: Bearer'"
r=$(req -X POST "$URL/mcp" -H "Authorization: Bearer $TOKEN" -H 'Origin: http://evil.example' -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":8,"method":"ping"}')
check "a browser request (with an Origin) is refused (403)"           "echo \"\$r\" | head -1 | grep -q 403"
r=$(req "$URL/mcp")
check "GET /mcp is 405 with Allow: POST"                              "echo \"\$r\" | head -1 | grep -q 405 && echo \"\$r\" | grep -qi 'Allow: POST'"

r=$(req "$URL/mcp/server-card")
check "the Server Card is public, with the card media type"          "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -qi 'Content-Type: application/mcp-server-card+json'"
check "the Server Card names the endpoint and header, no secret"      "echo \"\$r\" | grep -q 'server-card.schema.json' && echo \"\$r\" | grep -q 'http://{host}/mcp' && echo \"\$r\" | grep -q 'Bearer {token}' && ! echo \"\$r\" | grep -q '$TOKEN'"
r=$(req "$URL/.well-known/ai-catalog.json")
check "the AI Catalog points to the card"                             "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -qi 'Content-Type: application/ai-catalog+json' && echo \"\$r\" | grep -q 'http://skywatch.local/mcp/server-card'"

# the settings page saves (and applies on the board: it reconnects its WiFi)
r=$(req -u "skywatch:$TOKEN" -X POST "$URL/save" --data 'ssid=emu-ap&pass=&loc=auto&lat=38.7223&lon=-9.1393&radius=33&plane=7&night=off&from=1&to=5&contact=&web=on&mcp=on')
check "saving settings over the web works"                            "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q '<h1>Saved</h1>' && ! echo \"\$r\" | grep -q '$TOKEN'"
r=$(req -u "skywatch:$TOKEN" -X POST "$URL/save" --data 'ssid=emu-ap&pass=&loc=auto&radius=999&plane=7&night=off&from=1&to=5&web=on&mcp=on')
check "a bad value is refused and nothing is saved (radius 999)"      "echo \"\$r\" | grep -q 'radius must be'"
r=$(req -X POST "$URL/save" --data 'ssid=evil&pass=x&loc=auto&radius=10&plane=7&night=off&from=1&to=5&web=on&mcp=on')
check "saving without the token is refused"                            "echo \"\$r\" | head -1 | grep -q 401"

# the plane data is refreshed periodically: the age of the last good fetch must keep resetting
refreshes=0; prev=-1; maxage=0
for _ in $(seq 1 14); do
  a=$(curl -s -m 8 -u "skywatch:$TOKEN" "$URL/api/status" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("last_fetch_s_ago", -1))' 2>/dev/null || echo -1)
  if [ "$a" -ge 0 ] 2>/dev/null; then
    [ "$prev" -ge 0 ] && [ "$a" -lt "$prev" ] && refreshes=$((refreshes + 1))
    [ "$a" -gt "$maxage" ] && maxage=$a
    prev=$a
  fi
  sleep 3
done
echo "info: plane data refreshed $refreshes times while sampling, oldest age seen ${maxage} s"
check "plane data is refreshed periodically (age reset at least twice, never older than 90 s)" "[ $refreshes -ge 2 ] && [ $maxage -le 90 ]"

# wrong tokens are locked out after five (a request with no token at all is not counted)
for i in 1 2 3 4 5; do curl -s -o /dev/null -m 15 -X POST "$URL/mcp" -H 'Authorization: Bearer WRONGTOKEN' -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":9,"method":"ping"}' || true; done
r=$(RPC '{"jsonrpc":"2.0","id":10,"method":"ping"}')
check "five wrong tokens lock the board out for a minute (429), even for the right one" "echo \"\$r\" | head -1 | grep -q 429"

# 'set webauth off' (the default) opens the whole web page, settings included. (A request with no
# credentials is not counted towards the lock-out, so these checks cannot lock the board.)
check "'set webauth off' opens the plane list's JSON without a login"  "waitfor view_open"
r=$(req "$URL/")
check "the plane list page opens without a login too"                  "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q '<title>skywatch</title>'"
r=$(req "$URL/api/status")
check "/api/status opens without a login and still holds no secret"    "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q '\"firmware\":\"emu\"' && ! echo \"\$r\" | grep -Eqi 'token|password|ssid'"
r=$(req "$URL/settings")
check "the settings page opens without a login too (200)"              "echo \"\$r\" | head -1 | grep -q 200"
r=$(req -u "skywatch:$TOKEN" "$URL/settings")
check "the settings page opens with the token"                          "echo \"\$r\" | head -1 | grep -q 200 && echo \"\$r\" | grep -q 'name=webauth'"
check "'set webauth on' closes the plane list again (401)"             "waitfor view_closed"

# the switches: each is turned off by a serial command at a later emulated time (see actions.txt); wait for the effect
check "'set advertise off' withdraws the public Server Card and the catalog (404)" "waitfor card_gone"
check "with advertising off the MCP endpoint still works (after the lock-out ends)" "waitfor mcp_up"
check "'set web off' turns the web page and JSON off (404)"           "waitfor web_gone"
check "with the web page off the MCP endpoint still works"            "mcp_up"
check "'set mcp off' turns the MCP endpoint off too"                  "waitfor mcp_gone"
check "with web and MCP both off the board opens no port at all"       "waitfor port_closed"

log="$D/run.log"
check "no panic or abort in the emulator log"                        "! grep -Eqi 'Guru Meditation|abort\\(\\)|panic|stack overflow|assert failed' $log"
# the only restart allowed is the one 'set advertise off' causes; the 10-minute stuck-fetch restart used to fire here
check "no unexpected restart (the run is longer than the 10-minute stuck-fetch limit)" "! grep 'restarting:' $log | grep -qv 'advertising turned off'"
check "the board printed its web address"                              "grep -q 'web: http://' $log"
[ "$fail" -eq 0 ] && echo "emulator web/MCP test passed" || { echo "emulator web/MCP test FAILED (see $log)" >&2; exit 1; }
