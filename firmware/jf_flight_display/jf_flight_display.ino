// Nearby-flights display for the ESP32-2432S028 (CYD).
// Standalone: fetches ADS-B data from adsb.fi directly over WiFi. Settings are stored in
// flash and changed over USB serial (115200 baud, type "help").
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <new>
#include <time.h>

#include "airline.h"
#include "board.h"
#include "config.h"
#include "display.h"
#include "flights.h"
#include "layout.h"
#include "photos.h"
#include "portal.h"
#include "portal_util.h"
#include "plate.h"
#include "routes.h"
#include "version.h"
#include "timing_util.h"
#include "version_util.h"
#include "webapi.h"
#include "secrets.h"  // build-time defaults; copy secrets.h.example -> secrets.h (git-ignored)

namespace {
constexpr uint32_t kPollMs = 20000;
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kWatchdogMs = 90000;
constexpr uint32_t kStuckRestartMs = 10UL * 60UL * 1000UL;  // no good fetch for this long
constexpr uint32_t kDailyRestartMs = 24UL * 60UL * 60UL * 1000UL;
constexpr size_t kMinFreeHeap = 30000;

// Constructed in setup(), not as a global: the LGFX constructor runs floating-point code (touch
// calibration), which the ESP32-S3 forbids in global constructors (they run before the scheduler:
// "Coprocessor exception" and a boot loop). Found by running the S3 build in the simulator.
alignas(LGFX) uint8_t tftStorage[sizeof(LGFX)];
LGFX& tft = *reinterpret_cast<LGFX*>(tftStorage);
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
uint32_t nightTestUntil = 0;  // "night test": force night mode until this millis() (not saved)
int lastBacklight = -1;
// Stability: reset reason, consecutive crashes (panic / watchdog / brownout) and safe mode.
esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;
int crashCount = 0;
bool safeMode = false;
bool stableMarked = false;
bool forceRedraw = false;
// 0.96" SSD1306 text-only OLED (panel 6): no 3D model, photo, LED or touch; the BOOT button changes plane.
bool isOled() { return cfg.panel == 6; }
// LilyGO TTGO T-Display (panel 7): colour 135x240 screen, no touch, no RGB LED (GPIOs 4/16 drive its screen),
// no light sensor; the left button (GPIO 0) shows the next plane.
bool isTDisplay() { return cfg.panel == 7; }
// M5StickC (panel 8): 80x160 colour screen behind an AXP192 power chip; button A (GPIO 37) shows the next plane.
bool isStick() { return cfg.panel == 8; }
bool hasButton() { return isOled() || isTDisplay() || isStick(); }
int buttonPin() { return isStick() ? 37 : 0; }  // 37 is input-only and has a pull-up on the board
// Short screens (the T-Display) show one data field at a time and rotate it every few seconds.
uint32_t planeMs() { return static_cast<uint32_t>(cfg.plane_sec) * 1000UL; }  // carousel: how long each plane stays
bool compactScreen() { return !isOled() && jf::layoutFor(tft.width(), tft.height(), static_cast<jf::LayoutMode>(cfg.layout)).compact; }
uint32_t pageStart = 0;  // when the plane on screen appeared: its data fields always start with distance
char pageCall[12] = "";
// The route of the plane on screen as text of at most n-1 characters (city names when they fit, else airport codes);
// false when it is not known (yet). The 160x80 screen has room for "LIS>YYZ" only.
bool routeText(char* out, size_t n) {
  out[0] = '\0';
  if (flightCount == 0 || selected >= flightCount) return false;
  const jf::Route* rt = jf::routeFor(flights[selected].callsign);
  if (rt == nullptr || !jf::formatRoute(rt->o_city, rt->o_iata, rt->d_city, rt->d_iata, out, n)) return false;
  return true;
}
size_t routeChars() { return isOled() ? 17 : jf::layoutFor(tft.width(), tft.height(), static_cast<jf::LayoutMode>(cfg.layout)).micro ? 10 : 14; }
// Route text for the screen at hand, or false. On the micro screen the spaces around '>' are dropped to fit.
bool screenRoute(char* out, size_t n) {
  if (!routeText(out, n < routeChars() + 1 ? n : routeChars() + 1)) return false;
  if (!isOled() && jf::layoutFor(tft.width(), tft.height(), static_cast<jf::LayoutMode>(cfg.layout)).micro) {
    char* w = out;
    for (const char* r = out; *r != '\0'; ++r) {
      if (*r != ' ') *w++ = *r;
    }
    *w = '\0';
  }
  return out[0] != '\0';
}
// Pages of data on a screen that shows one thing at a time: the compact screens rotate distance, altitude, speed (and
// the route when known); the OLED alternates the airline row with the route.
int pageCount() {
  if (!compactScreen() && !isOled()) return 1;
  char t[24];
  return (screenRoute(t, sizeof t) ? 1 : 0) + (isOled() ? 1 : 3);
}
uint32_t pageMs() {  // time per page: the time per plane shared out, never under a second
  const uint32_t each = planeMs() / static_cast<uint32_t>(pageCount());
  return each < 1000 ? 1000 : each;
}
int dataPage() { return pageCount() > 1 ? static_cast<int>(((millis() - pageStart) / pageMs()) % static_cast<uint32_t>(pageCount())) : 0; }
int shownPage = 0;
bool setupMode = false;  // the board is running its own WiFi + settings page
void enterSetupMode(const char* why);
bool hasLed() { return board::kHasLed && !isOled() && !isTDisplay() && !isStick(); }
int lowBlockCount = 0;

// Readable name of an ESP reset reason, for the boot log and `status`.
const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "PANIC (crash)";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "BROWNOUT (weak power)";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_EXT: return "reset button";
    default: return "other";
  }
}
// True for resets that mean a crash (panic, watchdog, brownout): those count towards safe mode.
bool abnormalReset(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
         r == ESP_RST_BROWNOUT;
}

