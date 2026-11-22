#include "silhouette.h"

#include <math.h>
#include <string.h>

namespace jf {

static bool startsWithAny(const char* t, const char* const* list, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (strncmp(t, list[i], strlen(list[i])) == 0) return true;
  }
  return false;
}
static bool equalsAny(const char* t, const char* const* list, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (strcmp(t, list[i]) == 0) return true;
  }
  return false;
}
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

Category categorize(const char* t) {
  if (t == nullptr || t[0] == '\0') return Category::Jet;
  static const char* const kHeli[] = {"EC", "AS3", "AS5", "AS6", "H60", "H64", "H47", "H500",
                                      "R22", "R44", "R66", "B06", "B407", "B412", "B429", "A109",
                                      "A139", "A169", "A189", "S76", "S92", "AW", "MD52", "MD60",
                                      "BK17", "EXPL", "GAZL", "NH90", "CH47"};
  static const char* const kWide[] = {"A306", "A30B", "A310", "A33", "A34", "A35", "A38", "B74",
                                      "B76", "B77", "B78", "MD11", "DC10", "IL96", "A124", "C5M",
                                      "BLCF"};
  static const char* const kLightExact[] = {"C150", "C152", "C172", "C182", "C206", "C210",
                                            "PA28", "PA32", "PA34", "PA38", "PA44", "PA46",
                                            "SR20", "SR22", "DA20", "DA40", "DA42", "DA62",
                                            "P28A", "P28B", "P28R", "TB20", "TB10", "G115",
                                            "BE35", "BE36", "BE58", "M20P", "M20T", "RV7"};
  static const char* const kProp[] = {"AT4", "AT7", "AT8", "DH8", "D328", "SF34", "JS3", "JS4",
                                      "B190", "BE9", "BE20", "BE30", "C208", "PC12", "PC6",
                                      "SW4", "E120", "F50", "F27", "DHC", "SB20", "C130",
                                      "C30J", "P180", "L410"};
  static const char* const kBiz[] = {"C25", "C5", "C68", "C56", "GLF", "G15", "G28", "G45",
                                     "G65", "GL", "LJ", "FA", "F2TH", "F900", "E50", "E55",
                                     "CL3", "CL6", "H25", "PRM1", "BE40", "EA50", "HDJT"};
  if (startsWithAny(t, kHeli, COUNT(kHeli))) return Category::Heli;
  if (strcmp(t, "C17") == 0 || startsWithAny(t, kWide, COUNT(kWide))) return Category::Wide;
  if (equalsAny(t, kLightExact, COUNT(kLightExact))) return Category::Light;
  if (startsWithAny(t, kProp, COUNT(kProp))) return Category::Prop;
  if (startsWithAny(t, kBiz, COUNT(kBiz))) return Category::Biz;
  if (strncmp(t, "PA", 2) == 0 || strncmp(t, "SR2", 3) == 0 || strncmp(t, "DA", 2) == 0)
    return Category::Light;
  return Category::Jet;
}

