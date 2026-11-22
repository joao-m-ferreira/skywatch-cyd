#!/usr/bin/env bash
# Cut a release: scripts/tag-release.sh 0.2.0
# Checks that the tree is clean and on main, the commit is pushed and green on CI, that
# docs/releases/<version>.md and a CHANGELOG entry exist, then creates and pushes the annotated
# tag v<version>. The Release workflow then builds, verifies and publishes the binaries.
set -euo pipefail
cd "$(dirname "$0")/.."
V="${1:?usage: scripts/tag-release.sh <major.minor.patch[-pre]>}"
[[ "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]] || { echo "version must look like 1.2.3" >&2; exit 1; }
[ "$(git branch --show-current)" = main ] || { echo "release from main" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "working tree is not clean" >&2; exit 1; }
git fetch -q origin
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] || { echo "push main first (HEAD != origin/main)" >&2; exit 1; }
git rev-parse -q --verify "refs/tags/v$V" >/dev/null && { echo "tag v$V already exists" >&2; exit 1; }
[ -f "docs/releases/$V.md" ] || { echo "write docs/releases/$V.md (the release notes) first" >&2; exit 1; }
grep -q "^## \[$V\]" CHANGELOG.md || { echo "add a '## [$V]' section to CHANGELOG.md first" >&2; exit 1; }
n=$(gh run list --workflow ci.yml --commit "$(git rev-parse HEAD)" --status success --json databaseId --jq length)
[ "${n:-0}" -ge 1 ] || { echo "CI has not passed on this commit yet (gh run list --workflow ci.yml)" >&2; exit 1; }
git tag -a "v$V" -m "skywatch-cyd $V"
git push origin "v$V"
echo "pushed v$V: watch the Release workflow at $(gh repo view --json url --jq .url)/actions"
