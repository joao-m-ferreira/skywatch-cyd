# Architecture

```
adsb.fi open data --HTTPS--> firmware/ (ESP32 CYD, standalone) --> screen
```

The ESP32 is standalone: no companion server is needed. `service/` is an optional,
independent Go proxy that serves the same data as compact JSON; the firmware does not use it.

- `service/` (optional, unused by the firmware): `GET /v1/flights?lat&lon&radius_km&n` fetches adsb.fi (15 s cache, last-good on failure, coordinates rounded to ~1 km upstream), validates the payload, drops ground/no-position aircraft, returns the nearest `n` as `{"f":[{cs,ty,rg,alt,spd,trk,km,brg}]}`. Optional `X-Token` via `API_TOKEN`.
- `firmware/jf_flight_display/`: `board.h` (pins), `flights.{h,cpp}` (ADS-B parsing + formatting), `silhouette.{h,cpp}` (aircraft class + vector shapes), `airline.{h,cpp}` (callsign to airline name and brand colours), all host-tested; `plate.{h,cpp}` (the plate screen), `display.h` (LovyanGFX setup), `jf_flight_display.ino` (WiFi, polling, touch, LDR). The ESP32 queries `opendata.adsb.fi` directly, reading the body into memory and parsing it with an ArduinoJson filter (parsing straight from the TLS stream proved unreliable) and computing distance, bearing and the nearest-N on device. TLS certificate validation is skipped (`setInsecure`) because only public read-only data is fetched and no credentials are sent.
- `firmware/test/`: host unit tests built with g++ against ArduinoJson.
- `scripts/`: the command contract; `check_*.py` are the docs and release-artifact checks.

Decisions: C++ over MicroPython (compile-time checks, CI); standalone ESP32 (user requirement; no server to keep running), with the Go service kept as an optional proxy; no code copied from references (licences). See `docs/PRD.md`.
