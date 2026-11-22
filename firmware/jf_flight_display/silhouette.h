// Aircraft classification and a table of real aircraft dimensions (the input of the 3D
// model in model3d.h). Host-testable.
#pragma once
#include <stddef.h>

namespace jf {

enum class Category { Jet, Wide, Prop, Light, Biz, Heli };

// Classifies an ICAO type designator ("A320", "B77W", "C172", "EC35"...).
// Unknown or empty types default to Jet.
Category categorize(const char* icao_type);

enum class Mount { UnderWing = 0, Rear = 1, Prop = 2, Heli = 3 };

// Main dimensions of one aircraft type, in metres / degrees.
struct AircraftSpec {
  const char* prefix;  // matched against the start of the ICAO type designator
  float length;
  float span;
  float sweep_deg;    // leading-edge sweep
  float root_chord;
  float tip_chord;
  float fuselage_w;
  float wing_pos;     // wing root leading edge as a fraction of length from the nose
  int engines;        // 0 (none drawn / nose engine), 2, 3 (rear tri-jet), 4 or 6
  Mount mount;
  float nacelle_w;
  int deck;           // 0 normal, 1 upper-deck hump (747), 2 full double deck (A380)
};

// Best spec for a type; falls back to a representative spec for its Category.
const AircraftSpec& specFor(const char* icao_type);
// The table itself (for tests).
size_t specCount();
const AircraftSpec& specAt(size_t i);

}  // namespace jf
