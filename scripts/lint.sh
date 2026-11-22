#!/usr/bin/env bash
# gofmt (check only) + go vet + strict warnings on the host-testable C++ core.
# Note: clang-format/cppcheck are not installed in the reference environment, so the
# C++ lint is the compiler at -Wall -Wextra -Werror -pedantic.
set -euo pipefail
cd "$(dirname "$0")/.."
. scripts/_arduinojson.sh
bad="$(gofmt -l service)"
[ -z "$bad" ] || { echo "gofmt needed on:" >&2; echo "$bad" >&2; exit 1; }
(cd service && go vet ./...)
g++ -std=c++17 -fsyntax-only -Wall -Wextra -Werror -pedantic -I"$AJ_DIR" \
  firmware/jf_flight_display/flights.cpp firmware/test/test_flights.cpp
python3 scripts/check_review_tier.py --self-test >/dev/null
