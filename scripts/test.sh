#!/usr/bin/env bash
# Service tests + host-side firmware tests. No network.
set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_arduinojson.sh
(cd service && go test ./...)
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Werror -I"$AJ_DIR" \
  firmware/test/test_flights.cpp firmware/jf_flight_display/flights.cpp firmware/jf_flight_display/silhouette.cpp firmware/jf_flight_display/airline.cpp firmware/jf_flight_display/model3d.cpp firmware/jf_flight_display/config.cpp -o build/test_flights
build/test_flights
