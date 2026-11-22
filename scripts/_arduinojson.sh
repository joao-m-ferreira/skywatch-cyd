# Sourced by test/lint: locate the ArduinoJson headers for host builds.
AJ_DIR="${ARDUINOJSON_DIR:-$(arduino-cli config get directories.user 2>/dev/null)/libraries/ArduinoJson/src}"
[ -f "$AJ_DIR/ArduinoJson.h" ] || { echo "ArduinoJson not found at $AJ_DIR (run scripts/setup.sh)" >&2; exit 1; }
