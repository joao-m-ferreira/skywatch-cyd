// Nearby-flights display for the ESP32-2432S028 (CYD).
// Standalone: fetches ADS-B data from adsb.fi directly over WiFi. Settings are stored in
// flash and changed over USB serial (115200 baud, type "help").
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "airline.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "flights.h"
#include "photos.h"
#include "plate.h"
#include "routes.h"
#include "secrets.h"  // build-time defaults; copy secrets.h.example -> secrets.h (git-ignored)

namespace {
constexpr uint32_t kPollMs = 20000;
constexpr uint32_t kRotateMs = 6000;   // carousel: how long each plane stays on screen
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kWakeMs = 30000;    // a touch wakes the screen for this long at night
constexpr uint32_t kWatchdogMs = 90000;
constexpr uint32_t kStuckRestartMs = 10UL * 60UL * 1000UL;  // no good fetch for this long
constexpr uint32_t kDailyRestartMs = 24UL * 60UL * 60UL * 1000UL;
constexpr size_t kMinFreeHeap = 30000;

LGFX tft;
jf::Config cfg;
Preferences prefs;
jf::Flight flights[jf::kMaxFlights];
int flightCount = 0;
int selected = 0;
bool fetchFailed = false;
char lastError[48] = "";
uint32_t lastPoll = 0;
uint32_t lastRotate = 0;
uint32_t lastFetchOk = 0;
uint32_t lastGoodMs = 0;   // last successful fetch (millis), for the stuck-restart check
bool nightMode = false;    // currently inside the night window
bool screenOff = false;
uint32_t wakeUntil = 0;
bool netRestart = false;   // settings changed: reconnect WiFi, re-sync clock, refetch
bool geoDone = false;
uint32_t lastGeoTry = 0;
bool setupScreenShown = false;
uint32_t nightTestUntil = 0;  // "night test": force night mode until this millis() (not saved)
int lastBacklight = -1;

// ---------------------------------------------------------------- settings in flash

void loadConfig() {
  jf::configDefaults(&cfg, WIFI_SSID, WIFI_PASSWORD, HOME_LAT, HOME_LON, RADIUS_KM,
                     PLANESPOTTERS_CONTACT);
  prefs.begin("skywatch", true);
  if (prefs.isKey("ssid")) prefs.getString("ssid", cfg.ssid, sizeof cfg.ssid);
  if (prefs.isKey("pass")) prefs.getString("pass", cfg.pass, sizeof cfg.pass);
  cfg.lat = prefs.getDouble("lat", cfg.lat);
  cfg.lon = prefs.getDouble("lon", cfg.lon);
  cfg.radius_km = prefs.getInt("radius", cfg.radius_km);
  if (prefs.isKey("contact")) prefs.getString("contact", cfg.contact, sizeof cfg.contact);
  cfg.night_on = prefs.getBool("night", cfg.night_on);
  cfg.night_start = prefs.getInt("nstart", cfg.night_start);
  cfg.night_end = prefs.getInt("nend", cfg.night_end);
  if (prefs.isKey("tz")) prefs.getString("tz", cfg.tz, sizeof cfg.tz);
  cfg.auto_loc = prefs.getBool("aloc", cfg.auto_loc);
  cfg.tz_set = prefs.getBool("tzset", cfg.tz_set);
  prefs.end();
}

void saveConfig() {
  prefs.begin("skywatch", false);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putDouble("lat", cfg.lat);
  prefs.putDouble("lon", cfg.lon);
  prefs.putInt("radius", cfg.radius_km);
  prefs.putString("contact", cfg.contact);
  prefs.putBool("night", cfg.night_on);
  prefs.putInt("nstart", cfg.night_start);
  prefs.putInt("nend", cfg.night_end);
  prefs.putString("tz", cfg.tz);
  prefs.putBool("aloc", cfg.auto_loc);
  prefs.putBool("tzset", cfg.tz_set);
  prefs.end();
}

void restartNow(const char* why) {
  Serial.printf("restarting: %s\n", why);
  Serial.flush();
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------------- network

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(cfg.ssid, cfg.pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < kWifiTimeoutMs) {
    esp_task_wdt_reset();
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) return true;
  snprintf(lastError, sizeof lastError, "WiFi '%s' failed (status %d)", cfg.ssid,
           static_cast<int>(WiFi.status()));
  Serial.println(lastError);
  return false;
}

void startClock() { configTzTime(cfg.tz, "pool.ntp.org", "time.cloudflare.com"); }

bool timeValid() { return time(nullptr) > 1700000000; }

// Approximate position from the public IP (city level), via geojs.io. Only used while the
// location is "auto"; the result is saved so the last known position survives a failed lookup.
bool geoLocate() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();  // public data, nothing private is sent
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, "https://get.geojs.io/v1/ip/geo.json")) return false;
  bool ok = false;
  if (http.GET() == HTTP_CODE_OK) {
    String body = http.getString();
    JsonDocument filter, doc;
    filter["latitude"] = true;
    filter["longitude"] = true;
    filter["city"] = true;
    filter["timezone"] = true;
    if (!deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
      char* e1 = nullptr;
      char* e2 = nullptr;
      const char* sla = doc["latitude"] | "";
      const char* slo = doc["longitude"] | "";
      const double la = strtod(sla, &e1), lo = strtod(slo, &e2);
      if (e1 != sla && e2 != slo && la >= -90 && la <= 90 && lo >= -180 && lo <= 180) {
        cfg.lat = la;
        cfg.lon = lo;
        const char* tz = jf::posixTzFor(doc["timezone"] | "");
        if (tz != nullptr && !cfg.tz_set) {
          strncpy(cfg.tz, tz, sizeof cfg.tz - 1);
          startClock();
        }
        saveConfig();
        Serial.printf("location from IP (approximate): %s %.4f, %.4f\n",
                      static_cast<const char*>(doc["city"] | "?"), la, lo);
        ok = true;
      }
    }
  }
  http.end();
  return ok;
}

