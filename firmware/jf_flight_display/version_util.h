// Comparing release versions ("v0.5.0" vs "0.4.0"). No Arduino headers: host-testable.
#pragma once
#include <ctype.h>
#include <stdlib.h>

namespace jf {

// Parses "v1.2.3" / "1.2" into up to three numbers. False if there is no leading number (e.g. "dev").
inline bool parseVersion(const char* s, int out[3]) {
  if (*s == 'v' || *s == 'V') ++s;
  if (!isdigit(static_cast<unsigned char>(*s))) return false;
  for (int i = 0; i < 3; ++i) {
    out[i] = 0;
    if (isdigit(static_cast<unsigned char>(*s))) {
      char* end = nullptr;
      out[i] = static_cast<int>(strtol(s, &end, 10));
      s = end;
    }
    if (*s == '.') ++s;
  }
  return true;
}

// True if `latest` is a newer release than `current`. A development build ("dev", "emu") is never
// reported as out of date: its version is unknown.
inline bool versionNewer(const char* latest, const char* current) {
  int a[3], b[3];
  if (!parseVersion(latest, a) || !parseVersion(current, b)) return false;
  for (int i = 0; i < 3; ++i) {
    if (a[i] != b[i]) return a[i] > b[i];
  }
  return false;
}

}  // namespace jf
