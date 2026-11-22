#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

namespace jf {
namespace {

void copy(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

char* trim(char* s) {
  while (isspace(static_cast<unsigned char>(*s))) ++s;
  char* e = s + strlen(s);
  while (e > s && isspace(static_cast<unsigned char>(e[-1]))) *--e = '\0';
  return s;
}

bool parseDouble(const char* s, double lo, double hi, double* out) {
  char* end = nullptr;
  const double v = strtod(s, &end);
  if (end == s || *end != '\0' || v < lo || v > hi) return false;
  *out = v;
  return true;
}

bool parseInt(const char* s, int lo, int hi, int* out) {
  char* end = nullptr;
  const long v = strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < lo || v > hi) return false;
  *out = static_cast<int>(v);
  return true;
}

const char kHelp[] =
    "Commands (115200 baud, end with Enter):\n"
    "  show                   current settings (password hidden)\n"
    "  status                 uptime, memory, WiFi, clock\n"
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
    "  night test             force night mode for 20 s to try it (not saved)\n"
    "  reboot                 restart the board\n"
    "  factory                forget saved settings (back to build defaults)\n";

}  // namespace

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
  copy(c->tz, sizeof c->tz, kDefaultTz);
  c->auto_loc = true;
  c->tz_set = false;
}

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

bool configWifiReady(const Config& c) {
  return c.ssid[0] != '\0' && strcmp(c.ssid, "your-ssid") != 0;
}

bool isNightHour(const Config& c, int hour) {
  if (!c.night_on || c.night_start == c.night_end) return false;
  if (c.night_start < c.night_end) return hour >= c.night_start && hour < c.night_end;
  return hour >= c.night_start || hour < c.night_end;  // across midnight
}

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
    snprintf(reply, n, "%s", kHelp);
    return CmdAction::None;
  }
  if (strcasecmp(p, "show") == 0) {
    snprintf(reply, n,
             "ssid     %s\npass     %s\nlocation %s\nlat      %.5f\nlon      %.5f\nradius   %d km\n"
             "contact  %s\nnight    %s (%02d:00 - %02d:00)\ntz       %s\n",
             c->ssid[0] ? c->ssid : "(not set)", c->pass[0] ? "****" : "(empty)",
             c->auto_loc ? "auto (geojs.io, from your public IP)" : "static", c->lat, c->lon,
             c->radius_km, c->contact, c->night_on ? "on" : "off", c->night_start, c->night_end,
             c->tz);
    return CmdAction::None;
  }
  if (strcasecmp(p, "reboot") == 0) {
    snprintf(reply, n, "rebooting...\n");
    return CmdAction::Reboot;
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
  } else {
    snprintf(reply, n, "unknown setting '%s' (type help)\n", key);
    return CmdAction::None;
  }
  snprintf(reply, n, "ok, saved (%s)\n", key);
  return CmdAction::Changed;
}

}  // namespace jf