// Standalone: query adsb.fi open data directly (HTTPS).
bool fetchFlightsOnce() {
  if (WiFi.status() != WL_CONNECTED && !connectWifi()) return false;
  char url[128];
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v2/lat/%.2f/lon/%.2f/dist/%d", cfg.lat,
           cfg.lon, static_cast<int>(cfg.radius_km / 1.852) + 1);
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
        flightCount =
            jf::adsbToFlights(doc, cfg.lat, cfg.lon, cfg.radius_km, flights, jf::kMaxFlights);
        lastFetchOk = lastGoodMs = millis();
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

// The HTTPS body occasionally arrives truncated; one retry fixes it.
bool fetchFlights() {
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (fetchFlightsOnce()) return true;
    delay(400);
  }
  return false;
}

// ---------------------------------------------------------------- LED

// RGB LED on the back of the board (active low). Shows the airline's primary colour;
// colours are normalised to full brightness for their hue, then scaled by kLedBrightness.
constexpr float kLedBrightness = 1.0f;  // 0..1

// This LED's green is much stronger than its red, so yellow looks green/white unless the channels
// are balanced first.
constexpr float kGainR = 1.0f, kGainG = 0.45f, kGainB = 0.8f;

void setLed(int r, int g, int b) {
  const float fr = static_cast<float>(r) * kGainR, fg = static_cast<float>(g) * kGainG,
              fb = static_cast<float>(b) * kGainB;
  const float peak = fr > fg ? (fr > fb ? fr : fb) : (fg > fb ? fg : fb);
  const float k = peak > 0.0f ? 255.0f / peak * kLedBrightness : 0.0f;
  ledcWrite(board::kLedR, 255 - static_cast<int>(fr * k));
  ledcWrite(board::kLedG, 255 - static_cast<int>(fg * k));
  ledcWrite(board::kLedB, 255 - static_cast<int>(fb * k));
}

