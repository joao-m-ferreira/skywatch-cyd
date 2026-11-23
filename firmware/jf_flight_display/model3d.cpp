#include "model3d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "silhouette.h"

namespace jf {

// The mesh lives on the heap, allocated once on first use: it is too big for the static DRAM segment
// (which is nearly full), and a per-draw malloc fragmented the heap. Never freed.
static Mesh& gm() {
  static Mesh* p = static_cast<Mesh*>(calloc(1, sizeof(Mesh)));
  if (p == nullptr) abort();  // out of memory at first use: restart rather than draw garbage
  return *p;
}

static float rad(float d) { return d * static_cast<float>(M_PI) / 180.0f; }

static size_t addV(float x, float y, float z) {
  if (gm().nv >= kMaxVerts) return kMaxVerts - 1;
  gm().v[gm().nv] = {x, y, z};
  return gm().nv++;
}
static void addT(size_t a, size_t b, size_t c, Rgb col) {
  if (gm().nt >= kMaxTris) return;
  gm().t[gm().nt++] = {static_cast<uint16_t>(a), static_cast<uint16_t>(b),
                         static_cast<uint16_t>(c), col, 0, 0, 0};
}
// A flat quad lying on a surface whose outward direction is (0, side, 0).
static void addOverlay(float x0, float x1, float z0, float z1, float y, int side, Rgb col) {
  const size_t a = addV(x0, y, z0), b = addV(x1, y, z0), c = addV(x1, y, z1), d = addV(x0, y, z1);
  if (gm().nt + 2 > kMaxTris) return;
  gm().t[gm().nt++] = {static_cast<uint16_t>(a), static_cast<uint16_t>(b), static_cast<uint16_t>(c),
                         col, 0, static_cast<int8_t>(side), 0};
  gm().t[gm().nt++] = {static_cast<uint16_t>(a), static_cast<uint16_t>(c), static_cast<uint16_t>(d),
                         col, 0, static_cast<int8_t>(side), 0};
}
static void addQuad(size_t a, size_t b, size_t c, size_t d, Rgb col) {
  addT(a, b, c, col);
  addT(a, c, d, col);
}

// A slab from two quads (loop p0..p3 and loop q0..q3, q above p).
static void addSlab(const Vertex* p, const Vertex* q, Rgb col) {
  size_t i[8];
  for (int k = 0; k < 4; ++k) i[k] = addV(p[k].x, p[k].y, p[k].z);
  for (int k = 0; k < 4; ++k) i[4 + k] = addV(q[k].x, q[k].y, q[k].z);
  addQuad(i[0], i[1], i[2], i[3], col);
  addQuad(i[4], i[5], i[6], i[7], col);
  addQuad(i[0], i[1], i[5], i[4], col);
  addQuad(i[1], i[2], i[6], i[5], col);
  addQuad(i[2], i[3], i[7], i[6], col);
  addQuad(i[3], i[0], i[4], i[7], col);
}

// Swept, tapered, thick horizontal surface on one side (sign = +1 right, -1 left).
static void addHorizontal(float sign, float y_root, float y_tip, float x_le_root, float x_te_root,
                          float x_le_tip, float x_te_tip, float z_root, float z_tip,
                          float t_root, float t_tip, Rgb col) {
  const Vertex p[4] = {{x_le_root, sign * y_root, z_root},
                       {x_le_tip, sign * y_tip, z_tip},
                       {x_te_tip, sign * y_tip, z_tip},
                       {x_te_root, sign * y_root, z_root}};
  const Vertex q[4] = {{x_le_root, sign * y_root, z_root + t_root},
                       {x_le_tip, sign * y_tip, z_tip + t_tip},
                       {x_te_tip, sign * y_tip, z_tip + t_tip},
                       {x_te_root, sign * y_root, z_root + t_root}};
  addSlab(p, q, col);
}

// Thin vertical surface in the plane y = y0 (fin, winglet), +-half_t thick.
static void addVertical(float x_le0, float x_te0, float z0, float x_le1, float x_te1, float z1,
                        float y0, float half_t, Rgb col) {
  const Vertex p[4] = {{x_le0, y0 - half_t, z0}, {x_le1, y0 - half_t, z1},
                       {x_te1, y0 - half_t, z1}, {x_te0, y0 - half_t, z0}};
  const Vertex q[4] = {{x_le0, y0 + half_t, z0}, {x_le1, y0 + half_t, z1},
                       {x_te1, y0 + half_t, z1}, {x_te0, y0 + half_t, z0}};
  addSlab(p, q, col);
}

// Winglet / fin-like slab from (z0, y0) up to (z1, y1): leading edges x_le0 -> x_le1.
static void addWinglet(float x_le0, float x_te0, float z0, float y0, float x_le1, float x_te1,
                       float z1, float y1, float half_t, Rgb col) {
  const Vertex p[4] = {{x_le0, y0 - half_t, z0}, {x_le1, y1 - half_t, z1},
                       {x_te1, y1 - half_t, z1}, {x_te0, y0 - half_t, z0}};
  const Vertex q[4] = {{x_le0, y0 + half_t, z0}, {x_le1, y1 + half_t, z1},
                       {x_te1, y1 + half_t, z1}, {x_te0, y0 + half_t, z0}};
  addSlab(p, q, col);
}

// A wheel: an 8-sided prism whose axis runs along y, centred at (x, y, z).
static void addWheel(float x, float y, float z, float r, float w, Rgb col) {
  constexpr int M = 8;
  size_t a[M], b[M];
  for (int k = 0; k < M; ++k) {
    const float th = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / M;
    a[k] = addV(x + r * cosf(th), y - w / 2, z + r * sinf(th));
    b[k] = addV(x + r * cosf(th), y + w / 2, z + r * sinf(th));
  }
  const size_t ca = addV(x, y - w / 2, z), cb = addV(x, y + w / 2, z);
  for (int k = 0; k < M; ++k) {
    const int n = (k + 1) % M;
    addQuad(a[k], a[n], b[n], b[k], col);
    addT(ca, a[k], a[n], col);
    addT(cb, b[k], b[n], col);
  }
}

// Cylinder/cone along x from x0 (front) to x1 (back) centred at (y, z).
static void addCyl(float x0, float x1, float y, float z, float r0, float r1, Rgb body,
                   Rgb front_cap) {
  constexpr int M = 8;
  size_t a[M], b[M];
  for (int k = 0; k < M; ++k) {
    const float th = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k) / M;
    a[k] = addV(x0, y + r0 * sinf(th), z + r0 * cosf(th));
    b[k] = addV(x1, y + r1 * sinf(th), z + r1 * cosf(th));
  }
  const size_t c = addV(x0, y, z);
  for (int k = 0; k < M; ++k) {
    const int n = (k + 1) % M;
    addQuad(a[k], a[n], b[n], b[k], body);
    addT(c, a[k], a[n], front_cap);
  }
}

