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

// ---- Shapes. Polygons are convex, listed in DRAW ORDER (later ones paint on top).
// Half-polygons (mirror = true) are reflected across x = 0; edges lying on x = 0 are not
// outlined so the two halves read as one body.
// part: 0 hull (fuselage), 1 hull (wings, tailplane), 2 livery accent (fin), 3 engines.

static const Poly kJetPolys[] = {
    // wings, swept and tapered, root hidden under the fuselage
    {4, {0.04f, 0.95f, 0.95f, 0.04f}, {-0.16f, 0.22f, 0.31f, 0.20f}, true, 1},
    // tailplane
    {4, {0.04f, 0.38f, 0.38f, 0.04f}, {0.74f, 0.92f, 0.98f, 0.90f}, true, 1},
    // engines, ahead of the wing leading edge
    {4, {0.27f, 0.36f, 0.36f, 0.27f}, {-0.13f, -0.13f, 0.07f, 0.07f}, true, 3},
    // fuselage with a rounded nose and tapered tail
    {8, {0, 0.05f, 0.08f, 0.085f, 0.085f, 0.055f, 0.02f, 0}, {-1.0f, -0.96f, -0.85f, -0.6f, 0.5f, 0.85f, 1.0f, 1.0f}, true, 0},
    // fin
    {4, {0, 0.045f, 0.045f, 0}, {0.56f, 0.60f, 0.93f, 0.93f}, true, 2},
};
static const Poly kWidePolys[] = {
    {4, {0.04f, 1.0f, 1.0f, 0.04f}, {-0.25f, 0.27f, 0.38f, 0.26f}, true, 1},
    {4, {0.04f, 0.42f, 0.42f, 0.04f}, {0.72f, 0.92f, 0.99f, 0.90f}, true, 1},
    {4, {0.30f, 0.40f, 0.40f, 0.30f}, {-0.12f, -0.12f, 0.10f, 0.10f}, true, 3},
    {4, {0.60f, 0.70f, 0.70f, 0.60f}, {0.02f, 0.02f, 0.22f, 0.22f}, true, 3},
    {8, {0, 0.06f, 0.10f, 0.105f, 0.105f, 0.065f, 0.025f, 0}, {-1.0f, -0.96f, -0.84f, -0.6f, 0.5f, 0.85f, 1.0f, 1.0f}, true, 0},
    {4, {0, 0.05f, 0.05f, 0}, {0.55f, 0.60f, 0.93f, 0.93f}, true, 2},
};
static const Poly kPropPolys[] = {
    // straight wings
    {4, {0.04f, 0.95f, 0.95f, 0.04f}, {-0.28f, -0.26f, -0.05f, -0.03f}, true, 1},
    {4, {0.04f, 0.40f, 0.40f, 0.04f}, {0.78f, 0.78f, 0.92f, 0.92f}, true, 1},
    // engine nacelles
    {4, {0.30f, 0.38f, 0.38f, 0.30f}, {-0.50f, -0.50f, -0.04f, -0.04f}, true, 3},
    {8, {0, 0.05f, 0.075f, 0.08f, 0.08f, 0.05f, 0.02f, 0}, {-1.0f, -0.95f, -0.85f, -0.6f, 0.5f, 0.85f, 1.0f, 1.0f}, true, 0},
    {4, {0, 0.04f, 0.04f, 0}, {0.62f, 0.66f, 0.92f, 0.92f}, true, 2},
};
static const Poly kLightPolys[] = {
    {4, {0.04f, 0.95f, 0.95f, 0.04f}, {-0.28f, -0.26f, -0.02f, 0.0f}, true, 1},
    {4, {0.04f, 0.38f, 0.38f, 0.04f}, {0.78f, 0.78f, 0.93f, 0.93f}, true, 1},
    {8, {0, 0.06f, 0.09f, 0.09f, 0.09f, 0.05f, 0.02f, 0}, {-0.9f, -0.85f, -0.7f, -0.4f, 0.4f, 0.8f, 1.0f, 1.0f}, true, 0},
    {4, {0, 0.04f, 0.04f, 0}, {0.62f, 0.66f, 0.92f, 0.92f}, true, 2},
};
static const Poly kBizPolys[] = {
    {4, {0.04f, 0.95f, 0.95f, 0.04f}, {-0.02f, 0.30f, 0.38f, 0.22f}, true, 1},
    {4, {0.04f, 0.36f, 0.36f, 0.04f}, {0.78f, 0.90f, 0.96f, 0.92f}, true, 1},
    // rear-mounted engines
    {4, {0.095f, 0.17f, 0.17f, 0.095f}, {0.50f, 0.50f, 0.82f, 0.82f}, true, 3},
    {8, {0, 0.045f, 0.07f, 0.075f, 0.075f, 0.05f, 0.02f, 0}, {-1.0f, -0.96f, -0.85f, -0.6f, 0.5f, 0.85f, 1.0f, 1.0f}, true, 0},
    {4, {0, 0.04f, 0.04f, 0}, {0.55f, 0.60f, 0.93f, 0.93f}, true, 2},
};
static const Poly kHeliPolys[] = {
    // tail boom and tail fin
    {4, {0.03f, 0.03f, -0.03f, -0.03f}, {0.3f, 1.0f, 1.0f, 0.3f}, false, 1},
    {4, {0.03f, 0.2f, 0.2f, 0.03f}, {0.88f, 0.88f, 0.98f, 0.98f}, true, 2},
    // cabin
    {6, {0, 0.14f, 0.2f, 0.2f, 0.12f, 0.0f}, {-0.55f, -0.45f, -0.2f, 0.15f, 0.38f, 0.4f}, true, 0},
};

static const Shape kJet = {kJetPolys, COUNT(kJetPolys), 0.0f, 0.9f};
static const Shape kWide = {kWidePolys, COUNT(kWidePolys), 0.0f, 1.0f};
static const Shape kProp = {kPropPolys, COUNT(kPropPolys), 0.0f, 0.8f};
static const Shape kLight = {kLightPolys, COUNT(kLightPolys), 0.0f, 0.65f};
static const Shape kBizShape = {kBizPolys, COUNT(kBizPolys), 0.0f, 0.75f};
static const Shape kHeliShape = {kHeliPolys, COUNT(kHeliPolys), 0.9f, 0.7f};

const Shape& shapeFor(Category c) {
  switch (c) {
    case Category::Wide: return kWide;
    case Category::Prop: return kProp;
    case Category::Light: return kLight;
    case Category::Biz: return kBizShape;
    case Category::Heli: return kHeliShape;
    case Category::Jet: break;
  }
  return kJet;
}

void transformPoint(float x, float y, float heading_deg, float scale, int cx, int cy, int* sx,
                    int* sy) {
  const float h = heading_deg * static_cast<float>(M_PI) / 180.0f;
  const float c = cosf(h), s = sinf(h);
  *sx = cx + static_cast<int>(lroundf(scale * (x * c - y * s)));
  *sy = cy + static_cast<int>(lroundf(scale * (x * s + y * c)));
}

}  // namespace jf
