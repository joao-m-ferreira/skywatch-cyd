// Airline identification from an ICAO callsign and brand colours for the livery.
// Brand colours only (no logos). Host-testable, no Arduino headers.
#pragma once

#include <stddef.h>

#include "livery.h"
namespace jf {

struct Airline {
  const char* icao;  // 3-letter ICAO designator, e.g. "RYR"
  const char* name;  // short display name
  unsigned char r1, g1, b1;  // primary colour (fuselage, wings)
  unsigned char r2, g2, b2;  // secondary colour (tail, engines)
};

// Looks up the airline for a callsign like "RYR8XY" (3 letters, then a digit).
// Returns nullptr for registrations ("CS-DXX"), unknown operators, or null input.
const Airline* airlineFor(const char* callsign);
// Livery design for an airline (defaults for those without a specific design).
Livery liveryFor(const Airline* airline);
// True for "ABC123"-style callsigns (3 letters + digit): an operator code we may not know.
bool looksLikeAirlineCallsign(const char* callsign);
// The livery table (for tests).
size_t liveryCount();
const char* liveryCode(size_t i);
// The table itself (for tests).
size_t airlineCount();
const Airline& airlineAt(size_t i);

}  // namespace jf
