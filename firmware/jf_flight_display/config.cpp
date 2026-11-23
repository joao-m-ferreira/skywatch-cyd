#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace jf {
namespace {

// Copies a string into a fixed buffer: always terminated, truncated when too long.
void copy(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

// Strips leading and trailing whitespace in place and returns the start of the trimmed text.
char* trim(char* s) {
  while (isspace(static_cast<unsigned char>(*s))) ++s;
  char* e = s + strlen(s);
  while (e > s && isspace(static_cast<unsigned char>(e[-1]))) *--e = '\0';
  return s;
}

// Parses text that is entirely a number inside [lo, hi]; false (and `out` untouched) otherwise.
bool parseDouble(const char* s, double lo, double hi, double* out) {
  char* end = nullptr;
  const double v = strtod(s, &end);
  if (end == s || *end != '\0' || v < lo || v > hi) return false;
  *out = v;
  return true;
}

// Parses text that is entirely a whole number inside [lo, hi]; false (and `out` untouched) otherwise.
bool parseInt(const char* s, int lo, int hi, int* out) {
  char* end = nullptr;
  const long v = strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < lo || v > hi) return false;
  *out = static_cast<int>(v);
  return true;
}

#ifdef ESP8266
#include <pgmspace.h>  // const data lives in RAM on this chip unless it is PROGMEM: 3 KB for the help text alone
#else
#define PROGMEM
#endif
const char kHelp[] PROGMEM =
    "Commands (115200 baud, end with Enter):\n"
    "  show                   current settings (password hidden)\n"
    "  status                 uptime, memory, WiFi, clock\n"
    "  flights                aircraft currently detected\n"
    "  set ssid <name>        WiFi network name\n"
    "  set pass <password>    WiFi password ('-' = open network)\n"
    "  set location auto      locate from your public IP via geojs.io (default)\n"
    "  set location static <lat> <lon>   fixed position, e.g. static 38.7223 -9.1393\n"
    "  set lat <deg>          latitude only, e.g. 38.7223 (switches to static)\n"
    "  set lon <deg>          your longitude, e.g. -9.1393\n"
    "  set radius <km>        search radius, 1..250\n"
    "  set contact <email>    contact sent to planespotters.net\n"
    "  set tz <posix tz>      e.g. WET0WEST,M3.5.0/1,M10.5.0\n"
    "  set night <from> <to>  screen off between these hours, e.g. set night 23 7\n"
    "  set night off|on       disable / enable night mode\n"
    "  set web on|off         web page on the board (planes + settings), needs the access token; default on\n"
    "  set mcp on|off         MCP server at /mcp (read-only tools for AI assistants), needs the token; default on\n"
    "  set mcpauth on|off     require the token for /mcp in a header (Authorization: Bearer <token> or X-API-Key); default off\n"
    "  set advertise on|off   announce the board on the network (skywatch.local, _mcp._tcp) and serve its public\n"
    "                         Server Card / AI Catalog; default on\n"
    "  token                  print the access token; 'token new' makes a new one\n"
    "  set max <planes>       most planes shown, nearest first, 1..20 (default 10)\n"
    "  set plane <seconds>    how long each plane stays on screen, 3..120 (default 6); small screens split it between altitude, speed and distance\n"
    "  set wake <minutes>     how long a touch wakes the screen at night, 1..120 (default 10)\n"
    "  set panel ili9341|st7789|st7796|ili9488|tdisplay|m5stickc  display controller (two-USB CYD: st7789; 3.5in: st7796/ili9488); restarts\n"
    "  (ESP32-S3 AMOLED board: set panel co5300 (V2, default) or sh8601 (V1))\n"
    "  set i2c <sda> <scl>    OLED (ssd1306) I2C pins, e.g. set i2c 21 22; restarts\n"
    "  set backlight <gpio>   backlight pin (21 on 2.8in boards, 27 on 3.5in); restarts\n"
    "  set touch separate|shared  touch on its own SPI bus (2.8in) or the display's (3.5in); restarts\n"
    "  set invert on|off      colour inversion (colours look negative: toggle it); restarts\n"
    "  set rgb normal|swap    swap red and blue (red looks blue: use swap); restarts\n"
    "  set rotation 0..3      1 landscape, 3 upside down, 0 / 2 portrait; restarts\n"
    "  set layout auto|side|flip|stacked  plane left / plane right / plane on top (auto by shape)\n"
    "  set led <0..100>       RGB LED brightness percent (lower it if the board resets on weak USB)\n"
    "  display test           colour test card to identify your panel variant\n"
    "  safe on|off            force / leave safe mode (no photos or 3D model; used after repeated crashes)\n"
    "  night test             force night mode for 20 s to try it (not saved)\n"
    "  version                firmware version, build date and board\n"
    "  update                 check GitHub for a newer release (needs WiFi)\n"
    "  setup                  open the WiFi setup page (the board makes its own WiFi; see the screen)\n"
    "  reboot                 restart the board\n"
    "  factory                forget saved settings (back to build defaults)\n";

}  // namespace