uint32_t wakeMs() { return static_cast<uint32_t>(cfg.wake_min) * 60000UL; }  // touch-to-wake at night

// ---------------------------------------------------------------- settings in flash

// Loads the settings: the build-time defaults, then this board's macros (firmware/boards.json), then whatever is saved
// in flash, then a sanity check (flash can hold garbage or another firmware version's values).
void loadConfig() {
  jf::configDefaults(&cfg, WIFI_SSID, WIFI_PASSWORD, HOME_LAT, HOME_LON, RADIUS_KM,
                     PLANESPOTTERS_CONTACT);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  cfg.panel = 4;     // Waveshare AMOLED 1.8 (V2) ...
  cfg.rotation = 0;  // ... in its native portrait orientation: the stacked layout
#endif
  // Per-hardware defaults baked in by the release builds (firmware/boards.json), so a user does not
  // need the serial console for their board. Anything saved in flash still overrides them.
#ifdef SKYWATCH_PANEL
  cfg.panel = SKYWATCH_PANEL;
#endif
#ifdef SKYWATCH_INVERT
  cfg.invert = SKYWATCH_INVERT != 0;
#endif
#ifdef SKYWATCH_BL_PIN
  cfg.bl_pin = SKYWATCH_BL_PIN;
#endif
#ifdef SKYWATCH_TOUCH_SHARED
  cfg.touch_shared = SKYWATCH_TOUCH_SHARED != 0;
#endif
#ifdef SKYWATCH_I2C_SDA
  cfg.i2c_sda = SKYWATCH_I2C_SDA;
#endif
#ifdef SKYWATCH_I2C_SCL
  cfg.i2c_scl = SKYWATCH_I2C_SCL;
#endif
#ifdef SKYWATCH_ROTATION
  cfg.rotation = SKYWATCH_ROTATION;
#endif
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
  cfg.wake_min = prefs.getInt("wake", cfg.wake_min);
  cfg.plane_sec = prefs.getInt("plane", cfg.plane_sec);
  cfg.max_planes = prefs.getInt("maxpl", cfg.max_planes);
  cfg.web_on = prefs.getBool("web", cfg.web_on);
  cfg.mcp_on = prefs.getBool("mcp", cfg.mcp_on);
  cfg.adv_on = prefs.getBool("adv", cfg.adv_on);
  cfg.mcp_auth = prefs.getBool("mcpauth", cfg.mcp_auth);
  if (prefs.isKey("token")) prefs.getString("token", cfg.token, sizeof cfg.token);
  if (prefs.isKey("tz")) prefs.getString("tz", cfg.tz, sizeof cfg.tz);
  cfg.auto_loc = prefs.getBool("aloc", cfg.auto_loc);
  cfg.tz_set = prefs.getBool("tzset", cfg.tz_set);
  cfg.panel = prefs.getInt("panel", cfg.panel);
  cfg.bl_pin = prefs.getInt("blpin", cfg.bl_pin);
  cfg.touch_shared = prefs.getBool("tshared", cfg.touch_shared);
  cfg.i2c_sda = prefs.getInt("sda", cfg.i2c_sda);
  cfg.i2c_scl = prefs.getInt("scl", cfg.i2c_scl);
  cfg.invert = prefs.getBool("invert", cfg.invert);
  cfg.rgb_swap = prefs.getBool("rgbswap", cfg.rgb_swap);
  cfg.rotation = prefs.getInt("rot", cfg.rotation);
  cfg.layout = prefs.getInt("layout", cfg.layout);
  cfg.led_pct = prefs.getInt("led", cfg.led_pct);
  prefs.end();
  const int fixed = jf::configSanitize(&cfg);  // flash can hold garbage or another version's values
  if (fixed > 0) Serial.printf("warning: %d stored setting(s) were out of range and were reset to defaults\n", fixed);
}

// Writes every setting to flash; called after each accepted serial command or settings-page save.
void saveConfig() {
  if (!prefs.begin("skywatch", false)) {
    Serial.println("error: the settings storage could not be opened; nothing was saved");
    return;
  }
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
  prefs.putDouble("lat", cfg.lat);
  prefs.putDouble("lon", cfg.lon);
  prefs.putInt("radius", cfg.radius_km);
  prefs.putString("contact", cfg.contact);
  prefs.putBool("night", cfg.night_on);
  prefs.putInt("nstart", cfg.night_start);
  prefs.putInt("nend", cfg.night_end);
  prefs.putInt("wake", cfg.wake_min);
  prefs.putInt("plane", cfg.plane_sec);
  prefs.putInt("maxpl", cfg.max_planes);
  prefs.putBool("web", cfg.web_on);
  prefs.putBool("mcp", cfg.mcp_on);
  prefs.putBool("adv", cfg.adv_on);
  prefs.putBool("mcpauth", cfg.mcp_auth);
  prefs.putString("token", cfg.token);
  prefs.putString("tz", cfg.tz);
  prefs.putBool("aloc", cfg.auto_loc);
  prefs.putBool("tzset", cfg.tz_set);
  prefs.putInt("panel", cfg.panel);
  prefs.putInt("blpin", cfg.bl_pin);
  prefs.putBool("tshared", cfg.touch_shared);
  prefs.putInt("sda", cfg.i2c_sda);
  prefs.putInt("scl", cfg.i2c_scl);
  prefs.putBool("invert", cfg.invert);
  prefs.putBool("rgbswap", cfg.rgb_swap);
  prefs.putInt("rot", cfg.rotation);
  prefs.putInt("layout", cfg.layout);
  prefs.putInt("led", cfg.led_pct);
  prefs.end();
}

