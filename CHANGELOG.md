# Changelog

Releases follow [Semantic Versioning](https://semver.org/). Each release has notes in
`docs/releases/<version>.md`, and its binaries are attached to the GitHub release.

## [Unreleased]
- Board variants: `set panel ili9341|st7789`, `set invert`, `set rgb swap`, `set rotation 3`, and a `display test` colour card to identify a variant.
- Stability: bodies are read to their Content-Length (no more half-parsed documents), a fragmented heap skips the fetch instead of failing, the reset reason and crash count are shown in `status`, and three crashes in a row start a safe mode (no photos or 3D model).
- Screen layouts that adapt to the panel: plane left / right / on top (`set layout auto|side|flip|stacked`), portrait rotations, and an auto choice by panel shape; the default 320x240 plate is unchanged. Experimental ST7796 / ILI9488 3.5" panels, backlight pin and shared touch bus settings.
- ESP32-S3 port for the Waveshare ESP32-S3-Touch-AMOLED-1.8 (CO5300/SH8601 over QSPI, CST820/FT3168 touch): tested in the esp32sim simulator only, not on hardware. The simulator also found a real S3 bug: the display object was built in a global constructor, which crashes the S3 at boot (now built in `setup()`).
- `set led <percent>` lowers the RGB LED current (helps boards on weak USB ports).
- Render: geometry the fixed camera never sees is not built (about 30% fewer triangles); wings are grey; per-family and per-type aircraft shapes.
- `scripts/emu-smoke.sh`: runs the real firmware in the esp32sim emulator and checks boot, WiFi, fetch, draw time and memory.

## [0.2.0] - 2026-11-23
- Many more accurate liveries: TAP, easyJet, Air Europa, Emirates, Etihad, Maersk Air Cargo, Air Transat
  and ~25 new airlines; new tail and fuselage designs.
- Night-mode touch wake lasts 10 minutes by default (`set wake <minutes>`).
- `flights` serial command lists the detected aircraft.
- Release pipeline fixed: the verify job no longer needs the Arduino toolchain, so binaries publish.

## [0.1.0] - 2026-11-22
First release.
- Standalone ESP32-2432S028 display of nearby aircraft (adsb.fi), carousel, touch, compass.
- Shaded 3D aircraft models per type (~110) with airline liveries; planespotters.net photos.
- Route (origin > destination) from adsbdb, shown in the header.
- RGB LED in the airline's colour (fades between two colours when the airline has two).
- Serial setup console, settings kept in flash; IP-based or static location.
- Night mode, WiFi/hardware watchdogs, automatic restarts.
- Tag-driven release pipeline: `scripts/tag-release.sh <version>`.
