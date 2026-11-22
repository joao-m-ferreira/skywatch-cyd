// Host-testable flight model: parsing the service JSON and formatting text.
// Must not include Arduino headers.
#pragma once
#include <stddef.h>

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

// Parses {"f":[{cs,ty,alt,spd,trk,km,brg},...]}. Returns the number of flights
// stored (0..max) or -1 when the payload is not valid.
int parseFlights(const char* json, size_t len, Flight* out, size_t max);

// "FL120" at or above 6000 ft (typical transition), else "3500 ft".
void formatAltitude(int alt_ft, char* buf, size_t n);
// 8-point compass name for a bearing in degrees ("N", "NE", ...).
const char* compass(int bearing);
// "12.3 km" under 100 km, else "123 km".
void formatDistance(float km, char* buf, size_t n);

// LDR reading (0..4095, higher = brighter) to backlight duty 20..255.
int backlightFor(int ldr_raw);

}  // namespace jf