// ---- Aircraft specs (approximate real dimensions) ----
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define UW Mount::UnderWing
#define RR Mount::Rear
#define PP Mount::Prop
#define A_NB(c, L, S) {c, L, S, 25.f, 6.1f, 1.5f, 3.95f, .36f, 2, UW, 2.4f}
#define A_220(c, L, S) {c, L, S, 24.f, 5.5f, 1.4f, 3.5f, .37f, 2, UW, 2.2f}
#define B_737(c, L, S) {c, L, S, 25.f, 6.6f, 1.5f, 3.76f, .37f, 2, UW, 2.3f}
#define B_757(c, L, S) {c, L, S, 25.f, 7.0f, 1.8f, 3.76f, .37f, 2, UW, 2.9f}
#define B_767(c, L, S) {c, L, S, 31.5f, 8.5f, 2.0f, 5.03f, .36f, 2, UW, 3.2f}
#define A_300(c, L, S) {c, L, S, 28.f, 8.5f, 2.0f, 5.64f, .36f, 2, UW, 3.0f}
#define A_330(c, L, S) {c, L, S, 30.f, 10.f, 2.2f, 5.64f, .36f, 2, UW, 3.0f}
#define A_340(c, L, S) {c, L, S, 30.f, 10.f, 2.2f, 5.64f, .36f, 4, UW, 2.5f}
#define A_350(c, L, S) {c, L, S, 32.f, 9.5f, 2.0f, 5.96f, .36f, 2, UW, 3.1f}
#define A_380(c, L, S) {c, L, S, 33.5f, 15.f, 3.5f, 7.14f, .36f, 4, UW, 3.0f}
#define B_777(c, L, S) {c, L, S, 31.6f, 12.5f, 2.3f, 6.2f, .36f, 2, UW, 3.8f}
#define B_787(c, L, S) {c, L, S, 32.2f, 9.5f, 2.0f, 5.8f, .36f, 2, UW, 3.3f}
#define B_747(c, L, S) {c, L, S, 37.5f, 14.f, 4.0f, 6.5f, .36f, 4, UW, 2.9f}
#define E_JET(c, L, S) {c, L, S, 23.f, 5.0f, 1.3f, 3.0f, .37f, 2, UW, 1.9f}
#define E_145(c, L, S) {c, L, S, 22.f, 3.8f, 1.1f, 2.3f, .42f, 2, RR, 1.4f}
#define CRJ(c, L, S) {c, L, S, 26.f, 4.2f, 1.2f, 2.7f, .42f, 2, RR, 1.5f}
#define MD80(c, L, S) {c, L, S, 24.f, 5.0f, 1.4f, 3.3f, .42f, 2, RR, 2.2f}
#define ATR(c, L, S) {c, L, S, 0.f, 2.4f, 1.6f, 2.8f, .38f, 2, PP, 0.9f}
#define DASH(c, L, S) {c, L, S, 0.f, 2.8f, 1.7f, 2.7f, .38f, 2, PP, 1.0f}
#define LIGHT(c, L, S) {c, L, S, 0.f, 1.6f, 1.1f, 1.3f, .30f, 0, PP, 0.5f}
#define BIZ(c, L, S) {c, L, S, 28.f, 3.0f, 1.0f, 2.3f, .42f, 2, RR, 1.3f}
#define HELI(c, L, S) {c, L, S, 0.f, 0.f, 0.f, 0.f, 0.f, 0, Mount::Heli, 0.f}

