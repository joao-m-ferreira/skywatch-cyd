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
  bool web_on;       // serve the dashboard and settings page on the board's address (needs the token); default on
  bool mcp_on;       // serve the MCP endpoint (/mcp, read-only tools; needs the token); default on
  bool mcp_auth;     // require the access token in an HTTP header (Authorization: Bearer, or X-API-Key) for /mcp; default off
  bool adv_on;       // announce the board on the network (mDNS name and _mcp._tcp) and serve the public Server Card; default on
  char token[33];    // access token for both; generated when either is first switched on
  int max_planes;    // most planes shown, nearest first (1..kMaxPlanesLimit)
  int plane_sec;     // seconds each plane stays on screen (carousel); compact screens split it between their data fields
  int wake_min;      // minutes a touch wakes the screen during the night window
  char tz[48];       // POSIX TZ string for the clock
  bool auto_loc;     // true: work out the location from the public IP (until lat/lon are set)
  bool tz_set;       // true once the user chose a timezone (never overridden by geo-IP)
  // Board variant (the "CYD" is sold with several panels). Changing these restarts the board.
  int panel;         // 0 ILI9341 (single-USB ESP32-2432S028R), 1 ST7789 (two-USB), 2 ST7796 / 3 ILI9488 (3.5" 480x320, experimental), 6 SSD1306 0.96" OLED (text only)
  int i2c_sda;       // I2C pins of a 0.96" SSD1306 OLED (panel 6)
  int i2c_scl;
  int bl_pin;        // backlight GPIO (21 on the 2.8" boards, 27 on the 3.5" ones)
  bool touch_shared; // XPT2046 on the display's SPI bus (3.5" boards) instead of its own
  bool invert;       // colour inversion (some panels need it)
  bool rgb_swap;     // swap red and blue
  int rotation;      // 0..3: 1 landscape, 3 landscape upside down, 0 / 2 portrait
  int layout;        // jf::LayoutMode: 0 auto, 1 plane left, 2 plane right, 3 plane on top
  int led_pct;       // RGB LED brightness 0..100 (lower it if a weak USB port browns out)
};

enum class CmdAction { None, Changed, Display, Redraw, Reboot, Factory, Setup, Version, Update, Token, NewToken };  // Display: saved, then restart; Redraw: saved, redraw now; Setup: open the WiFi setup access point; Token: print the access token; NewToken: make a new one

constexpr int kMaxPlanesLimit = 20;  // keep equal to kMaxFlights (flights.h)

constexpr const char* kDefaultTz = "WET0WEST,M3.5.0/1,M10.5.0";  // Lisbon

// A short name of a panel setting ("ILI9341", "M5StickC", ...) for status output.
const char* panelName(int panel);

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
