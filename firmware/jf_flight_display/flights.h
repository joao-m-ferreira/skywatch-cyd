// Host-testable flight model: parsing the service JSON and formatting text.
// Must not include Arduino headers.
#pragma once
#include <stddef.h>

#include <ArduinoJson.h>

namespace jf {

constexpr size_t kMaxFlights = 8;

struct Flight {
  char callsign[12];
  char type[8];
  int alt_ft;
  int speed_kt;
  int track;
  float dist_km;
  int bearing;
};

// Keys to keep when parsing an ADS-B (adsb.fi / adsb.lol v2) response, so a large
// payload fits in RAM. Pass to DeserializationOption::Filter.
void adsbFilter(JsonDocument& filter);
// True when the document has an aircraft list (key "aircraft" or "ac").
bool adsbValid(const JsonDocument& doc);
// Turns a (filtered) ADS-B document into the nearest airborne flights, nearest first:
// drops ground/no-position aircraft and anything beyond radius_km. Returns the count.
int adsbToFlights(const JsonDocument& doc, double lat, double lon, double radius_km,
                  Flight* out, size_t max);
// Convenience for tests: filter + parse + convert from a buffer; -1 if invalid.
int parseAdsb(const char* json, size_t len, double lat, double lon, double radius_km,
              Flight* out, size_t max);

// "FL120" at or above 6000 ft (typical transition), else "3500 ft".
void formatAltitude(int alt_ft, char* buf, size_t n);
// 8-point compass name for a bearing in degrees ("N", "NE", ...).
const char* compass(int bearing);
// "12.3 km" under 100 km, else "123 km".
void formatDistance(float km, char* buf, size_t n);

// LDR reading (0..4095, higher = brighter) to backlight duty 20..255.
int backlightFor(int ldr_raw);

}  // namespace jf