// Order matters: the first prefix that matches wins, so specific codes come first.
static const AircraftSpec kSpecs[] = {
    A_NB("A318", 31.5f, 34.1f), A_NB("A319", 33.8f, 35.8f), A_NB("A19N", 33.8f, 35.8f),
    A_NB("A320", 37.6f, 35.8f), A_NB("A20N", 37.6f, 35.8f), A_NB("A321", 44.5f, 35.8f),
    A_NB("A21N", 44.5f, 35.8f), A_220("BCS1", 35.0f, 35.1f), A_220("BCS3", 38.7f, 35.1f),
    B_737("B731", 29.5f, 28.4f), B_737("B732", 30.5f, 28.4f), B_737("B733", 33.4f, 28.9f),
    B_737("B734", 36.4f, 28.9f), B_737("B735", 31.0f, 28.9f), B_737("B736", 31.2f, 34.3f),
    B_737("B737", 33.6f, 35.8f), B_737("B738", 39.5f, 35.8f), B_737("B739", 42.1f, 35.8f),
    B_737("B37M", 35.6f, 35.9f), B_737("B38M", 39.5f, 35.9f), B_737("B39M", 43.8f, 35.9f),
    B_737("B3XM", 43.8f, 35.9f),
    B_757("B752", 47.3f, 38.0f), B_757("B753", 54.4f, 38.0f),
    B_767("B762", 48.5f, 47.6f), B_767("B763", 54.9f, 47.6f), B_767("B764", 61.4f, 51.9f),
    A_300("A306", 54.1f, 44.8f), A_300("A30B", 53.6f, 44.8f), A_300("A310", 46.7f, 43.9f),
    A_330("A332", 58.8f, 60.3f), A_330("A333", 63.7f, 60.3f), A_330("A338", 58.8f, 64.0f),
    A_330("A339", 63.7f, 64.0f),
    A_340("A342", 59.4f, 60.3f), A_340("A343", 63.7f, 60.3f), A_340("A345", 67.9f, 63.4f),
    A_340("A346", 75.4f, 63.4f),
    A_350("A359", 66.8f, 64.8f), A_350("A35K", 73.8f, 64.8f),
    A_380("A388", 72.7f, 79.8f),
    B_777("B772", 63.7f, 60.9f), B_777("B773", 73.9f, 60.9f), B_777("B77L", 63.7f, 64.8f),
    B_777("B77W", 73.9f, 64.8f), B_777("B778", 69.8f, 71.8f), B_777("B779", 76.7f, 71.8f),
    B_787("B788", 56.7f, 60.1f), B_787("B789", 62.8f, 60.1f), B_787("B78X", 68.3f, 60.1f),
    B_747("B744", 70.7f, 64.4f), B_747("B748", 76.3f, 68.4f), B_747("B74S", 56.3f, 59.6f),
    B_747("B743", 70.7f, 59.6f), B_747("B74", 70.7f, 64.4f),
    E_JET("E170", 29.9f, 26.0f), E_JET("E75", 31.7f, 26.0f), E_JET("E190", 36.2f, 28.7f),
    E_JET("E195", 38.7f, 28.7f), E_JET("E290", 36.2f, 33.7f), E_JET("E295", 41.5f, 35.1f),
    E_145("E145", 29.9f, 20.0f), E_145("E135", 26.3f, 20.0f),
    CRJ("CRJ2", 26.8f, 21.2f), CRJ("CRJ7", 32.5f, 23.2f), CRJ("CRJ9", 36.2f, 24.9f),
    CRJ("CRJX", 39.1f, 26.2f), CRJ("F100", 35.5f, 28.1f),
    MD80("MD8", 45.1f, 32.9f), MD80("MD9", 40.7f, 32.9f),
    ATR("AT72", 27.2f, 27.05f), ATR("AT76", 27.2f, 27.05f), ATR("AT7", 22.7f, 24.57f),
    ATR("AT4", 22.7f, 24.57f),
    DASH("DH8D", 32.8f, 28.4f), DASH("DH8C", 25.7f, 27.4f), DASH("DH8", 22.3f, 25.9f),
    DASH("SF34", 19.7f, 21.4f), DASH("SB20", 27.3f, 24.8f),
    LIGHT("C172", 8.3f, 11.0f), LIGHT("C152", 7.3f, 10.2f), LIGHT("C182", 8.8f, 11.0f),
    LIGHT("C208", 11.5f, 15.9f), LIGHT("PA28", 7.3f, 10.7f), LIGHT("SR22", 7.9f, 11.7f),
    LIGHT("SR20", 7.9f, 10.8f), LIGHT("DA40", 8.1f, 11.9f), LIGHT("DA42", 8.6f, 13.4f),
    BIZ("GLF4", 26.9f, 23.7f), BIZ("GLF5", 29.4f, 28.5f), BIZ("GLF6", 30.4f, 30.4f),
    BIZ("GLEX", 30.3f, 28.7f), BIZ("C56X", 19.0f, 17.2f), BIZ("C25", 14.0f, 14.3f),
    BIZ("C68", 19.4f, 22.0f), BIZ("E55P", 12.8f, 12.3f), BIZ("E50P", 12.8f, 12.3f),
    BIZ("FA7X", 20.9f, 26.2f), BIZ("F2TH", 20.2f, 19.3f), BIZ("LJ", 14.5f, 14.0f),
};

// Representative specs when the exact type is unknown (by class).
static const AircraftSpec kDefJet = A_NB("", 37.6f, 35.8f);
static const AircraftSpec kDefWide = B_787("", 56.7f, 60.1f);
static const AircraftSpec kDefProp = ATR("", 27.2f, 27.05f);
static const AircraftSpec kDefLight = LIGHT("", 8.3f, 11.0f);
static const AircraftSpec kDefBiz = BIZ("", 26.9f, 23.7f);
static const AircraftSpec kDefHeli = HELI("", 12.0f, 10.0f);

