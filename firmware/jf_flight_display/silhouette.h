// Per-type vector aircraft silhouettes (side view), generated from real
// dimensions, plus aircraft classification. Original geometry; host-testable.
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
  int engines;        // 0 (none drawn / nose engine), 2 or 4
  Mount mount;
  float nacelle_w;
};

// Best spec for a type; falls back to a representative spec for its Category.
const AircraftSpec& specFor(const char* icao_type);
// The table itself (for tests).
size_t specCount();
const AircraftSpec& specAt(size_t i);

constexpr size_t kMaxPolyPoints = 10;
constexpr size_t kMaxPolys = 16;

// Roles of a polygon, which decide how it is painted.
enum Part {
  kHull = 0,       // fuselage (hull colour, outlined)
  kSurface = 1,    // wings, tailplane (hull colour, outlined)
  kFin = 2,        // vertical tail (primary livery colour, outlined)
  kEngine = 3,     // engine nacelles (secondary livery colour, outlined)
  kGear = 4,       // landing gear, only drawn when the gear is down (dark)
  kWindows = 5,    // window row: a dashed line from point 0 to point 1
  kGlass = 6,      // cockpit windshield
  kStripe = 7,     // livery cheatline (primary colour)
  kDark = 8        // propellers, rotors, skids (dark)
};

// A convex polygon in metres, side view: x along the aircraft with the nose at
// x = -length/2 (pointing left), y down, y = 0 on the fuselage centre line.
struct Poly {
  size_t n;
  float x[kMaxPolyPoints];
  float y[kMaxPolyPoints];
  int part;
};

struct Shape {
  const Poly* polys;  // drawn in order, later ones on top
  size_t count;
  float scale;        // pixels per metre so the aircraft fits the drawing area
  float half_len;     // all points have |x| <= half_len (metres)
  float top, bottom;  // vertical extent in metres (top < 0 < bottom)
};

// Builds the side-view silhouette for a type. The result lives in static storage and is
// valid until the next call (not reentrant).
const Shape& shapeForType(const char* icao_type);

// Maps shape coordinates to the screen. With facing_right the aircraft is mirrored so
// its nose points right.
void transformSide(float x, float y, bool facing_right, float scale, int cx, int cy, int* sx,
                   int* sy);

}  // namespace jf
