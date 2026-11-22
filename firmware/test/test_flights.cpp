// Host-side unit tests (no Arduino). Run via scripts/test.sh.
#include <stdio.h>
#include <string.h>

#include "../jf_flight_display/board.h"
#include "../jf_flight_display/airline.h"
#include "../jf_flight_display/flights.h"
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

  int sx, sy;  // nose (0,-1): north at heading 0, east at 90, south at 180
  jf::transformPoint(0, -1, 0, 10, 100, 100, &sx, &sy);
  CHECK(sx == 100 && sy == 90);
  jf::transformPoint(0, -1, 90, 10, 100, 100, &sx, &sy);
  CHECK(sx == 110 && sy == 100);
  jf::transformPoint(0, -1, 180, 10, 100, 100, &sx, &sy);
  CHECK(sx == 100 && sy == 110);
  for (Category c : {Category::Jet, Category::Wide, Category::Prop, Category::Light,
                     Category::Biz, Category::Heli}) {
    const jf::Shape& sh = jf::shapeFor(c);
    CHECK(sh.count > 0 && sh.scale > 0.0f);
    for (size_t p = 0; p < sh.count; ++p) {
      CHECK(sh.polys[p].n >= 3 && sh.polys[p].n <= jf::kMaxPolyPoints);
      for (size_t i = 0; i < sh.polys[p].n; ++i) {
        CHECK(sh.polys[p].x[i] >= -1.0f && sh.polys[p].x[i] <= 1.0f);
        CHECK(sh.polys[p].y[i] >= -1.0f && sh.polys[p].y[i] <= 1.0f);
      }
    }
  }

  const jf::Airline* a = jf::airlineFor("RYR8XY");
  CHECK(a != nullptr && strcmp(a->name, "Ryanair") == 0 && a->b1 == 144);
  CHECK(jf::airlineFor("TAP1234") != nullptr);
  CHECK(jf::airlineFor("CS-DXX") == nullptr);   // registration, not a callsign
  CHECK(jf::airlineFor("N512GX") == nullptr);   // 4th char is a letter
  CHECK(jf::airlineFor("XYZ123") == nullptr);   // unknown operator
  CHECK(jf::airlineFor("AB") == nullptr && jf::airlineFor(nullptr) == nullptr);
  CHECK(strlen(jf::airlineFor("EZY12")->name) <= 14);

  if (failures == 0) printf("all firmware host tests passed\n");
  return failures ? 1 : 0;
}