size_t specCount() { return COUNT(kSpecs); }
const AircraftSpec& specAt(size_t i) { return kSpecs[i]; }

const AircraftSpec& specFor(const char* t) {
  if (t != nullptr && t[0] != '\0') {
    for (const AircraftSpec& s : kSpecs) {
      if (strncmp(t, s.prefix, strlen(s.prefix)) == 0) return s;
    }
  }
  switch (categorize(t)) {
    case Category::Wide: return kDefWide;
    case Category::Prop: return kDefProp;
    case Category::Light: return kDefLight;
    case Category::Biz: return kDefBiz;
    case Category::Heli: return kDefHeli;
    case Category::Jet: break;
  }
  return kDefJet;
}

// ---- Shape generation (side view) ----
static Poly gPolys[kMaxPolys];
static Shape gShape;

static Poly* addPoly(size_t* n, int part) {
  Poly* p = &gPolys[(*n)++];
  p->n = 0;
  p->part = part;
  return p;
}
static void pt(Poly* p, float x, float y) {
  p->x[p->n] = x;
  p->y[p->n] = y;
  ++p->n;
}
static void rect(Poly* p, float x0, float y0, float x1, float y1) {
  pt(p, x0, y0);
  pt(p, x1, y0);
  pt(p, x1, y1);
  pt(p, x0, y1);
}
static void octagon(Poly* p, float cx, float cy, float r) {
  for (int i = 0; i < 8; ++i) {
    const float a = (45.0f * static_cast<float>(i) + 22.5f) * static_cast<float>(M_PI) / 180.0f;
    pt(p, cx + r * cosf(a), cy + r * sinf(a));
  }
}
// Engine pod seen from the side: a rounded lip at the front, flat top and bottom.
static void pod(Poly* p, float x0, float x1, float cy, float half_h) {
  const float xm = x0 + 0.25f * (x1 - x0);
  pt(p, x0, cy - 0.8f * half_h);
  pt(p, xm, cy - half_h);
  pt(p, x1, cy - half_h);
  pt(p, x1, cy + half_h);
  pt(p, xm, cy + half_h);
  pt(p, x0, cy + 0.8f * half_h);
}
static float rad(float d) { return d * static_cast<float>(M_PI) / 180.0f; }

static void buildHeli(const AircraftSpec& s, size_t* n) {
  const float L = s.length, R = s.span / 2.0f;
  // Skids and struts first, so the cabin covers their tops.
  rect(addPoly(n, kDark), -0.20f * L, 0.27f * L, 0.18f * L, 0.295f * L);
  rect(addPoly(n, kDark), -0.12f * L, 0.18f * L, -0.09f * L, 0.27f * L);
  rect(addPoly(n, kDark), 0.08f * L, 0.18f * L, 0.11f * L, 0.27f * L);
  // Tail boom and tail fin.
  Poly* boom = addPoly(n, kSurface);
  pt(boom, 0.12f * L, -0.10f * L);
  pt(boom, 0.47f * L, -0.05f * L);
  pt(boom, 0.47f * L, 0.0f);
  pt(boom, 0.12f * L, 0.08f * L);
  Poly* fin = addPoly(n, kFin);
  pt(fin, 0.40f * L, -0.04f * L);
  pt(fin, 0.45f * L, -0.20f * L);
  pt(fin, 0.50f * L, -0.20f * L);
  pt(fin, 0.48f * L, 0.0f);
  // Cabin.
  Poly* cab = addPoly(n, kHull);
  pt(cab, -0.30f * L, 0.03f * L);
  pt(cab, -0.22f * L, -0.13f * L);
  pt(cab, 0.0f, -0.19f * L);
  pt(cab, 0.18f * L, -0.12f * L);
  pt(cab, 0.20f * L, 0.12f * L);
  pt(cab, -0.20f * L, 0.20f * L);
  Poly* gl = addPoly(n, kGlass);
  pt(gl, -0.27f * L, 0.02f * L);
  pt(gl, -0.20f * L, -0.10f * L);
  pt(gl, -0.04f * L, -0.15f * L);
  pt(gl, -0.04f * L, 0.04f * L);
  rect(addPoly(n, kStripe), -0.18f * L, 0.10f * L, 0.18f * L, 0.14f * L);
  // Rotor mast and blades.
  rect(addPoly(n, kDark), -0.02f * L, -0.25f * L, 0.02f * L, -0.18f * L);
  rect(addPoly(n, kDark), -R, -0.255f * L, R, -0.235f * L);
}