constexpr int kRing = 12;  // vertices per fuselage ring; ring angle 0 = top

struct Ring {
  float x, zc, ry, rz;
};

// Lofts rings (listed nose to tail) into triangles; `seg_color(i, j)` colours the quad
// between ring i and i+1 at ring segment j.
template <typename F>
static void loft(const Ring* rings, size_t n, F seg_color) {
  size_t idx[32][kRing];
  for (size_t i = 0; i < n; ++i) {
    for (int j = 0; j < kRing; ++j) {
      const float th = 2.0f * static_cast<float>(M_PI) * static_cast<float>(j) / kRing;
      idx[i][j] = addV(rings[i].x, rings[i].ry * sinf(th), rings[i].zc + rings[i].rz * cosf(th));
    }
  }
  for (size_t i = 0; i + 1 < n; ++i) {
    for (int j = 0; j < kRing; ++j) {
      const int k = (j + 1) % kRing;
      addQuad(idx[i][j], idx[i][k], idx[i + 1][k], idx[i + 1][j], seg_color(i, j));
    }
  }
}

// Colour of the fin at (u, v): u runs leading edge to trailing edge, v root to tip.
static Rgb finColor(const Palette& pal, float u, float v) {
  bool motif = false;
  switch (pal.tail) {
    case kTailDisc: {
      const float du = (u - 0.5f) * 1.1f, dv = (v - 0.55f) * 0.9f;
      motif = du * du + dv * dv < 0.075f;
      break;
    }
    case kTailDiagonal: {
      const float d = u - 0.55f * v;
      motif = d > 0.15f && d < 0.45f;
      break;
    }
    case kTailStripes: motif = static_cast<int>(v * 6.0f) % 2 == 1; break;
    case kTailSplit: motif = u > 0.5f; break;
    case kTailCross:
      motif = (fabsf(u - 0.5f) < 0.13f && v > 0.22f && v < 0.88f) ||
              (fabsf(v - 0.55f) < 0.11f && u > 0.22f && u < 0.78f);
      break;
    case kTailSolid: break;
    case kTailChecker: motif = (static_cast<int>(u * 5.0f) + static_cast<int>(v * 5.0f)) % 2 == 1; break;
    case kTailDiagSplit: motif = (u - 0.6f * v) > 0.3f; break;
    case kTailCap: motif = v + 0.25f * u > 0.85f; break;  // a motif-coloured cap at the top (Iberia, Norwegian)
    case kTailFacet: {
      // Facets: a hash of the cell picks the colour; gold and brown dominate towards the leading
      // edge and the top, cream and white towards the trailing edge and the root.
      const unsigned iu = static_cast<unsigned>(u * 7.0f), iv = static_cast<unsigned>(v * 6.0f);
      const unsigned h = (iu * 73856093u) ^ (iv * 19349663u) ^ 0x9e3779b9u;
      const float r1 = static_cast<float>((h >> 3) % 100) / 100.0f;
      const float r2 = static_cast<float>((h >> 11) % 100) / 100.0f;
      const float w = (1.0f - u) * 0.6f + v * 0.55f;
      if (r1 * 1.15f < w) return r2 < 0.6f ? pal.primary : pal.secondary;
      if (r2 < 0.5f) return {236, 230, 214};
      return {250, 250, 248};
    }
    case kTailFlag: {
      // UAE flag: green, white and black stripes rising to the trailing edge, red at the front.
      const float t = v - 0.5f * u;
      if (u < 0.3f && v < 0.62f - 0.5f * u) return {200, 16, 46};
      if (u > 0.2f && t > 0.52f) return {0, 122, 61};
      if (u > 0.2f && t > 0.40f) return {250, 250, 250};
      if (u > 0.2f && t > 0.10f) return {22, 22, 24};
      return pal.fin;
    }
    case kTailTap: {
      // Red fin; a green wedge hugs the leading edge from the root (narrowing to the top),
      // parted from the red by a thin white flash, and a small white flag near the top.
      const float edge = 0.62f * (1.0f - v / 0.8f);  // wedge boundary as a function of height
      if (v < 0.8f && u < edge) return pal.motif;
      if (v < 0.85f && u < edge + 0.12f) return {250, 250, 250};
      if (v > 0.55f && v < 0.8f && u > 0.3f && u < 0.52f) return {250, 250, 250};
      return pal.fin;
    }
    default: motif = v < 0.2f; break;  // kTailBand
  }
  return motif ? pal.motif : pal.fin;
}

// Fin as a grid of cells on both faces so a design can be painted on it.
static void addPatternFin(float x_le0, float x_te0, float z0, float x_le1, float x_te1, float z1,
                          float half_t, const Palette& pal) {
  // The TAP design needs a finer grid (a little more than the default; the budget is tight).
  const bool fine = (pal.tail == kTailTap || pal.tail == kTailFlag || pal.tail == kTailFacet || pal.tail == kTailCap) && gm().length < 75.0f;
  constexpr int NU_MAX = 7, NV_MAX = 6;
  const int NU = fine ? NU_MAX : 6, NV = fine ? NV_MAX : 5;
  for (int side = -1; side <= 1; side += 2) {
    size_t id[NV_MAX + 1][NU_MAX + 1];
    for (int j = 0; j <= NV; ++j) {
      const float v = static_cast<float>(j) / NV;
      const float le = x_le0 + (x_le1 - x_le0) * v, te = x_te0 + (x_te1 - x_te0) * v;
      for (int i = 0; i <= NU; ++i) {
        const float u = static_cast<float>(i) / NU;
        id[j][i] = addV(le + (te - le) * u, side * half_t, z0 + (z1 - z0) * v);
      }
    }
    for (int j = 0; j < NV; ++j) {
      for (int i = 0; i < NU; ++i) {
        addQuad(id[j][i], id[j][i + 1], id[j + 1][i + 1], id[j + 1][i],
                finColor(pal, (i + 0.5f) / NU, (j + 0.5f) / NV));
      }
    }
  }
}

