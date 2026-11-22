#!/usr/bin/env bash
# Toolchain for the tag-driven release workflow (arduino-cli itself is installed by the workflow).
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/setup.sh
