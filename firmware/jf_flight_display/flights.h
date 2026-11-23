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
  char reg[10];  // registration, e.g. "CS-TNA" (empty if unknown)
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

// Header text for a route, e.g. "Murcia > Dublin". Prefers city names, falls back to IATA codes
// when the cities do not fit in `n`, and handles a missing origin or destination.
// Returns false (and leaves `out` empty) when nothing is known.
bool formatRoute(const char* o_city, const char* o_iata, const char* d_city, const char* d_iata,
                 char* out, size_t n);

// "FL120" at or above 6000 ft (typical transition), else "3500 ft".
void formatAltitude(int alt_ft, char* buf, size_t n);
// 8-point compass name for a bearing in degrees ("N", "NE", ...).
const char* compass(int bearing);
// "12.3 km" under 100 km, else "123 km".
void formatDistance(float km, char* buf, size_t n);

// The text of a 128x64 text-only display (0.96" OLED): every field is at most 16 characters.
struct OledText {
  char call[12];       // "TAP443"
  char type[8];        // "A21N"
  char airline[17];    // "TAP Portugal", "Operator ABC" or "Private / other"
  char alt_speed[32];  // "FL068  259 kt", "3500 ft  120 kt" (at most 16 characters in practice; the room is for the compiler)
  char dist_dir[32];   // "22.2 km S  W271": distance, bearing from you, heading of the plane
};
// airline_name may be null; looks_like_airline says whether the callsign starts with an operator code.
void formatOledText(const Flight& f, const char* airline_name, bool looks_like_airline, OledText* out);

// LDR reading (0..4095, higher = brighter) to backlight duty 90..255.
int backlightFor(int ldr_raw);

}  // namespace jf
