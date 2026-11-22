# Changelog

Releases follow [Semantic Versioning](https://semver.org/). Each release has notes in
`docs/releases/<version>.md`, and its binaries are attached to the GitHub release.

## [Unreleased]

## [0.1.0] - 2026-11-22
First release.
- Standalone ESP32-2432S028 display of nearby aircraft (adsb.fi), carousel, touch, compass.
- Shaded 3D aircraft models per type (~110) with airline liveries; planespotters.net photos.
- Route (origin > destination) from adsbdb, shown in the header.
- RGB LED in the airline's colour (fades between two colours when the airline has two).
- Serial setup console, settings kept in flash; IP-based or static location.
- Night mode, WiFi/hardware watchdogs, automatic restarts.
- Tag-driven release pipeline: `scripts/tag-release.sh <version>`.
