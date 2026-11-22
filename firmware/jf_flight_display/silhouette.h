// Per-type vector aircraft silhouettes (top-down, nose up), generated from real
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

constexpr size_t kMaxPolyPoints = 8;
constexpr size_t kMaxPolys = 10;

// A convex polygon in metres: x to the right, y down, nose at y = -length/2.
// When mirror is true the polygon is also drawn reflected across x = 0.
struct Poly {
  size_t n;
  float x[kMaxPolyPoints];
  float y[kMaxPolyPoints];
  bool mirror;
  // 0 fuselage, 1 wings/tailplane (hull colour); 2 fin (primary livery colour);
  // 3 engines (secondary livery colour).
  int part;
};

struct Shape {
  const Poly* polys;  // drawn in order, later ones on top
  size_t count;
  float rotor_radius;   // metres; > 0 draws a rotor ring (helicopters)
  float scale;          // pixels per metre so the aircraft fits the drawing area
  float half_extent;    // all points lie within +-half_extent metres
};

// Builds the silhouette for a type. The result lives in static storage and is valid
// until the next call (not reentrant).
const Shape& shapeForType(const char* icao_type);

// Rotates (x, y) clockwise by heading_deg (0 = north/up, 90 = east/right),
// scales by `scale` and offsets to (cx, cy).
void transformPoint(float x, float y, float heading_deg, float scale, int cx, int cy, int* sx,
                    int* sy);

}  // namespace jf