// Logs why, then restarts. The reason stays in the serial log; crashes are counted separately at boot (safe mode).
void restartNow(const char* why) {
  Serial.printf("restarting: %s\n", why);
  Serial.flush();
  delay(200);
  ESP.restart();
}

// ---------------------------------------------------------------- network

// Joins the saved WiFi, waiting up to kWifiTimeoutMs; false (with lastError set) when it did not connect.
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
          snprintf(cfg.tz, sizeof cfg.tz, "%s", tz);
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

// A Stream that waits for the network instead of reporting "no data" at the first gap: ArduinoJson treats -1 as the
// end of the document, and a TLS stream often has none ready for a moment. It ends when the server closes the
// connection (or after 8 s of silence). Used to parse the aircraft list straight off the connection.
class WaitingStream : public Stream {
 public:
  explicit WaitingStream(WiFiClient* c) : c_(c) {}
  int available() override { return c_->available(); }
  int peek() override { return c_->peek(); }
  size_t write(uint8_t) override { return 0; }
  int read() override {
    if (started_ == 0) started_ = millis() | 1u;  // never 0, which means "not started"
    // Whatever the server does, one document ends: after kMaxBytes or kMaxMs the stream reports its end (the parser
    // then fails cleanly with an incomplete-document error and the fetch is retried later).
    if (total_ >= kMaxBytes || jf::olderThan(millis(), started_, kMaxMs)) return -1;
    const uint32_t t0 = millis();
    while (c_->available() == 0) {
      if (!c_->connected() || jf::olderThan(millis(), t0, 8000)) return -1;
      delay(1);
      esp_task_wdt_reset();
    }
    ++total_;
    return c_->read();
  }

 private:
  static constexpr size_t kMaxBytes = 400000;   // far above any aircraft list (about 1 KB per plane)
  static constexpr uint32_t kMaxMs = 30000;     // for the whole document
  WiFiClient* c_;
  size_t total_ = 0;
  uint32_t started_ = 0;
};

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
    // Parse straight off the connection through the filter: the body is about 1 KB per plane and has no
    // Content-Length, and collecting it first needs a block of that size (it dominated peak heap and, on small
    // heaps, silently lost chunks). WaitingStream bridges the momentary "no data yet" of a TLS stream.
    WaitingStream stream(http.getStreamPtr());
    JsonDocument filter, doc;
    jf::adsbFilter(filter);
    DeserializationError err = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
    if (!err && jf::adsbValid(doc)) {
      // Keep showing the same plane across refreshes when it is still in range.
      char keep[sizeof flights[0].callsign] = "";
      if (flightCount > 0) strncpy(keep, flights[selected].callsign, sizeof keep - 1);
      flightCount =
          jf::adsbToFlights(doc, cfg.lat, cfg.lon, cfg.radius_km, flights,
                              static_cast<size_t>(cfg.max_planes));
      lastFetchOk = lastGoodMs = millis();
      selected = 0;
      for (int i = 0; i < flightCount; ++i) {
        if (strcmp(flights[i].callsign, keep) == 0) selected = i;
      }
      ok = true;
      lastError[0] = '\0';
    } else {
      snprintf(lastError, sizeof lastError, "JSON: %s", err ? err.c_str() : "no aircraft list");
      Serial.println(lastError);
    }
  }
  http.end();
  return ok;
}

// The HTTPS body occasionally arrives truncated; one retry fixes it.
bool fetchFlights() {
  // mbedTLS needs 16 KB buffers in one piece: when the heap is too fragmented for that, skip the fetch
  // instead of failing half-way, and restart if it stays that way.
  if (ESP.getMaxAllocHeap() < 24000) {
    snprintf(lastError, sizeof lastError, "Low memory (%u B block)", static_cast<unsigned>(ESP.getMaxAllocHeap()));
    Serial.println(lastError);
    if (++lowBlockCount >= 5) restartNow("heap too fragmented");
    return false;
  }
  lowBlockCount = 0;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (fetchFlightsOnce()) return true;
    delay(400);
  }
  return false;
}

// ---------------------------------------------------------------- LED

// RGB LED on the back of the board (active low). Shows the airline's primary colour;
// colours are normalised to full brightness for their hue, then scaled by the `led` setting.
float ledBrightness() { return static_cast<float>(cfg.led_pct) / 100.0f; }  // 0..1, set led <percent>

// This LED's green is much stronger than its red, so yellow looks green/white unless the channels
// are balanced first.
constexpr float kGainR = 1.0f, kGainG = 0.45f, kGainB = 0.8f;

// Shows an RGB colour on the LED: corrected for the LED's unequal channels and scaled to the `led` brightness
// setting (the LED is active low, so the PWM values are inverted).
void setLed(int r, int g, int b) {
  const float fr = static_cast<float>(r) * kGainR, fg = static_cast<float>(g) * kGainG,
              fb = static_cast<float>(b) * kGainB;
  const float peak = fr > fg ? (fr > fb ? fr : fb) : (fg > fb ? fg : fb);
  const float k = peak > 0.0f ? 255.0f / peak * ledBrightness() : 0.0f;
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
  ledcWrite(board::kLedR, 255 - static_cast<int>(fr * k));
  ledcWrite(board::kLedG, 255 - static_cast<int>(fg * k));
  ledcWrite(board::kLedB, 255 - static_cast<int>(fb * k));
#else
  (void)fr; (void)fg; (void)fb; (void)k;  // no RGB LED on the AMOLED board
#endif
}

