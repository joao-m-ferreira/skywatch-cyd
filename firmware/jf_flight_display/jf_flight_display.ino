// Nearby-flights display for the ESP32-2432S028 (CYD).
// Standalone: fetches ADS-B data from adsb.fi directly over WiFi.
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

#include "board.h"
#include "display.h"
#include "flights.h"
#include "plate.h"
#include "secrets.h"  // copy secrets.h.example -> secrets.h (git-ignored)

namespace {
constexpr uint32_t kPollMs = 20000;
constexpr uint32_t kRotateMs = 6000;  // carousel: seconds each plane stays on screen
constexpr uint32_t kWifiTimeoutMs = 15000;

LGFX tft;
jf::Flight flights[jf::kMaxFlights];
int flightCount = 0;
int selected = 0;
bool fetchFailed = false;
char lastError[48] = "";
uint32_t lastPoll = 0;
uint32_t lastRotate = 0;

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < kWifiTimeoutMs) delay(250);
  if (WiFi.status() == WL_CONNECTED) return true;
  snprintf(lastError, sizeof lastError, "WiFi '%s' failed (status %d)", WIFI_SSID,
           static_cast<int>(WiFi.status()));
  Serial.println(lastError);
  return false;
}

// Standalone: query adsb.fi open data directly (HTTPS) and filter while streaming so the
// large response never has to fit in RAM.
bool fetchFlights() {
  if (WiFi.status() != WL_CONNECTED && !connectWifi()) return false;
  char url[128];
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v2/lat/%.2f/lon/%.2f/dist/%d",
           static_cast<double>(HOME_LAT), static_cast<double>(HOME_LON),
           static_cast<int>(RADIUS_KM / 1.852) + 1);
  // Public read-only data, no credentials sent: certificate validation is skipped to
  // avoid shipping/expiring a CA bundle. Do not reuse this client for anything private.
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding: the stream is plain JSON
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    snprintf(lastError, sizeof lastError, "HTTP begin failed");
    return false;
  }
  bool ok = false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    snprintf(lastError, sizeof lastError, "HTTP %d %s", code,
             code < 0 ? HTTPClient::errorToString(code).c_str() : "");
    Serial.println(lastError);
  } else {
    // Read the whole body first: parsing straight from the TLS stream can hit a
    // momentary "no data yet" and be mistaken for the end of the document.
    String body = http.getString();
    constexpr size_t kMaxBody = 120000;
    if (body.length() == 0 || body.length() > kMaxBody) {
      snprintf(lastError, sizeof lastError, "Body size %u", static_cast<unsigned>(body.length()));
      Serial.println(lastError);
    } else {
      JsonDocument filter, doc;
      jf::adsbFilter(filter);
      DeserializationError err =
          deserializeJson(doc, body, DeserializationOption::Filter(filter));
      if (!err && jf::adsbValid(doc)) {
      // Keep showing the same plane across refreshes when it is still in range.
      char keep[sizeof flights[0].callsign] = "";
      if (flightCount > 0) strncpy(keep, flights[selected].callsign, sizeof keep - 1);
      flightCount = jf::adsbToFlights(doc, HOME_LAT, HOME_LON, RADIUS_KM, flights,
                                      jf::kMaxFlights);
      selected = 0;
      for (int i = 0; i < flightCount; ++i) {
        if (strcmp(flights[i].callsign, keep) == 0) selected = i;
      }
        ok = true;
        lastError[0] = '\0';
      } else {
        snprintf(lastError, sizeof lastError, "JSON: %s (%u B)",
                 err ? err.c_str() : "no aircraft list", static_cast<unsigned>(body.length()));
        Serial.println(lastError);
      }
    }
  }
  http.end();
  return ok;
}

void drawScreen() {
  char note[24] = "LIVE";
  if (flightCount > 0) snprintf(note, sizeof note, "LIVE %d/%d", selected + 1, flightCount);
  if (flightCount == 0) {
    drawPlateMessage(tft, fetchFailed ? "Fetch failed" : "No aircraft in range",
                     fetchFailed ? lastError : "", note, fetchFailed);
    return;
  }
  if (selected >= flightCount) selected = 0;
  drawPlate(tft, flights[selected], selected, flightCount, note);
}

void updateBacklight() {
  tft.setBrightness(jf::backlightFor(analogRead(board::kLdr)));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);  // 320x240 landscape
  drawPlateMessage(tft, "Connecting...", WIFI_SSID, "LIVE", false);
  connectWifi();  // a failure is retried by fetchFlights() in loop()
  lastPoll = millis() - kPollMs;
}

void loop() {
  uint32_t now = millis();
  bool redraw = false;
  if (now - lastPoll >= kPollMs) {
    lastPoll = now;
    fetchFailed = !fetchFlights();
    if (fetchFailed) Serial.println("fetch failed; keeping last data");
    else Serial.printf("fetched %d aircraft\n", flightCount);
    redraw = true;
  }
  int32_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (flightCount > 0) {
      selected = (selected + 1) % flightCount;
      lastRotate = now;
      redraw = true;
    }
    delay(300);  // debounce
  }
  if (flightCount > 1 && now - lastRotate >= kRotateMs) {  // carousel
    selected = (selected + 1) % flightCount;
    lastRotate = now;
    redraw = true;
  }
  static uint32_t lastLdr = 0;
  if (now - lastLdr > 1000) {
    lastLdr = now;
    updateBacklight();
  }
  if (redraw || now < 1000) drawScreen();
  delay(20);
}
