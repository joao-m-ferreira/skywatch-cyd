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
// True when the airline has two distinct, real colours (so the LED should alternate between
// them): the secondary must be saturated and bright (not grey/black/white) and differ from the
// primary in hue. Airlines with one dominant colour return false.
bool airlineHasTwoColours(const Airline& airline);

// Colour of the two-colour LED pattern at time `ms`: holds `a` for 1.2 s, fades to `b` in 0.4 s,
// holds `b` for 1.2 s, fades back (3.2 s cycle). Both colours stay clearly visible.
void ledBlend(const int a[3], const int b[3], unsigned long ms, int out[3]);

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
