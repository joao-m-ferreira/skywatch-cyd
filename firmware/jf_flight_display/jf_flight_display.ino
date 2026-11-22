// Nearby-flights display for the ESP32-2432S028 (CYD).
// Fetches compact JSON from the Go service in ../../service. Without WiFi or
// a reachable service it falls back to built-in demo data.
#include <HTTPClient.h>
#include <WiFi.h>

#include "board.h"
#include "display.h"
#include "flights.h"
#include "secrets.h"  // copy secrets.h.example -> secrets.h (git-ignored)

namespace {
constexpr uint32_t kPollMs = 20000;
constexpr uint32_t kWifiTimeoutMs = 15000;

LGFX tft;
jf::Flight flights[jf::kMaxFlights];
int flightCount = 0;
int selected = 0;
bool demo = false;
uint32_t lastPoll = 0;

const char kDemoJson[] =
    "{\"f\":[{\"cs\":\"TAP1234\",\"ty\":\"A320\",\"alt\":3500,\"spd\":180,\"trk\":200,"
    "\"km\":4.2,\"brg\":225},{\"cs\":\"RYR8XY\",\"ty\":\"B738\",\"alt\":12000,\"spd\":310,"
    "\"trk\":88,\"km\":18.7,\"brg\":10},{\"cs\":\"CS-DXX\",\"ty\":\"C172\",\"alt\":2000,"
    "\"spd\":90,\"trk\":10,\"km\":31.0,\"brg\":340}]}";

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < kWifiTimeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

bool fetchFlights() {
  if (WiFi.status() != WL_CONNECTED && !connectWifi()) return false;
  char url[192];
  snprintf(url, sizeof url, "%s/v1/flights?lat=%.4f&lon=%.4f&radius_km=%d&n=%d", SERVICE_URL,
           static_cast<double>(HOME_LAT), static_cast<double>(HOME_LON), RADIUS_KM,
           static_cast<int>(jf::kMaxFlights));
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(url)) return false;
  if (SERVICE_TOKEN[0] != '\0') http.addHeader("X-Token", SERVICE_TOKEN);
  bool ok = false;
  if (http.GET() == HTTP_CODE_OK) {
    String body = http.getString();
    int n = jf::parseFlights(body.c_str(), body.length(), flights, jf::kMaxFlights);
    if (n >= 0) {
      flightCount = n;
      ok = true;
    }
  }
  http.end();
  return ok;
}

void loadDemo() {
  flightCount = jf::parseFlights(kDemoJson, sizeof kDemoJson - 1, flights, jf::kMaxFlights);
}

void drawScreen() {
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(top_left);
  tft.setFont(&fonts::Font2);
  tft.drawString(demo ? "DEMO DATA" : "Flights nearby", 6, 4);
  if (flightCount == 0) {
    tft.setFont(&fonts::FreeSansBold12pt7b);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("No aircraft in range", 20, 100);
    tft.endWrite();
    return;
  }
  if (selected >= flightCount) selected = 0;
  const jf::Flight& f = flights[selected];
  char buf[24];
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setFont(&fonts::FreeSansBold24pt7b);
  tft.drawString(f.callsign, 6, 26);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFont(&fonts::FreeSans12pt7b);
  tft.drawString(f.type[0] ? f.type : "type n/a", 6, 78);
  jf::formatAltitude(f.alt_ft, buf, sizeof buf);
  tft.drawString(buf, 6, 108);
  snprintf(buf, sizeof buf, "%d kt  hdg %d", f.speed_kt, f.track);
  tft.drawString(buf, 6, 138);
  char d[16];
  jf::formatDistance(f.dist_km, d, sizeof d);
  snprintf(buf, sizeof buf, "%s %s", d, jf::compass(f.bearing));
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(buf, 6, 168);
  // Next-planes list along the bottom.
  tft.setFont(&fonts::Font2);
  for (int i = 0; i < flightCount && i < 4; ++i) {
    tft.setTextColor(i == selected ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof buf, "%s %.0fkm", flights[i].callsign,
             static_cast<double>(flights[i].dist_km));
    tft.drawString(buf, 6 + (i % 2) * 160, 205 + (i / 2) * 16);
  }
  tft.endWrite();
}

void updateBacklight() {
  tft.setBrightness(jf::backlightFor(analogRead(board::kLdr)));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);  // 320x240 landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Connecting...", 10, 10, &fonts::Font2);
  if (!connectWifi()) {
    demo = true;
    loadDemo();
  }
  lastPoll = millis() - kPollMs;
}

void loop() {
  uint32_t now = millis();
  bool redraw = false;
  if (!demo && now - lastPoll >= kPollMs) {
    lastPoll = now;
    if (!fetchFlights()) Serial.println("fetch failed; keeping last data");
    redraw = true;
  }
  int32_t x, y;
  if (tft.getTouch(&x, &y) && flightCount > 0) {
    selected = (selected + 1) % flightCount;
    redraw = true;
    delay(250);  // debounce
  }
  static uint32_t lastLdr = 0;
  if (now - lastLdr > 1000) {
    lastLdr = now;
    updateBacklight();
  }
  if (redraw || now < 1000) drawScreen();
  delay(20);
}