static void buildHeli(const AircraftSpec& s, const Palette& pal) {
  const float L = s.length, R = s.span / 2.0f;
  // Skids.
  for (int sg = -1; sg <= 1; sg += 2) {
    addCyl(0.22f * L, -0.22f * L, sg * 0.12f * L, -0.24f * L, 0.014f * L, 0.014f * L, pal.dark, pal.dark);
    addVertical(0.08f * L, 0.06f * L, -0.24f * L, 0.08f * L, 0.06f * L, -0.13f * L, sg * 0.12f * L, 0.006f * L, pal.dark);
    addVertical(-0.10f * L, -0.12f * L, -0.24f * L, -0.10f * L, -0.12f * L, -0.13f * L, sg * 0.12f * L, 0.006f * L, pal.dark);
  }
  // Cabin: an ellipsoid.
  Ring rg[8];
  const float x0 = 0.32f * L, x1 = -0.22f * L;
  for (int i = 0; i < 8; ++i) {
    const float u = static_cast<float>(i) / 7.0f, w = 2.0f * u - 1.0f;
    const float e = sqrtf(fmaxf(0.0f, 1.0f - w * w));
    const float r = fmaxf(0.015f * L, 0.14f * L * e);
    rg[i] = {x0 + (x1 - x0) * u, 0.0f, r, r * 1.1f};
  }
  loft(rg, 8, [&](size_t i, int j) {
    if (i >= 1 && i <= 3 && (j == 11 || j == 0 || j == 1 || j == 10)) return pal.glass;
    if (j == 3 || j == 8) return pal.primary;
    return (j >= 4 && j <= 7) ? pal.belly : pal.hull;
  });
  addCyl(-0.15f * L, -0.48f * L, 0, 0.03f * L, 0.05f * L, 0.022f * L, pal.hull, pal.hull);  // boom
  addVertical(-0.40f * L, -0.48f * L, 0.03f * L, -0.45f * L, -0.51f * L, 0.21f * L, 0, 0.008f * L, pal.primary);
  addCyl(0.01f * L, -0.01f * L, 0, 0.16f * L, 0.012f * L, 0.012f * L, pal.dark, pal.dark);  // mast
  // Rotor blades: two crossed flat blades.
  const float zr = 0.22f * L, bw = 0.02f * L;
  const size_t a0 = addV(R, -bw, zr), a1 = addV(R, bw, zr), a2 = addV(-R, bw, zr), a3 = addV(-R, -bw, zr);
  addQuad(a0, a1, a2, a3, pal.dark);
  const size_t b0 = addV(-bw, R, zr), b1 = addV(bw, R, zr), b2 = addV(bw, -R, zr), b3 = addV(-bw, -R, zr);
  addQuad(b0, b1, b2, b3, pal.dark);
}

// TAP's forward-fuselage logo block: lime, green and red vertical bands from the roof down to the
// belly on both sides, starting behind the cockpit windows. `seg` is the fuselage segment between
// ring seg and seg+1 (rings listed nose to tail).
static bool tapLogo(size_t seg, int j, const Palette& pal, const Ring* rg, float h, float nc,
                    Rgb* out) {
  if (j == 0 || (j >= 5 && j <= 7)) return false;  // roof and belly stay white
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);  // distance of the segment from the nose
  const float s0 = nc;  // the cabin starts here; its rings are evenly spaced, one band per segment
  const float w = fabsf(rg[seg].x - rg[seg + 1].x);
  if (w < 1e-3f || d < s0 || d >= s0 + 4.0f * w) return false;
  if (d < s0 + w) *out = pal.primary;                      // lime
  else if (d < s0 + 2.0f * w) *out = {70, 150, 50};        // green
  else *out = pal.secondary;                               // red
  return true;
}

// easyJet's fuselage: an orange crown from behind the cockpit windows that narrows towards the
// wing, and an orange rear fuselage below a slanting line (here: belly first, then the whole side).
static bool easyBody(size_t seg, int j, const Palette& pal, const Ring* rg, float h, float L, float nc,
                     Rgb* out) {
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);
  const bool top = j == 11 || j == 0 || j == 1;
  const bool crown = top || j == 10 || j == 2;
  const bool front = d >= 0.55f * nc && ((d < 0.45f * L && crown) || (d < 0.58f * L && top));
  const bool rear = (d >= 0.70f * L && j >= 4 && j <= 8) || (d >= 0.80f * L && !top);
  if (!front && !rear) return false;
  *out = pal.primary;
  return true;
}

// Airline name painted along the forward fuselage, just above the windows (both sides).
static bool titleBody(size_t seg, int j, const Ring* rg, float h, float L, float nc) {
  if (j != 2 && j != 10) return false;
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);
  return d >= 0.9f * nc && d < nc + 0.30f * L;
}

// Maersk Air Cargo: a blue lower fuselage that widens into an all-blue rear fuselage, and the
// name in the secondary colour (navy) along the forward fuselage.
static bool maerskBody(size_t seg, int j, const Palette& pal, const Ring* rg, float h, float L, float nc,
                       bool front_stripe, Rgb* out) {
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);
  if (d >= 0.55f * nc && d < nc + 0.28f * L && (j == 2 || j == 10)) {
    *out = pal.secondary;
    return true;
  }
  const bool lower = front_stripe ? (d >= 0.5f * nc && j >= 4 && j <= 8) : (d >= 0.58f * L && j >= 4 && j <= 8);
  const bool rear = (d >= 0.66f * L && j >= 3 && j <= 9) || (d >= 0.76f * L && j >= 2 && j <= 10);
  if (!lower && !rear) return false;
  *out = pal.primary;
  return true;
}