// Attaches the LED pins to PWM and flashes red, green, blue once as a boot test. The AMOLED board has no LED.
void setupLed() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  return;  // no RGB LED on the AMOLED board
#else
  for (int pin : {board::kLedR, board::kLedG, board::kLedB}) ledcAttach(pin, 5000, 8);
  setLed(0, 0, 0);
  const int test[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};  // boot test: R, G, B
  for (const auto& c : test) {
    setLed(c[0], c[1], c[2]);
    delay(250);
  }
  setLed(0, 0, 0);
#endif
}

// LED state, owned by a small background task so the colours keep animating while the main loop
// is busy with network requests: off, a steady colour, or a hold-and-fade between two colours.
enum LedMode { kLedOff = 0, kLedSteady = 1, kLedTwo = 2 };
volatile int ledMode = kLedOff;
volatile int ledA[3] = {0, 0, 0}, ledB[3] = {0, 0, 0};

// Picks the LED pattern for the plane on screen: the airline's colour (blending two colours when it has two),
// or off for an unknown operator, no plane, or night.
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

// Runs on its own task and animates the LED every 25 ms from ledMode, so slow network calls on the main loop never
// make it stutter.
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

// One place for the status screens: the colour plate, or a short text on the OLED.
void message(const char* msg, const char* detail, const char* left, const char* right, bool error) {
  if (isOled()) drawOledMessage(tft, msg, detail);
  else drawPlateMessage(tft, msg, detail, left, right, error, cfg.layout);
}

// Draws what the screen should show now: a message (no aircraft, fetch failed) or the plate of the selected plane.
// A new plane restarts its rotating data pages at distance.
void drawScreen() {
  if (flightCount > 0 && selected < flightCount && strcmp(pageCall, flights[selected].callsign) != 0) {
    snprintf(pageCall, sizeof pageCall, "%s", flights[selected].callsign);  // a new plane: start again at distance
    pageStart = millis();
  }
  shownPage = dataPage();
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
    message(fetchFailed ? "Fetch failed" : "No aircraft in range",
                     fetchFailed ? lastError : "", left, right, fetchFailed);
    return;
  }
  if (selected >= flightCount) selected = 0;
  ledForPlane(&flights[selected]);
  char route[24];
  const bool haveRoute = screenRoute(route, sizeof route);
  if (isOled()) {
    drawPlateOled(tft, flights[selected], haveRoute && shownPage == 1 ? route : nullptr);
    return;
  }
  drawPlate(tft, flights[selected], selected, flightCount, left, right,
            safeMode ? nullptr : jf::photoFor(flights[selected].reg), safeMode, cfg.layout, shownPage,
            haveRoute ? route : nullptr);
}

// Light sensor: the CYD has an LDR on GPIO 34; the AMOLED board has none (fixed daytime brightness).
int readLdr() {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  return 2000;
#else
  if (isOled() || isTDisplay() || isStick()) return 2000;  // no light sensor on these boards
  return analogRead(board::kLdr);
#endif
}

// Sets the backlight from the light sensor (dimmer in the dark), or low during night mode.
void updateBacklight() {
  lastBacklight = nightMode ? 70 : jf::backlightFor(readLdr());
  tft.setBrightness(lastBacklight);
}

// ---------------------------------------------------------------- serial console

// The `status` command: uptime, memory, WiFi, clock, night mode, last fetch and the reset reason.
void printStatus() {
  struct tm t;
  char when[32] = "not synced";
  if (timeValid() && getLocalTime(&t, 0)) strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &t);
  Serial.printf("version  %s\nuptime   %lu s\nheap     %u free, %u lowest\nwifi     %s (%s) rssi %d\n"
                "clock    %s\nnight    %s%s\nscreen   %s, backlight %d/255, light sensor %d/4095\n"
                "aircraft %d, last fetch %s\n",
                SKYWATCH_VERSION, static_cast<unsigned long>(millis() / 1000), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMinFreeHeap()),
                WiFi.status() == WL_CONNECTED ? "connected" : "DOWN",
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-",
                WiFi.RSSI(), when, cfg.night_on ? "enabled" : "disabled",
                nightMode ? " (active now)" : "", screenOff ? "OFF" : "on", lastBacklight,
                readLdr(), flightCount,
                lastFetchOk ? "ok" : (lastError[0] ? lastError : "none yet"));
  Serial.printf("reset    %s, %d crash(es) in a row%s\nheap     largest block %u\n", resetName(resetReason),
                crashCount, safeMode ? " - SAFE MODE (no photos, no 3D model)" : "",
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}

// Colour test card: tells the panel variants apart. Left to right: red, green, blue, white, black,
// yellow, cyan, magenta; below, a grey ramp.
void drawTestCard() {
  if (isOled()) {
    drawOledMessage(tft, "Display test", "128x64 OLED works");
    Serial.println("test card: an OLED is monochrome; if you see this text the display works.");
    return;
  }
  const int w = tft.width(), h = tft.height();
  const uint16_t cols[8] = {tft.color565(255, 0, 0),   tft.color565(0, 255, 0),   tft.color565(0, 0, 255),
                            tft.color565(255, 255, 255), tft.color565(0, 0, 0),    tft.color565(255, 255, 0),
                            tft.color565(0, 255, 255), tft.color565(255, 0, 255)};
  tft.startWrite();
  for (int i = 0; i < 8; ++i) tft.fillRect(i * w / 8, 0, w / 8 + 1, h / 2, cols[i]);
  for (int x = 0; x < w; ++x) {
    const int v = x * 255 / (w - 1);
    tft.drawFastVLine(x, h / 2, h / 2 - 28, tft.color565(v, v, v));
  }
  tft.fillRect(0, h - 28, w, 28, tft.color565(40, 40, 40));
  tft.setFont(&lgfx::fonts::Font2);
  tft.setTextColor(tft.color565(255, 255, 255), tft.color565(40, 40, 40));
  tft.setTextDatum(lgfx::middle_center);
  tft.drawString("R G B W K Y C M   (see serial for help)", w / 2, h - 14);
  tft.endWrite();
  Serial.println("test card: bars should read red, green, blue, white, black, yellow, cyan, magenta.\n"
                 "  red shows as blue      -> set rgb swap\n"
                 "  colours look negative  -> set invert on (or off)\n"
                 "  screen is upside down  -> set rotation 3\n"
                 "  garbage / white screen -> set panel st7789 (or ili9341)");
}

