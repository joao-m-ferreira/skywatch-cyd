#!/usr/bin/env bash
# Toolchain for the tag-driven release workflow. arduino-cli itself is installed by the workflow in
# the build job only; the verify job just inspects the binaries and needs no toolchain, so this
# does nothing there.
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli not installed: nothing to set up (verify-only job)"
  exit 0
fi
scripts/setup.sh