int configSanitize(Config* c) {
  Config d;
  configDefaults(&d, "", "", 0.0, 0.0, 40, "");  // what anything out of range is replaced with
  int fixed = 0;
  auto clampInt = [&fixed](int* v, int lo, int hi, int fallback) {
    if (*v < lo || *v > hi) {
      *v = fallback;
      ++fixed;
    }
  };
  c->ssid[sizeof c->ssid - 1] = '\0';  // a string without its terminator would run into the next field
  c->pass[sizeof c->pass - 1] = '\0';
  c->contact[sizeof c->contact - 1] = '\0';
  c->tz[sizeof c->tz - 1] = '\0';
  c->token[sizeof c->token - 1] = '\0';
  // A string that starts with 0xFF is erased flash, not text. (WiFi names and passwords may hold any other byte.)
  char* const texts[] = {c->ssid, c->pass, c->contact, c->token};  // (an array: this file avoids <initializer_list>)
  for (char* str : texts) {
    if (static_cast<unsigned char>(str[0]) == 0xFF) {
      str[0] = '\0';
      ++fixed;
    }
  }
  bool tz_ok = c->tz[0] != '\0';  // a POSIX timezone is plain printable ASCII
  for (const char* q = c->tz; *q != '\0'; ++q) tz_ok = tz_ok && *q >= 32 && *q < 127;
  if (!tz_ok) {
    copy(c->tz, sizeof c->tz, kDefaultTz);
    ++fixed;
  }
  // A position outside the globe (or NaN, which fails both comparisons) falls back to "work it out from my address".
  if (!(c->lat >= -90.0 && c->lat <= 90.0) || !(c->lon >= -180.0 && c->lon <= 180.0)) {
    c->lat = c->lon = 0.0;
    c->auto_loc = true;
    ++fixed;
  }
  clampInt(&c->radius_km, 1, 250, d.radius_km);
  clampInt(&c->night_start, 0, 23, d.night_start);
  clampInt(&c->night_end, 0, 23, d.night_end);
  clampInt(&c->wake_min, 1, 120, d.wake_min);
  clampInt(&c->plane_sec, 3, 120, d.plane_sec);
  clampInt(&c->max_planes, 1, kMaxPlanesLimit, d.max_planes);
  clampInt(&c->panel, 0, 8, d.panel);
  clampInt(&c->rotation, 0, 3, d.rotation);
  clampInt(&c->layout, 0, 3, d.layout);
  clampInt(&c->led_pct, 0, 100, d.led_pct);
  clampInt(&c->bl_pin, 0, 48, d.bl_pin);
  clampInt(&c->i2c_sda, 0, 48, d.i2c_sda);
  clampInt(&c->i2c_scl, 0, 48, d.i2c_scl);
  if (c->i2c_sda == c->i2c_scl) {  // one pin cannot be both lines
    c->i2c_sda = d.i2c_sda;
    c->i2c_scl = d.i2c_scl;
    ++fixed;
  }
  return fixed;
}

// See config.h.
const char* panelName(int panel) {
  switch (panel) {
    case 1: return "ST7789";
    case 2: return "ST7796";
    case 3: return "ILI9488";
    case 4: return "CO5300";
    case 5: return "SH8601";
    case 6: return "SSD1306";
    case 7: return "ST7789 T-Display";
    case 8: return "ST7735S M5StickC";
    default: return "ILI9341";
  }
}

