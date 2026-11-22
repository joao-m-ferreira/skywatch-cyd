// Origin / destination of a flight from adsbdb.com (by callsign, cached in RAM).
#pragma once
#include <stddef.h>

namespace jf {

struct Route {
  char callsign[12];
  char o_iata[5], o_city[28];  // origin
  char d_iata[5], d_city[28];  // destination
  bool tried;                  // looked up already (also when nothing was found)
  bool ok;                     // origin and/or destination known
};

// The cached route for a callsign, or nullptr when unknown (yet).
const Route* routeFor(const char* callsign);
// Looks the callsign up (blocking, ~1-3 s) unless it was tried before. True when a new
// route became available during this call.
bool routePrefetch(const char* callsign);

}  // namespace jf