const Shape& shapeForType(const char* type) {
  const AircraftSpec& s = specFor(type);
  size_t n = 0;
  const float L = s.length, h = L / 2.0f;

  if (s.mount == Mount::Heli) {
    buildHeli(s, &n);
  } else {
    const float D = s.fuselage_w;
    const float nc = 1.6f * D, tc = 2.6f * D;
    const bool high = s.mount == Mount::Prop;  // high-wing turboprops and light aircraft
    const float b = s.span / 2.0f, tanL = tanf(rad(s.sweep_deg));
    const float yw = high ? -0.46f * D : 0.30f * D;           // wing root height
    const float dih = high ? 0.01f * b : 0.087f * b;          // dihedral rise at the tip
    const float xr = -h + s.wing_pos * L;                     // wing root leading edge
    float hf;                                                 // fin height above the fuselage
    if (s.mount == Mount::Rear) hf = 0.16f * L;
    else if (s.mount == Mount::Prop) hf = s.engines > 0 ? 0.14f * L : 0.12f * L;
    else hf = D >= 5.0f ? 0.12f * L : 0.145f * L;

    // Landing gear (drawn only when the gear is down).
    {
      const float gh = 0.6f * D, rw = (D >= 5.0f ? 0.14f : 0.16f) * D;
      const float xn = -h + 0.16f * L, xm = xr + 0.5f * s.root_chord;
      rect(addPoly(&n, kGear), xn - 0.03f * D, 0.40f * D, xn + 0.03f * D, 0.5f * D + gh);
      octagon(addPoly(&n, kGear), xn, 0.5f * D + gh, rw);
      rect(addPoly(&n, kGear), xm - 0.04f * D, 0.40f * D, xm + 0.04f * D, 0.5f * D + 1.05f * gh);
      octagon(addPoly(&n, kGear), xm, 0.5f * D + 1.05f * gh, rw * 1.1f);
    }
    // Vertical fin, root along the top of the tail.
    {
      const float x1 = h - 0.16f * L + 0.55f * hf;
      Poly* f = addPoly(&n, kFin);
      pt(f, h - 0.16f * L, -0.5f * D);
      pt(f, x1, -0.5f * D - hf);
      pt(f, x1 + 0.07f * L, -0.5f * D - hf);
      pt(f, h - 0.01f * L, -0.15f * D);
    }
    // Horizontal tailplane, seen edge-on (T-tail on rear-engine types).
    {
      const float ys = s.mount == Mount::Rear ? -0.5f * D - 0.95f * hf : -0.35f * D;
      Poly* t = addPoly(&n, kSurface);
      pt(t, h - 0.14f * L, ys + 0.03f * D);
      pt(t, h - 0.03f * L, ys - 0.01f * L);
      pt(t, h, ys - 0.01f * L);
      pt(t, h - 0.11f * L, ys + 0.04f * D);
    }
    // Fuselage: rounded nose, up-swept tail cone.
    {
      Poly* f = addPoly(&n, kHull);
      pt(f, -h, 0.10f * D);
      pt(f, -h + 0.15f * nc, -0.28f * D);
      pt(f, -h + 0.55f * nc, -0.47f * D);
      pt(f, -h + nc, -0.5f * D);
      pt(f, h - tc, -0.5f * D);
      pt(f, h, -0.12f * D);
      pt(f, h - 0.5f * tc, 0.32f * D);
      pt(f, h - tc, 0.5f * D);
      pt(f, -h + nc, 0.5f * D);
      pt(f, -h + 0.4f * nc, 0.42f * D);
    }
    rect(addPoly(&n, kStripe), -h + nc, 0.10f * D, h - 0.9f * tc, 0.18f * D);  // cheatline
    {
      Poly* g = addPoly(&n, kGlass);  // cockpit windshield
      pt(g, -h + 0.30f * nc, -0.30f * D);
      pt(g, -h + 0.60f * nc, -0.46f * D);
      pt(g, -h + 0.88f * nc, -0.46f * D);
      pt(g, -h + 0.88f * nc, -0.30f * D);
    }
    if (D >= 2.0f) {  // window row
      Poly* w = addPoly(&n, kWindows);
      const float x0 = -h + 1.1f * nc, x1 = h - 0.95f * tc;
      pt(w, x0, -0.25f * D);
      pt(w, x1, -0.25f * D);
      pt(w, x1, -0.20f * D);
      pt(w, x0, -0.20f * D);
    }
    // Near wing, edge-on: tip displaced aft by the sweep, raised by the dihedral.
    {
      Poly* w = addPoly(&n, kSurface);
      const float xt = xr + 0.9f * b * tanL;
      pt(w, xr, yw);
      pt(w, xt, yw - dih);
      pt(w, xt + s.tip_chord, yw - dih + 0.05f * D);
      pt(w, xr + s.root_chord, yw + 0.07f * D);
    }
    // Engines.
    const float nw = s.nacelle_w * 0.95f, nl = 1.7f * nw;
    auto leAt = [&](float xe) { return xr + (xe - 0.5f * D * 0.5f) * tanL; };
    if (s.mount == Mount::UnderWing && s.engines >= 2) {
      const float xs[2] = {s.engines == 4 ? 0.30f * b : 0.32f * b, 0.62f * b};
      for (int i = 0; i < (s.engines == 4 ? 2 : 1); ++i) {
        const float xe = xs[i], front = leAt(xe) - 0.4f * nl;
        const float cy = yw - dih * (xe / b) + 0.07f * D + 0.45f * nw;
        pod(addPoly(&n, kEngine), front, front + nl, cy, nw / 2);
      }
    } else if (s.mount == Mount::Rear) {
      const float front = -h + 0.62f * L;
      pod(addPoly(&n, kEngine), front, front + 3.0f * nw, -0.25f * D, nw / 2);
    } else if (s.mount == Mount::Prop && s.engines >= 2) {
      const float xe = 0.30f * b, pl = 5.0f * nw, front = leAt(xe) - 0.55f * pl;
      const float cy = yw + 0.25f * D;
      pod(addPoly(&n, kEngine), front, front + pl, cy, 0.7f * nw);
      rect(addPoly(&n, kDark), front - 0.03f * D, cy - 1.7f * nw, front, cy + 1.7f * nw);  // prop
    } else if (s.mount == Mount::Prop) {
      rect(addPoly(&n, kDark), -h - 0.03f * D, -0.05f * D - 0.73f * D, -h, -0.05f * D + 0.73f * D);
    }
  }

  // Extent and fit: small aircraft draw smaller, the biggest fill the panel.
  float hl = 0, top = 0, bottom = 0;
  for (size_t p = 0; p < n; ++p) {
    for (size_t i = 0; i < gPolys[p].n; ++i) {
      const float ax = fabsf(gPolys[p].x[i]);
      if (ax > hl) hl = ax;
      if (gPolys[p].y[i] < top) top = gPolys[p].y[i];
      if (gPolys[p].y[i] > bottom) bottom = gPolys[p].y[i];
    }
  }
  float frac = L / 70.0f;
  if (frac > 1.0f) frac = 1.0f;
  gShape.polys = gPolys;
  gShape.count = n;
  gShape.half_len = hl;
  gShape.top = top;
  gShape.bottom = bottom;
  gShape.scale = (90.0f + 58.0f * frac) / (2.0f * hl);
  return gShape;
}

void transformSide(float x, float y, bool facing_right, float scale, int cx, int cy, int* sx,
                   int* sy) {
  *sx = cx + static_cast<int>(lroundf(scale * (facing_right ? -x : x)));
  *sy = cy + static_cast<int>(lroundf(scale * y));
}

}  // namespace jf
