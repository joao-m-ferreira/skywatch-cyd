// Host-side unit tests (no Arduino). Run via scripts/test.sh.
#include <stdio.h>
#include <string.h>

#include "../jf_flight_display/board.h"
#include "../jf_flight_display/airline.h"
#include "../jf_flight_display/config.h"
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
  CHECK(strcmp(f[0].reg, "OE-IJA") == 0 || f[0].reg[0] == '\0');
  CHECK(strcmp(f[1].callsign, "TAP93MA") == 0);

  char rt[40];
  CHECK(jf::formatRoute("Murcia", "RMU", "Dublin", "DUB", rt, 27) && strcmp(rt, "Murcia > Dublin") == 0);
  CHECK(jf::formatRoute("Frankfurt am Main", "FRA", "Santiago de Compostela", "SCQ", rt, 27) &&
        strlen(rt) <= 26 && strstr(rt, " > ") != nullptr);
  CHECK(jf::formatRoute("", "LIS", "", "OPO", rt, 27) && strcmp(rt, "LIS > OPO") == 0);
  CHECK(jf::formatRoute("Lisbon", "LIS", "", "", rt, 27) && strcmp(rt, "from Lisbon") == 0);
  CHECK(jf::formatRoute("", "", "Porto", "OPO", rt, 27) && strcmp(rt, "to Porto") == 0);
  CHECK(!jf::formatRoute("", "", "", "", rt, 27) && rt[0] == '\0');
  CHECK(!jf::formatRoute(nullptr, nullptr, nullptr, nullptr, rt, 27));
  char tiny[8];
  CHECK(jf::formatRoute("Lisbon", "LIS", "Porto", "OPO", tiny, sizeof tiny) && strlen(tiny) < sizeof tiny);

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

  CHECK(jf::backlightFor(-5) == 90);
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
  jf::Palette pal = {{252, 250, 242}, {222, 222, 228}, {200, 20, 40}, {40, 40, 40},
                     {150, 190, 220}, {44, 32, 22},    {255, 255, 255}, {205, 205, 210},
                     {200, 20, 40},   0,               1,             jf::kTailDisc, jf::kBodyCheat,
                     {200, 20, 40}};
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
      int ascents = 0;
      CHECK(n > m.nt / 2);
      for (size_t i = 0; i < n; ++i) {
        for (int k = 0; k < 3; ++k) {
          CHECK(tris[i].x[k] >= 100 - 76 && tris[i].x[k] <= 100 + 76);   // inside the box (+1px)
          CHECK(tris[i].y[k] >= 100 - 61 && tris[i].y[k] <= 100 + 61);
        }
        if (i > 0 && tris[i - 1].depth < tris[i].depth) ++ascents;
      }
      CHECK(ascents <= 1);  // far-to-near in two runs: the body, then the window/door overlays
    }
    ++checked;
  };
  for (size_t i = 0; i < jf::specCount(); ++i) checkType(jf::specAt(i).prefix);
  for (int tail = jf::kTailBand; tail <= jf::kTailCap; ++tail) {  // every livery design builds
    for (int body = jf::kBodyCheat; body <= jf::kBodyTransat; ++body) {
      pal.tail = static_cast<uint8_t>(tail);
      pal.body = static_cast<uint8_t>(body);
      checkType("A320");
    }
  }
  pal.tail = jf::kTailTap;  // TAP's finer fin and extra fuselage rings must fit every aircraft
  pal.body = jf::kBodyPlain;
  for (size_t i = 0; i < jf::specCount(); ++i) checkType(jf::specAt(i).prefix);
  pal.tail = jf::kTailDisc;
  pal.body = jf::kBodyCheat;
  for (const char* ty : {"", "ZZZZ", "EC35", "C172", "GLF5", "AT76", "CRJ9"}) checkType(ty);
  CHECK(checked == 2 * jf::specCount() + 7 + 130);
  // Types look different from each other.
  CHECK(jf::specFor("A319").length < jf::specFor("A320").length);
  CHECK(jf::specFor("A320").length < jf::specFor("A321").length);
  CHECK(jf::specFor("A388").engines == 4 && jf::specFor("B744").engines == 4);
  CHECK(jf::specFor("B722").engines == 3 && jf::specFor("B722").mount == jf::Mount::Rear);
  CHECK(jf::specFor("B712").length < 40.0f && jf::specFor("B703").engines == 4);
  CHECK(jf::specFor("A225").engines == 6 && jf::specFor("A124").engines == 4);
  CHECK(jf::specFor("A388").deck == 2 && jf::specFor("B744").deck == 1 && jf::specFor("A320").deck == 0);
  CHECK(jf::liveryFor(jf::airlineFor("ACA1234")).fin_base == jf::kFinSecondary);
  CHECK(jf::liveryFor(jf::airlineFor("NSZ1234")).nose_red == 1);
  CHECK(jf::liveryFor(jf::airlineFor("QTR1234")).hull == jf::kHullPearl);
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
  {
    const int ca[3] = {0, 0, 200}, cb[3] = {240, 200, 0};
    int o[3];
    jf::ledBlend(ca, cb, 0, o);
    CHECK(o[0] == 0 && o[2] == 200);                        // starts on colour A
    jf::ledBlend(ca, cb, 1000, o);
    CHECK(o[0] == 0 && o[2] == 200);                        // still holding A
    jf::ledBlend(ca, cb, 1400, o);
    CHECK(o[0] == 120 && o[2] == 100);                      // halfway through the fade
    jf::ledBlend(ca, cb, 2000, o);
    CHECK(o[0] == 240 && o[1] == 200 && o[2] == 0);         // holding B (yellow) for 1.2 s
    jf::ledBlend(ca, cb, 3200 + 100, o);
    CHECK(o[0] == 0 && o[2] == 200);                        // the cycle repeats
  }
  CHECK(jf::airlineHasTwoColours(*jf::airlineFor("RYR8XY")));    // blue + yellow
  CHECK(jf::airlineHasTwoColours(*jf::airlineFor("TAP123")));    // green + red
  CHECK(jf::airlineHasTwoColours(*jf::airlineFor("EIN123")));    // teal + lime
  CHECK(!jf::airlineHasTwoColours(*jf::airlineFor("EZY123")));   // orange + dark grey
  CHECK(!jf::airlineHasTwoColours(*jf::airlineFor("ACA123")));   // red + black
  CHECK(!jf::airlineHasTwoColours(*jf::airlineFor("KLM123")));   // sky blue + navy: one colour family
  CHECK(!jf::airlineHasTwoColours(*jf::airlineFor("SWR123")));   // red + grey
  for (size_t i = 0; i < jf::liveryCount(); ++i) {  // every livery row names a known airline
    char cs[8];
    snprintf(cs, sizeof cs, "%s1", jf::liveryCode(i));
    CHECK(jf::airlineFor(cs) != nullptr);
  }
  CHECK(jf::liveryFor(jf::airlineFor("SWR123")).tail == jf::kTailCross);
  CHECK(jf::liveryFor(jf::airlineFor("JST446")).body == jf::kBodyBelly);
  CHECK(jf::liveryFor(nullptr).tail == jf::kTailBand);
  CHECK(jf::looksLikeAirlineCallsign("QQQ123") && !jf::looksLikeAirlineCallsign("CS-DXX"));

  {  // serial configuration
    jf::Config c;
    jf::configDefaults(&c, "your-ssid", "pw", 38.7223, -9.1393, 40, "you@example.com");
    char r[1800];  // the help text is long
    CHECK(!jf::configWifiReady(c));
    CHECK(jf::configCommand(&c, "set ssid My Home WiFi", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(strcmp(c.ssid, "My Home WiFi") == 0 && jf::configWifiReady(c));  // spaces kept
    CHECK(jf::configCommand(&c, "set pass hunter 2 !", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(strcmp(c.pass, "hunter 2 !") == 0);
    CHECK(jf::configCommand(&c, "  SET   LAT   41.15  ", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(c.lat > 41.14 && c.lat < 41.16);
    CHECK(jf::configCommand(&c, "set lat 91", r, sizeof r) == jf::CmdAction::None && c.lat < 42.0);
    CHECK(strstr(r, "error") != nullptr);
    CHECK(jf::configCommand(&c, "set lat abc", r, sizeof r) == jf::CmdAction::None);
    CHECK(!c.auto_loc);  // fixing lat turned the IP lookup off
    CHECK(jf::configCommand(&c, "set location auto", r, sizeof r) == jf::CmdAction::Changed && c.auto_loc);
    CHECK(jf::configCommand(&c, "set location mars", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set location static 41.15 -8.62", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(!c.auto_loc && c.lat > 41.14 && c.lat < 41.16 && c.lon < -8.61 && c.lon > -8.63);
    CHECK(jf::configCommand(&c, "set location static 91 0", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set location static 41.15", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "show", r, sizeof r) == jf::CmdAction::None && strstr(r, "location static") != nullptr);
    CHECK(jf::configCommand(&c, "set location auto", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(jf::configCommand(&c, "show", r, sizeof r) == jf::CmdAction::None && strstr(r, "geojs.io") != nullptr);
    CHECK(jf::configCommand(&c, "set lon -8.6", r, sizeof r) == jf::CmdAction::Changed && !c.auto_loc);
    CHECK(!c.tz_set);
    CHECK(strcmp(jf::posixTzFor("Europe/Lisbon"), "WET0WEST,M3.5.0/1,M10.5.0") == 0);
    CHECK(jf::posixTzFor("Mars/Olympus") == nullptr && jf::posixTzFor(nullptr) == nullptr);
    CHECK(jf::configCommand(&c, "set lon -181", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set lon -8.6", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(jf::configCommand(&c, "set radius 0", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set radius 251", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set radius 25", r, sizeof r) == jf::CmdAction::Changed && c.radius_km == 25);
    CHECK(jf::configCommand(&c, "set contact no", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set contact me@site.org", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(jf::configCommand(&c, "set night 22 6", r, sizeof r) == jf::CmdAction::Changed);
    CHECK(c.night_on && c.night_start == 22 && c.night_end == 6);
    CHECK(jf::configCommand(&c, "set night 24 6", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set night 5", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::isNightHour(c, 23) && jf::isNightHour(c, 0) && jf::isNightHour(c, 5));  // wraps midnight
    CHECK(!jf::isNightHour(c, 6) && !jf::isNightHour(c, 12) && !jf::isNightHour(c, 21));
    jf::configCommand(&c, "set night 1 5", r, sizeof r);
    CHECK(jf::isNightHour(c, 1) && jf::isNightHour(c, 4) && !jf::isNightHour(c, 5) && !jf::isNightHour(c, 0));
    jf::configCommand(&c, "set night off", r, sizeof r);
    CHECK(!c.night_on && !jf::isNightHour(c, 2));
    jf::configCommand(&c, "set night on", r, sizeof r);
    CHECK(c.night_on);
    CHECK(c.wake_min == 10);
    CHECK(jf::configCommand(&c, "set wake 15", r, sizeof r) == jf::CmdAction::Changed && c.wake_min == 15);
    CHECK(jf::configCommand(&c, "set wake 0", r, sizeof r) == jf::CmdAction::None && c.wake_min == 15);
    CHECK(jf::configCommand(&c, "set wake 121", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set tz CET-1CEST,M3.5.0,M10.5.0/3", r, sizeof r) == jf::CmdAction::Changed && c.tz_set);
    CHECK(jf::configCommand(&c, "set colour red", r, sizeof r) == jf::CmdAction::None);
    CHECK(strstr(r, "unknown setting") != nullptr);
    CHECK(jf::configCommand(&c, "frobnicate", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "", r, sizeof r) == jf::CmdAction::None && r[0] == '\0');
    CHECK(jf::configCommand(&c, "show", r, sizeof r) == jf::CmdAction::None);
    CHECK(strstr(r, "My Home WiFi") != nullptr && strstr(r, "hunter") == nullptr && strstr(r, "****") != nullptr);
    CHECK(jf::configCommand(&c, "help", r, sizeof r) == jf::CmdAction::None && strstr(r, "set night") != nullptr);
    CHECK(jf::configCommand(&c, "reboot", r, sizeof r) == jf::CmdAction::Reboot);
    CHECK(jf::configCommand(&c, "factory", r, sizeof r) == jf::CmdAction::Factory);
    CHECK(jf::configCommand(&c, "set pass -", r, sizeof r) == jf::CmdAction::Changed && c.pass[0] == '\0');
    char small[16];  // tiny reply buffers never overflow
    jf::configCommand(&c, "help", small, sizeof small);
    CHECK(strlen(small) < sizeof small);
  }

  if (failures == 0) printf("all firmware host tests passed\n");
  return failures ? 1 : 0;
}
