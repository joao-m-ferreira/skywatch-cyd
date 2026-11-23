# skywatch-cyd

**Nearby flights on a €15 touchscreen: standalone, no server.** An ESP32-2432S028 ("Cheap Yellow
Display") fetches the aircraft around you from open ADS-B data and shows them one at a time: airline,
type, route, altitude, speed, distance, a compass, and a shaded 3D model in the airline's livery
(or a photo of that exact plane). The board's RGB LED glows in the airline's colour.

![3D aircraft models rendered by the firmware's own code (host-side preview)](docs/images/models.png)

<sub>Twelve airlines on twelve aircraft types, rendered on the host from the same 3D code that runs on the board, so the shapes and liveries match; the real screen is 320×240 and looks smaller and blockier. Regenerate with `python3 scripts/readme_image.py`.</sub>

## Features
- **Carousel** of the nearest aircraft (default 40 km), touch to skip ahead. 20 s data refresh.
- **Per-plane details:** callsign, airline, type, altitude / FL, speed and track, distance and
  bearing, **route** (origin > destination), compass with the direction of travel.
- **3D models** built on the board from real dimensions of ~110 types (A220–A380, 737–787, E-Jets, CRJ,
  ATR, Dash 8, business jets, light aircraft, helicopters), adapted per family (nose and tail shape,
  engine size, raked 777/787 tips, T-tails, winglet styles) with windows, doors, landing gear when low
  and slow, and the 747 hump / A380 double deck.
- **Liveries** for ~160 airlines: fuselage scheme, tail designs (disc, stripes, diagonal, checker,
  cross, faceted, flag stripes…), tinted fuselages, engine and winglet colours, plus airline-specific
  paint such as TAP's logo block, easyJet's orange crown and Etihad's gold facets.
- **Photos** of the actual aircraft from planespotters.net by registration (photographer credited),
  with the 3D model as fallback.
- **RGB LED** in the airline's colour; airlines with two real colours (Ryanair, TAP…) hold and fade
  between them.
- **Night mode:** screen and LED off on a schedule, touch to wake for 10 min (`set wake <minutes>`). `flights` over serial lists the detected aircraft.
- **Location from your IP** (city level) or a fixed one.
- **Robust:** WiFi watchdog, hardware watchdog, restarts if stuck, out of memory, or once a day.

## Hardware
One **ESP32-2432S028R** (2.8" ILI9341, resistive touch, RGB LED, light sensor) and a micro-USB
cable, roughly €15–25. Capacitive "IPS" variants use different touch pins and are not supported.

## Install (flash the prebuilt firmware)
**Easiest: from your browser.** Open the web portal, choose your board, press **Install**, then type your
WiFi name and password into the page: it sends them to the board over the USB cable. You need Chrome or Edge
on a computer and a USB *data* cable, and nothing else. The portal is published from this repository's latest
release (see [Web portal](#web-portal)).

**Or by hand**, with the files from [Releases](https://github.com/joao-m-ferreira/skywatch-cyd/releases):
there is one pair of files per board, named `skywatch-cyd-<version>-<board>-merged.bin` (everything, flash at
`0x0`) and `-firmware.bin` (the app only, flash at `0x10000`, keeps your settings).

| Board | `<board>` | Chip | Status |
|---|---|---|---|
| ESP32-2432S028R "Cheap Yellow Display" 2.8" (ILI9341, one USB port) | `cyd` | ESP32 | tested on real hardware |
| 2.8" CYD with two USB ports (ST7789) | `cyd-st7789` | ESP32 | not tested on hardware |
| 3.5" "big CYD" (ST7796, 480x320) | `cyd35` | ESP32 | experimental |
| Waveshare ESP32-S3-Touch-AMOLED-1.8 | `amoled18` | ESP32-S3 | emulator only |

1. Download the `-merged.bin` for your board (and `SHA256SUMS` to check it:
   `shasum -a 256 -c SHA256SUMS`).
2. Plug the board in with a micro-USB **data** cable and find its port: macOS `/dev/cu.wchusbserial*`
   (or `/dev/cu.usbserial*`), Linux `/dev/ttyUSB0`, Windows `COM3`. If no port appears, install the
   CH340 USB driver.
3. Flash it, either with `esptool` (`pip install esptool`):

   ```sh
   # first install: one file at 0x0 (also erases saved settings); use --chip esp32s3 for the AMOLED board
   esptool.py --chip esp32 --port <port> --baud 460800 write_flash 0x0 skywatch-cyd-<version>-<board>-merged.bin
   # update an installed board: the app only (keeps your WiFi and settings)
   esptool.py --chip esp32 --port <port> --baud 460800 write_flash 0x10000 skywatch-cyd-<version>-<board>-firmware.bin
   ```

   or with Espressif's browser flasher: open [esptool-js](https://espressif.github.io/esptool-js/), connect, set
   the address to `0x0`, pick the `-merged.bin` file and press Program.
4. If the upload cannot connect, hold the **BOOT** button while it starts, then release it.
5. Open a serial terminal at **115200 baud** (`screen /dev/cu.wchusbserial* 115200`, Arduino serial
   monitor, …), type `help`, and set your WiFi:

   ```
   set ssid <your WiFi>
   set pass <password>
   ```

The screen shows the planes around you within about a minute. The location comes from your public IP;
use `set location static <lat> <lon>` for an exact one.

## Settings (USB serial, 115200 baud)
Settings are saved in the board's flash. The build-time defaults come from `secrets.h`.

| Command | What it does |
|---|---|
| `show` / `status` | current settings (password hidden) / uptime, memory, WiFi, clock |
| `set ssid <name>` / `set pass <password>` | WiFi (`set pass -` for an open network) |
| `set location auto` | locate from your public IP via geojs.io (default) |
| `set location static <lat> <lon>` | fixed position (also `set lat` / `set lon`) |
| `set radius <km>` | search radius, 1–250 |
| `set contact <email>` | contact sent to planespotters.net (required by their API) |
| `set night <from> <to>` / `set night off` | screen off between these hours, e.g. `set night 23 7` |
| `set tz <posix tz>` | clock timezone (found from your location when auto) |
| `set panel ili9341\|st7789\|st7796\|ili9488` / `set invert on\|off` / `set rgb normal\|swap` / `set rotation 0..3` | board variant: panel, colour inversion, red/blue order, orientation (restarts to apply) |
| `set backlight <gpio>` / `set touch separate\|shared` | 3.5" boards: backlight pin (27) and touch on the display's SPI bus (restarts to apply) |
| `set layout auto\|side\|flip\|stacked` | plane left / plane right / plane on top, text below (auto by panel shape) |
| `display test` | colour test card for 15 s, to identify your panel variant |
| `set led <0..100>` | RGB LED brightness percent |
| `safe on\|off` | force / leave safe mode |
| `night test` | try night mode for 20 s (not saved) |
| `reboot` / `factory` | restart / forget saved settings |

## Board variants and troubleshooting
The "Cheap Yellow Display" is sold with the same pinout but different panels. The default matches the
single-USB **ESP32-2432S028R** (ILI9341). If colours are wrong on your board, run `display test` and
compare the bars with red, green, blue, white, black, yellow, cyan, magenta:

| You see | Do |
|---|---|
| red and blue swapped | `set rgb swap` |
| colours look like a negative | `set invert on` (or `off`) |
| upside down | `set rotation 3` |
| white or garbled screen, two USB ports (micro + USB-C) | `set panel st7789`, then `set invert on` if needed |

The board restarts to apply each display setting and remembers it.

**Layouts.** The plate adapts to the panel: the canvas always fills the screen (nothing is
letterboxed) and `set layout` chooses how it is arranged:

| Layout | Plane | Text | Default for |
|---|---|---|---|
| `side` | left | right | landscape panels (320x240, 480x320, ...) |
| `flip` | right | left | |
| `stacked` | on top | below | portrait and square panels (`set rotation 0`, 480x480, ...) |

A layout the panel's shape cannot hold falls back (for example `stacked` on a wide panel).

**Bigger "CYD" boards (3.5", 480x320) are experimental.** `set panel st7796` (or `ili9488`),
`set backlight 27` and `set touch shared` select what those boards use; the pins are the common ones
for ESP32-3248S035-style boards and have not been verified on hardware here. Capacitive touch
(GT911/FT6x36 on I2C), P4 boards and RGB-parallel panels are not supported.

**ESP32-S3: Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448 AMOLED) — SIMULATOR ONLY.** The firmware
builds for this board (an S3 build selects it automatically: QSPI AMOLED, CST820 / FT3168 touch,
portrait with the stacked layout, no RGB LED or light sensor). It has only ever run in the
[esp32sim](https://github.com/mikroverk/esp32sim) emulator (`scripts/emu-smoke.sh 60 s3`), where it
boots, joins WiFi, fetches and draws; it has **not been run on the real board**, the panel power-up
(AXP2101 / TCA9554) is left as the board starts, and the emulator's panel model does not show windowed
drawing from LovyanGFX. Build it with
`arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc firmware/jf_flight_display`;
`set panel sh8601` selects the older V1 panel (also untested). No S3 binary is published in releases.

`status` shows why the board last restarted (power on, software, panic, watchdog, **brownout**), the
largest free block of memory and the number of crashes in a row. A **brownout** usually means a weak
USB port or cable: try another port, or lower the LED with `set led 30`. After three crashes in a row
the board starts in **safe mode** (no photos, no 3D model, text only) so it can still be reached;
five stable minutes, or `safe off`, bring it back to normal.

## Web portal
`web/` is a small static site that flashes the firmware from the browser ([ESP Web Tools](https://github.com/esphome/esp-web-tools))
and then sends the WiFi name, password and a few optional settings to the board over USB (Web Serial). It
is built from the latest release by `.github/workflows/pages.yml` and published with GitHub Pages
(*Settings, Pages, Source: GitHub Actions*; the repository has to be public). Build it locally with
`python3 scripts/build_site.py --dist dist --out build/site`.

Each board's files come from `firmware/boards.json`, which also holds its test status; the portal shows that
status and asks for confirmation before installing a board nobody has tried on real hardware.

**Forks and flashes.** Forks and stars are read live from GitHub by the page. Finished installs are counted by
an optional tiny Cloudflare Worker (`workers/flash-counter/`, one number per board, nothing about the visitor);
without it the page simply shows no flash count.

## Build from source
Needs `arduino-cli`, Go (optional proxy only), and the ESP32 core.

```sh
scripts/setup.sh        # core + libraries, creates firmware/jf_flight_display/secrets.h
# edit secrets.h (WiFi, contact) or configure over serial after flashing
scripts/lint.sh && scripts/test.sh && scripts/build.sh
arduino-cli upload -p <port> --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --input-dir build/firmware
```
`scripts/test.sh` runs host-side tests for the parsing, aircraft table, 3D meshes, airline table and
serial commands. See [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Releases
Versions follow [Semantic Versioning](https://semver.org/) and live as git tags (`v0.1.0`); the
firmware reports its version at boot and in `status`.

1. Add a section to [CHANGELOG.md](CHANGELOG.md) and write `docs/releases/<version>.md`.
2. Merge to `main` and wait for CI to pass.
3. Run `scripts/tag-release.sh <version>`: it checks everything, then pushes the tag.
4. The Release workflow builds the binaries from a clean checkout (no secrets), verifies them, and
   publishes a `-merged.bin` and `-firmware.bin` for every board in `firmware/boards.json`, and `SHA256SUMS`.
   The Pages workflow then rebuilds the [web portal](#web-portal) from that release.

## Data sources and credits
- Aircraft positions: [adsb.fi](https://adsb.fi) open data (personal, non-commercial use).
- Routes: [adsbdb](https://www.adsbdb.com) (community data: can be wrong or missing).
- Photos (optional, fetched while the board runs): the [planespotters.net](https://www.planespotters.net) photo API. Each photo belongs to its photographer, who is credited on screen; no photos are stored in this repository.
- Location: [geojs.io](https://www.geojs.io) (sends your public IP; use a static location to avoid it).
- Inspired by [suvl/ledmatrix](https://github.com/suvl/ledmatrix) and
  [dmellok/tesserae-widget-overhead](https://github.com/dmellok/tesserae-widget-overhead); no code was
  copied from either.

## Limits
No airline logos or lettering (trademarks, and the screen is too small), no AI-painted images, no
weather. Liveries and colours are approximations. The optional Go service in `service/` is a simple
proxy the firmware does not use.

## Licence
The code in this repository is released under the [MIT License](LICENSE). The data it shows has its own
terms: adsb.fi open data is for personal, non-commercial use, and the photos belong to their photographers
(see the credits above). The firmware builds on LovyanGFX (FreeBSD licence), ArduinoJson (MIT) and
TJpg_Decoder; the web portal loads ESP Web Tools (Apache-2.0).
