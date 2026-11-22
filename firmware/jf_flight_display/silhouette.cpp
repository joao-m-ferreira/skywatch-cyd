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

// ---- Shapes: left-half polygons are mirrored where `mirror` is true. ----

static const Poly kJetPolys[] = {
    {7, {0, 0.08f, 0.08f, 0.03f, -0.03f, -0.08f, -0.08f}, {-1, -0.85f, 0.75f, 1, 1, 0.75f, -0.85f}, false, 0},
    {4, {0.08f, 0.92f, 0.92f, 0.08f}, {-0.12f, 0.28f, 0.42f, 0.18f}, true, 1},
    {4, {0.05f, 0.36f, 0.36f, 0.05f}, {0.74f, 0.92f, 0.99f, 0.88f}, true, 2},
    {4, {0.30f, 0.38f, 0.38f, 0.30f}, {0.02f, 0.06f, 0.20f, 0.16f}, true, 3},
};
static const Poly kWidePolys[] = {
    {7, {0, 0.1f, 0.1f, 0.04f, -0.04f, -0.1f, -0.1f}, {-1, -0.85f, 0.75f, 1, 1, 0.75f, -0.85f}, false, 0},
    {4, {0.1f, 1.0f, 1.0f, 0.1f}, {-0.22f, 0.30f, 0.46f, 0.26f}, true, 1},
    {4, {0.06f, 0.40f, 0.40f, 0.06f}, {0.74f, 0.92f, 0.99f, 0.88f}, true, 2},
    {4, {0.30f, 0.38f, 0.38f, 0.30f}, {-0.02f, 0.02f, 0.18f, 0.14f}, true, 3},
    {4, {0.55f, 0.63f, 0.63f, 0.55f}, {0.10f, 0.14f, 0.30f, 0.26f}, true, 3},
};
static const Poly kPropPolys[] = {
    {7, {0, 0.08f, 0.08f, 0.03f, -0.03f, -0.08f, -0.08f}, {-1, -0.85f, 0.75f, 1, 1, 0.75f, -0.85f}, false, 0},
    {4, {0.08f, 0.95f, 0.95f, 0.08f}, {-0.30f, -0.30f, -0.08f, -0.08f}, true, 1},
    {4, {0.05f, 0.40f, 0.40f, 0.05f}, {0.80f, 0.80f, 0.93f, 0.93f}, true, 2},
    {4, {0.45f, 0.53f, 0.53f, 0.45f}, {-0.45f, -0.45f, -0.30f, -0.30f}, true, 3},
};
static const Poly kHeliPolys[] = {
    {6, {0, 0.2f, 0.2f, 0, -0.2f, -0.2f}, {-0.5f, -0.3f, 0.2f, 0.35f, 0.2f, -0.3f}, false, 0},
    {4, {0.03f, 0.03f, -0.03f, -0.03f}, {0.3f, 1.0f, 1.0f, 0.3f}, false, 2},
    {4, {0.03f, 0.2f, 0.2f, 0.03f}, {0.9f, 0.9f, 1.0f, 1.0f}, true, 3},
};

static const Shape kJet = {kJetPolys, COUNT(kJetPolys), 0.0f, 0.85f};
static const Shape kWide = {kWidePolys, COUNT(kWidePolys), 0.0f, 1.0f};
static const Shape kProp = {kPropPolys, COUNT(kPropPolys), 0.0f, 0.8f};
static const Shape kLight = {kPropPolys, COUNT(kPropPolys), 0.0f, 0.6f};
static const Shape kBizShape = {kJetPolys, COUNT(kJetPolys), 0.0f, 0.65f};
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
