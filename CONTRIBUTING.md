# Contributing

skywatch-cyd is firmware for small ESP32 touchscreens that shows nearby aircraft. Read
[docs/PRD.md](docs/PRD.md) (what and why), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) (how it fits
together) and [docs/core-beliefs.md](docs/core-beliefs.md) (the engineering principles) first.

## Commands
| Task | Command |
|---|---|
| set up the toolchain | `scripts/setup.sh` |
| test | `scripts/test.sh` (Go tests + host-side firmware tests, no network) |
| lint | `scripts/lint.sh` (gofmt, go vet, `g++ -Wall -Wextra -Werror -pedantic`) |
| format | `scripts/format.sh` |
| build | `scripts/build.sh` (service binary + ESP32 and ESP32-S3 compiles) |
| run the optional service | `cd service && go run .` |
| ESP8266 build | `scripts/stage_esp8266.sh` gathers the sketch and the code it shares with the ESP32 build; `scripts/build.sh` compiles it (the emulator cannot run an ESP8266: test on a board) |
| emulator test of the web page and MCP server | `scripts/emu-web.sh` (reaches the emulated board's port 80 through the emulator's port forwarding; needs internet) |
| emulator smoke test | `scripts/emu-smoke.sh` (boots the real firmware in [esp32sim](https://github.com/mikroverk/esp32sim); needs internet) |
| docs check | `python3 scripts/check_docs.py` |
| release files | `VERSION=x.y.z scripts/package.sh` then `scripts/verify-package.sh dist` |

CI runs lint, test, build and the docs check on every push and pull request.

## Rules
- Do not weaken or skip a check to get green: fix the cause.
- The pinout lives only in `firmware/jf_flight_display/board.h`.
- `flights.h/.cpp`, `board.h`, `layout.h` and the 3D model code stay free of Arduino headers, so they are
  testable on the host.
- Secrets (WiFi, contact) live only in the git-ignored `firmware/jf_flight_display/secrets.h`; never commit them.
- Data credit: adsb.fi open data (personal, non-commercial use); photos from planespotters.net are
  credited on screen and are never committed to the repository.
- Hardware honesty: say what has actually been tested on a real board, what only in the emulator, and
  what not at all (see the board table in the README).

## Licence
By contributing you agree that your work is released under the [MIT License](LICENSE).

## Releases
Versions follow Semantic Versioning. See the Releases section of the [README](README.md).