void setupLed() {
  for (int pin : {board::kLedR, board::kLedG, board::kLedB}) ledcAttach(pin, 5000, 8);
  setLed(0, 0, 0);
  const int test[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};  // boot test: R, G, B
  for (const auto& c : test) {
    setLed(c[0], c[1], c[2]);
    delay(250);
  }
  setLed(0, 0, 0);
}

// LED state, owned by a small background task so the colours keep animating while the main loop
// is busy with network requests: off, a steady colour, or a hold-and-fade between two colours.
enum LedMode { kLedOff = 0, kLedSteady = 1, kLedTwo = 2 };
volatile int ledMode = kLedOff;
volatile int ledA[3] = {0, 0, 0}, ledB[3] = {0, 0, 0};

void ledForPlane(const jf::Flight* f) {
  const jf::Airline* al = f != nullptr ? jf::airlineFor(f->callsign) : nullptr;
  if (al == nullptr || nightMode) {  // unknown operator, no aircraft, or night: LED off
    ledMode = kLedOff;
    return;
  }
  ledA[0] = al->r1; ledA[1] = al->g1; ledA[2] = al->b1;
  ledB[0] = al->r2; ledB[1] = al->g2; ledB[2] = al->b2;
  ledMode = jf::airlineHasTwoColours(*al) ? kLedTwo : kLedSteady;
}

void ledTask(void*) {
  for (;;) {
    const int mode = ledMode;
    if (mode == kLedOff) {
      setLed(0, 0, 0);
    } else if (mode == kLedSteady) {
      setLed(ledA[0], ledA[1], ledA[2]);
    } else {
      const int a[3] = {ledA[0], ledA[1], ledA[2]}, b[3] = {ledB[0], ledB[1], ledB[2]};
      int c[3];
      jf::ledBlend(a, b, millis(), c);
      setLed(c[0], c[1], c[2]);
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

// ---------------------------------------------------------------- screen

void drawScreen() {
  char left[40], right[24];
  snprintf(right, sizeof right, "%d/%d", selected + 1, flightCount);
  if (lastFetchOk != 0) {
    snprintf(left, sizeof left, "%d aircraft within %d km", flightCount, cfg.radius_km);
    if (flightCount > 0) {  // the route of the plane on screen replaces the count when known
      const jf::Route* rt = jf::routeFor(flights[selected < flightCount ? selected : 0].callsign);
      if (rt != nullptr) jf::formatRoute(rt->o_city, rt->o_iata, rt->d_city, rt->d_iata, left, 27);
    }
    snprintf(right, sizeof right, "%d/%d  %us ago", selected + 1, flightCount,
             static_cast<unsigned>((millis() - lastFetchOk) / 1000));
  } else {
    snprintf(left, sizeof left, "Flights nearby");
    right[0] = '\0';
  }
  if (flightCount == 0) {
    right[0] = '\0';
    ledForPlane(nullptr);
    drawPlateMessage(tft, fetchFailed ? "Fetch failed" : "No aircraft in range",
                     fetchFailed ? lastError : "", left, right, fetchFailed);
    return;
  }
  if (selected >= flightCount) selected = 0;
  ledForPlane(&flights[selected]);
  drawPlate(tft, flights[selected], selected, flightCount, left, right,
            jf::photoFor(flights[selected].reg));
}

void updateBacklight() {
  lastBacklight = nightMode ? 70 : jf::backlightFor(analogRead(board::kLdr));
  tft.setBrightness(lastBacklight);
}

// ---------------------------------------------------------------- serial console

void printStatus() {
  struct tm t;
  char when[32] = "not synced";
  if (timeValid() && getLocalTime(&t, 0)) strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &t);
  Serial.printf("uptime   %lu s\nheap     %u free, %u lowest\nwifi     %s (%s) rssi %d\n"
                "clock    %s\nnight    %s%s\nscreen   %s, backlight %d/255, light sensor %d/4095\n"
                "aircraft %d, last fetch %s\n",
                static_cast<unsigned long>(millis() / 1000), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMinFreeHeap()),
                WiFi.status() == WL_CONNECTED ? "connected" : "DOWN",
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-",
                WiFi.RSSI(), when, cfg.night_on ? "enabled" : "disabled",
                nightMode ? " (active now)" : "", screenOff ? "OFF" : "on", lastBacklight,
                analogRead(board::kLdr), flightCount,
                lastFetchOk ? "ok" : (lastError[0] ? lastError : "none yet"));
}

void handleLine(const char* line) {
  if (strcasecmp(line, "status") == 0) {
    printStatus();
    return;
  }
  if (strcasecmp(line, "night test") == 0) {  // 20 s of night mode, never saved
    nightTestUntil = millis() + 20000;
    Serial.println("night test: screen and LED off for 20 s (touch the screen to wake it for 30 s)");
    return;
  }
  static char reply[1800];
  const jf::CmdAction action = jf::configCommand(&cfg, line, reply, sizeof reply);
  Serial.print(reply);
  switch (action) {
    case jf::CmdAction::Changed:
      saveConfig();
      jf::photoSetContact(cfg.contact);
      netRestart = true;  // pick the new settings up
      break;
    case jf::CmdAction::Factory:
      prefs.begin("skywatch", false);
      prefs.clear();
      prefs.end();
      loadConfig();
      jf::photoSetContact(cfg.contact);
      netRestart = true;
      break;
    case jf::CmdAction::Reboot:
      restartNow("requested over serial");
      break;
    case jf::CmdAction::None:
      break;
  }
}

void pollSerial() {
  static char buf[200];
  static size_t len = 0;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        buf[len] = '\0';
        len = 0;
        handleLine(buf);
      }
    } else if (len + 1 < sizeof buf) {
      buf[len++] = c;
    }
  }
}