// The `flights` command: the planes found, nearest first.
void printFlights() {
  if (lastFetchOk == 0) {
    Serial.println("no data yet");
    return;
  }
  Serial.printf("%d aircraft within %d km of %.4f, %.4f (fetched %lu s ago)\n", flightCount,
                cfg.radius_km, cfg.lat, cfg.lon,
                static_cast<unsigned long>((millis() - lastFetchOk) / 1000));
  for (int i = 0; i < flightCount; ++i) {
    const jf::Flight& f = flights[i];
    const jf::Airline* al = jf::airlineFor(f.callsign);
    const jf::Route* rt = jf::routeFor(f.callsign);
    char alt[16], route[16] = "";
    jf::formatAltitude(f.alt_ft, alt, sizeof alt);
    if (rt != nullptr) snprintf(route, sizeof route, "%s>%s", rt->o_iata, rt->d_iata);
    Serial.printf("%c%d  %-8s %-5s %-8s %-9s %5d kt  trk %03d  %5.1f km %-2s  %s  %s\n",
                  i == selected ? '>' : ' ', i + 1, f.callsign, f.type[0] ? f.type : "-",
                  f.reg[0] ? f.reg : "-", alt, f.speed_kt, f.track, static_cast<double>(f.dist_km),
                  jf::compass(f.bearing), al != nullptr ? al->name : "?",
                  route);
  }
}

// ---------------------------------------------------------------- web page and MCP server (optional)

void onWebSaved() {  // the settings page on the board's address saved something
  saveConfig();
  jf::photoSetContact(cfg.contact);
  netRestart = true;
}

// A copy of what the board knows for the web page and the MCP server, taken per request (they never read the
// globals directly).
jf::McpContext webSnapshot() {
  jf::McpContext c;
  c.flights = flights;
  c.count = flightCount;
  c.selected = selected;
  c.version = SKYWATCH_VERSION;
  c.uptime_s = millis() / 1000;
  c.free_heap = ESP.getFreeHeap();
  c.wifi = WiFi.status() == WL_CONNECTED;
  c.rssi = WiFi.RSSI();
  c.last_fetch_age_s = lastFetchOk != 0 ? static_cast<int>((millis() - lastFetchOk) / 1000) : -1;
  c.radius_km = cfg.radius_km;
  c.max_planes = cfg.max_planes;
  c.panel = jf::panelName(cfg.panel);
  return c;
}

// Makes a new random access token (120 bits, no look-alike characters) and saves it.
void makeToken() {
  uint32_t words[4];
  for (uint32_t& w : words) w = esp_random();
  jf::randomToken(words, cfg.token);
  saveConfig();
}

// The web page and the MCP server both need a token: make one the first time either is switched on.
void ensureToken() {
  if ((cfg.web_on || cfg.mcp_on) && cfg.token[0] == '\0') {
    makeToken();
    Serial.printf("access token made: %s\n(keep it private; 'token' shows it again, 'token new' replaces it)\n", cfg.token);
  }
}

// The `version` command: firmware version, build time and chip.
void printVersion() {
  Serial.printf("skywatch-cyd %s (built %s %s)\nchip     %s rev %d, %d MHz, %u MB flash, panel %d\n", SKYWATCH_VERSION,
                __DATE__, __TIME__, ESP.getChipModel(), static_cast<int>(ESP.getChipRevision()),
                static_cast<int>(ESP.getCpuFreqMHz()), static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)),
                cfg.panel);
}

// Asks GitHub which release is the latest: /releases/latest redirects to /releases/tag/<tag>, so only the
// redirect's Location header is read (no page, no login, nothing about this board is sent).
void checkForUpdate() {
  printVersion();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("update check: not connected to WiFi");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();  // public information only
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.setUserAgent("skywatch-cyd");
  const char* keys[] = {"Location"};
  if (!http.begin(client, "https://github.com/joao-m-ferreira/skywatch-cyd/releases/latest")) {
    Serial.println("update check: could not start");
    return;
  }
  http.collectHeaders(keys, 1);
  const int code = http.GET();
  String loc = http.header("Location");
  http.end();
  const int at = loc.lastIndexOf("/tag/");
  if ((code != 301 && code != 302) || at < 0) {
    Serial.printf("update check failed (HTTP %d)\n", code);
    return;
  }
  const String tag = loc.substring(at + 5);
  if (jf::versionNewer(tag.c_str(), SKYWATCH_VERSION)) {
    Serial.printf("update available: %s (running %s)\nget it at https://github.com/joao-m-ferreira/skywatch-cyd/releases/latest\n"
                  "or flash it from https://joao-m-ferreira.github.io/skywatch-cyd/\n",
                  tag.c_str(), SKYWATCH_VERSION);
  } else if (strcmp(SKYWATCH_VERSION, "dev") == 0 || strcmp(SKYWATCH_VERSION, "emu") == 0) {
    Serial.printf("latest release is %s; this is a development build (no version to compare)\n", tag.c_str());
  } else {
    Serial.printf("up to date: running %s, latest release %s\n", SKYWATCH_VERSION, tag.c_str());
  }
}

