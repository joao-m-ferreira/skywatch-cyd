#include "flights.h"

#include <ArduinoJson.h>
#include <math.h>
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

void adsbFilter(JsonDocument& filter) {
  JsonObject a = filter["ac"][0].to<JsonObject>();
  a["flight"] = true;
  a["t"] = true;
  a["r"] = true;
  a["lat"] = true;
  a["lon"] = true;
  a["alt_baro"] = true;
  a["gs"] = true;
  a["track"] = true;
}

static double rad(double d) { return d * M_PI / 180.0; }

static void distBearing(double lat1, double lon1, double lat2, double lon2, double* km,
                        int* brg) {
  double dLat = rad(lat2 - lat1), dLon = rad(lon2 - lon1);
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(rad(lat1)) * cos(rad(lat2)) * sin(dLon / 2) * sin(dLon / 2);
  *km = 2 * 6371.0 * asin(sqrt(a));
  double y = sin(dLon) * cos(rad(lat2));
  double x = cos(rad(lat1)) * sin(rad(lat2)) - sin(rad(lat1)) * cos(rad(lat2)) * cos(dLon);
  double b = fmod(atan2(y, x) * 180.0 / M_PI + 360.0, 360.0);
  *brg = static_cast<int>(lround(b)) % 360;
}

int adsbToFlights(const JsonDocument& doc, double lat, double lon, double radius_km,
                  Flight* out, size_t max) {
  JsonArrayConst arr = doc["ac"].as<JsonArrayConst>();
  if (arr.isNull() || max == 0) return 0;
  size_t n = 0;
  for (JsonObjectConst o : arr) {
    if (!o["lat"].is<double>() || !o["lon"].is<double>()) continue;
    if (!o["alt_baro"].is<int>()) continue;  // "ground" or missing: not airborne
    double km;
    int brg;
    distBearing(lat, lon, o["lat"].as<double>(), o["lon"].as<double>(), &km, &brg);
    if (km > radius_km) continue;
    if (n == max && static_cast<float>(km) >= out[n - 1].dist_km) continue;
    Flight f;
    const char* cs = o["flight"] | "";
    while (*cs == ' ') ++cs;
    copyStr(f.callsign, sizeof f.callsign, cs);
    for (int i = static_cast<int>(strlen(f.callsign)) - 1; i >= 0 && f.callsign[i] == ' '; --i)
      f.callsign[i] = '\0';
    if (f.callsign[0] == '\0') copyStr(f.callsign, sizeof f.callsign, o["r"] | "");
    if (f.callsign[0] == '\0') continue;
    copyStr(f.type, sizeof f.type, o["t"] | "");
    f.alt_ft = o["alt_baro"].as<int>();
    f.speed_kt = static_cast<int>(lround(o["gs"] | 0.0));
    f.track = static_cast<int>(lround(o["track"] | 0.0));
    f.dist_km = static_cast<float>(lround(km * 10) / 10.0);
    f.bearing = brg;
    size_t i = (n < max) ? n++ : max - 1;  // insert sorted, dropping the farthest
    while (i > 0 && out[i - 1].dist_km > f.dist_km) {
      out[i] = out[i - 1];
      --i;
    }
    out[i] = f;
  }
  return static_cast<int>(n);
}

int parseAdsb(const char* json, size_t len, double lat, double lon, double radius_km,
              Flight* out, size_t max) {
  JsonDocument filter, doc;
  adsbFilter(filter);
  if (json == nullptr ||
      deserializeJson(doc, json, len, DeserializationOption::Filter(filter)))
    return -1;
  if (doc["ac"].isNull()) return -1;
  return adsbToFlights(doc, lat, lon, radius_km, out, max);
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
