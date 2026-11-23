// Tiny 3D aircraft model: builds a coloured triangle mesh from an AircraftSpec and
// projects it (perspective, flat shading, painter's algorithm) to 2D triangles.
// No Arduino headers; host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "livery.h"
namespace jf {

struct Rgb {
  uint8_t r, g, b;
};

// Livery colours baked into the mesh.
struct Palette {
  Rgb hull, belly, primary, secondary, glass, dark;
  Rgb motif;        // tail emblem colour
  Rgb engine;       // engine cowling colour
  Rgb fin;          // fin base colour
  uint8_t nose_red; // 1 = nose cone in the primary colour
  uint8_t gear;     // 1 = landing gear down (drawn when the plane is low and slow)
  uint8_t tail;     // jf::Tail
  uint8_t body;     // jf::Body
  Rgb tip;          // winglet / wingtip colour
};

constexpr size_t kMaxVerts = 900;    // the largest mesh in the table (An-225, gear down) has ~810
constexpr size_t kMaxTris = 1250;    // ... and ~1160

struct Vertex {
  float x, y, z;  // x forward (nose +x), y to the right wing, z up; metres
};
struct Tri {
  uint16_t a, b, c;
  Rgb color;
  // Overlay (window, door): lies on the fuselage; drawn after the body and only when its
  // outward direction (ox, oy, oz) faces the camera. 0,0,0 for ordinary triangles.
  int8_t ox, oy, oz;
};
struct Mesh {
  Vertex v[kMaxVerts];
  Tri t[kMaxTris];
  size_t nv, nt;
  float length;  // aircraft length in metres (used to scale small aircraft down)
};

// Builds the mesh for an ICAO type. Static storage: valid until the next call.
const Mesh& buildAircraft(const char* icao_type, const Palette& palette);

struct ScreenTri {
  int16_t x[3], y[3];
  Rgb color;    // already shaded
  float depth;  // mean view depth (larger = farther); used for sorting
};

// Projects the mesh into a (box_w x box_h) box centred at (cx, cy), nose to the right
// unless !facing_right. Triangles are written far-to-near. Returns how many. Uses static
// scratch (not reentrant); `out` must hold kMaxTris entries.
size_t projectMesh(const Mesh& mesh, bool facing_right, int cx, int cy, int box_w, int box_h,
                   ScreenTri* out, size_t cap);

}  // namespace jf