// Emirates: the name in the secondary colour (gold) above the forward windows, and a red streak
// with a black one above it sweeping along the lower rear fuselage.
static bool emiratesBody(size_t seg, int j, const Palette& pal, const Ring* rg, float h, float L, float nc,
                         Rgb* out) {
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);
  if ((j == 2 || j == 10) && d >= 0.9f * nc && d < nc + 0.32f * L) {
    *out = pal.secondary;
    return true;
  }
  if ((j == 4 || j == 8) && d >= 0.66f * L) {
    *out = pal.primary;
    return true;
  }
  if ((j == 3 || j == 9) && d >= 0.82f * L) {
    *out = {22, 22, 24};
    return true;
  }
  return false;
}

// Etihad: the name in the primary colour (gold) above the forward windows, and faceted gold and
// brown patches on the lower rear fuselage and across the rear crown.
static bool etihadBody(size_t seg, int j, const Palette& pal, const Ring* rg, float h, float L, float nc,
                       Rgb* out) {
  const float d = h - 0.5f * (rg[seg].x + rg[seg + 1].x);
  if ((j == 2 || j == 10) && d >= 0.9f * nc && d < nc + 0.28f * L) {
    *out = pal.primary;
    return true;
  }
  const bool rear = d >= 0.66f * L && j >= 3 && j <= 9;
  const bool crown = d >= 0.58f * L && d < 0.72f * L && (j == 11 || j == 0 || j == 1);
  if (!rear && !crown) return false;
  switch ((seg * 7u + static_cast<unsigned>(j) * 3u) % 5u) {
    case 0: case 2: *out = pal.primary; return true;
    case 1: *out = pal.secondary; return true;
    default: return false;
  }
}

enum Winglet { kWNone, kWFence, kWShark, kWBlend, kWScimitar, kWCant };

static bool hasPrefix(const char* t, const char* p) { return t != nullptr && strncmp(t, p, strlen(p)) == 0; }
static bool isNeo(const char* t) {
  return hasPrefix(t, "A19N") || hasPrefix(t, "A20N") || hasPrefix(t, "A21N") ||
         hasPrefix(t, "A338") || hasPrefix(t, "A339") || hasPrefix(t, "B38M") ||
         hasPrefix(t, "B39M") || hasPrefix(t, "B37M");
}
static Winglet wingletFor(const char* t, const AircraftSpec& s) {
  if (s.mount != Mount::UnderWing) return kWNone;
  for (const char* p : {"B748", "B70", "B72", "A124", "A225"}) if (hasPrefix(t, p)) return kWNone;  // raked tips / no winglets
  for (const char* p : {"A306", "A30B", "A310"}) if (hasPrefix(t, p)) return kWFence;
  for (const char* p : {"B37M", "B38M", "B39M", "B3XM"}) if (hasPrefix(t, p)) return kWScimitar;
  for (const char* p : {"A318", "A319", "A320", "A321"}) if (hasPrefix(t, p)) return kWFence;
  for (const char* p : {"A19N", "A20N", "A21N", "A338", "A339", "A35", "BCS"}) if (hasPrefix(t, p)) return kWShark;
  if (hasPrefix(t, "B73") || hasPrefix(t, "B75")) return kWBlend;
  for (const char* p : {"E1", "E7", "E29", "A332", "A333", "A34", "B74", "A388"}) if (hasPrefix(t, p)) return kWCant;
  for (const char* p : {"B77", "B78", "B76"}) if (hasPrefix(t, p)) return kWNone;  // raked tips
  return kWShark;
}

// Per-family shape adaptations on top of the dimensions table: nose and tail-cone shape, engine
// size and how tightly they tuck under the wing, fin size, raked wingtips, T-tails.
struct Style {
  float nose_pow = 1.9f;   // nose profile: 2.0 hemisphere; lower = more pointed, higher = blunter
  float nose_len = 1.0f;   // nose cone length factor
  float tail_len = 1.0f;   // tail cone length factor
  float fin = 1.0f;        // vertical fin height factor
  float eng_w = 1.0f;      // engine diameter factor
  float eng_len = 1.0f;    // engine length factor
  float tuck = 0.45f;      // engine drop below the wing, in nacelle diameters
  bool rake = false;       // raked wingtips (777, 787)
  bool ttail = false;      // horizontal tail on top of the fin
  bool low_wing = false;   // light aircraft with a low wing (Piper, Cirrus, Diamond)
};
static Style styleFor(const char* t, const AircraftSpec& s) {
  Style st;
  st.ttail = s.mount == Mount::Rear || (s.mount == Mount::Prop && s.engines > 0);
  auto any = [&](std::initializer_list<const char*> ps) {
    for (const char* p : ps) {
      if (hasPrefix(t, p)) return true;
    }
    return false;
  };
  if (any({"PA28", "SR2", "DA4"})) {  // low-wing light aircraft (the Cessnas keep their high wing)
    st.low_wing = true; st.nose_pow = 2.2f;
  } else if (any({"A318", "A319", "A320", "A321", "A19N", "A20N", "A21N"})) {
    st.nose_pow = 1.9f; st.tail_len = 0.95f;
  } else if (any({"BCS"})) {  // A220: short blunt nose, big fin and engines
    st.nose_pow = 2.4f; st.nose_len = 0.9f; st.tail_len = 0.9f; st.fin = 1.12f; st.eng_w = 1.15f;
  } else if (any({"B73", "B37", "B38", "B39", "B3X"})) {  // 737: slim engines tucked right up to the wing
    st.nose_pow = 1.95f; st.tail_len = 0.9f; st.fin = 0.95f; st.eng_w = 0.92f; st.tuck = 0.28f;
  } else if (any({"B75"})) {
    st.nose_pow = 1.7f; st.nose_len = 1.1f; st.tail_len = 1.05f; st.eng_w = 1.05f;
  } else if (any({"B77"})) {  // 777: huge engines, raked tips
    st.nose_pow = 1.9f; st.tail_len = 1.1f; st.fin = 0.95f; st.eng_w = 1.22f; st.eng_len = 1.1f; st.rake = true;
  } else if (any({"B78"})) {  // 787: long pointed nose, long nacelles, raked tips
    st.nose_pow = 1.65f; st.nose_len = 1.1f; st.tail_len = 1.12f; st.fin = 0.95f; st.eng_len = 1.25f; st.rake = true;
  } else if (any({"A30", "A31", "A33"})) {  // A300/A310/A330: big engines close to the fuselage
    st.nose_pow = 1.9f; st.eng_w = 1.12f; st.tail_len = 1.05f;
  } else if (any({"A35"})) {  // A350: pointed nose, big fin, long nacelles
    st.nose_pow = 1.65f; st.nose_len = 1.1f; st.tail_len = 1.1f; st.fin = 1.05f; st.eng_len = 1.2f;
  } else if (any({"B748"})) {  // 747-8: raked tips, no winglets
    st.fin = 1.05f; st.rake = true; st.eng_len = 1.15f;
  } else if (any({"B74"})) {
    st.fin = 1.05f;
  } else if (any({"A388"})) {
    st.nose_pow = 2.1f;
  } else if (any({"E17", "E75", "E19", "E29"})) {
    st.nose_pow = 2.1f; st.nose_len = 0.95f; st.eng_w = hasPrefix(t, "E29") ? 1.15f : 0.95f;  // E2: bigger geared fans
  } else if (s.mount == Mount::Prop) {
    st.nose_pow = 2.4f;
  } else if (s.mount == Mount::Rear && s.engines > 0 && s.length < 40.0f) {  // business jets, regional jets
    st.nose_pow = 1.7f; st.nose_len = 1.1f;
  }
  return st;
}

