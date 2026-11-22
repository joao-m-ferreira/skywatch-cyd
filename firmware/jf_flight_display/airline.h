// Airline identification from an ICAO callsign and brand colours for the livery.
// Brand colours only (no logos). Host-testable, no Arduino headers.
#pragma once

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

}  // namespace jf
