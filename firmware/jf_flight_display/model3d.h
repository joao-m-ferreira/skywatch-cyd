// Tiny 3D aircraft model: builds a coloured triangle mesh from an AircraftSpec and
// projects it (perspective, flat shading, painter's algorithm) to 2D triangles.
// No Arduino headers; host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace jf {

struct Rgb {
  uint8_t r, g, b;
};

// Livery colours baked into the mesh.
struct Palette {
  Rgb hull, belly, primary, secondary, glass, dark;
};

constexpr size_t kMaxVerts = 480;  // largest mesh in the table is 420
constexpr size_t kMaxTris = 800;    // ... and 732

struct Vertex {
  float x, y, z;  // x forward (nose +x), y to the right wing, z up; metres
};
struct Tri {
  uint16_t a, b, c;
  Rgb color;
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
// unless !facing_right. Triangles are written far-to-near. Returns how many. Uses a
// temporary heap buffer (freed before returning); `out` must hold kMaxTris entries.
size_t projectMesh(const Mesh& mesh, bool facing_right, int cx, int cy, int box_w, int box_h,
                   ScreenTri* out, size_t cap);

}  // namespace jf
