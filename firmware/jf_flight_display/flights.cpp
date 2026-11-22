#include "flights.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace jf {

static void copyStr(char* dst, size_t n, const char* src) {
  if (n == 0) return;
  if (src == nullptr) src = "";
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

int parseFlights(const char* json, size_t len, Flight* out, size_t max) {
  JsonDocument doc;
  if (json == nullptr || deserializeJson(doc, json, len)) return -1;
  JsonArrayConst arr = doc["f"].as<JsonArrayConst>();
  if (arr.isNull()) return -1;
  size_t n = 0;
  for (JsonObjectConst o : arr) {
    if (n >= max) break;
    if (!o["cs"].is<const char*>()) continue;  // validate at the boundary
    Flight& f = out[n];
    copyStr(f.callsign, sizeof f.callsign, o["cs"]);
    copyStr(f.type, sizeof f.type, o["ty"] | "");
    f.alt_ft = o["alt"] | 0;
    f.speed_kt = o["spd"] | 0;
    f.track = o["trk"] | 0;
    f.dist_km = o["km"] | 0.0f;
    f.bearing = o["brg"] | 0;
    ++n;
  }
  return static_cast<int>(n);
}

void formatAltitude(int alt_ft, char* buf, size_t n) {
  if (alt_ft >= 6000) {
    snprintf(buf, n, "FL%03d", alt_ft / 100);
  } else {
    snprintf(buf, n, "%d ft", alt_ft);
  }
}

const char* compass(int bearing) {
  static const char* const kNames[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int b = ((bearing % 360) + 360) % 360;
  return kNames[((b + 22) / 45) % 8];
}

void formatDistance(float km, char* buf, size_t n) {
  if (km < 100.0f) {
    snprintf(buf, n, "%.1f km", static_cast<double>(km));
  } else {
    snprintf(buf, n, "%d km", static_cast<int>(km + 0.5f));
  }
}

int backlightFor(int ldr_raw) {
  if (ldr_raw < 0) ldr_raw = 0;
  if (ldr_raw > 4095) ldr_raw = 4095;
  return 20 + (ldr_raw * 235) / 4095;
}

}  // namespace jf
