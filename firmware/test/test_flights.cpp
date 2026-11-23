// Host-side unit tests (no Arduino). Run via scripts/test.sh.
#include <stdio.h>
#include <string.h>

#include "../jf_flight_display/board.h"
#include "../jf_flight_display/airline.h"
#include "../jf_flight_display/config.h"
#include "../jf_flight_display/crc_util.h"
#include "../jf_flight_display/flights.h"
#include "../jf_flight_display/layout.h"
#include "../jf_flight_display/mcp.h"
#include "../jf_flight_display/model3d.h"
#include "../jf_flight_display/portal_util.h"
#include "../jf_flight_display/silhouette.h"
#include "../jf_flight_display/timing_util.h"
#include "../jf_flight_display/version_util.h"

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
    CHECK(c.plane_sec == 6);  // seconds each plane stays on screen
    CHECK(jf::configCommand(&c, "set plane 12", r, sizeof r) == jf::CmdAction::Changed && c.plane_sec == 12);
    CHECK(jf::configCommand(&c, "set plane 2", r, sizeof r) == jf::CmdAction::None && c.plane_sec == 12);
    CHECK(jf::configCommand(&c, "set plane 121", r, sizeof r) == jf::CmdAction::None && c.plane_sec == 12);
    CHECK(jf::configCommand(&c, "set plane x", r, sizeof r) == jf::CmdAction::None && c.plane_sec == 12);
    CHECK(c.max_planes == 10 && static_cast<int>(jf::kMaxFlights) >= jf::kMaxPlanesLimit);  // most planes shown
    CHECK(jf::configCommand(&c, "set max 4", r, sizeof r) == jf::CmdAction::Changed && c.max_planes == 4);
    CHECK(jf::configCommand(&c, "set max 20", r, sizeof r) == jf::CmdAction::Changed && c.max_planes == 20);
    CHECK(jf::configCommand(&c, "set max 0", r, sizeof r) == jf::CmdAction::None && c.max_planes == 20);
    CHECK(jf::configCommand(&c, "set max 21", r, sizeof r) == jf::CmdAction::None && c.max_planes == 20);
    CHECK(jf::configCommand(&c, "set max many", r, sizeof r) == jf::CmdAction::None && c.max_planes == 20);
    jf::Config stick = c;  // a copy: the panel choice must not leak into the checks that follow
    CHECK(jf::configCommand(&stick, "set panel m5stickc", r, sizeof r) == jf::CmdAction::Display && stick.panel == 8);
    CHECK(jf::configCommand(&stick, "set panel tdisplay", r, sizeof r) == jf::CmdAction::Display && stick.panel == 7);
    // board variants: display settings restart the board, everything is validated
    CHECK(c.panel == 0 && !c.invert && !c.rgb_swap && c.rotation == 1 && c.led_pct == 100);
    CHECK(jf::configCommand(&c, "set panel st7789", r, sizeof r) == jf::CmdAction::Display && c.panel == 1);
    CHECK(jf::configCommand(&c, "set panel ili9341", r, sizeof r) == jf::CmdAction::Display && c.panel == 0);
    CHECK(jf::configCommand(&c, "set panel hx8357", r, sizeof r) == jf::CmdAction::None && c.panel == 0);
    CHECK(jf::configCommand(&c, "set panel st7796", r, sizeof r) == jf::CmdAction::Display && c.panel == 2);
    CHECK(jf::configCommand(&c, "set panel ili9488", r, sizeof r) == jf::CmdAction::Display && c.panel == 3);
    CHECK(jf::configCommand(&c, "set panel ssd1306", r, sizeof r) == jf::CmdAction::Display && c.panel == 6);
    CHECK(c.i2c_sda == 21 && c.i2c_scl == 22);
    CHECK(jf::configCommand(&c, "set i2c 4 15", r, sizeof r) == jf::CmdAction::Display && c.i2c_sda == 4 && c.i2c_scl == 15);
    CHECK(jf::configCommand(&c, "set i2c 4", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set i2c 4 4", r, sizeof r) == jf::CmdAction::None);
    CHECK(jf::configCommand(&c, "set i2c 4 99", r, sizeof r) == jf::CmdAction::None && c.i2c_scl == 15);
    jf::configCommand(&c, "set i2c 21 22", r, sizeof r);
    jf::configCommand(&c, "set panel ili9341", r, sizeof r);
    CHECK(jf::configCommand(&c, "set panel co5300", r, sizeof r) == jf::CmdAction::Display && c.panel == 4);
    CHECK(jf::configCommand(&c, "set panel sh8601", r, sizeof r) == jf::CmdAction::Display && c.panel == 5);
    jf::configCommand(&c, "set panel ili9341", r, sizeof r);
    CHECK(c.bl_pin == 21 && !c.touch_shared);
    CHECK(jf::configCommand(&c, "set backlight 27", r, sizeof r) == jf::CmdAction::Display && c.bl_pin == 27);
    CHECK(jf::configCommand(&c, "set backlight 40", r, sizeof r) == jf::CmdAction::None && c.bl_pin == 27);
    CHECK(jf::configCommand(&c, "set touch shared", r, sizeof r) == jf::CmdAction::Display && c.touch_shared);
    CHECK(jf::configCommand(&c, "set touch wireless", r, sizeof r) == jf::CmdAction::None && c.touch_shared);
    {  // the exact lines the web portal (web/app.js) sends after flashing must be accepted as they are
      jf::Config w;
      jf::configDefaults(&w, "", "", 0, 0, 40, "x@example.invalid");
      CHECK(jf::configCommand(&w, "set ssid My Home WiFi", r, sizeof r) == jf::CmdAction::Changed);
      CHECK(strcmp(w.ssid, "My Home WiFi") == 0);                    // spaces in the name are kept
      CHECK(jf::configCommand(&w, "set pass hunter2hunter2", r, sizeof r) == jf::CmdAction::Changed);
      CHECK(jf::configCommand(&w, "set pass -", r, sizeof r) == jf::CmdAction::Changed && w.pass[0] == '\0');  // open network
      CHECK(jf::configCommand(&w, "set location auto", r, sizeof r) == jf::CmdAction::Changed && w.auto_loc);
      CHECK(jf::configCommand(&w, "set location static 38.7223 -9.1393", r, sizeof r) == jf::CmdAction::Changed && !w.auto_loc);
      CHECK(jf::configCommand(&w, "set radius 55", r, sizeof r) == jf::CmdAction::Changed && w.radius_km == 55);
      CHECK(jf::configCommand(&w, "set night 1 5", r, sizeof r) == jf::CmdAction::Changed && w.night_on && w.night_start == 1 && w.night_end == 5);
      CHECK(jf::configCommand(&w, "set night off", r, sizeof r) == jf::CmdAction::Changed && !w.night_on);
      CHECK(strncmp(r, "ok", 2) == 0);                               // the page waits for a reply starting with "ok"
      jf::configCommand(&w, "set radius 999", r, sizeof r);
      CHECK(strncmp(r, "error", 5) == 0);                            // ... or "error"
    }
    jf::configCommand(&c, "set backlight 21", r, sizeof r);
    jf::configCommand(&c, "set touch separate", r, sizeof r);
    CHECK(jf::configCommand(&c, "set invert on", r, sizeof r) == jf::CmdAction::Display && c.invert);
    CHECK(jf::configCommand(&c, "set invert maybe", r, sizeof r) == jf::CmdAction::None && c.invert);
    CHECK(jf::configCommand(&c, "set rgb swap", r, sizeof r) == jf::CmdAction::Display && c.rgb_swap);
    CHECK(jf::configCommand(&c, "set rgb normal", r, sizeof r) == jf::CmdAction::Display && !c.rgb_swap);
    CHECK(jf::configCommand(&c, "set rotation 3", r, sizeof r) == jf::CmdAction::Display && c.rotation == 3);
    CHECK(jf::configCommand(&c, "set rotation 0", r, sizeof r) == jf::CmdAction::Display && c.rotation == 0);  // portrait
    CHECK(jf::configCommand(&c, "set rotation 4", r, sizeof r) == jf::CmdAction::None && c.rotation == 0);
    CHECK(jf::configCommand(&c, "set rotation x", r, sizeof r) == jf::CmdAction::None);
    jf::configCommand(&c, "set rotation 3", r, sizeof r);
    CHECK(c.layout == 0);
    CHECK(jf::configCommand(&c, "set layout stacked", r, sizeof r) == jf::CmdAction::Redraw && c.layout == 3);
    CHECK(jf::configCommand(&c, "set layout flip", r, sizeof r) == jf::CmdAction::Redraw && c.layout == 2);
    CHECK(jf::configCommand(&c, "set layout side", r, sizeof r) == jf::CmdAction::Redraw && c.layout == 1);
    CHECK(jf::configCommand(&c, "set layout auto", r, sizeof r) == jf::CmdAction::Redraw && c.layout == 0);
    CHECK(jf::configCommand(&c, "set layout diagonal", r, sizeof r) == jf::CmdAction::None && c.layout == 0);
    CHECK(jf::configCommand(&c, "set led 40", r, sizeof r) == jf::CmdAction::Changed && c.led_pct == 40);
    CHECK(jf::configCommand(&c, "set led 101", r, sizeof r) == jf::CmdAction::None && c.led_pct == 40);
    CHECK(jf::configCommand(&c, "set led -1", r, sizeof r) == jf::CmdAction::None && c.led_pct == 40);
    jf::configCommand(&c, "show", r, sizeof r);
    CHECK(strstr(r, "ILI9341") != nullptr && strstr(r, "rotation 3") != nullptr && strstr(r, "led 40%") != nullptr);
    CHECK(!jf::configSafeMode(0) && !jf::configSafeMode(2) && jf::configSafeMode(3) && jf::configSafeMode(9));
    {  // the whole help text must fit the console reply buffer of the firmware (3000 bytes), with room to spare
      char big[4096];
      CHECK(jf::configCommand(&c, "help", big, sizeof big) == jf::CmdAction::None);
      CHECK(strstr(big, "factory") != nullptr && strlen(big) < 3800);
    }
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

  // WiFi setup mode helpers and the new serial commands.
  {
    const uint8_t mac[6] = {0x24, 0x6F, 0x28, 0x01, 0xAB, 0x05};
    char name[24], pw[9], pw2[9], esc[64], qr[80];
    jf::apName(mac, name, sizeof name);
    CHECK(strcmp(name, "skywatch-AB05") == 0);
    jf::apPassword(0x12345678u, 0x9ABCDEF0u, pw);
    jf::apPassword(0x12345679u, 0x9ABCDEF0u, pw2);
    CHECK(strlen(pw) == 8 && strcmp(pw, pw2) != 0);
    for (const char* q = pw; *q; ++q) CHECK(strchr("23456789ABCDEFGHJKLMNPQRSTUVWXYZ", *q) != nullptr);  // no 0/O/1/I
    // every password character is reachable and the alphabet has no look-alikes
    jf::apPassword(0u, 0u, pw);
    CHECK(strcmp(pw, "22222222") == 0);
    jf::apPassword(0xFFFFFFFFu, 0xFFFFFFFFu, pw);
    CHECK(strcmp(pw, "ZZZZZZZZ") == 0);
    jf::htmlEscape("a<b>&\"c'", esc, sizeof esc);
    CHECK(strcmp(esc, "a&lt;b&gt;&amp;&quot;c&#39;") == 0);
    char tiny[8];
    jf::htmlEscape("<<<<<<", tiny, sizeof tiny);  // truncated, never overruns, always terminated
    CHECK(strlen(tiny) < sizeof tiny);
    CHECK(jf::formValueOk("My WiFi 5G") && !jf::formValueOk("a\nset ssid x") && !jf::formValueOk("a\rb"));
    jf::wifiQrText("skywatch-AB05", "ABCD2345", qr, sizeof qr);
    CHECK(strcmp(qr, "WIFI:T:WPA;S:skywatch-AB05;P:ABCD2345;;") == 0);
    CHECK(strlen(qr) <= 53);  // fits a version 3 QR code (low error correction)

    CHECK(jf::versionNewer("v0.5.0", "0.4.0") && jf::versionNewer("0.4.1", "v0.4.0") && jf::versionNewer("1.0.0", "0.9.9"));
    CHECK(!jf::versionNewer("v0.4.0", "0.4.0") && !jf::versionNewer("0.3.9", "0.4.0") && !jf::versionNewer("0.10.0", "0.10.1"));
    CHECK(jf::versionNewer("0.10.0", "0.9.0"));  // numeric, not textual
    CHECK(!jf::versionNewer("v0.5.0", "dev") && !jf::versionNewer("v0.5.0", "emu") && !jf::versionNewer("garbage", "0.4.0"));

    jf::Config c;
    jf::configDefaults(&c, "s", "p", 1.0, 2.0, 40, "a@b.pt");
    char r[4096];
    CHECK(jf::configCommand(&c, "setup", r, sizeof r) == jf::CmdAction::Setup && strstr(r, "setup mode") != nullptr);
    CHECK(jf::configCommand(&c, "version", r, sizeof r) == jf::CmdAction::Version);
    CHECK(jf::configCommand(&c, "update", r, sizeof r) == jf::CmdAction::Update);
    jf::configCommand(&c, "help", r, sizeof r);
    CHECK(strstr(r, "  setup ") != nullptr && strstr(r, "  version ") != nullptr && strstr(r, "  update ") != nullptr);
    CHECK(strlen(r) < 3800);  // the sketch's reply buffer is 4096 bytes
  }

  // Web portal / MCP server: settings, the access token helpers, and the MCP request handler.
  {
    jf::Config c;
    jf::configDefaults(&c, "s", "p", 1.0, 2.0, 40, "a@b.pt");
    char r[4096];
    CHECK(c.web_on && c.mcp_on && c.adv_on && c.token[0] == '\0');  // all on by default; the token is made when first needed
    CHECK(jf::configCommand(&c, "set web on", r, sizeof r) == jf::CmdAction::Changed && c.web_on);
    CHECK(jf::configCommand(&c, "set mcp on", r, sizeof r) == jf::CmdAction::Changed && c.mcp_on);
    CHECK(jf::configCommand(&c, "set mcp maybe", r, sizeof r) == jf::CmdAction::None && c.mcp_on);
    CHECK(jf::configCommand(&c, "set web off", r, sizeof r) == jf::CmdAction::Changed && !c.web_on);
    CHECK(jf::configCommand(&c, "set advertise off", r, sizeof r) == jf::CmdAction::Changed && !c.adv_on && c.mcp_on);  // independent switches
    CHECK(jf::configCommand(&c, "set advertise on", r, sizeof r) == jf::CmdAction::Changed && c.adv_on);
    CHECK(jf::configCommand(&c, "set advertise sometimes", r, sizeof r) == jf::CmdAction::None && c.adv_on);
    CHECK(jf::configCommand(&c, "set token ABCD2345EFGH6789", r, sizeof r) == jf::CmdAction::Changed && strcmp(c.token, "ABCD2345EFGH6789") == 0);
    CHECK(jf::configCommand(&c, "set token short", r, sizeof r) == jf::CmdAction::None && strcmp(c.token, "ABCD2345EFGH6789") == 0);
    CHECK(jf::configCommand(&c, "set token ABCD2345EFGH678!", r, sizeof r) == jf::CmdAction::None);  // letters and digits only
    CHECK(jf::configCommand(&c, "token", r, sizeof r) == jf::CmdAction::Token);
    CHECK(jf::configCommand(&c, "token new", r, sizeof r) == jf::CmdAction::NewToken);
    jf::configCommand(&c, "show", r, sizeof r);
    CHECK(strstr(r, "off, mcp on (header auth off), advertise on, token set") != nullptr && strstr(r, "ABCD2345") == nullptr);  // never printed by 'show'

    const uint32_t words[4] = {0x12345678u, 0x9ABCDEF0u, 0x0F1E2D3Cu, 0xFFFFFFFFu};
    char tok[25], tok2[25];
    jf::randomToken(words, tok);
    CHECK(strlen(tok) == 24);
    for (const char* q = tok; *q; ++q) CHECK(strchr("23456789ABCDEFGHJKLMNPQRSTUVWXYZ", *q) != nullptr);
    const uint32_t other[4] = {0x12345678u, 0x9ABCDEF0u, 0x0F1E2D3Cu, 0xFFFFFFFEu};
    jf::randomToken(other, tok2);
    CHECK(strcmp(tok, tok2) != 0);  // the last word reaches the last characters
    CHECK(jf::secretEquals("abc", "abc") && !jf::secretEquals("abc", "abd") && !jf::secretEquals("abc", "abcd") && !jf::secretEquals("", "a"));
    CHECK(strcmp(jf::bearerToken("Bearer xyz"), "xyz") == 0 && jf::bearerToken("Basic xyz")[0] == '\0' && jf::bearerToken("bearer xyz")[0] == '\0');

    // the MCP handler
    jf::Flight fl[2] = {};
    snprintf(fl[0].callsign, sizeof fl[0].callsign, "TAP443");
    snprintf(fl[0].type, sizeof fl[0].type, "A21N");
    snprintf(fl[0].reg, sizeof fl[0].reg, "CS-TJA");
    fl[0].alt_ft = 6800; fl[0].speed_kt = 259; fl[0].track = 271; fl[0].dist_km = 22.24f; fl[0].bearing = 180;
    snprintf(fl[1].callsign, sizeof fl[1].callsign, "RYR9NF");
    snprintf(fl[1].type, sizeof fl[1].type, "B738");
    fl[1].alt_ft = 12000; fl[1].dist_km = 30.0f; fl[1].bearing = 90;
    jf::McpContext ctx;
    ctx.flights = fl; ctx.count = 2; ctx.selected = 1; ctx.version = "0.5.0"; ctx.uptime_s = 99; ctx.wifi = true;
    ctx.radius_km = 40; ctx.max_planes = 10; ctx.panel = "m5stickc"; ctx.last_fetch_age_s = 4;
    char out[8192];
    const char* init = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    size_t w = jf::mcpHandle(init, strlen(init), ctx, out, sizeof out);
    CHECK(w > 0 && strstr(out, "\"protocolVersion\":\"2025-03-26\"") && strstr(out, "skywatch-cyd") && strstr(out, "\"id\":1"));
    const char* tl = "{\"jsonrpc\":\"2.0\",\"id\":\"a\",\"method\":\"tools/list\"}";
    w = jf::mcpHandle(tl, strlen(tl), ctx, out, sizeof out);
    CHECK(strstr(out, "get_nearby_flights") && strstr(out, "get_current_plane") && strstr(out, "get_status") && strstr(out, "\"id\":\"a\""));
    const char* note = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}";
    CHECK(jf::mcpHandle(note, strlen(note), ctx, out, sizeof out) == 0);  // notifications get no response
    const char* call = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"get_nearby_flights\",\"arguments\":{\"limit\":1}}}";
    w = jf::mcpHandle(call, strlen(call), ctx, out, sizeof out);
    CHECK(strstr(out, "TAP443") && strstr(out, "CS-TJA") && strstr(out, "TAP Portugal") && !strstr(out, "RYR9NF") && strstr(out, "\"isError\":false"));
    const char* cur = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"get_current_plane\"}}";
    w = jf::mcpHandle(cur, strlen(cur), ctx, out, sizeof out);
    CHECK(strstr(out, "RYR9NF") && !strstr(out, "TAP443"));  // the selected one
    const char* st = "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"get_status\"}}";
    w = jf::mcpHandle(st, strlen(st), ctx, out, sizeof out);
    CHECK(strstr(out, "0.5.0") && strstr(out, "m5stickc") && !strstr(out, "token") && !strstr(out, "ssid") && !strstr(out, "lat"));
    const char* bad = "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"set_ssid\"}}";
    jf::mcpHandle(bad, strlen(bad), ctx, out, sizeof out);
    CHECK(strstr(out, "\"error\"") && strstr(out, "-32602"));  // only the read-only tools exist
    const char* lim = "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"get_nearby_flights\",\"arguments\":{\"limit\":999}}}";
    jf::mcpHandle(lim, strlen(lim), ctx, out, sizeof out);
    CHECK(strstr(out, "-32602"));
    const char* nope = "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"resources/list\"}";
    jf::mcpHandle(nope, strlen(nope), ctx, out, sizeof out);
    CHECK(strstr(out, "-32601"));
    jf::mcpHandle("{not json", 9, ctx, out, sizeof out);
    CHECK(strstr(out, "-32700") && strstr(out, "\"id\":null"));
    jf::mcpHandle("[1,2]", 5, ctx, out, sizeof out);
    CHECK(strstr(out, "-32700"));
    char tiny[40];  // a too-small buffer is never overrun
    w = jf::mcpHandle(tl, strlen(tl), ctx, tiny, sizeof tiny);
    CHECK(w < sizeof tiny);
    // discovery documents: the card describes the endpoint and its header, never the token; the catalog points to it
    char card[1536], cat[512];  // the sizes webapi.cpp uses
    CHECK(jf::serverCardJson("skywatch.local", "0.5.0", true, card, sizeof card) > 0);
    CHECK(strstr(card, "\"$schema\":\"https://static.modelcontextprotocol.io/schemas/v1/server-card.schema.json\""));
    CHECK(strstr(card, "\"name\":\"io.github.joao-m-ferreira/skywatch-cyd\"") && strstr(card, "\"version\":\"0.5.0\""));
    CHECK(strstr(card, "\"type\":\"streamable-http\"") && strstr(card, "http://{host}/mcp") && strstr(card, "\"supportedProtocolVersions\":[\"2025-03-26\"]"));
    CHECK(strstr(card, "\"name\":\"Authorization\"") && strstr(card, "Bearer {token}") && strstr(card, "\"isSecret\":true"));
    {
      JsonDocument parsed;
      CHECK(!deserializeJson(parsed, card));
      const char* desc = parsed["description"] | "";
      const char* nm = parsed["name"] | "";
      CHECK(strlen(desc) >= 1 && strlen(desc) <= 100);  // the schema's limits
      const char* slash = strchr(nm, '/');
      CHECK(slash != nullptr && strchr(slash + 1, '/') == nullptr);  // namespace/name, exactly one slash
    }
    CHECK(jf::aiCatalogJson("skywatch.local", cat, sizeof cat) > 0);
    CHECK(strstr(cat, "\"specVersion\":\"1.0\"") && strstr(cat, "application/mcp-server-card+json") && strstr(cat, "http://skywatch.local/mcp/server-card") && strstr(cat, "urn:air:skywatch.local:mcp:skywatch-cyd"));
    char small[20];
    CHECK(jf::serverCardJson("h", "v", true, small, sizeof small) == 0 && jf::aiCatalogJson("h", small, sizeof small) == 0);  // too small: nothing written
    // MCP header authentication is a switch, off by default; the card lists the headers only when it is on
    {
      jf::Config a;
      jf::configDefaults(&a, "s", "p", 1.0, 2.0, 40, "a@b.pt");
      CHECK(!a.mcp_auth);
      CHECK(jf::configCommand(&a, "set mcpauth on", r, sizeof r) == jf::CmdAction::Changed && a.mcp_auth);
      CHECK(jf::configCommand(&a, "set mcpauth off", r, sizeof r) == jf::CmdAction::Changed && !a.mcp_auth);
      CHECK(jf::configCommand(&a, "set mcpauth maybe", r, sizeof r) == jf::CmdAction::None);
      char with[1536], without[1536];
      CHECK(jf::serverCardJson("skywatch.local", "0.5.0", true, with, sizeof with) > 0);
      CHECK(jf::serverCardJson("skywatch.local", "0.5.0", false, without, sizeof without) > 0);
      CHECK(strstr(with, "Bearer {token}") && strstr(with, "X-API-Key"));
      CHECK(!strstr(without, "headers") && !strstr(without, "Bearer") && strstr(without, "http://{host}/mcp"));
    }
    // elapsed time: the stuck-fetch restart fired after every fetch because now - lastGoodMs wrapped to ~49 days
    CHECK(jf::elapsedMs(1000, 400) == 600 && jf::elapsedMs(400, 1000) == -600);
    CHECK(jf::elapsedMs(5, 0xFFFFFFF0u) == 21);  // across the wrap of millis()
    CHECK(!jf::olderThan(700000, 701500, 600000));   // a timestamp 1.5 s in the future is not "10 minutes ago"
    CHECK(jf::olderThan(700000, 99000, 600000) && !jf::olderThan(700000, 100000, 600000) && jf::olderThan(700000, 99999, 600000));
    jf::McpContext empty;
    jf::mcpHandle(cur, strlen(cur), empty, out, sizeof out);
    CHECK(strstr(out, "No aircraft in range"));
  }

  // Resiliency: stored settings are sanitised, and flash checksums detect damage.
  {
    jf::Config g;
    jf::configDefaults(&g, "s", "p", 38.7, -9.1, 40, "a@b.pt");
    CHECK(jf::configSanitize(&g) == 0);  // good data is left alone
    CHECK(g.lat == 38.7 && g.lon == -9.1 && g.radius_km == 40 && g.plane_sec == 6);
    // garbage: every byte 0xFF (what an erased flash sector reads as) must come out valid
    jf::Config bad;
    memset(&bad, 0xFF, sizeof bad);
    CHECK(jf::configSanitize(&bad) > 0);
    CHECK(bad.radius_km >= 1 && bad.radius_km <= 250 && bad.plane_sec >= 3 && bad.plane_sec <= 120);
    CHECK(bad.max_planes >= 1 && bad.max_planes <= jf::kMaxPlanesLimit && bad.wake_min >= 1 && bad.wake_min <= 120);
    CHECK(bad.night_start >= 0 && bad.night_start <= 23 && bad.night_end >= 0 && bad.night_end <= 23);
    CHECK(bad.panel >= 0 && bad.panel <= 8 && bad.rotation >= 0 && bad.rotation <= 3 && bad.layout >= 0 && bad.layout <= 3);
    CHECK(bad.led_pct >= 0 && bad.led_pct <= 100 && bad.bl_pin >= 0 && bad.bl_pin <= 48);
    CHECK(bad.i2c_sda != bad.i2c_scl && bad.i2c_sda >= 0 && bad.i2c_scl >= 0);
    CHECK(bad.lat == 0.0 && bad.lon == 0.0 && bad.auto_loc);  // impossible position: look it up again
    CHECK(strlen(bad.ssid) < sizeof bad.ssid && strlen(bad.pass) < sizeof bad.pass && strlen(bad.contact) < sizeof bad.contact);
    CHECK(strlen(bad.tz) < sizeof bad.tz && strlen(bad.token) < sizeof bad.token && strcmp(bad.tz, jf::kDefaultTz) == 0);
    CHECK(bad.ssid[0] == '\0' && bad.pass[0] == '\0' && bad.contact[0] == '\0' && bad.token[0] == '\0');  // erased flash is not text
    jf::Config few = g;  // single bad fields are repaired one by one
    few.plane_sec = 0;
    few.rotation = 9;
    few.lat = 95.0;
    few.i2c_sda = few.i2c_scl = 5;
    CHECK(jf::configSanitize(&few) == 4 && few.plane_sec == 6 && few.rotation == 1 && few.auto_loc && few.i2c_sda != few.i2c_scl);
    jf::Config nan_pos = g;
    nan_pos.lat = nan_pos.lon = 0.0 / 0.0;
    CHECK(jf::configSanitize(&nan_pos) == 1 && nan_pos.auto_loc);  // NaN is not a position
    // CRC-32: the standard check value, and any change is noticed
    CHECK(jf::crc32("123456789", 9) == 0xCBF43926u);
    unsigned char blob[64];
    memset(blob, 0x5A, sizeof blob);
    const uint32_t before = jf::crc32(blob, sizeof blob);
    blob[17] ^= 0x01;
    CHECK(jf::crc32(blob, sizeof blob) != before);
  }

  // Screen layouts: every element fits every panel shape in every mode, the plane's area and the
  // text never overlap, and Auto picks a sensible mode.
  {
    struct Panel {
      int w, h;
    };
    const Panel panels[] = {{320, 240}, {480, 320}, {480, 480}, {320, 480}, {240, 320}, {800, 480},
                            {448, 368}, {368, 448}, {480, 272}, {1024, 600}};
    const jf::LayoutMode modes[] = {jf::LayoutMode::Auto, jf::LayoutMode::Side, jf::LayoutMode::SideFlip,
                                    jf::LayoutMode::Stacked};
    auto inside = [](const jf::Layout& l, jf::Rect r) {
      const int x0 = l.X(r.x), y0 = l.Y(r.y), x1 = l.X(r.x + r.w), y1 = l.Y(r.y + r.h);
      return x0 >= 0 && y0 >= 0 && x1 <= l.w && y1 <= l.h && r.w > 0 && r.h > 0;
    };
    auto overlap = [](jf::Rect a, jf::Rect b) {
      return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
    };
    for (const Panel& p : panels) {
      for (jf::LayoutMode want : modes) {
        const jf::Layout l = jf::layoutFor(p.w, p.h, want);
        CHECK(l.mode != jf::LayoutMode::Auto && l.s > 0.4f);
        CHECK(inside(l, l.model) && inside(l, l.info));
        CHECK(!overlap(l.model, l.info));
        CHECK(l.model.w >= 120 && l.model.h >= 90);  // room for the plane
        // the canvas fills the screen: no unused margin
        CHECK(l.cw * l.s <= p.w + 2.0f && l.ch * l.s <= p.h + 2.0f);
        CHECK(l.cw * l.s >= p.w - 2.0f && l.ch * l.s >= p.h - 2.0f);
        // text rows sit inside the text area, in order, without touching
        CHECK(l.call.y >= l.info.y && l.distance.y + 34 <= l.info.y + l.info.h + 2);
        CHECK(l.altitude.x >= l.info.x && l.speed.x + (l.mode == jf::LayoutMode::Stacked ? 144 : 124) <= l.info.x + l.info.w + 10);
        CHECK(l.call.y + 24 <= l.airline.y + 2 && l.airline.y + 16 <= l.rule.y + 2 && l.rule.y < l.altitude.y);
        if (l.mode == jf::LayoutMode::Stacked) {
          CHECK(l.model.y + l.model.h <= l.compass.y - 12 + 4);                  // the plane is above the text
          CHECK(l.speed.y == l.altitude.y && l.distance.y > l.altitude.y + 30);  // two columns, then a row
          CHECK(l.speed.x - l.altitude.x >= 140);                                // columns do not collide
        } else {
          CHECK(l.speed.y > l.altitude.y + 30 && l.distance.y > l.speed.y + 30);  // three rows
          CHECK(l.compass.y + 12 <= l.ch && l.dots.y + 4 <= l.ch);
        }
      }
    }
    // the default 320x240 layout is exactly the original design
    const jf::Layout d = jf::layoutFor(320, 240);
    CHECK(d.mode == jf::LayoutMode::Side && d.s == 1.0f && d.model.x == 14 && d.model.w == 156);
    CHECK(d.call.x == 186 && d.call.y == 40 && d.altitude.y == 112 && d.speed.y == 150 && d.distance.y == 188);
    CHECK(d.dots.x == 92 && d.compass.x == 40 && d.divider_x == 176 && d.header_right.x == 306);
    // Auto: landscape panels get side-by-side, portrait and square get stacked; a stack is never forced onto a landscape panel
    CHECK(jf::layoutFor(480, 320).mode == jf::LayoutMode::Side);
    CHECK(jf::layoutFor(480, 480).mode == jf::LayoutMode::Stacked);
    CHECK(jf::layoutFor(320, 480).mode == jf::LayoutMode::Stacked);
    CHECK(jf::layoutFor(480, 320, jf::LayoutMode::Stacked).mode == jf::LayoutMode::Side);
    CHECK(jf::layoutFor(480, 480, jf::LayoutMode::Side).mode == jf::LayoutMode::Stacked);  // no room for side by side
    CHECK(jf::layoutFor(480, 480, jf::LayoutMode::SideFlip).mode == jf::LayoutMode::Stacked);
    // short panels (TTGO T-Display, 240x135) get the compact layout: pixel-exact, one rotating data field
    for (jf::LayoutMode want : {jf::LayoutMode::Auto, jf::LayoutMode::Side, jf::LayoutMode::SideFlip}) {
      const jf::Layout c = jf::layoutFor(240, 135, want);
      CHECK(c.compact && c.s == 1.0f && c.cw == 240 && c.ch == 135);
      CHECK(inside(c, c.model) && inside(c, c.info) && !overlap(c.model, c.info));
      CHECK(c.model.w >= 100 && c.model.h >= 70);  // room for the plane
      // rows (heights: bold 9pt 18, oblique 9pt 18, Font2 16, field label 16 + value 24) stay inside the column
      CHECK(c.call.y + 18 <= c.type.y && c.type.y + 18 <= c.airline.y && c.airline.y + 16 <= c.bars.y);
      CHECK(c.bars.y < c.rule.y && c.rule.y < c.data.y && c.data.y + 17 + 24 <= c.info.y + c.info.h + 2);
      CHECK(c.compass.y + 10 <= c.ch && c.dots.y + 4 <= c.model.y);
    }
    // the M5StickC's 160x80: the micro variant (no dots, rule or airline name), everything inside the screen
    for (jf::LayoutMode want : {jf::LayoutMode::Auto, jf::LayoutMode::Side, jf::LayoutMode::SideFlip}) {
      const jf::Layout m = jf::layoutFor(160, 80, want);
      CHECK(m.compact && m.micro && m.s == 1.0f && m.cw == 160 && m.ch == 80);
      CHECK(inside(m, m.model) && inside(m, m.info) && !overlap(m.model, m.info));
      CHECK(m.model.w >= 60 && m.model.h >= 40);
      // rows: bold 9pt 18, oblique 9pt 18, bars 3, field label 16 + value 16
      CHECK(m.call.y + 18 <= m.type.y && m.type.y + 18 <= m.bars.y && m.bars.y + 3 <= m.data.y);
      CHECK(m.data.y + 16 + 16 <= m.info.y + m.info.h);
      CHECK(m.compass.y + 7 <= m.ch && m.compass.y - 7 - 6 >= m.model.y + m.model.h);  // compass under the plane
      CHECK(m.rule_w >= 64 && m.data.x + m.rule_w <= m.info.x + m.info.w + 1);
    }
    CHECK(!jf::layoutFor(240, 135).micro && !jf::layoutFor(320, 240).micro);
    CHECK(!jf::layoutFor(320, 240).compact && !jf::layoutFor(480, 272).compact && !jf::layoutFor(240, 320).compact);
    CHECK(jf::layoutFor(240, 135, jf::LayoutMode::SideFlip).model.x > jf::layoutFor(240, 135, jf::LayoutMode::SideFlip).info.x);
    CHECK(jf::layoutFor(240, 135, jf::LayoutMode::Stacked).compact);  // a stack never fits a short panel
    // flipped: the plane is on the right, the text on the left
    const jf::Layout f =jf::layoutFor(480, 320, jf::LayoutMode::SideFlip);
    CHECK(f.model.x > f.info.x && f.call.x < f.model.x && f.divider_x < f.model.x);
    const jf::Layout n = jf::layoutFor(480, 320, jf::LayoutMode::Side);
    CHECK(n.model.x < n.info.x && n.call.x > n.model.x + n.model.w);
  }

  // Text-only display: every line fits 16 characters, whatever the aircraft
  {
    jf::Flight f = {};
    snprintf(f.callsign, sizeof f.callsign, "TAP443");
    snprintf(f.type, sizeof f.type, "A21N");
    f.alt_ft = 6800; f.speed_kt = 259; f.track = 271; f.dist_km = 22.2f; f.bearing = 180;
    jf::OledText o;
    jf::formatOledText(f, "TAP Portugal", true, &o);
    CHECK(strcmp(o.call, "TAP443") == 0 && strcmp(o.type, "A21N") == 0 && strcmp(o.airline, "TAP Portugal") == 0);
    CHECK(strcmp(o.alt_speed, "FL068  259 kt") == 0);
    CHECK(strcmp(o.dist_dir, "22.2 km S  W271") == 0);
    f.alt_ft = 3500; f.speed_kt = 120; f.dist_km = 123.0f; f.bearing = 315; f.track = 305;
    snprintf(f.callsign, sizeof f.callsign, "ABCDEFGH");
    f.type[0] = '\0';
    jf::formatOledText(f, nullptr, true, &o);
    CHECK(strcmp(o.airline, "Operator ABC") == 0 && strcmp(o.type, "-") == 0);
    CHECK(strcmp(o.alt_speed, "3500 ft  120 kt") == 0);
    CHECK(strlen(o.dist_dir) <= 16 && strlen(o.alt_speed) <= 16 && strlen(o.airline) <= 16);
    jf::formatOledText(f, nullptr, false, &o);
    CHECK(strcmp(o.airline, "Private / other") == 0);
    jf::formatOledText(f, "A very long airline name indeed", true, &o);
    CHECK(strlen(o.airline) == 16);
  }

  if (failures == 0) printf("all firmware host tests passed\n");
  return failures ? 1 : 0;
}
