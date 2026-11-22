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

// ---- Shape generation ----
static Poly gPolys[kMaxPolys];
static Shape gShape;

static Poly* addPoly(size_t* n, int part, bool mirror) {
  Poly* p = &gPolys[(*n)++];
  p->n = 0;
  p->mirror = mirror;
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
static float rad(float d) { return d * static_cast<float>(M_PI) / 180.0f; }

static void buildHeli(const AircraftSpec& s, size_t* n) {
  const float L = s.length;
  Poly* boom = addPoly(n, 1, false);
  pt(boom, 0.025f * L, 0.15f * L);
  pt(boom, 0.025f * L, 0.5f * L);
  pt(boom, -0.025f * L, 0.5f * L);
  pt(boom, -0.025f * L, 0.15f * L);
  Poly* fin = addPoly(n, 2, true);
  rect(fin, 0.025f * L, 0.42f * L, 0.10f * L, 0.5f * L);
  Poly* cabin = addPoly(n, 0, true);
  pt(cabin, 0, -0.30f * L);
  pt(cabin, 0.10f * L, -0.24f * L);
  pt(cabin, 0.14f * L, -0.10f * L);
  pt(cabin, 0.14f * L, 0.08f * L);
  pt(cabin, 0.08f * L, 0.20f * L);
  pt(cabin, 0, 0.22f * L);
}

const Shape& shapeForType(const char* type) {
  const AircraftSpec& s = specFor(type);
  size_t n = 0;
  const float L = s.length, h = L / 2.0f, b = s.span / 2.0f;
  float half_extent = h > b ? h : b;
  float rotor = 0.0f;

  if (s.mount == Mount::Heli) {
    buildHeli(s, &n);
    rotor = b;
  } else {
    const float hw = s.fuselage_w * 0.65f, x0 = hw * 0.5f;  // drawn ~30% wider for legibility
    const float tanL = tanf(rad(s.sweep_deg));
    const float yr = -h + s.wing_pos * L;  // wing root leading edge
    const float nw = s.nacelle_w * 1.2f, nl = 2.2f * nw;
    auto leAt = [&](float x) { return yr + (x - x0) * tanL; };

    // Under-wing engines first: wing and fuselage then cover their rear part.
    if (s.mount == Mount::UnderWing && s.engines >= 2) {
      const float xs[2] = {s.engines == 4 ? 0.30f * b : 0.32f * b, 0.62f * b};
      for (int i = 0; i < (s.engines == 4 ? 2 : 1); ++i) {
        const float xe = xs[i], front = leAt(xe) - 0.4f * nl;
        rect(addPoly(&n, 3, true), xe - nw / 2, front, xe + nw / 2, front + nl);
      }
    }
    // Rear engines hang off the aft fuselage.
    if (s.mount == Mount::Rear) {
      const float front = -h + 0.62f * L, x1 = hw * 0.8f;
      rect(addPoly(&n, 3, true), x1, front, x1 + nw, front + 3.0f * nw);
    }
    // Wing: swept and tapered, its root hidden under the fuselage.
    {
      Poly* w = addPoly(&n, 1, true);
      const float tipLe = leAt(b);
      pt(w, x0, yr);
      pt(w, b, tipLe);
      pt(w, b, tipLe + s.tip_chord);
      pt(w, x0, yr + s.root_chord);
    }
    // Propeller nacelles sit on top of the wing.
    if (s.mount == Mount::Prop && s.engines >= 2) {
      const float xe = 0.30f * b, pl = 5.0f * nw, front = leAt(xe) - 0.55f * pl;
      rect(addPoly(&n, 3, true), xe - nw / 2, front, xe + nw / 2, front + pl);
    }
    // Tailplane.
    {
      const bool straight = s.sweep_deg < 1.0f;
      const float ts = 0.36f * s.span, tcr = (straight ? 0.25f : 0.32f) * ts,
                  tct = (straight ? 0.22f : 0.11f) * ts;
      const float te = h - 0.075f * L, le = te - tcr;
      const float tipLe = le + (ts / 2 - x0) * tanf(rad(straight ? 5.0f : 33.0f));
      Poly* t = addPoly(&n, 1, true);
      pt(t, x0, le);
      pt(t, ts / 2, tipLe);
      pt(t, ts / 2, tipLe + tct);
      pt(t, x0, te);
    }
    // Fuselage: rounded nose, tapered tail.
    {
      const float nc = 1.6f * s.fuselage_w, tc = 2.6f * s.fuselage_w;
      Poly* f = addPoly(&n, 0, true);
      pt(f, 0, -h);
      pt(f, 0.55f * hw, -h + 0.18f * nc);
      pt(f, 0.92f * hw, -h + 0.55f * nc);
      pt(f, hw, -h + nc);
      pt(f, hw, h - tc);
      pt(f, 0.7f * hw, h - 0.5f * tc);
      pt(f, 0.2f * hw, h - 0.05f * tc);
      pt(f, 0, h);
    }
    // Fin, seen from above as a stripe along the tail.
    rect(addPoly(&n, 2, true), 0, h - 0.17f * L, 0.5f * hw, h - 0.03f * L);
    // Make it a proper half-polygon (a rectangle touching the centre line).
    gPolys[n - 1].x[3] = 0;
    gPolys[n - 1].x[0] = 0;
  }

  // Fit the drawing area: small aircraft smaller, the biggest fill the ring.
  float frac = half_extent / 40.0f;
  if (frac > 1.0f) frac = 1.0f;
  gShape.polys = gPolys;
  gShape.count = n;
  gShape.rotor_radius = rotor;
  gShape.half_extent = half_extent;
  gShape.scale = (40.0f + 24.0f * frac) / half_extent;
  return gShape;
}

void transformPoint(float x, float y, float heading_deg, float scale, int cx, int cy, int* sx,
                    int* sy) {
  const float h = heading_deg * static_cast<float>(M_PI) / 180.0f;
  const float c = cosf(h), s = sinf(h);
  *sx = cx + static_cast<int>(lroundf(scale * (x * c - y * s)));
  *sy = cy + static_cast<int>(lroundf(scale * (x * s + y * c)));
}

}  // namespace jf
