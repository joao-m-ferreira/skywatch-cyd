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

void adsbFilter(JsonDocument& filter) {
  // adsb.fi uses "aircraft", adsb.lol uses "ac": accept both.
  for (const char* key : {"aircraft", "ac"}) {
    JsonObject a = filter[key][0].to<JsonObject>();
    a["flight"] = true;
    a["t"] = true;
    a["r"] = true;
    a["lat"] = true;
    a["lon"] = true;
    a["alt_baro"] = true;
    a["gs"] = true;
    a["track"] = true;
  }
}

static JsonArrayConst adsbList(const JsonDocument& doc) {
  JsonArrayConst arr = doc["aircraft"].as<JsonArrayConst>();
  return arr.isNull() ? doc["ac"].as<JsonArrayConst>() : arr;
}

bool adsbValid(const JsonDocument& doc) { return !adsbList(doc).isNull(); }

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
  JsonArrayConst arr = adsbList(doc);
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
    copyStr(f.reg, sizeof f.reg, o["r"] | "");
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
  if (!adsbValid(doc)) return -1;
  return adsbToFlights(doc, lat, lon, radius_km, out, max);
}

static void shortCity(const char* city, size_t max, char* out) {  // cut at a word boundary
  if (city == nullptr) city = "";
  size_t len = strlen(city);
  if (len <= max) {
    memcpy(out, city, len + 1);
    return;
  }
  size_t cut = max;
  for (size_t i = max; i > max / 2; --i) {
    if (city[i] == ' ' || city[i] == '-') {
      cut = i;
      break;
    }
  }
  memcpy(out, city, cut);
  out[cut] = '\0';
}

bool formatRoute(const char* o_city, const char* o_iata, const char* d_city, const char* d_iata,
                 char* out, size_t n) {
  if (n == 0) return false;
  out[0] = '\0';
  const bool has_o = (o_iata != nullptr && o_iata[0]) || (o_city != nullptr && o_city[0]);
  const bool has_d = (d_iata != nullptr && d_iata[0]) || (d_city != nullptr && d_city[0]);
  if (!has_o && !has_d) return false;
  const size_t half = n > 8 ? (n - 4) / 2 : 4;  // room for " > " around two names
  char a[32], b[32];
  shortCity(o_city, half, a);
  shortCity(d_city, half, b);
  if (has_o && has_d) {
    if (a[0] && b[0]) snprintf(out, n, "%s > %s", a, b);
    else snprintf(out, n, "%s > %s", (o_iata && o_iata[0]) ? o_iata : a, (d_iata && d_iata[0]) ? d_iata : b);
  } else if (has_o) {
    snprintf(out, n, "from %s", a[0] ? a : o_iata);
  } else {
    snprintf(out, n, "to %s", b[0] ? b : d_iata);
  }
  return true;
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
  return 90 + (ldr_raw * 165) / 4095;  // never below ~35%: a dark room must not look like "off"
}

void formatOledText(const Flight& f, const char* airline_name, bool looks_like_airline, OledText* out) {
  snprintf(out->call, sizeof out->call, "%s", f.callsign);
  snprintf(out->type, sizeof out->type, "%s", f.type[0] ? f.type : "-");
  if (airline_name != nullptr) snprintf(out->airline, sizeof out->airline, "%.16s", airline_name);
  else if (looks_like_airline) snprintf(out->airline, sizeof out->airline, "Operator %.3s", f.callsign);
  else snprintf(out->airline, sizeof out->airline, "Private / other");
  char alt[16], dist[16];
  formatAltitude(f.alt_ft, alt, sizeof alt);
  snprintf(out->alt_speed, sizeof out->alt_speed, "%s  %d kt", alt, f.speed_kt);
  formatDistance(f.dist_km, dist, sizeof dist);
  snprintf(out->dist_dir, sizeof out->dist_dir, "%s %s  %s%03d", dist, compass(f.bearing), compass(f.track), f.track);
}

}  // namespace jf