// Runs one serial command line: the shared parser edits the settings and says what to do next; this applies it
// (save, restart, redraw, print...).
void handleLine(const char* line) {
  if (strcasecmp(line, "status") == 0) {
    printStatus();
    return;
  }
  if (strcasecmp(line, "flights") == 0) {
    printFlights();
    return;
  }
  if (strcasecmp(line, "display test") == 0) {
    drawTestCard();
    for (int i = 0; i < 150; ++i) {  // keep it on screen for 15 s
      esp_task_wdt_reset();
      delay(100);
    }
    forceRedraw = true;
    return;
  }
  if (strcasecmp(line, "safe on") == 0 || strcasecmp(line, "safe off") == 0) {
    const bool on = strcasecmp(line, "safe on") == 0;
    prefs.begin("skywatch", false);
    prefs.putBool("safe", on);
    prefs.putUChar("crashes", 0);
    prefs.end();
    Serial.printf("safe mode %s; restarting\n", on ? "forced on" : "off");
    restartNow("safe mode changed");
    return;
  }
  if (strcasecmp(line, "night test") == 0) {  // 20 s of night mode, never saved
    nightTestUntil = millis() + 20000;
    Serial.println("night test: screen and LED off for 20 s (touch the screen to wake it for the 'wake' time)");
    return;
  }
  static char reply[4096];  // the help text is the longest answer
  const jf::CmdAction action = jf::configCommand(&cfg, line, reply, sizeof reply);
  Serial.print(reply);
  switch (action) {
    case jf::CmdAction::Changed:
      saveConfig();
      jf::photoSetContact(cfg.contact);
      ensureToken();
      netRestart = true;  // pick the new settings up
      break;
    case jf::CmdAction::Redraw:  // layout changed: no restart needed
      saveConfig();
      forceRedraw = true;
      break;
    case jf::CmdAction::Display:  // panel variant changed: it only applies at start-up
      saveConfig();
      restartNow("display setting changed");
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
    case jf::CmdAction::Setup:
      enterSetupMode("requested over serial");
      break;
    case jf::CmdAction::Version:
      printVersion();
      break;
    case jf::CmdAction::Token:
      if (cfg.token[0] != '\0') Serial.printf("access token: %s\n", cfg.token);
      else Serial.println("no access token yet: it is made when 'set web on' or 'set mcp on' is used");
      break;
    case jf::CmdAction::NewToken:
      makeToken();
      Serial.printf("new access token: %s\n(clients using the old one stop working)\n", cfg.token);
      break;
    case jf::CmdAction::Update:
      checkForUpdate();
      break;
    case jf::CmdAction::None:
      break;
  }
}

// Collects serial input into lines. A line that does not fit the buffer is refused whole: running its first part
// could store a cut-off value (a truncated password, for example).
void pollSerial() {
  static char buf[200];
  static size_t len = 0;
  static bool overflow = false;  // the current line is longer than the buffer
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (overflow) {  // never run the first part of a line that did not fit: a cut-off 'set pass' would store a wrong password
        Serial.println("error: line too long (at most 199 characters); nothing was changed");
        overflow = false;
        len = 0;
      } else if (len > 0) {
        buf[len] = '\0';
        len = 0;
        handleLine(buf);
      }
    } else if (len + 1 < sizeof buf) {
      buf[len++] = c;
    } else {
      overflow = true;
    }
  }
}

// ---------------------------------------------------------------- night mode

// True inside the night window (or during `night test`) once the clock is valid.
bool isNightNow() {
  if (millis() < nightTestUntil) return true;
  if (!cfg.night_on || !timeValid()) return false;
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  return jf::isNightHour(cfg, t.tm_hour);
}

// ---------------------------------------------------------------- WiFi setup mode

char apSsid[20] = "";
char apPass[12] = "";
bool setupSaved = false;        // the settings page (or a serial 'set') saved: try the new WiFi
uint32_t setupSavedAt = 0;
constexpr uint32_t kSetupAfterMs = 180000;  // WiFi unreachable for this long: open the setup page
constexpr const char* kSetupUrl = "192.168.4.1";

// Setup page callback: the settings were saved, so try the new WiFi.
void onPortalSaved() {
  saveConfig();
  jf::photoSetContact(cfg.contact);
  setupSaved = true;
}

// Shows the setup instructions (network name, password, address) on whatever kind of screen this is.
void drawSetup(const char* detail) {
  if (isOled()) drawSetupOled(tft, apSsid, apPass, kSetupUrl, detail);
  else drawSetupScreen(tft, apSsid, apPass, kSetupUrl, detail, cfg.layout);
}

// Opens the board's own WiFi (password shown on the screen) with a settings page; the saved WiFi keeps
// being tried in the background, and setup mode ends by itself once it connects.
void enterSetupMode(const char* why) {
  if (setupMode) {
    drawSetup("");
    return;
  }
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  jf::apName(mac, apSsid, sizeof apSsid);
  jf::apPassword(esp_random(), esp_random(), apPass);
  setupMode = true;
  netRestart = false;
  setupSaved = false;
  setupSavedAt = 0;
  flightCount = 0;
  lastFetchOk = 0;
  screenOff = false;
  ledForPlane(nullptr);
  jf::webStop();  // the setup page needs port 80
  jf::portalStart(&cfg, apSsid, apPass, onPortalSaved);
  if (jf::configWifiReady(cfg)) WiFi.begin(cfg.ssid, cfg.pass);
  Serial.printf("setup mode (%s): join WiFi '%s' with password '%s', then open http://%s/\n", why, apSsid, apPass,
                WiFi.softAPIP().toString().c_str());
  drawSetup("");
}