void configDefaults(Config* c, const char* ssid, const char* pass, double lat, double lon,
                    int radius_km, const char* contact) {
  *c = Config{};
  copy(c->ssid, sizeof c->ssid, ssid);
  copy(c->pass, sizeof c->pass, pass);
  c->lat = lat;
  c->lon = lon;
  c->radius_km = radius_km;
  copy(c->contact, sizeof c->contact, contact);
  c->night_on = true;
  c->night_start = 23;
  c->night_end = 7;
  c->wake_min = 10;
  c->plane_sec = 6;
  c->max_planes = 10;
  c->web_on = true;
  c->mcp_on = true;
  c->adv_on = true;
  c->mcp_auth = false;
  c->token[0] = '\0';
  copy(c->tz, sizeof c->tz, kDefaultTz);
  c->auto_loc = true;
  c->tz_set = false;
  c->panel = 0;
  c->bl_pin = 21;
  c->i2c_sda = 21;
  c->i2c_scl = 22;
  c->touch_shared = false;
  c->invert = false;
  c->rgb_swap = false;
  c->rotation = 1;
  c->layout = 0;
  c->led_pct = 100;
}

bool configSafeMode(int consecutive_crashes) { return consecutive_crashes >= 3; }

// Maps an IANA timezone name ("Europe/Lisbon") to the POSIX string the clock needs; nullptr when unknown.
const char* posixTzFor(const char* iana) {
  struct Row {
    const char* name;
    const char* tz;
  };
  static const Row kRows[] = {
      {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
      {"Atlantic/Azores", "AZOT1AZOST,M3.5.0/0,M10.5.0/1"},
      {"Atlantic/Madeira", "WET0WEST,M3.5.0/1,M10.5.0"},
      {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
      {"Europe/Dublin", "GMT0IST,M3.5.0/1,M10.5.0"},
      {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3"},
      {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
      {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
      {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
      {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
      {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
      {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
      {"America/Sao_Paulo", "<-03>3"},
      {"Africa/Luanda", "WAT-1"},
      {"Atlantic/Cape_Verde", "<-01>1"},
  };
  if (iana == nullptr) return nullptr;
  for (const Row& r : kRows) {
    if (strcmp(iana, r.name) == 0) return r.tz;
  }
  return nullptr;
}

// True when a WiFi name has been set (the placeholder from secrets.h.example does not count).
bool configWifiReady(const Config& c) {
  return c.ssid[0] != '\0' && strcmp(c.ssid, "your-ssid") != 0;
}

// True when `hour` is inside the night window; the window may cross midnight.
bool isNightHour(const Config& c, int hour) {
  if (!c.night_on || c.night_start == c.night_end) return false;
  if (c.night_start < c.night_end) return hour >= c.night_start && hour < c.night_end;
  return hour >= c.night_start || hour < c.night_end;  // across midnight
}

// See config.h: parses one serial/web command, edits the settings and says what the caller must do next.
CmdAction configCommand(Config* c, const char* line, char* reply, size_t n) {
  char buf[200];
  copy(buf, sizeof buf, line);
  char* p = trim(buf);
  reply[0] = '\0';
  if (*p == '\0') return CmdAction::None;

  char* rest = p;
  while (*rest != '\0' && !isspace(static_cast<unsigned char>(*rest))) ++rest;
  if (*rest != '\0') *rest++ = '\0';
  rest = trim(rest);

  if (strcasecmp(p, "help") == 0 || strcmp(p, "?") == 0) {
#ifdef ESP8266
    strncpy_P(reply, kHelp, n - 1);
    reply[n - 1] = '\0';
#else
    snprintf(reply, n, "%s", kHelp);
#endif
    return CmdAction::None;
  }
  if (strcasecmp(p, "show") == 0) {
    snprintf(reply, n,
             "ssid     %s\npass     %s\nlocation %s\nlat      %.5f\nlon      %.5f\nradius   %d km\n"
             "contact  %s\nnight    %s (%02d:00 - %02d:00)\nwake     %d min (touch at night)\nplane    %d s each, at most %d planes\nweb      %s, mcp %s (header auth %s), advertise %s, token %s\ntz       %s\n"
             "display  %s, invert %s, rgb %s, rotation %d, layout %s, led %d%%, backlight GPIO %d, touch %s, i2c %d/%d\n",
             c->ssid[0] ? c->ssid : "(not set)", c->pass[0] ? "****" : "(empty)",
             c->auto_loc ? "auto (geojs.io, from your public IP)" : "static", c->lat, c->lon,
             c->radius_km, c->contact, c->night_on ? "on" : "off", c->night_start, c->night_end,
             c->wake_min, c->plane_sec, c->max_planes, c->web_on ? "on" : "off", c->mcp_on ? "on" : "off",
             c->mcp_auth ? "on" : "off", c->adv_on ? "on" : "off", c->token[0] ? "set" : "(none)", c->tz, panelName(c->panel), c->invert ? "on" : "off",
             c->rgb_swap ? "swap" : "normal", c->rotation,
             c->layout == 1 ? "side" : c->layout == 2 ? "flip" : c->layout == 3 ? "stacked" : "auto", c->led_pct, c->bl_pin,
             c->touch_shared ? "shared bus" : "own bus", c->i2c_sda, c->i2c_scl);
    return CmdAction::None;
  }
  if (strcasecmp(p, "reboot") == 0) {
    snprintf(reply, n, "rebooting...\n");
    return CmdAction::Reboot;
  }
  if (strcasecmp(p, "token") == 0) {
    if (strcasecmp(rest, "new") == 0) {
      snprintf(reply, n, "making a new access token...\n");
      return CmdAction::NewToken;
    }
    return CmdAction::Token;  // printed by the sketch
  }
  if (strcasecmp(p, "version") == 0) return CmdAction::Version;  // printed by the sketch (it knows the build)
  if (strcasecmp(p, "update") == 0) return CmdAction::Update;    // asks GitHub for the latest release
  if (strcasecmp(p, "setup") == 0) {
    snprintf(reply, n, "setup mode: join the WiFi shown on the screen, then open the address it shows\n");
    return CmdAction::Setup;
  }
  if (strcasecmp(p, "factory") == 0) {
    snprintf(reply, n, "saved settings erased; back to build defaults\n");
    return CmdAction::Factory;
  }
  if (strcasecmp(p, "set") != 0) {
    snprintf(reply, n, "unknown command '%s' (type help)\n", p);
    return CmdAction::None;
  }

  // set <key> <value...>
  char* value = rest;
  while (*value != '\0' && !isspace(static_cast<unsigned char>(*value))) ++value;
  if (*value != '\0') *value++ = '\0';
  value = trim(value);
  const char* key = rest;

  double d;
  int a, b;
  bool display_change = false, redraw_only = false;
  if (strcasecmp(key, "ssid") == 0) {
    if (*value == '\0' || strlen(value) > 32) {
      snprintf(reply, n, "error: ssid must be 1..32 characters\n");
      return CmdAction::None;
    }
    copy(c->ssid, sizeof c->ssid, value);
  } else if (strcasecmp(key, "pass") == 0) {
    if (*value == '\0' || strlen(value) > 64) {
      snprintf(reply, n, "error: password must be 1..64 characters ('-' for an open network)\n");
      return CmdAction::None;
    }
    copy(c->pass, sizeof c->pass, strcmp(value, "-") == 0 ? "" : value);
  } else if (strcasecmp(key, "lat") == 0) {
    if (!parseDouble(value, -90.0, 90.0, &d)) {
      snprintf(reply, n, "error: latitude must be a number from -90 to 90\n");
      return CmdAction::None;
    }
    c->lat = d;
    c->auto_loc = false;
  } else if (strcasecmp(key, "lon") == 0) {
    if (!parseDouble(value, -180.0, 180.0, &d)) {
      snprintf(reply, n, "error: longitude must be a number from -180 to 180\n");
      return CmdAction::None;
    }
    c->lon = d;
    c->auto_loc = false;
  } else if (strcasecmp(key, "radius") == 0) {
    if (!parseInt(value, 1, 250, &a)) {
      snprintf(reply, n, "error: radius must be a whole number of km from 1 to 250\n");
      return CmdAction::None;
    }
    c->radius_km = a;
  } else if (strcasecmp(key, "contact") == 0) {
    if (strlen(value) < 5 || strlen(value) >= sizeof c->contact || strchr(value, ' ') != nullptr) {
      snprintf(reply, n, "error: contact must be an email or URL without spaces\n");
      return CmdAction::None;
    }
    copy(c->contact, sizeof c->contact, value);
  } else if (strcasecmp(key, "tz") == 0) {
    if (*value == '\0' || strlen(value) >= sizeof c->tz) {
      snprintf(reply, n, "error: give a POSIX timezone, e.g. %s\n", kDefaultTz);
      return CmdAction::None;
    }
    copy(c->tz, sizeof c->tz, value);
    c->tz_set = true;
  } else if (strcasecmp(key, "location") == 0) {
    if (strcasecmp(value, "auto") == 0) {
      c->auto_loc = true;
    } else if (strncasecmp(value, "static", 6) == 0 && isspace(static_cast<unsigned char>(value[6]))) {
      char* args = trim(value + 6);
      char* sp = strchr(args, ' ');
      double la, lo;
      if (sp == nullptr) {
        snprintf(reply, n, "error: use 'set location static <lat> <lon>'\n");
        return CmdAction::None;
      }
      *sp = '\0';
      if (!parseDouble(args, -90.0, 90.0, &la) || !parseDouble(trim(sp + 1), -180.0, 180.0, &lo)) {
        snprintf(reply, n, "error: latitude -90..90 and longitude -180..180 expected\n");
        return CmdAction::None;
      }
      c->lat = la;
      c->lon = lo;
      c->auto_loc = false;
    } else {
      snprintf(reply, n, "use: set location auto | set location static <lat> <lon>\n");
      return CmdAction::None;
    }
  } else if (strcasecmp(key, "night") == 0) {
    if (strcasecmp(value, "off") == 0) {
      c->night_on = false;
    } else if (strcasecmp(value, "on") == 0) {
      c->night_on = true;
    } else {
      char* sp = strchr(value, ' ');
      if (sp == nullptr) {
        snprintf(reply, n, "error: use 'set night <from> <to>' (hours 0..23), or on/off\n");
        return CmdAction::None;
      }
      *sp = '\0';
      if (!parseInt(value, 0, 23, &a) || !parseInt(trim(sp + 1), 0, 23, &b)) {
        snprintf(reply, n, "error: night hours must be whole numbers 0..23\n");
        return CmdAction::None;
      }
      c->night_on = true;
      c->night_start = a;
      c->night_end = b;
    }
  } else if (strcasecmp(key, "panel") == 0) {
    if (strcasecmp(value, "ili9341") == 0) c->panel = 0;
    else if (strcasecmp(value, "st7789") == 0) c->panel = 1;
    else if (strcasecmp(value, "st7796") == 0) c->panel = 2;
    else if (strcasecmp(value, "ili9488") == 0) c->panel = 3;
    else if (strcasecmp(value, "m5stickc") == 0) c->panel = 8;  // M5StickC 80x160
    else if (strcasecmp(value, "m5stickc") == 0) c->panel = 8;  // M5StickC 80x160
    else if (strcasecmp(value, "tdisplay") == 0) c->panel = 7;  // LilyGO TTGO T-Display 135x240
    else if (strcasecmp(value, "ssd1306") == 0) c->panel = 6;  // 0.96" OLED, text only
    else if (strcasecmp(value, "co5300") == 0) c->panel = 4;  // ESP32-S3 AMOLED board, V2
    else if (strcasecmp(value, "sh8601") == 0) c->panel = 5;  // ESP32-S3 AMOLED board, V1
    else {
      snprintf(reply, n, "error: panel is ili9341, st7789, st7796, ili9488, ssd1306, tdisplay, m5stickc, co5300 or sh8601\n");
      return CmdAction::None;
    }
    display_change = true;
  } else if (strcasecmp(key, "invert") == 0) {
    if (strcasecmp(value, "on") == 0) c->invert = true;
    else if (strcasecmp(value, "off") == 0) c->invert = false;
    else {
      snprintf(reply, n, "error: invert is on or off\n");
      return CmdAction::None;
    }
    display_change = true;
  } else if (strcasecmp(key, "rgb") == 0) {
    if (strcasecmp(value, "swap") == 0) c->rgb_swap = true;
    else if (strcasecmp(value, "normal") == 0) c->rgb_swap = false;
    else {
      snprintf(reply, n, "error: rgb is normal or swap\n");
      return CmdAction::None;
    }
    display_change = true;
  } else if (strcasecmp(key, "rotation") == 0) {
    if (!parseInt(value, 0, 3, &a)) {
      snprintf(reply, n, "error: rotation is 0, 1, 2 or 3\n");
      return CmdAction::None;
    }
    c->rotation = a;
    display_change = true;
  } else if (strcasecmp(key, "i2c") == 0) {
    char* sp = strchr(value, ' ');
    if (sp == nullptr) {
      snprintf(reply, n, "error: use 'set i2c <sda> <scl>', e.g. set i2c 21 22\n");
      return CmdAction::None;
    }
    *sp = '\0';
    if (!parseInt(value, 0, 39, &a) || !parseInt(trim(sp + 1), 0, 39, &b) || a == b) {
      snprintf(reply, n, "error: two different GPIO numbers 0..39\n");
      return CmdAction::None;
    }
    c->i2c_sda = a;
    c->i2c_scl = b;
    display_change = true;
  } else if (strcasecmp(key, "backlight") == 0) {
    if (!parseInt(value, 0, 39, &a)) {
      snprintf(reply, n, "error: backlight is a GPIO number 0..39\n");
      return CmdAction::None;
    }
    c->bl_pin = a;
    display_change = true;
  } else if (strcasecmp(key, "touch") == 0) {
    if (strcasecmp(value, "shared") == 0) c->touch_shared = true;
    else if (strcasecmp(value, "separate") == 0) c->touch_shared = false;
    else {
      snprintf(reply, n, "error: touch is separate or shared\n");
      return CmdAction::None;
    }
    display_change = true;
  } else if (strcasecmp(key, "layout") == 0) {
    if (strcasecmp(value, "auto") == 0) c->layout = 0;
    else if (strcasecmp(value, "side") == 0) c->layout = 1;
    else if (strcasecmp(value, "flip") == 0) c->layout = 2;
    else if (strcasecmp(value, "stacked") == 0) c->layout = 3;
    else {
      snprintf(reply, n, "error: layout is auto, side, flip or stacked\n");
      return CmdAction::None;
    }
    redraw_only = true;
  } else if (strcasecmp(key, "led") == 0) {
    if (!parseInt(value, 0, 100, &a)) {
      snprintf(reply, n, "error: led is a percent from 0 to 100\n");
      return CmdAction::None;
    }
    c->led_pct = a;
  } else if (strcasecmp(key, "web") == 0 || strcasecmp(key, "mcp") == 0 || strcasecmp(key, "advertise") == 0 ||
             strcasecmp(key, "mcpauth") == 0) {
    bool on;
    if (strcasecmp(value, "on") == 0) on = true;
    else if (strcasecmp(value, "off") == 0) on = false;
    else {
      snprintf(reply, n, "error: %s is on or off\n", key);
      return CmdAction::None;
    }
    if (strcasecmp(key, "web") == 0) c->web_on = on;
    else if (strcasecmp(key, "mcp") == 0) c->mcp_on = on;
    else if (strcasecmp(key, "mcpauth") == 0) c->mcp_auth = on;
    else c->adv_on = on;
  } else if (strcasecmp(key, "token") == 0) {
    const size_t len = strlen(value);
    bool ok = len >= 16 && len < sizeof c->token;
    for (size_t i = 0; ok && i < len; ++i) ok = isalnum(static_cast<unsigned char>(value[i])) != 0;
    if (!ok) {
      snprintf(reply, n, "error: the token is 16 to 32 letters and digits (or use 'token new')\n");
      return CmdAction::None;
    }
    copy(c->token, sizeof c->token, value);
  } else if (strcasecmp(key, "max") == 0) {
    if (!parseInt(value, 1, kMaxPlanesLimit, &a)) {
      snprintf(reply, n, "error: max must be a whole number of planes from 1 to %d\n", kMaxPlanesLimit);
      return CmdAction::None;
    }
    c->max_planes = a;
  } else if (strcasecmp(key, "plane") == 0) {
    if (!parseInt(value, 3, 120, &a)) {
      snprintf(reply, n, "error: plane must be a whole number of seconds from 3 to 120\n");
      return CmdAction::None;
    }
    c->plane_sec = a;
  } else if (strcasecmp(key, "wake") == 0) {
    if (!parseInt(value, 1, 120, &a)) {
      snprintf(reply, n, "error: wake must be a whole number of minutes from 1 to 120\n");
      return CmdAction::None;
    }
    c->wake_min = a;
  } else {
    snprintf(reply, n, "unknown setting '%s' (type help)\n", key);
    return CmdAction::None;
  }
  snprintf(reply, n, display_change ? "ok, saved (%s); restarting to apply\n" : "ok, saved (%s)\n", key);
  return display_change ? CmdAction::Display : redraw_only ? CmdAction::Redraw : CmdAction::Changed;
}

}  // namespace jf
