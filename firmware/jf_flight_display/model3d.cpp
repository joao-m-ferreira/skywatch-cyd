#include "model3d.h"

#include <math.h>
#include <stdlib.h>

#include <algorithm>

#include "silhouette.h"

namespace jf {

static Mesh gMesh;

static float rad(float d) { return d * static_cast<float>(M_PI) / 180.0f; }

static size_t addV(float x, float y, float z) {
  if (gMesh.nv >= kMaxVerts) return kMaxVerts - 1;
  gMesh.v[gMesh.nv] = {x, y, z};
  return gMesh.nv++;
}
static void addT(size_t a, size_t b, size_t c, Rgb col) {
  if (gMesh.nt >= kMaxTris) return;
  gMesh.t[gMesh.nt++] = {static_cast<uint16_t>(a), static_cast<uint16_t>(b),
                         static_cast<uint16_t>(c), col};
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

const Mesh& buildAircraft(const char* type, const Palette& pal) {
  const AircraftSpec& s = specFor(type);
  gMesh.nv = gMesh.nt = 0;
  gMesh.length = s.length;
  if (s.mount == Mount::Heli) {
    buildHeli(s, pal);
    return gMesh;
  }
  const float L = s.length, h = L / 2.0f, D = s.fuselage_w * 1.3f, R = D / 2.0f;  // chunkier for legibility
  const float b = s.span / 2.0f, tanL = tanf(rad(s.sweep_deg));
  const bool high = s.mount == Mount::Prop;
  const float nc = 1.6f * D, tc = 2.6f * D;

  // --- Fuselage rings, nose to tail. ---
  Ring rg[28];
  size_t nr = 0;
  constexpr int kNose = 8;
  const float nose_u[kNose] = {0.0f, 0.03f, 0.09f, 0.18f, 0.32f, 0.52f, 0.76f, 1.0f};
  for (int i = 0; i < kNose; ++i) {
    const float u = nose_u[i];
    const float r = fmaxf(0.015f * R, R * sqrtf(fmaxf(0.0f, 2.0f * u - u * u)));
    rg[nr++] = {h - nc * u, -0.12f * R * (1.0f - u), r, r};
  }
  constexpr int kCabin = 12;
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
  const bool windows = D >= 2.0f;
  const Rgb window = {static_cast<uint8_t>(pal.glass.r * 0.45f), static_cast<uint8_t>(pal.glass.g * 0.5f),
                      static_cast<uint8_t>(pal.glass.b * 0.6f)};
  loft(rg, nr, [&](size_t i, int j) {
    if (i >= 4 && i <= 6 && (j == 10 || j == 11 || j == 0 || j == 1)) return pal.glass;  // windshield
    const bool cabin = i >= kNose && i < kNose + kCabin - 1;
    if (windows && cabin && (j == 2 || j == 9) && (i % 2 == 0)) return window;
    if (j == 3 || j == 8) return i >= 5 ? pal.primary : pal.hull;                            // cheatline
    if (j >= 4 && j <= 7) return pal.belly;
    return pal.hull;
  });

  // --- Wings: swept, tapered, with dihedral (sharklets on under-wing jets). ---
  const float z_root = high ? 0.75f * R : -0.45f * R;
  const float dih = high ? 0.01f * b : 0.07f * b;
  const float y_root = 0.6f * R;
  const float x_le = h - s.wing_pos * L;
  const float x_le_tip = x_le - (b - y_root) * tanL, x_te = x_le - s.root_chord;
  const float x_te_tip = x_le_tip - s.tip_chord;
  const float t_root = 0.12f * s.root_chord, t_tip = 0.10f * s.tip_chord + 0.02f;
  auto leAt = [&](float ye) { return x_le - (ye - y_root) * tanL; };
  auto zAt = [&](float ye) { return z_root + dih * (ye - y_root) / (b - y_root); };

  // Engines first (the wing is drawn over their rear part by depth sorting anyway).
  const float nw = s.nacelle_w * 0.95f;
  if (s.mount == Mount::UnderWing && s.engines >= 2) {
    const float ys[2] = {s.engines == 4 ? 0.30f * b : 0.32f * b, 0.62f * b};
    for (int i = 0; i < (s.engines == 4 ? 2 : 1); ++i) {
      const float ye = ys[i], nl = 1.9f * nw, front = leAt(ye) + 0.45f * nl;
      const float zc = zAt(ye) - 0.55f * nw;
      for (int sg = -1; sg <= 1; sg += 2) {
        addCyl(front, front - nl, sg * ye, zc, nw / 2, nw / 2 * 0.85f, pal.secondary, pal.dark);
        addVertical(front - 0.15f * nl, front - 0.85f * nl, zc + 0.4f * nw, front - 0.3f * nl,
                    front - 0.9f * nl, zAt(ye) + 0.02f, sg * ye, 0.04f * nw, pal.hull);  // pylon
      }
    }
  } else if (s.mount == Mount::Rear) {
    const float front = h - 0.58f * L;
    for (int sg = -1; sg <= 1; sg += 2) {
      addCyl(front, front - 3.0f * nw, sg * (R + 0.55f * nw), 0.35f * R, nw / 2, nw / 2 * 0.8f, pal.secondary, pal.dark);
    }
  } else if (s.mount == Mount::Prop && s.engines >= 2) {
    const float ye = 0.30f * b, pl = 5.0f * nw, front = leAt(ye) + 0.55f * pl;
    const float zc = zAt(ye) - 0.2f * nw;
    for (int sg = -1; sg <= 1; sg += 2) {
      addCyl(front, front - pl, sg * ye, zc, 0.6f * nw, 0.5f * nw, pal.secondary, pal.secondary);
      const float pr = 1.7f * nw, px = front + 0.02f * nw;  // propeller: two crossed blades
      const size_t a0 = addV(px, sg * ye - 0.06f * nw, zc + pr), a1 = addV(px, sg * ye + 0.06f * nw, zc + pr);
      const size_t a2 = addV(px, sg * ye + 0.06f * nw, zc - pr), a3 = addV(px, sg * ye - 0.06f * nw, zc - pr);
      addQuad(a0, a1, a2, a3, pal.dark);
    }
  } else if (s.mount == Mount::Prop) {
    const float pr = 0.73f * D, px = h + 0.03f * D;
    const size_t a0 = addV(px, -0.05f * D, pr), a1 = addV(px, 0.05f * D, pr);
    const size_t a2 = addV(px, 0.05f * D, -pr), a3 = addV(px, -0.05f * D, -pr);
    addQuad(a0, a1, a2, a3, pal.dark);
  }

  for (int sg = -1; sg <= 1; sg += 2) {
    addHorizontal(static_cast<float>(sg), y_root, b, x_le, x_te, x_le_tip, x_te_tip, z_root,
                  z_root + dih, t_root, t_tip, pal.hull);
    if (s.mount == Mount::UnderWing) {  // winglet
      const float wl = 0.045f * s.span;
      addVertical(x_le_tip, x_te_tip, z_root + dih, x_le_tip - 0.7f * wl, x_te_tip - 0.1f * wl,
                  z_root + dih + wl, sg * b, 0.03f * D, pal.primary);
    }
  }

  // --- Tail. ---
  float hf;
  if (s.mount == Mount::Rear) hf = 0.16f * L;
  else if (s.mount == Mount::Prop) hf = s.engines > 0 ? 0.14f * L : 0.12f * L;
  else hf = D >= 5.0f ? 0.12f * L : 0.145f * L;
  const float fin_le = -h + 0.16f * L, fin_te = -h + 0.01f * L;
  const float fin_tip_le = fin_le - 0.55f * hf, fin_tip_te = fin_tip_le - 0.07f * L;
  addVertical(fin_le, fin_te, 0.55f * R, fin_tip_le, fin_tip_te, R + hf, 0, 0.04f * D, pal.primary);
  {
    const bool straight = s.sweep_deg < 1.0f;
    const float ts = 0.36f * s.span, tcr = (straight ? 0.25f : 0.32f) * ts,
                tct = (straight ? 0.22f : 0.11f) * ts;
    const float te = -h + 0.075f * L, le = te + tcr;
    const float tip_le = le - (ts / 2 - y_root) * tanf(rad(straight ? 5.0f : 33.0f));
    const float zt = s.mount == Mount::Rear ? R + 0.95f * hf : 0.35f * R;
    for (int sg = -1; sg <= 1; sg += 2) {
      addHorizontal(static_cast<float>(sg), 0.3f * R, ts / 2, le, te, tip_le, tip_le - tct, zt, zt,
                    0.03f * D, 0.02f * D, pal.hull);
    }
  }
  return gMesh;
}

// ---- Projection ----

size_t projectMesh(const Mesh& m, bool facing_right, int cx, int cy, int box_w, int box_h,
                   ScreenTri* out, size_t cap) {
  // One temporary block: projected x/y, view-space x/y, depth for every vertex.
  float* buf = static_cast<float*>(malloc(5 * m.nv * sizeof(float)));
  if (buf == nullptr) return 0;
  float *vx = buf, *vy = buf + m.nv, *wx = buf + 2 * m.nv, *wy = buf + 3 * m.nv, *vd = buf + 4 * m.nv;
  const float yaw = rad(-35.0f), pitch = rad(18.0f);
  const float cyw = cosf(yaw), syw = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  const float extent = fmaxf(m.length, 1.0f), dist = 3.0f * extent;
  float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
  for (size_t i = 0; i < m.nv; ++i) {
    const float x1 = m.v[i].x * cyw - m.v[i].y * syw, y1 = m.v[i].x * syw + m.v[i].y * cyw;
    float X = x1;
    if (!facing_right) X = -X;
    const float U = m.v[i].z * cp + y1 * sp, d = y1 * cp - m.v[i].z * sp;
    const float s = dist / (dist + d);
    wx[i] = X;
    wy[i] = U;
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
                      (0.62f + 0.38f * frac);
  const float mx = (minx + maxx) / 2, my = (miny + maxy) / 2;

  size_t n = 0;
  // Light from the upper left, towards the camera (view space: x right, y up, d away).
  const float lx = -0.45f, ly = 0.80f, ld = -0.40f;
  const float ll = sqrtf(lx * lx + ly * ly + ld * ld);
  for (size_t i = 0; i < m.nt && n < cap; ++i) {
    const Tri& t = m.t[i];
    const float ax = wx[t.b] - wx[t.a], ay = wy[t.b] - wy[t.a], ad = vd[t.b] - vd[t.a];
    const float bx = wx[t.c] - wx[t.a], by = wy[t.c] - wy[t.a], bd = vd[t.c] - vd[t.a];
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
  free(buf);
  // Far to near (painter's algorithm).
  std::sort(out, out + n, [](const ScreenTri& a, const ScreenTri& b) { return a.depth > b.depth; });
  return n;
}

}  // namespace jf
