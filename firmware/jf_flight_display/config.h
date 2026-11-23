// Runtime settings and the USB-serial command parser. No Arduino headers: host-testable.
#pragma once
#include <stddef.h>

namespace jf {

struct Config {
  char ssid[33];
  char pass[65];
  double lat;
  double lon;
  int radius_km;
  char contact[64];  // planespotters.net asks for a contact in every request
  bool night_on;
  int night_start;   // hour 0..23 the screen goes off
  int night_end;     // hour 0..23 it comes back
  int wake_min;      // minutes a touch wakes the screen during the night window
  char tz[48];       // POSIX TZ string for the clock
  bool auto_loc;     // true: work out the location from the public IP (until lat/lon are set)
  bool tz_set;       // true once the user chose a timezone (never overridden by geo-IP)
  // Board variant (the "CYD" is sold with several panels). Changing these restarts the board.
  int panel;         // 0 = ILI9341 (the single-USB ESP32-2432S028R), 1 = ST7789 (two-USB variant)
  bool invert;       // colour inversion (some panels need it)
  bool rgb_swap;     // swap red and blue
  int rotation;      // 1 or 3 (3 = upside down)
  int led_pct;       // RGB LED brightness 0..100 (lower it if a weak USB port browns out)
};

enum class CmdAction { None, Changed, Display, Reboot, Factory };  // Display: saved, then restart to apply

constexpr const char* kDefaultTz = "WET0WEST,M3.5.0/1,M10.5.0";  // Lisbon

// Fills `c` with the build-time defaults (from secrets.h).
void configDefaults(Config* c, const char* ssid, const char* pass, double lat, double lon,
                    int radius_km, const char* contact);

// Executes one command line ("set lat 38.7", "show", ...). Writes a human-readable answer
// to `reply` (may be several lines) and says what the caller must do next.
CmdAction configCommand(Config* c, const char* line, char* reply, size_t reply_size);

// POSIX TZ string for a common IANA timezone name ("Europe/Lisbon"), or nullptr if unknown.
const char* posixTzFor(const char* iana);

// True when a WiFi network has been configured (not empty, not the example placeholder).
bool configWifiReady(const Config& c);

// True when the board has crashed (panic, watchdog, brownout) this many times in a row: start in safe mode.
bool configSafeMode(int consecutive_crashes);

// True when `hour` (0..23) is inside the night window; handles windows across midnight.
bool isNightHour(const Config& c, int hour);

}  // namespace jf
