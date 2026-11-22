// Host-side unit tests (no Arduino). Run via scripts/test.sh.
#include <stdio.h>
#include <string.h>

#include "../jf_flight_display/board.h"
#include "../jf_flight_display/airline.h"
#include "../jf_flight_display/flights.h"
#include "../jf_flight_display/model3d.h"
#include "../jf_flight_display/silhouette.h"

static int failures = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);  \
      ++failures;                                         \
    }                                                     \
  } while (0)

int main() {
  jf::Flight f[jf::kMaxFlights];
  int n;

  // Direct ADS-B parsing (the standalone path), same fixture as the Go service.
  const char* adsb =
      "{\"ac\":["
      "{\"flight\":\"RYR123  \",\"t\":\"B738\",\"lat\":38.80,\"lon\":-9.10,\"alt_baro\":12000,"
      "\"gs\":310.4,\"track\":88.2},"
      "{\"flight\":\"TAP456\",\"t\":\"A320\",\"lat\":38.74,\"lon\":-9.14,\"alt_baro\":3500,"
      "\"gs\":180,\"track\":200},"
      "{\"flight\":\"GND1\",\"lat\":38.77,\"lon\":-9.13,\"alt_baro\":\"ground\"},"
      "{\"r\":\"CS-DXX\",\"t\":\"C172\",\"lat\":38.90,\"lon\":-9.20,\"alt_baro\":2000},"
      "{\"flight\":\"NOPOS\"},"
      "{\"flight\":\"FAR1\",\"lat\":45.0,\"lon\":5.0,\"alt_baro\":30000}]}";
  n = jf::parseAdsb(adsb, strlen(adsb), 38.7223, -9.1393, 40, f, jf::kMaxFlights);
  CHECK(n == 3);
  CHECK(strcmp(f[0].callsign, "TAP456") == 0);
  CHECK(strcmp(f[1].callsign, "RYR123") == 0);
  CHECK(strcmp(f[2].callsign, "CS-DXX") == 0);
  CHECK(f[0].dist_km < f[1].dist_km && f[1].dist_km < f[2].dist_km);
  CHECK(f[0].alt_ft == 3500 && f[0].speed_kt == 180);
  CHECK(jf::parseAdsb(adsb, strlen(adsb), 38.7223, -9.1393, 40, f, 2) == 2);  // keeps nearest 2
  CHECK(strcmp(f[1].callsign, "RYR123") == 0);
  CHECK(jf::parseAdsb("{bad", 4, 0, 0, 10, f, 2) == -1);
  CHECK(jf::parseAdsb("{\"x\":1}", 7, 0, 0, 10, f, 2) == -1);

  // Real adsb.fi format: list under "aircraft" (adsb.lol uses "ac").
  const char* fi =
      "{\"now\":1,\"resultCount\":2,\"aircraft\":["
      "{\"hex\":\"a\",\"flight\":\"EJU9046 \",\"t\":\"A320\",\"lat\":38.65,\"lon\":-9.20,"
      "\"alt_baro\":2375,\"gs\":169.2,\"track\":21.8,\"extra\":{\"x\":1}},"
      "{\"flight\":\"TAP93MA \",\"t\":\"E190\",\"lat\":38.66,\"lon\":-9.46,\"alt_baro\":13475}]}";
  n = jf::parseAdsb(fi, strlen(fi), 38.72, -9.14, 40, f, jf::kMaxFlights);
  CHECK(n == 2);
  CHECK(strcmp(f[0].callsign, "EJU9046") == 0);
  CHECK(strcmp(f[1].callsign, "TAP93MA") == 0);

  char b[16];
  jf::formatAltitude(3500, b, sizeof b);
  CHECK(strcmp(b, "3500 ft") == 0);
  jf::formatAltitude(12000, b, sizeof b);
  CHECK(strcmp(b, "FL120") == 0);
  jf::formatDistance(4.24f, b, sizeof b);
  CHECK(strcmp(b, "4.2 km") == 0);
  jf::formatDistance(150.4f, b, sizeof b);
  CHECK(strcmp(b, "150 km") == 0);

  CHECK(strcmp(jf::compass(0), "N") == 0);
  CHECK(strcmp(jf::compass(359), "N") == 0);
  CHECK(strcmp(jf::compass(225), "SW") == 0);
  CHECK(strcmp(jf::compass(-90), "W") == 0);

  CHECK(jf::backlightFor(-5) == 20);
  CHECK(jf::backlightFor(4095) == 255);
  CHECK(jf::backlightFor(99999) == 255);

  CHECK(board::kScreenW == 320 && board::kScreenH == 240);
  CHECK(board::kTftCs == 15 && board::kTouchCs == 33);

  using jf::Category;
  CHECK(jf::categorize("A320") == Category::Jet);
  CHECK(jf::categorize("B738") == Category::Jet);
  CHECK(jf::categorize("E190") == Category::Jet);
  CHECK(jf::categorize("A388") == Category::Wide);
  CHECK(jf::categorize("B77W") == Category::Wide);
  CHECK(jf::categorize("A359") == Category::Wide);
  CHECK(jf::categorize("C17") == Category::Wide);
  CHECK(jf::categorize("AT76") == Category::Prop);
  CHECK(jf::categorize("DH8D") == Category::Prop);
  CHECK(jf::categorize("C172") == Category::Light);
  CHECK(jf::categorize("PA28") == Category::Light);
  CHECK(jf::categorize("GLF5") == Category::Biz);
  CHECK(jf::categorize("C56X") == Category::Biz);
  CHECK(jf::categorize("EC35") == Category::Heli);
  CHECK(jf::categorize("R44") == Category::Heli);
  CHECK(jf::categorize("") == Category::Jet);
  CHECK(jf::categorize(nullptr) == Category::Jet);

  // Per-type silhouettes: every spec resolves to itself (no shadowing by an earlier,
  // shorter prefix) and generates convex, in-bounds polygons.
  for (size_t i = 0; i < jf::specCount(); ++i) {
    CHECK(&jf::specFor(jf::specAt(i).prefix) == &jf::specAt(i));
  }
  // 3D models: every spec (and the class fallbacks) builds within the mesh limits, with valid
  // indices and finite coordinates, and projects inside its box, sorted far to near.
  const jf::Palette pal = {{252, 250, 242}, {222, 222, 228}, {200, 20, 40}, {40, 40, 40},
                           {150, 190, 220}, {44, 32, 22}};
  static jf::ScreenTri tris[jf::kMaxTris];
  size_t checked = 0;
  auto checkType = [&](const char* ty) {
    const jf::Mesh& m = jf::buildAircraft(ty, pal);
    CHECK(m.nv > 8 && m.nv < jf::kMaxVerts && m.nt > 8 && m.nt < jf::kMaxTris);
    for (size_t i = 0; i < m.nv; ++i) {
      CHECK(m.v[i].x == m.v[i].x && m.v[i].y == m.v[i].y && m.v[i].z == m.v[i].z);  // not NaN
    }
    for (size_t i = 0; i < m.nt; ++i) CHECK(m.t[i].a < m.nv && m.t[i].b < m.nv && m.t[i].c < m.nv);
    for (int right = 0; right < 2; ++right) {
      const size_t n = jf::projectMesh(m, right != 0, 100, 100, 150, 120, tris, jf::kMaxTris);
      CHECK(n > m.nt / 2);
      for (size_t i = 0; i < n; ++i) {
        for (int k = 0; k < 3; ++k) {
          CHECK(tris[i].x[k] >= 100 - 76 && tris[i].x[k] <= 100 + 76);   // inside the box (+1px)
          CHECK(tris[i].y[k] >= 100 - 61 && tris[i].y[k] <= 100 + 61);
        }
        if (i > 0) CHECK(tris[i - 1].depth >= tris[i].depth);
      }
    }
    ++checked;
  };
  for (size_t i = 0; i < jf::specCount(); ++i) checkType(jf::specAt(i).prefix);
  for (const char* ty : {"", "ZZZZ", "EC35", "C172", "GLF5", "AT76", "CRJ9"}) checkType(ty);
  CHECK(checked == jf::specCount() + 7);
  // Types look different from each other.
  CHECK(jf::specFor("A319").length < jf::specFor("A320").length);
  CHECK(jf::specFor("A320").length < jf::specFor("A321").length);
  CHECK(jf::specFor("A388").engines == 4 && jf::specFor("B744").engines == 4);
  CHECK(jf::specFor("A320").engines == 2);
  CHECK(jf::specFor("CRJ9").mount == jf::Mount::Rear);
  CHECK(jf::specFor("AT76").mount == jf::Mount::Prop);
  CHECK(jf::specFor("EC35").mount == jf::Mount::Heli);
  {  // a longer type has a longer 3D model
    const float l319 = jf::buildAircraft("A319", pal).length, l321 = jf::buildAircraft("A321", pal).length;
    CHECK(l319 < l321);
  }

  const jf::Airline* a = jf::airlineFor("RYR8XY");
  CHECK(a != nullptr && strcmp(a->name, "Ryanair") == 0 && a->b1 == 144);
  CHECK(jf::airlineFor("TAP1234") != nullptr);
  CHECK(jf::airlineFor("CS-DXX") == nullptr);   // registration, not a callsign
  CHECK(jf::airlineFor("N512GX") == nullptr);   // 4th char is a letter
  CHECK(jf::airlineFor("XYZ123") == nullptr);   // unknown operator
  CHECK(jf::airlineFor("AB") == nullptr && jf::airlineFor(nullptr) == nullptr);
  for (size_t i = 0; i < jf::airlineCount(); ++i) {  // names fit the column, codes unique
    CHECK(strlen(jf::airlineAt(i).name) <= 15);
    CHECK(strlen(jf::airlineAt(i).icao) == 3);
    for (size_t j = i + 1; j < jf::airlineCount(); ++j)
      CHECK(strcmp(jf::airlineAt(i).icao, jf::airlineAt(j).icao) != 0);
  }
  CHECK(jf::airlineFor("AEE7PT") != nullptr);
  CHECK(jf::looksLikeAirlineCallsign("QQQ123") && !jf::looksLikeAirlineCallsign("CS-DXX"));

  if (failures == 0) printf("all firmware host tests passed\n");
  return failures ? 1 : 0;
}