const Mesh& buildAircraft(const char* type, const Palette& pal) {
  const AircraftSpec& s = specFor(type);
  gm().nv = gm().nt = 0;
  gm().length = s.length;
  if (s.mount == Mount::Heli) {
    buildHeli(s, pal);
    return gm();
  }
  const float L = s.length, h = L / 2.0f, D = s.fuselage_w * 1.3f, R = D / 2.0f;  // a little chunkier than real, for legibility
  const float b = s.span / 2.0f, tanL = tanf(rad(s.sweep_deg));
  const Style st = styleFor(type, s);
  const bool high = s.mount == Mount::Prop && !st.low_wing;
  const float nc = 1.6f * D * st.nose_len, tc = 2.6f * D * st.tail_len;

  // --- Fuselage rings, nose to tail. ---
  Ring rg[28];
  size_t nr = 0;
  constexpr int kNose = 8;
  const float nose_u[kNose] = {0.0f, 0.03f, 0.09f, 0.18f, 0.32f, 0.52f, 0.76f, 1.0f};
  for (int i = 0; i < kNose; ++i) {
    const float u = nose_u[i];
    const float r = fmaxf(0.015f * R, R * sqrtf(1.0f - powf(1.0f - u, st.nose_pow)));  // rounded nose: 2.0 is a hemisphere, lower = slimmer
    rg[nr++] = {h - nc * u, -0.12f * R * (1.0f - u), r, r};
  }
  constexpr int kCabin = 14;
  for (int i = 1; i < kCabin - 1; ++i) {
    const float t = static_cast<float>(i) / (kCabin - 1);
    rg[nr++] = {(h - nc) + ((-h + tc) - (h - nc)) * t, 0.0f, R, R};
  }
  const float tail_v[4] = {0.0f, 0.35f, 0.7f, 1.0f};
  for (int i = 0; i < 4; ++i) {
    const float v = tail_v[i];
    const float r = R * (1.0f - 0.88f * powf(v, 1.1f));
    rg[nr++] = {-h + tc - tc * v, 0.5f * R * v * v, r, r};
  }
  // Deck shape: 747 upper-deck hump over the front cabin, A380 full double deck.
  for (size_t i = 0; i < nr; ++i) {
    const float dx = h - rg[i].x;
    if (s.deck == 2) {
      rg[i].rz *= 1.18f;
      rg[i].zc += 0.06f * R * (rg[i].rz > 0.2f * R ? 1.0f : 0.0f);
    } else if (s.deck == 1) {
      float bump = 0.0f;
      if (dx >= 0.07f * L && dx < 0.12f * L) bump = (dx - 0.07f * L) / (0.05f * L);
      else if (dx >= 0.12f * L && dx < 0.24f * L) bump = 1.0f;
      else if (dx >= 0.24f * L && dx < 0.34f * L) bump = (0.34f * L - dx) / (0.10f * L);
      rg[i].rz = R * (rg[i].rz / R) * (1.0f + 0.22f * bump);
      rg[i].zc += 0.22f * R * bump;
    }
  }
  const bool windows = D >= 2.0f;
  const bool neo = isNeo(type);
  const Rgb window = {38, 52, 78};  // dark slate: reads as a continuous window band
  Rgb logo = pal.hull;
  loft(rg, nr, [&](size_t i, int j) {
    if (i >= 5 && i <= 6 && (j == 11 || j == 0 || j == 1)) return pal.glass;  // windshield
    if (pal.nose_red && i < 4) return pal.primary;                       // red nose cone
    if (i == 6 && (j == 10 || j == 2)) return neo ? pal.dark : window;      // cockpit side windows
    if (pal.tail == kTailTap && tapLogo(i, j, pal, rg, h, nc, &logo)) return logo;
    if (pal.body == kBodyEasy && easyBody(i, j, pal, rg, h, L, nc, &logo)) return logo;
    if (pal.body == kBodyMaersk && maerskBody(i, j, pal, rg, h, L, nc, true, &logo)) return logo;
    if (pal.body == kBodyTransat && maerskBody(i, j, pal, rg, h, L, nc, false, &logo)) return logo;
    if (pal.body == kBodyEmirates && emiratesBody(i, j, pal, rg, h, L, nc, &logo)) return logo;
    if (pal.body == kBodyEtihad && etihadBody(i, j, pal, rg, h, L, nc, &logo)) return logo;
    if (pal.body == kBodyTitle && titleBody(i, j, rg, h, L, nc)) return pal.primary;
    if (pal.body == kBodyFull && !(i >= 4 && i <= 6 && j == 0)) return pal.primary;
    if (pal.body == kBodyBelly && i >= 5 && (j >= 3 && j <= 8)) return pal.primary;          // coloured belly
    if ((j == 3 || j == 8) && pal.body == kBodyCheat) return i >= 5 ? pal.primary : pal.hull;  // cheatline
    if (j >= 4 && j <= 7) return pal.belly;
    return pal.hull;
  });

  // --- Wings: swept, tapered, with dihedral (sharklets on under-wing jets). ---
  const float z_root = high ? 0.75f * R : -0.45f * R;
  const float dih = high ? 0.01f * b : 0.07f * b;
  const float y_root = 0.6f * R;
  const float x_le = h - s.wing_pos * L;
  // Wing chords are drawn ~30% larger than real: the fuselage is fattened for legibility too.
  const float rc = s.root_chord * 1.3f, tcd = s.tip_chord * 1.25f;
  const float x_le_tip = x_le - (b - y_root) * tanL, x_te = x_le - rc;
  const float x_te_tip = x_le_tip - tcd;
  const float t_root = 0.15f * rc, t_tip = 0.12f * tcd + 0.03f;
  const Rgb wingc = {234, 232, 228};  // light grey-white whatever the hull tint (real wings are unpainted)  // slightly grey: shows the planform
  auto leAt = [&](float ye) { return x_le - (ye - y_root) * tanL; };
  auto zAt = [&](float ye) { return z_root + dih * (ye - y_root) / (b - y_root); };

  // Engines first (the wing is drawn over their rear part by depth sorting anyway).
  const float nw = s.nacelle_w * 1.35f * (neo ? 1.12f : 1.0f) * st.eng_w;
  if (s.mount == Mount::UnderWing && s.engines >= 2) {
    const float ys6[3] = {0.20f * b, 0.40f * b, 0.62f * b};
    const float ys4[2] = {0.30f * b, 0.62f * b};
    const float ys2[1] = {0.32f * b};
    const float* ys = s.engines == 6 ? ys6 : s.engines == 4 ? ys4 : ys2;
    for (int i = 0; i < s.engines / 2; ++i) {
      const float ye = ys[i], nl = 1.8f * nw * st.eng_len, front = leAt(ye) + (neo ? 0.62f : 0.5f) * nl;
      const float zc = zAt(ye) - st.tuck * nw;
      for (int sg = -1; sg <= 1; sg += 2) {
        addCyl(front, front - nl, sg * ye, zc, nw / 2, nw / 2 * 0.85f, pal.engine, pal.dark);
        addVertical(front - 0.15f * nl, front - 0.85f * nl, zc + 0.4f * nw, front - 0.3f * nl,
                    front - 0.9f * nl, zAt(ye) + 0.02f, sg * ye, 0.04f * nw, pal.hull);  // pylon
      }
    }
  } else if (s.mount == Mount::Rear) {
    const float front = h - 0.58f * L;
    for (int sg = -1; sg <= 1; sg += 2) {
      addCyl(front, front - 3.0f * nw, sg * (R + 0.55f * nw), 0.35f * R, nw / 2, nw / 2 * 0.8f, pal.engine, pal.dark);
    }
    if (s.engines == 3) {  // tri-jet: third engine in the tail, at the base of the fin
      const float fx = -h + 0.30f * L;
      addCyl(fx, fx - 3.2f * nw, 0, 0.95f * R + 0.3f * nw, nw / 2, nw / 2 * 0.8f, pal.engine, pal.dark);
    }
  } else if (s.mount == Mount::Prop && s.engines >= 2) {
    const float ye = 0.30f * b, pl = 5.0f * nw, front = leAt(ye) + 0.55f * pl;
    const float zc = zAt(ye) - 0.2f * nw;
    for (int sg = -1; sg <= 1; sg += 2) {
      addCyl(front, front - pl, sg * ye, zc, 0.6f * nw, 0.5f * nw, pal.engine, pal.engine);
      const float pr = 1.7f * nw, px = front + 0.02f * nw;  // propeller: two crossed blades
      const size_t a0 = addV(px, sg * ye - 0.09f * nw, zc + pr), a1 = addV(px, sg * ye + 0.09f * nw, zc + pr);
      const size_t a2 = addV(px, sg * ye + 0.09f * nw, zc - pr), a3 = addV(px, sg * ye - 0.09f * nw, zc - pr);
      addQuad(a0, a1, a2, a3, pal.dark);
      const size_t c0 = addV(px, sg * ye - pr, zc - 0.09f * nw), c1 = addV(px, sg * ye + pr, zc - 0.09f * nw);
      const size_t c2 = addV(px, sg * ye + pr, zc + 0.09f * nw), c3 = addV(px, sg * ye - pr, zc + 0.09f * nw);
      addQuad(c0, c1, c2, c3, pal.dark);  // second blade: a cross reads as a propeller
    }
  } else if (s.mount == Mount::Prop) {
    const float pr = 0.73f * D, px = h + 0.03f * D;
    const size_t a0 = addV(px, -0.06f * D, pr), a1 = addV(px, 0.06f * D, pr);
    const size_t a2 = addV(px, 0.06f * D, -pr), a3 = addV(px, -0.06f * D, -pr);
    addQuad(a0, a1, a2, a3, pal.dark);
    const size_t c0 = addV(px, -pr, -0.06f * D), c1 = addV(px, pr, -0.06f * D);
    const size_t c2 = addV(px, pr, 0.06f * D), c3 = addV(px, -pr, 0.06f * D);
    addQuad(c0, c1, c2, c3, pal.dark);
  }

  // Wing-body fairing under the wing root (airliners with under-wing engines).
  if (s.mount == Mount::UnderWing && D >= 3.0f) {
    addCyl(x_le - 0.08f * rc, x_te + 0.10f * rc, 0, -0.68f * R, 0.36f * R, 0.28f * R, pal.belly, pal.belly);
  }
  // APU exhaust at the very end of the tail cone.
  if (s.mount != Mount::Prop && D >= 3.0f) {
    addCyl(-h + 0.012f * L, -h - 0.012f * L, 0, 0.5f * R, 0.13f * R, 0.10f * R, pal.dark, pal.dark);
  }
  for (int sg = -1; sg <= 1; sg += 2) {
    if (st.rake) {  // main panel to 84% span, then a more swept, tapered outer panel
      const float yr = 0.84f * b, f = (yr - y_root) / (b - y_root);
      const float xte_r = x_te + (x_te_tip - x_te) * f, tr = t_root + (t_tip - t_root) * f;
      addHorizontal(static_cast<float>(sg), y_root, yr, x_le, x_te, leAt(yr), xte_r, z_root, zAt(yr), t_root, tr, wingc);
      const float le_o = leAt(yr) - (b - yr) * tanf(rad(s.sweep_deg + 32.0f));
      addHorizontal(static_cast<float>(sg), yr, b, leAt(yr), xte_r, le_o, le_o - 0.55f * tcd, zAt(yr), zAt(yr) + 0.02f * b,
                    tr, t_tip, wingc);
    } else {
      addHorizontal(static_cast<float>(sg), y_root, b, x_le, x_te, x_le_tip, x_te_tip, z_root,
                    z_root + dih, t_root, t_tip, wingc);
    }
    const float zt = z_root + dih, S = s.span, ht = 0.03f * D;
    switch (wingletFor(type, s)) {
      case kWFence:  // small fence above and below the tip
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.3f * S * 0.02f, x_te_tip - 0.1f, zt + 0.02f * S, sg * b, ht, pal.tip);
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.2f, x_te_tip - 0.1f, zt - 0.012f * S, sg * b, ht, pal.tip);
        break;
      case kWShark:  // tall, curved, swept back
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.055f * S, x_te_tip - 0.012f * S, zt + 0.05f * S, sg * (b + 0.008f * S), ht, pal.tip);
        break;
      case kWBlend:  // tall blended winglet
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.04f * S, x_te_tip - 0.02f * S, zt + 0.06f * S, sg * (b + 0.012f * S), ht, pal.tip);
        break;
      case kWScimitar:  // split scimitar: blended winglet plus a lower blade
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.045f * S, x_te_tip - 0.015f * S, zt + 0.055f * S, sg * (b + 0.01f * S), ht, pal.tip);
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.03f * S, x_te_tip - 0.01f * S, zt - 0.02f * S, sg * (b + 0.004f * S), ht, pal.tip);
        break;
      case kWCant:  // angled, canted outward
        addWinglet(x_le_tip, x_te_tip, zt, sg * b, x_le_tip - 0.035f * S, x_te_tip - 0.015f * S, zt + 0.04f * S, sg * (b + 0.025f * S), ht, pal.tip);
        break;
      case kWNone: break;
    }
  }

  // --- Tail. ---
  float hf;
  if (s.mount == Mount::Rear) hf = 0.16f * L;
  else if (s.mount == Mount::Prop) hf = s.engines > 0 ? 0.14f * L : 0.12f * L;
  else hf = D >= 5.0f ? 0.12f * L : 0.145f * L;
  hf *= st.fin;
  const float fin_le = -h + 0.16f * L, fin_te = -h + 0.01f * L;
  const float fin_tip_le = fin_le - 0.55f * hf, fin_tip_te = fin_tip_le - 0.07f * L;
  addPatternFin(fin_le, fin_te, 0.55f * R, fin_tip_le, fin_tip_te, R + hf, 0.04f * D, pal);
  {
    const bool straight = s.sweep_deg < 1.0f;
    const float ts = 0.36f * s.span, tcr = (straight ? 0.25f : 0.32f) * ts,
                tct = (straight ? 0.22f : 0.11f) * ts;
    // T-tail: the stabiliser sits on top of the fin tip; otherwise it joins the rear fuselage.
    const float le = st.ttail ? fin_tip_te + 0.75f * tcr : -h + 0.075f * L + tcr, te = le - tcr;
    const float tip_le = le - (ts / 2 - y_root) * tanf(rad(straight ? 5.0f : 33.0f));
    const float zt = st.ttail ? R + 0.95f * hf : 0.35f * R;
    for (int sg = -1; sg <= 1; sg += 2) {
      addHorizontal(static_cast<float>(sg), 0.3f * R, ts / 2, le, te, tip_le, tip_le - tct, zt, zt,
                    0.03f * D, 0.02f * D, pal.hull);
    }
  }
  // --- Landing gear (only when the plane is low and slow). ---
  if (pal.gear) {
    const float leg = (high ? 0.8f : 0.95f) * R, rw = (D >= 5.0f ? 0.11f : 0.14f) * D, ww = 0.55f * rw;
    const float zw = -R - leg;
    const float xn = h - 0.16f * L, xm = x_le - 0.55f * rc, ym = (D >= 5.0f ? 0.17f : 0.21f) * b;
    addVertical(xn + 0.02f * D, xn - 0.02f * D, -0.7f * R, xn + 0.02f * D, xn - 0.02f * D, zw, 0, 0.02f * D, pal.dark);
    addWheel(xn, 0, zw, rw * 0.9f, 2.4f * ww, pal.dark);
    for (int sg = -1; sg <= 1; sg += 2) {
      addVertical(xm + 0.03f * D, xm - 0.03f * D, -0.6f * R, xm + 0.03f * D, xm - 0.03f * D, zw, sg * ym, 0.02f * D, pal.dark);
      addWheel(xm, sg * ym, zw, rw, 3.0f * ww, pal.dark);
      if (D >= 5.0f) addWheel(xm - 2.6f * rw, sg * ym, zw, rw, 3.0f * ww, pal.dark);  // wide-body bogie
    }
  }
  // --- Windows and doors (overlays on both sides). ---
  if (windows) {
    const int N = static_cast<int>(fminf(26.0f, fmaxf(10.0f, roundf(L * 0.6f))));  // scaled to the real count
    const float xs = h - nc * 1.05f, xe = -h + tc * 0.95f, pitch = (xs - xe) / N, ww = 0.8f * pitch;
    const float zc = 0.24f * R, hh = 0.12f * R, yw = sqrtf(R * R - zc * zc);
    const Rgb doorc = {static_cast<uint8_t>(pal.hull.r * 0.95f), static_cast<uint8_t>(pal.hull.g * 0.95f),
                       static_cast<uint8_t>(pal.hull.b * 0.96f)};  // doors: barely darker than the hull
    for (int sg = -1; sg <= 1; sg += 2) {
      for (int k = 0; k < N; ++k) {
        const float xk = xs - pitch * (static_cast<float>(k) + 0.5f);
        addOverlay(xk - ww / 2, xk + ww / 2, zc - hh, zc + hh, sg * yw * 1.004f, sg, window);
      }
      // Doors: front and rear; wide-bodies and the A321 family also have two more by the wing.
      const bool four = D >= 5.0f || hasPrefix(type, "A321") || hasPrefix(type, "A21N");
      const float dw = 0.22f * R, y_d = sg * 0.95f * R;
      float dx[4] = {xs + 0.045f * L, xe - 0.035f * L, x_le + 0.06f * L, x_te - 0.05f * L};
      for (int d = 0; d < (four ? 4 : 2); ++d) addOverlay(dx[d] - dw / 2, dx[d] + dw / 2, -0.10f * R, 0.52f * R, y_d, sg, doorc);
      if (!four && s.mount == Mount::UnderWing) {  // over-wing exits
        for (int e = 0; e < 2; ++e) {
          const float ex = x_le - (0.35f + 0.28f * static_cast<float>(e)) * rc;
          addOverlay(ex - 0.07f * R, ex + 0.07f * R, 0.14f * R, 0.46f * R, y_d, sg, doorc);
        }
      }
    }
  }
  return gm();
}