// Ends setup mode once the board is online: closes the access point and resumes normal operation.
void leaveSetupMode() {
  jf::portalStop();
  setupMode = false;
  Serial.println("setup mode: connected, the setup WiFi is closed");
  lastGoodMs = millis();
  startClock();
  geoDone = false;
  if (cfg.auto_loc) geoDone = geoLocate();
  forceRedraw = true;
  lastPoll = millis() - kPollMs;
}

// ---------------------------------------------------------------- robustness

// Watchdogs that run on every loop pass: WiFi down too long, no fetch for ten minutes, the daily restart, forgiving
// old crashes, low memory. `sleeping` is true while the screen is off at night.
void housekeeping(uint32_t now, bool sleeping) {
  esp_task_wdt_reset();
  now = millis();  // the caller's value can be stale by a whole fetch; timestamps below are taken with millis()

  // WiFi watchdog: if the link stays down, start over instead of waiting forever.
  static uint32_t downSince = 0;
  if (WiFi.status() != WL_CONNECTED && jf::configWifiReady(cfg)) {
    if (downSince == 0) downSince = now;
    if (jf::olderThan(now, downSince, 45000)) {
      Serial.println("wifi: down for 45 s, reconnecting");
      WiFi.disconnect();
      WiFi.begin(cfg.ssid, cfg.pass);
      downSince = now;
    }
  } else {
    downSince = 0;
  }

  // Nothing fetched for 10 minutes although WiFi is configured: restart.
  if (!sleeping && jf::configWifiReady(cfg) && now > kStuckRestartMs && jf::olderThan(now, lastGoodMs, kStuckRestartMs)) {
    restartNow("no successful fetch for 10 minutes");
  }
  // Daily restart, preferably while the screen is off at night.
  if (now > kDailyRestartMs && (sleeping || now > kDailyRestartMs + 2UL * 3600UL * 1000UL)) {
    restartNow("daily restart");
  }
  // Five stable minutes: forget earlier crashes (leaves safe mode on the next start).
  if (!stableMarked && now > 300000UL) {
    stableMarked = true;
    if (crashCount > 0) {
      prefs.begin("skywatch", false);
      prefs.putUChar("crashes", 0);
      prefs.end();
    }
  }
  // Heap safety net (three low readings in a row, checked every 5 s).
  static uint32_t lastHeapCheck = 0;
  static int low = 0;
  if (jf::olderThan(now, lastHeapCheck, 4999)) {
    lastHeapCheck = now;
    low = ESP.getFreeHeap() < kMinFreeHeap ? low + 1 : 0;
    if (low >= 3) restartNow("low memory");
  }
}
}  // namespace

// Arduino entry point: settings, display, LED and watchdog, then connect (or open setup mode when no WiFi is saved).
void setup() {
  new (tftStorage) LGFX();  // see above
  Serial.begin(115200);
  loadConfig();
  jf::photoSetContact(cfg.contact);
  // Crash accounting: three abnormal resets in a row start safe mode; five minutes of stable running
  // (or `safe off`) clear it.
  resetReason = esp_reset_reason();
  prefs.begin("skywatch", false);
  crashCount = prefs.getUChar("crashes", 0);
  if (abnormalReset(resetReason) && crashCount < 250) prefs.putUChar("crashes", ++crashCount);
  const bool forcedSafe = prefs.getBool("safe", false);
  prefs.end();
  safeMode = forcedSafe || jf::configSafeMode(crashCount);
  tft.useVariant(cfg.panel, cfg.invert, cfg.rgb_swap, cfg.bl_pin, cfg.touch_shared, cfg.i2c_sda, cfg.i2c_scl);
  if (hasButton()) pinMode(buttonPin(), buttonPin() >= 34 ? INPUT : INPUT_PULLUP);  // BOOT / A button
  tft.init();
  tft.setRotation(cfg.rotation);  // 320x240 landscape (3 = upside down)
  if (hasLed()) setupLed();  // the OLED boards use GPIOs 4/16/17 for I2C or nothing: leave them alone
  if (hasLed()) xTaskCreatePinnedToCore(ledTask, "led", 3072, nullptr, 1, nullptr, 0);  // animates the LED independently

  // Hardware watchdog for the main loop: restart if it hangs for 90 s.
  const esp_task_wdt_config_t wdt = {.timeout_ms = kWatchdogMs, .idle_core_mask = 0, .trigger_panic = true};
  if (esp_task_wdt_reconfigure(&wdt) != ESP_OK) esp_task_wdt_init(&wdt);
  esp_task_wdt_add(nullptr);

  Serial.printf("\nskywatch-cyd %s: type 'help' for settings\n", SKYWATCH_VERSION);
  Serial.printf("reset reason: %s, crashes in a row: %d%s\n", resetName(resetReason), crashCount,
                safeMode ? " - SAFE MODE" : "");
  if (jf::configWifiReady(cfg)) {
    message("Connecting...", cfg.ssid, "Flights nearby", "", false);
    if (connectWifi()) {
      startClock();
      if (cfg.auto_loc) geoDone = geoLocate();
    }
  } else {
    enterSetupMode("no WiFi saved yet");
  }
  lastPoll = millis() - kPollMs;
}

