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
};

enum class CmdAction { None, Changed, Reboot, Factory };

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

// True when `hour` (0..23) is inside the night window; handles windows across midnight.
bool isNightHour(const Config& c, int hour);

}  // namespace jf