// ---- Projection ----

size_t projectMesh(const Mesh& m, bool facing_right, int cx, int cy, int box_w, int box_h,
                   ScreenTri* out, size_t cap) {
  // Fixed scratch (no heap allocation: repeated malloc/free fragmented the heap on the board):
  // projected x/y and depth for every vertex (view-space x/y are recomputed from them).
  static float* scratch = static_cast<float*>(malloc(3 * kMaxVerts * sizeof(float)));  // once, never freed
  if (scratch == nullptr) abort();
  float* const vx = scratch;
  float* const vy = scratch + kMaxVerts;
  float* const vd = scratch + 2 * kMaxVerts;
  const float yaw = rad(-40.0f), pitch = rad(24.0f), roll = rad(7.0f);  // seen from above, banked slightly
  const float cr = cosf(roll), sr = sinf(roll);
  const float cyw = cosf(yaw), syw = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  const float extent = fmaxf(m.length, 1.0f), dist = 3.0f * extent;
  float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
  for (size_t i = 0; i < m.nv; ++i) {
    const float ry = m.v[i].y * cr - m.v[i].z * sr, rz = m.v[i].y * sr + m.v[i].z * cr;  // bank
    const float x1 = m.v[i].x * cyw - ry * syw, y1 = m.v[i].x * syw + ry * cyw;
    float X = x1;
    if (!facing_right) X = -X;
    const float U = rz * cp + y1 * sp, d = y1 * cp - rz * sp;
    const float s = dist / (dist + d);
    vd[i] = d;
    vx[i] = X * s;
    vy[i] = U * s;
    minx = fminf(minx, vx[i]);
    maxx = fmaxf(maxx, vx[i]);
    miny = fminf(miny, vy[i]);
    maxy = fmaxf(maxy, vy[i]);
  }
  float frac = m.length / 70.0f;
  if (frac > 1.0f) frac = 1.0f;
  const float bw = fmaxf(maxx - minx, 0.01f), bh = fmaxf(maxy - miny, 0.01f);
  const float scale = fminf(static_cast<float>(box_w) / bw, static_cast<float>(box_h) / bh) *
                      (0.78f + 0.22f * frac);
  const float mx = (minx + maxx) / 2, my = (miny + maxy) / 2;

  size_t n = 0;
  // Light from the upper left, towards the camera (view space: x right, y up, d away).
  const float lx = -0.45f, ly = 0.80f, ld = -0.40f;
  const float ll = sqrtf(lx * lx + ly * ly + ld * ld);
  size_t n_base = 0;
  for (int pass = 0; pass < 2; ++pass) {  // pass 0: body, pass 1: overlays (windows, doors)
    if (pass == 1) n_base = n;
    for (size_t i = 0; i < m.nt && n < cap; ++i) {
      const Tri& t = m.t[i];
      const bool overlay = t.ox != 0 || t.oy != 0 || t.oz != 0;
      if (overlay != (pass == 1)) continue;
      if (overlay) {  // skip overlays on the far side of the fuselage
        const float ox = t.ox, oy = t.oy * cr - t.oz * sr, oz = t.oy * sr + t.oz * cr;  // banked normal
        const float y1 = ox * syw + oy * cyw;
        if (y1 * cp - oz * sp >= 0.0f) continue;
      }
      auto wx = [&](uint16_t i) { return vx[i] * (dist + vd[i]) / dist; };  // undo the perspective
      auto wy = [&](uint16_t i) { return vy[i] * (dist + vd[i]) / dist; };
      const float ax = wx(t.b) - wx(t.a), ay = wy(t.b) - wy(t.a), ad = vd[t.b] - vd[t.a];
      const float bx = wx(t.c) - wx(t.a), by = wy(t.c) - wy(t.a), bd = vd[t.c] - vd[t.a];
      float nx = ay * bd - ad * by, ny = ad * bx - ax * bd, nd = ax * by - ay * bx;
      const float nl = sqrtf(nx * nx + ny * ny + nd * nd);
      if (nl < 1e-6f) continue;  // degenerate
      if (nd > 0) {              // face the camera
        nx = -nx;
        ny = -ny;
        nd = -nd;
      }
      const float lam = fmaxf(0.0f, (nx * lx + ny * ly + nd * ld) / (nl * ll));
      const float shade = 0.62f + 0.45f * lam;
      ScreenTri& o = out[n];
      const uint16_t vi[3] = {t.a, t.b, t.c};
      for (int k = 0; k < 3; ++k) {
        o.x[k] = static_cast<int16_t>(lroundf(static_cast<float>(cx) + (vx[vi[k]] - mx) * scale));
        o.y[k] = static_cast<int16_t>(lroundf(static_cast<float>(cy) - (vy[vi[k]] - my) * scale));
      }
      auto sh = [&](uint8_t c) {
        const float v = c * shade;
        return static_cast<uint8_t>(v > 255.0f ? 255.0f : v);
      };
      o.color = {sh(t.color.r), sh(t.color.g), sh(t.color.b)};
      o.depth = (vd[t.a] + vd[t.b] + vd[t.c]) / 3.0f;
      ++n;
    }
  }
  // Far to near (painter's algorithm): body first, then the overlays on top of it.
  auto far_first = [](const ScreenTri& a, const ScreenTri& b) { return a.depth > b.depth; };
  std::sort(out, out + n_base, far_first);
  std::sort(out + n_base, out + n, far_first);
  return n;
}

}  // namespace jf
