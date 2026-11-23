# Changelog

Releases follow [Semantic Versioning](https://semver.org/). Each release has notes in
`docs/releases/<version>.md`, and its binaries are attached to the GitHub release.

## [Unreleased]

## [0.5.1] - 2026-11-23
- **ESP8266:** the web page, JSON API, MCP server and mDNS announcement now run on the `esp8266-oled` build too (the same code as the ESP32 builds, behind one small compatibility header), with smaller limits (2 KB request bodies, a 4 KB tool result) and without the WiFi setup access point. To make room, the serial help text and the settings-page stylesheet live in flash instead of RAM, and the TLS receive buffer is 6 KB. Verified on a real board that it serves, announces and keeps fetching; not yet used from another computer.
- `base64Decode` (the HTTP Basic login) no longer depends on mbedTLS, so it works on both chips and is host-tested.

## [0.5.0] - 2026-11-23
- **WiFi setup mode:** with no WiFi saved, or none reachable for 3 minutes, the board opens its own WiFi (`skywatch-XXXX`, random password) and a captive settings page for WiFi, location, radius, seconds per plane and night mode. The screen shows a QR code, the name, the password and the address. `setup` over serial or a 3 s button hold starts it by hand.
- **TTGO T-Display** (`ttgo-tdisplay`, 240x135) and **M5StickC** (`m5stickc`, 160x80) boards, both tested on real hardware, with compact layouts that show one value at a time (distance, altitude, speed) and the button for the next plane.
- `set plane <seconds>` (time per plane, shared between the values on small screens) and `set max <planes>` (most planes shown, default 10, up to 20).
- `version` shows the firmware version, build date and board; `update` checks GitHub for a newer release.
- Message screens shrink their font to fit narrow screens.
- **ESP8266 with a 0.96" OLED** (`esp8266-oled`): a separate text-only build that shares the flight parsing, the OLED text and the serial commands with the ESP32 build; one image at `0x0` (install and update are the same file, settings are kept). Tested on one board. No 3D model, photos, setup page, web page or MCP server.
- **The aircraft list is parsed straight off the connection** (ArduinoJson filter over a stream that waits for the network) instead of being collected in a 5-10 KB string first: lower peak memory and, in the emulator, 15 good fetches out of 15 where the old code managed about half. Found while porting to the ESP8266, where collecting the body silently lost chunks.
- A reference page for the network interfaces (web page, JSON API, MCP server, mDNS discovery, the Server Card and catalog): `docs/network-interfaces.md`.
- **Resiliency:** stored settings are checked on load and out-of-range or corrupt values are reset (the ESP8266 build also keeps a checksum and ignores a damaged settings sector); an over-long serial line is refused instead of running its first part; the aircraft-list download is bounded in size and time; timers use wrap-safe arithmetic throughout; a setting change now rejoins the WiFi without switching the radio off (an intermittent network-stack panic was seen once in the emulator after a settings change; it could not be reproduced, and this is the safer path, as setup mode already used).
- The small screens show the **route** (where the plane came from and is going to): a fourth page on the T-Display and M5StickC (after distance, altitude and speed), and on the OLED it alternates with the airline row. The OLED builds now fetch routes too (they used to skip the lookup, so the route never appeared), and the ESP8266 build looks routes up as airport codes (`LIS > YYZ`).
- **Fixed: the board restarted itself about every 10 minutes after boot.** The "no successful fetch for 10 minutes" check compared the time with a timestamp taken a moment later during the fetch, which wrapped to about 49 days, so the first good fetch after 10 minutes of uptime looked like a failure. Found by the emulator test; the comparison is now signed and tested.
- MCP authentication through HTTP headers (`Authorization: Bearer` or `X-API-Key`) is a switch, `set mcpauth on|off`, **off by default**; the web page and settings always need the token.
- **Web page and MCP server** on the board (on by default; `set web on|off`, `set mcp on|off`, `set advertise on|off` or the setup page switch each one): the planes and the settings at `http://skywatch.local/`, and a read-only MCP endpoint at `/mcp`, both behind an access token (`token`, `token new`). Discovery (`advertise`) through mDNS (`skywatch.local`, `_mcp._tcp`), an MCP Server Card at `/mcp/server-card` and an AI Catalog at `/.well-known/ai-catalog.json`. Written from the MCP specification (the `mcpesp` library that inspired it has no licence, so none of its code is used). Tested in the esp32sim emulator (`scripts/emu-web.sh`), not on a real board.

## [0.4.0] - 2026-11-23
- A text-only version for boards with a 0.96" SSD1306 OLED (`oled96`, `oled96-onboard`): the same firmware without the 3D model, `set panel ssd1306`, `set i2c <sda> <scl>`, BOOT button for the next plane.
- The web portal has a Download firmware button next to Install for every board.

## [0.3.0] - 2026-11-23
- Release builds for every hardware model (`firmware/boards.json`): CYD 2.8" (tested), CYD two-USB, 3.5" CYD and the Waveshare S3 AMOLED, each with its panel settings built in and an honest test status.
- A web portal (`web/`) that flashes a board from the browser and sets the WiFi, location, search distance and night mode over USB; published by GitHub Pages. Optional Cloudflare Worker to count flashes.
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
