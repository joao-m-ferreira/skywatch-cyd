# jf-flight-display

Nearby flights on an ESP32-2432S028 ("Cheap Yellow Display"). Ideas from [suvl/ledmatrix](https://github.com/suvl/ledmatrix) and [dmellok/tesserae-widget-overhead](https://github.com/dmellok/tesserae-widget-overhead); no code copied from either.

1. `scripts/setup.sh`, then edit `firmware/jf_flight_display/secrets.h` (WiFi, service URL, location).
2. Run the service on a machine the display can reach: `cd service && go run .`
3. Flash manually (board on `/dev/cu.wchusbserial*`):
   `arduino-cli compile --fqbn esp32:esp32:esp32 firmware/jf_flight_display && arduino-cli upload -p <port> --fqbn esp32:esp32:esp32 firmware/jf_flight_display`
   A full-flash backup of the original firmware is in `../jf-espxx-misc/esp32-2432-generic/firmware-backup`.

Develop: `scripts/lint.sh`, `scripts/test.sh`, `scripts/build.sh`. See `CONTRIBUTING.md`.
Data: adsb.fi open data (personal, non-commercial use), https://adsb.fi.
