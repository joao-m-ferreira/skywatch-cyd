# Architecture

```
adsb.fi open data --> service/ (Go) --compact JSON--> firmware/ (ESP32 CYD) --> screen
```

- `service/`: `GET /v1/flights?lat&lon&radius_km&n` fetches adsb.fi (15 s cache, last-good on failure, coordinates rounded to ~1 km upstream), validates the payload, drops ground/no-position aircraft, returns the nearest `n` as `{"f":[{cs,ty,rg,alt,spd,trk,km,brg}]}`. Optional `X-Token` via `API_TOKEN`.
- `firmware/jf_flight_display/`: `board.h` (pins), `flights.{h,cpp}` (parse + formatting, host-tested), `display.h` (LovyanGFX setup), `jf_flight_display.ino` (WiFi, polling, touch, LDR, demo mode). The ESP32 never talks TLS to upstreams.
- `firmware/test/`: host unit tests built with g++ against ArduinoJson.
- `scripts/`: the command contract; `check_*.py` are the docs and release-artifact checks.

Decisions: C++ over MicroPython (compile-time checks, CI); thin client + Go service (keeps TLS/JSON/caching off the ESP32); no code copied from references (licences). See `docs/PRD.md`.