// Arduino main loop: serial, setup mode, web page / MCP, location and fetching, night mode, touch and button, the
// carousel, drawing and the watchdogs.
void loop() {
  uint32_t now = millis();
  pollSerial();

  if (setupMode) {  // own WiFi + settings page; the saved WiFi is retried in the background
    jf::portalLoop();
    if (netRestart) {  // a serial 'set' while in setup mode
      netRestart = false;
      setupSaved = true;
    }
    if (setupSaved) {
      setupSaved = false;
      setupSavedAt = now;
      WiFi.disconnect(false);
      if (jf::configWifiReady(cfg)) WiFi.begin(cfg.ssid, cfg.pass);
      message("Saved", cfg.ssid, "WiFi setup", "", false);
    }
    if (jf::configWifiReady(cfg) && WiFi.status() == WL_CONNECTED) {
      leaveSetupMode();
      return;
    }
    if (setupSavedAt != 0 && jf::olderThan(now, setupSavedAt, 25000)) {  // the new settings did not connect
      setupSavedAt = 0;
      char d[40];
      snprintf(d, sizeof d, "Could not join %.20s", cfg.ssid);
      drawSetup(d);
    }
    housekeeping(now, true);
    delay(5);
    return;
  }
  if (!jf::configWifiReady(cfg)) {  // nothing to connect to (first run, or after 'factory')
    enterSetupMode("no WiFi saved");
    return;
  }
  // The optional web page / MCP server on the board's address (off by default; needs the access token).
  if (cfg.web_on || cfg.mcp_on) {
    if (!jf::webActive() && WiFi.status() == WL_CONNECTED) {
      ensureToken();
      jf::webStart(&cfg, webSnapshot, onWebSaved);
    }
    jf::webLoop();
    if (jf::webWantsRestart()) restartNow("advertising turned off");
  } else if (jf::webActive()) {
    jf::webStop();
  }
  // WiFi unreachable for three minutes (router replaced, password changed...): open the setup page.
  static uint32_t offlineSince = 0;
  if (WiFi.status() == WL_CONNECTED) {
    offlineSince = 0;
  } else if (offlineSince == 0) {
    offlineSince = now;
  } else if (!nightMode && jf::olderThan(now, offlineSince, kSetupAfterMs)) {
    offlineSince = 0;
    enterSetupMode("WiFi unreachable for 3 minutes");
    return;
  }

  if (netRestart) {  // a setting changed
    netRestart = false;
    message("Applying settings...", cfg.ssid, "Flights nearby", "", false);
    // Drop the connection but keep the radio and its network interface: switching the radio off here (disconnect(true))
    // let a late packet from the old connection be freed through the destroyed interface, which crashed the board
    // (found by scripts/emu-web.sh). connectWifi() below joins again with the current settings.
    WiFi.disconnect(false);
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
  if (cfg.auto_loc && !geoDone && jf::olderThan(now, lastGeoTry, 600000) && WiFi.status() == WL_CONNECTED) {
    lastGeoTry = now;
    geoDone = geoLocate();
    if (geoDone) lastPoll = now - kPollMs;
  }

  // Night mode: screen and LED off inside the window; a touch wakes it for cfg.wake_min minutes.
  static uint32_t lastNightCheck = 0;
  if (jf::olderThan(now, lastNightCheck, 999) || lastNightCheck == 0) {
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
    if (tft.getTouch(&tx, &ty)) {  // touch: wake with fresh data
      wakeUntil = now + wakeMs();
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

  bool redraw = forceRedraw;
  forceRedraw = false;
  if (!redraw && flightCount > 0 && !safeMode && dataPage() != shownPage && now > 1000) {
    // compact screens: the next data field, repainted on its own (no flicker, no 3D model redraw)
    shownPage = dataPage();
    if (selected >= flightCount) selected = 0;
    if (isOled()) {
      drawScreen();  // the airline row and the route alternate: a cheap full redraw
    } else {
      char route[24];
      const bool haveRoute = screenRoute(route, sizeof route);
      drawPlateField(tft, flights[selected], shownPage, cfg.layout, haveRoute ? route : nullptr);
    }
  }
  if (jf::olderThan(now, lastPoll, kPollMs - 1)) {
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
    if (nightMode) wakeUntil = now + wakeMs();  // keep the screen on while it is being used
    delay(300);  // debounce
  }
  if (hasButton()) {  // BOOT button: a tap shows the next plane, holding it for 3 s opens the WiFi setup page
    static uint32_t downAt = 0;
    const bool down = digitalRead(buttonPin()) == LOW;
    if (down && downAt == 0) downAt = now;
    if (down && downAt != 0 && jf::olderThan(now, downAt, 2999)) {
      downAt = 0;
      enterSetupMode("button held");
      return;
    }
    if (!down && downAt != 0) {
      const uint32_t held = now - downAt;
      downAt = 0;
      if (held >= 40 && flightCount > 0) {
        selected = (selected + 1) % flightCount;
        lastRotate = now;
        redraw = true;
      }
    }
  }
  if (flightCount > 1 && jf::olderThan(now, lastRotate, planeMs() - 1)) {  // carousel
    selected = (selected + 1) % flightCount;
    lastRotate = now;
    redraw = true;
  }
  static uint32_t lastLdr = 0;
  if (jf::olderThan(now, lastLdr, 1000)) {
    lastLdr = now;
    updateBacklight();
  }
  if (redraw || now < 1000) {
    const uint32_t t0 = millis();
    drawScreen();
    Serial.printf("drawn in %u ms, free heap %u\n", static_cast<unsigned>(millis() - t0),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    // Photos: fetch the current plane's first (redraw when it arrives), then the next one's.
    if (flightCount > 0 && !safeMode && !isOled()) {
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
    if (flightCount > 0 && compactScreen() && !safeMode) {  // the prefetches took time: give the plane its full time again
      pageStart = lastRotate = millis();
      shownPage = 0;
      char route[24];
      const bool haveRoute = screenRoute(route, sizeof route);
      drawPlateField(tft, flights[selected], 0, cfg.layout, haveRoute ? route : nullptr);
    }
  }
  housekeeping(now, false);
  delay(20);
}