// ---------------------------------------------------------------- night mode

bool isNightNow() {
  if (millis() < nightTestUntil) return true;
  if (!cfg.night_on || !timeValid()) return false;
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  return jf::isNightHour(cfg, t.tm_hour);
}

// ---------------------------------------------------------------- robustness

void housekeeping(uint32_t now, bool sleeping) {
  esp_task_wdt_reset();

  // WiFi watchdog: if the link stays down, start over instead of waiting forever.
  static uint32_t downSince = 0;
  if (WiFi.status() != WL_CONNECTED && jf::configWifiReady(cfg)) {
    if (downSince == 0) downSince = now;
    if (now - downSince > 45000) {
      Serial.println("wifi: down for 45 s, reconnecting");
      WiFi.disconnect();
      WiFi.begin(cfg.ssid, cfg.pass);
      downSince = now;
    }
  } else {
    downSince = 0;
  }

  // Nothing fetched for 10 minutes although WiFi is configured: restart.
  if (!sleeping && jf::configWifiReady(cfg) && now > kStuckRestartMs && now - lastGoodMs > kStuckRestartMs) {
    restartNow("no successful fetch for 10 minutes");
  }
  // Daily restart, preferably while the screen is off at night.
  if (now > kDailyRestartMs && (sleeping || now > kDailyRestartMs + 2UL * 3600UL * 1000UL)) {
    restartNow("daily restart");
  }
  // Heap safety net (three low readings in a row, checked every 5 s).
  static uint32_t lastHeapCheck = 0;
  static int low = 0;
  if (now - lastHeapCheck >= 5000) {
    lastHeapCheck = now;
    low = ESP.getFreeHeap() < kMinFreeHeap ? low + 1 : 0;
    if (low >= 3) restartNow("low memory");
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  loadConfig();
  jf::photoSetContact(cfg.contact);
  tft.init();
  tft.setRotation(1);  // 320x240 landscape
  setupLed();
  xTaskCreatePinnedToCore(ledTask, "led", 3072, nullptr, 1, nullptr, 0);  // animates the LED independently

  // Hardware watchdog for the main loop: restart if it hangs for 90 s.
  const esp_task_wdt_config_t wdt = {.timeout_ms = kWatchdogMs, .idle_core_mask = 0, .trigger_panic = true};
  if (esp_task_wdt_reconfigure(&wdt) != ESP_OK) esp_task_wdt_init(&wdt);
  esp_task_wdt_add(nullptr);

  Serial.println("\nskywatch-cyd: type 'help' for settings");
  if (jf::configWifiReady(cfg)) {
    drawPlateMessage(tft, "Connecting...", cfg.ssid, "Flights nearby", "", false);
    if (connectWifi()) {
      startClock();
      if (cfg.auto_loc) geoDone = geoLocate();
    }
  }
  lastPoll = millis() - kPollMs;
}

void loop() {
  uint32_t now = millis();
  pollSerial();

  if (!jf::configWifiReady(cfg)) {  // first run: nothing to connect to yet
    if (!setupScreenShown || netRestart) {
      ledForPlane(nullptr);
      drawPlateMessage(tft, "WiFi not set up", "USB serial 115200: type 'help'", "Flights nearby",
                       "", true);
      setupScreenShown = true;
      netRestart = false;
    }
    housekeeping(now, false);
    delay(100);
    return;
  }
  setupScreenShown = false;

  if (netRestart) {  // a setting changed
    netRestart = false;
    drawPlateMessage(tft, "Applying settings...", cfg.ssid, "Flights nearby", "", false);
    WiFi.disconnect(true);
    flightCount = 0;
    lastFetchOk = 0;
    lastGoodMs = now;
    if (connectWifi()) {
      startClock();
      geoDone = false;
      if (cfg.auto_loc) geoDone = geoLocate();
    }
    lastPoll = millis() - kPollMs;
    now = millis();
  }
  // Auto location: retry every 10 minutes until a lookup has worked.
  if (cfg.auto_loc && !geoDone && now - lastGeoTry > 600000 && WiFi.status() == WL_CONNECTED) {
    lastGeoTry = now;
    geoDone = geoLocate();
    if (geoDone) lastPoll = now - kPollMs;
  }

  // Night mode: screen and LED off inside the window; a touch wakes it for 30 s.
  static uint32_t lastNightCheck = 0;
  if (now - lastNightCheck >= 1000 || lastNightCheck == 0) {
    lastNightCheck = now;
    const bool night = isNightNow();
    if (night != nightMode) {
      nightMode = night;
      Serial.println(night ? "night mode: on" : "night mode: off");
      if (!night) {  // morning: wake up and refresh
        screenOff = false;
        updateBacklight();
        lastPoll = now - kPollMs;
      }
    }
  }
  if (nightMode && now >= wakeUntil) {
    if (!screenOff) {
      ledMode = kLedOff;
      tft.setBrightness(0);
      screenOff = true;
    }
    int32_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {  // touch: wake for 30 s with fresh data
      wakeUntil = now + kWakeMs;
      screenOff = false;
      updateBacklight();
      lastPoll = now - kPollMs;
      delay(300);
    } else {
      housekeeping(now, true);
      delay(100);
      return;  // no fetching, no drawing while asleep
    }
  }

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
    if (nightMode) wakeUntil = now + kWakeMs;  // keep the screen on while it is being used
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
  if (redraw || now < 1000) {
    const uint32_t t0 = millis();
    drawScreen();
    Serial.printf("drawn in %u ms, free heap %u\n", static_cast<unsigned>(millis() - t0),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    // Photos: fetch the current plane's first (redraw when it arrives), then the next one's.
    if (flightCount > 0) {
      esp_task_wdt_reset();
      if (jf::routePrefetch(flights[selected].callsign)) drawScreen();
      esp_task_wdt_reset();
      if (jf::photoPrefetch(flights[selected].reg)) drawScreen();
      esp_task_wdt_reset();
      if (flightCount > 1) {
        const jf::Flight& nx = flights[(selected + 1) % flightCount];
        jf::routePrefetch(nx.callsign);
        esp_task_wdt_reset();
        jf::photoPrefetch(nx.reg);
      }
    }
  }
  housekeeping(now, false);
  delay(20);
}
