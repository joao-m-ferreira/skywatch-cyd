// Vector aircraft silhouettes (top-down, nose up) and aircraft classification.
// Original geometry; host-testable, no Arduino headers.
#pragma once
#include <stddef.h>

namespace jf {

enum class Category { Jet, Wide, Prop, Light, Biz, Heli };

// Classifies an ICAO type designator ("A320", "B77W", "C172", "EC35"...).
// Unknown or empty types default to Jet.
Category categorize(const char* icao_type);

constexpr size_t kMaxPolyPoints = 8;

// A convex polygon in unit space: x to the right, y down, nose at y = -1.
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
  const Poly* polys;
  size_t count;
  float rotor_radius;  // > 0 draws a rotor disc outline (helicopters)
  float scale;         // relative size, 1.0 = full width of the drawing area
};

const Shape& shapeFor(Category c);

// Rotates (x, y) clockwise by heading_deg (0 = north/up, 90 = east/right),
// scales by `scale` pixels per unit and offsets to (cx, cy).
void transformPoint(float x, float y, float heading_deg, float scale, int cx, int cy, int* sx,
                    int* sy);

}  // namespace jf
