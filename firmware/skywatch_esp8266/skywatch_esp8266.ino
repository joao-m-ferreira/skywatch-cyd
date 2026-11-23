// skywatch for the ESP8266: the nearby aircraft as text on a 0.96" SSD1306 OLED (128x64, I2C), on the usual
// ESP-12 boards with the OLED built in (SDA GPIO 14, SCL GPIO 12). The same data and the same settings commands as
// the ESP32 build (flight parsing, OLED text and the serial commands are the shared code in ../jf_flight_display,
// copied next to this sketch by scripts/stage_esp8266.sh before it is compiled); only the display, WiFi/TLS and storage are ESP8266 code. No 3D model, photos, touch, web page or MCP server: an
// ESP8266 has about 80 KB of RAM, and TLS takes a large share of it.
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>

#include "airline.h"
#include "config.h"
#include "flights.h"
#include "timing_util.h"
#include "version.h"
#include "version_util.h"

#ifndef SKYWATCH_I2C_SDA
#define SKYWATCH_I2C_SDA 14
#endif
#ifndef SKYWATCH_I2C_SCL
#define SKYWATCH_I2C_SCL 12
#endif

namespace {
constexpr uint32_t kPollMs = 20000;                      // data refresh
constexpr uint32_t kWifiRetryMs = 20000;                 // a connection attempt that has not worked by now is restarted
// TLS receive buffer. The servers used here (Cloudflare and others) do not accept small TLS records (MFLN), so the
// buffer must hold the largest record they send; 6 KB worked in testing for ~10 planes and 8 KB gives headroom.
// The rest of the handshake needs about 8 KB more: with less free than that, say so instead of crashing the TLS
// library (it writes through a null pointer when an allocation fails).
constexpr int kTlsRx = 8192, kTlsTx = 512;
constexpr uint32_t kTlsOverhead = 8000;
constexpr uint32_t kStuckRestartMs = 10UL * 60UL * 1000UL;  // no good fetch for this long: restart

// Bit-banged I2C: works on any two pins (the hardware I2C pins of an ESP8266 are not fixed anyway).
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /*clock=*/SKYWATCH_I2C_SCL, /*data=*/SKYWATCH_I2C_SDA, U8X8_PIN_NONE);

jf::Config cfg;
jf::Flight flights[jf::kMaxFlights];
int flightCount = 0;
int selected = 0;
uint32_t lastFetchMs = 0, lastGoodMs = 0, lastRotateMs = 0, lastDrawMs = 0, wifiStartMs = 0, wakeUntilMs = 0;
bool fetchedOnce = false, fetchFailed = false, geoDone = false, clockStarted = false, screenOff = false, netChanged = true;
char lastError[48] = "";

// ---------------------------------------------------------------- settings in flash (EEPROM emulation)
struct Stored {
  uint32_t magic;  // changes with the Config layout: after an update that changes it the settings are reset
  jf::Config cfg;
};
constexpr uint32_t kMagic = 0x534B5700u ^ static_cast<uint32_t>(sizeof(jf::Config));

void defaults() {
  jf::configDefaults(&cfg, "", "", 38.7223, -9.1393, 40, "you@example.com");
  cfg.panel = 6;
  cfg.web_on = cfg.mcp_on = cfg.adv_on = cfg.mcp_auth = false;  // no web page or MCP server on this board
  cfg.i2c_sda = SKYWATCH_I2C_SDA;
  cfg.i2c_scl = SKYWATCH_I2C_SCL;
}

void loadConfig() {
  defaults();
  EEPROM.begin(sizeof(Stored));
  Stored s;
  EEPROM.get(0, s);
  if (s.magic == kMagic) cfg = s.cfg;
  cfg.web_on = cfg.mcp_on = cfg.adv_on = cfg.mcp_auth = false;
  cfg.ssid[sizeof cfg.ssid - 1] = '\0';  // never trust flash content blindly
  cfg.pass[sizeof cfg.pass - 1] = '\0';
  cfg.contact[sizeof cfg.contact - 1] = '\0';
  cfg.tz[sizeof cfg.tz - 1] = '\0';
  if (cfg.max_planes < 1 || cfg.max_planes > static_cast<int>(jf::kMaxFlights)) cfg.max_planes = 10;
  if (cfg.plane_sec < 3 || cfg.plane_sec > 120) cfg.plane_sec = 6;
}

void saveConfig() {
  Stored s;
  s.magic = kMagic;
  s.cfg = cfg;
  EEPROM.put(0, s);
  EEPROM.commit();
}

// ---------------------------------------------------------------- screen
void message(const char* l1, const char* l2 = "", const char* l3 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.drawStr(0, 15, l1);
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawStr(0, 36, l2);
  u8g2.drawStr(0, 52, l3);
  u8g2.sendBuffer();
}

void drawPlane() {
  const jf::Flight& f = flights[selected];
  const jf::Airline* al = jf::airlineFor(f.callsign);
  jf::OledText t;
  jf::formatOledText(f, al != nullptr ? al->name : nullptr, jf::looksLikeAirlineCallsign(f.callsign), &t);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.drawStr(0, 14, t.call);
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawStr(128 - u8g2.getStrWidth(t.type), 13, t.type);
  u8g2.drawStr(0, 30, t.airline);
  u8g2.drawStr(0, 46, t.alt_speed);
  u8g2.drawStr(0, 62, t.dist_dir);
  u8g2.sendBuffer();
}

void drawScreen() {
  if (!jf::configWifiReady(cfg)) {
    message("WiFi not set up", "USB serial 115200:", "type 'help'");
  } else if (WiFi.status() != WL_CONNECTED) {
    message("Connecting...", cfg.ssid);
  } else if (flightCount == 0) {
    message(fetchFailed ? "Fetch failed" : "No aircraft", fetchFailed ? lastError : "in range");
  } else {
    if (selected >= flightCount) selected = 0;
    drawPlane();
  }
  lastDrawMs = millis();
}

// ---------------------------------------------------------------- network
bool timeValid() { return time(nullptr) > 1700000000; }

void startClock() {
  configTime(cfg.tz, "pool.ntp.org", "time.cloudflare.com");
  clockStarted = true;
}

// A Stream that waits for the network instead of reporting "no data" at the first gap: ArduinoJson treats -1 as the
// end of the document, and a TLS stream often has none ready for a moment. It ends when the server closes the
// connection (or after 8 s of silence).
class WaitingStream : public Stream {
 public:
  explicit WaitingStream(WiFiClient* c) : c_(c) {}
  int available() override { return c_->available(); }
  int peek() override { return c_->peek(); }
  size_t write(uint8_t) override { return 0; }
  int read() override {
    const uint32_t t0 = millis();
    while (c_->available() == 0) {
      if (!c_->connected() || millis() - t0 > 8000) return -1;
      delay(1);
      ESP.wdtFeed();
    }
    return c_->read();
  }

 private:
  WiFiClient* c_;
};

// GET `url` over HTTPS and parse the JSON straight off the stream through `filter` into `doc`. The body of the
// aircraft list is about 1 KB per plane and has no Content-Length; collecting it in a String needs a block that
// this heap cannot always give (growth failed silently and dropped chunks), while the filtered document is small.
bool httpsJson(const char* host, const char* url, JsonDocument& filter, JsonDocument& doc) {
  BearSSL::WiFiClientSecure client;
  client.setInsecure();  // public read-only data, nothing private is sent
  // TLS wants a 16 KB receive buffer unless the server agrees to small records (MFLN). If it does not, and there is
  // no such block free, refuse: the TLS library dereferences a null pointer when its allocation fails.
  if (client.probeMaxFragmentLength(host, 443, 1024)) {
    client.setBufferSizes(1024, 1024);  // a server that allows small records: nearly free
  } else if (ESP.getFreeHeap() < kTlsRx + kTlsTx + kTlsOverhead || ESP.getMaxFreeBlockSize() < kTlsRx + 2000) {
    snprintf(lastError, sizeof lastError, "Not enough memory for TLS");
    return false;
  } else {
    client.setBufferSizes(kTlsRx, kTlsTx);
  }
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);  // no chunked encoding: the stream is plain JSON
  if (!http.begin(client, url)) {
    snprintf(lastError, sizeof lastError, "HTTP begin failed");
    return false;
  }
  const int code = http.GET();
  bool ok = false;
  if (code == HTTP_CODE_OK) {
    WaitingStream stream(http.getStreamPtr());
    const DeserializationError err = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
    ok = !err;
    if (err) snprintf(lastError, sizeof lastError, "JSON: %s", err.c_str());
  } else {
    snprintf(lastError, sizeof lastError, "HTTP %d", code);
  }
  http.end();
  return ok;
}

bool fetchOnce() {
  char url[96];
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v2/lat/%.2f/lon/%.2f/dist/%d", cfg.lat, cfg.lon,
           static_cast<int>(cfg.radius_km / 1.852) + 1);
  JsonDocument filter, doc;
  jf::adsbFilter(filter);
  if (!httpsJson("opendata.adsb.fi", url, filter, doc)) return false;
  if (!jf::adsbValid(doc)) {
    snprintf(lastError, sizeof lastError, "JSON: no aircraft list");
    return false;
  }
  char keep[sizeof flights[0].callsign] = "";
  if (flightCount > 0) strncpy(keep, flights[selected].callsign, sizeof keep - 1);
  flightCount = jf::adsbToFlights(doc, cfg.lat, cfg.lon, cfg.radius_km, flights, static_cast<size_t>(cfg.max_planes));
  selected = 0;
  for (int i = 0; i < flightCount; ++i) {
    if (strcmp(flights[i].callsign, keep) == 0) selected = i;  // keep showing the same plane while it is in range
  }
  lastGoodMs = millis();
  fetchedOnce = true;
  lastError[0] = '\0';
  return true;
}

bool fetchFlights() {
  if (fetchOnce()) return true;
  delay(500);
  return fetchOnce();  // a connection can drop mid-body: one retry fixes it
}

// Approximate position from the public IP (city level), only while the location is "auto".
bool geoLocate() {
  JsonDocument filter, doc;
  filter["latitude"] = true;
  filter["longitude"] = true;
  filter["city"] = true;
  filter["timezone"] = true;
  if (!httpsJson("get.geojs.io", "https://get.geojs.io/v1/ip/geo.json", filter, doc)) return false;
  char* e1 = nullptr;
  char* e2 = nullptr;
  const char* sla = doc["latitude"] | "";
  const char* slo = doc["longitude"] | "";
  const double la = strtod(sla, &e1), lo = strtod(slo, &e2);
  if (e1 == sla || e2 == slo || la < -90 || la > 90 || lo < -180 || lo > 180) return false;
  cfg.lat = la;
  cfg.lon = lo;
  const char* tz = jf::posixTzFor(doc["timezone"] | "");
  if (tz != nullptr && !cfg.tz_set) {
    strncpy(cfg.tz, tz, sizeof cfg.tz - 1);
    startClock();
  }
  saveConfig();
  Serial.printf("location from IP (approximate): %s %.4f, %.4f\n", static_cast<const char*>(doc["city"] | "?"), la, lo);
  return true;
}

void startWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.ssid, cfg.pass);
  wifiStartMs = millis();
}

// ---------------------------------------------------------------- serial console (the same commands as the ESP32 build)
void printStatus() {
  char when[32] = "not synced";
  if (timeValid()) {
    const time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &tmv);
  }
  Serial.printf("version  %s\nuptime   %lu s\nheap     %u free, largest block %u\nwifi     %s (%s) rssi %d\nclock    %s\n"
                "aircraft %d, last fetch %s\n",
                SKYWATCH_VERSION, static_cast<unsigned long>(millis() / 1000), static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getMaxFreeBlockSize()), WiFi.status() == WL_CONNECTED ? "connected" : "DOWN",
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-", WiFi.RSSI(), when, flightCount,
                fetchedOnce ? "ok" : (lastError[0] ? lastError : "none yet"));
}

void printVersion() {
  Serial.printf("skywatch-cyd %s (ESP8266 OLED build, built %s %s)\nchip     ESP8266 %u MHz, %u MB flash, SDA %d SCL %d\n",
                SKYWATCH_VERSION, __DATE__, __TIME__, static_cast<unsigned>(ESP.getCpuFreqMHz()),
                static_cast<unsigned>(ESP.getFlashChipRealSize() / (1024 * 1024)), cfg.i2c_sda, cfg.i2c_scl);
}

// GitHub redirects /releases/latest to /releases/tag/<tag>: only that Location header is read.
void checkForUpdate() {
  printVersion();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("update check: not connected to WiFi");
    return;
  }
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 1024);  // the redirect is tiny; if the server insists on larger records this fails safely
  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* keys[] = {"Location"};
  if (!http.begin(client, "https://github.com/joao-m-ferreira/skywatch-cyd/releases/latest")) return;
  http.collectHeaders(keys, 1);
  const int code = http.GET();
  const String loc = http.header("Location");
  http.end();
  const int at = loc.lastIndexOf("/tag/");
  if ((code != 301 && code != 302) || at < 0) {
    Serial.printf("update check failed (HTTP %d)\n", code);
    return;
  }
  const String tag = loc.substring(at + 5);
  if (jf::versionNewer(tag.c_str(), SKYWATCH_VERSION)) Serial.printf("update available: %s (running %s)\nhttps://github.com/joao-m-ferreira/skywatch-cyd/releases/latest\n", tag.c_str(), SKYWATCH_VERSION);
  else if (strcmp(SKYWATCH_VERSION, "dev") == 0) Serial.printf("latest release is %s; this is a development build\n", tag.c_str());
  else Serial.printf("up to date: running %s, latest release %s\n", SKYWATCH_VERSION, tag.c_str());
}

void printFlights() {
  if (!fetchedOnce) {
    Serial.println("no data yet");
    return;
  }
  Serial.printf("%d aircraft within %d km (fetched %lu s ago)\n", flightCount, cfg.radius_km, static_cast<unsigned long>((millis() - lastGoodMs) / 1000));
  for (int i = 0; i < flightCount; ++i) {
    const jf::Flight& f = flights[i];
    Serial.printf("  %-8s %-5s %-9s %6d ft %4d kt trk %03d  %5.1f km %s\n", f.callsign, f.type[0] ? f.type : "-", f.reg[0] ? f.reg : "-",
                  f.alt_ft, f.speed_kt, f.track, f.dist_km, jf::compass(f.bearing));
  }
}

void handleLine(const char* line) {
  if (strcasecmp(line, "status") == 0) {
    printStatus();
    return;
  }
  if (strcasecmp(line, "flights") == 0) {
    printFlights();
    return;
  }
  char* reply = static_cast<char*>(malloc(4096));  // the help text is the longest answer; only held while it is needed
  if (reply == nullptr) {
    Serial.println("out of memory");
    return;
  }
  const jf::CmdAction action = jf::configCommand(&cfg, line, reply, 4096);
  Serial.print(reply);
  free(reply);
  if (cfg.web_on || cfg.mcp_on || cfg.adv_on || cfg.mcp_auth) {  // accepted by the shared parser, but there is nothing to switch on
    cfg.web_on = cfg.mcp_on = cfg.adv_on = cfg.mcp_auth = false;
    Serial.println("note: the web page, MCP server and network announcement are not available on the ESP8266 build (they stay off)");
  }
  switch (action) {
    case jf::CmdAction::Changed:
      saveConfig();
      netChanged = true;  // pick the new WiFi / location / radius up
      break;
    case jf::CmdAction::Redraw:
      saveConfig();
      lastDrawMs = 0;
      break;
    case jf::CmdAction::Display:  // pins or panel: only applies at start-up
      saveConfig();
      Serial.println("restarting to apply it...");
      Serial.flush();
      delay(200);
      ESP.restart();
      break;
    case jf::CmdAction::Factory:
      defaults();
      saveConfig();
      netChanged = true;
      break;
    case jf::CmdAction::Reboot:
      Serial.flush();
      delay(200);
      ESP.restart();
      break;
    case jf::CmdAction::Version:
      printVersion();
      break;
    case jf::CmdAction::Update:
      checkForUpdate();
      break;
    case jf::CmdAction::Setup:
    case jf::CmdAction::Token:
    case jf::CmdAction::NewToken:
      Serial.println("not available on the ESP8266 build (no setup page, web page or MCP server); use the serial commands");
      break;
    case jf::CmdAction::None:
      break;
  }
}

void pollSerial() {
  static char buf[200];
  static size_t len = 0;
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len > 0) {
        buf[len] = '\0';
        len = 0;
        handleLine(buf);
      }
    } else if (len < sizeof buf - 1) {
      buf[len++] = static_cast<char>(c);
    }
  }
}

bool isNightNow() {
  if (!cfg.night_on || !timeValid()) return false;
  const time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return jf::isNightHour(cfg, tmv.tm_hour);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  loadConfig();
  u8g2.begin();
  u8g2.setContrast(160);
  pinMode(0, INPUT_PULLUP);  // the FLASH button: next plane, or wake the screen at night
  Serial.printf("\nskywatch-cyd %s (ESP8266 OLED): type 'help' for settings\n", SKYWATCH_VERSION);
  message("skywatch", "starting...");
  if (jf::configWifiReady(cfg)) startWifi();
  lastGoodMs = millis();
  drawScreen();
}

void loop() {
  const uint32_t now = millis();
  pollSerial();

  if (netChanged) {  // first pass, or a setting changed: (re)connect and refetch
    netChanged = false;
    flightCount = 0;
    lastFetchMs = 0;
    geoDone = false;
    lastGoodMs = now;
    if (jf::configWifiReady(cfg)) startWifi();
    else WiFi.disconnect();
    drawScreen();
  }

  const bool online = WiFi.status() == WL_CONNECTED;
  if (!online && jf::configWifiReady(cfg) && jf::olderThan(now, wifiStartMs, kWifiRetryMs)) {
    startWifi();  // a stuck attempt: start over
  }
  if (online && !clockStarted) startClock();
  if (online && cfg.auto_loc && !geoDone) {
    geoDone = true;  // one try per connection; the last known position is kept on failure
    if (flightCount == 0) message("Locating...", "from your IP address");
    if (geoLocate()) lastFetchMs = 0;
  }

  // Night mode: the screen is off in the window; the FLASH button wakes it for 'wake' minutes.
  const bool button = digitalRead(0) == LOW;
  const bool night = isNightNow();
  if (night && !(wakeUntilMs != 0 && jf::elapsedMs(wakeUntilMs, now) > 0)) {
    if (button) {
      wakeUntilMs = now + static_cast<uint32_t>(cfg.wake_min) * 60000UL;
      delay(300);
    } else if (!screenOff) {
      u8g2.setPowerSave(1);
      screenOff = true;
    }
    if (screenOff) {
      delay(50);
      return;
    }
  }
  if (screenOff) {
    u8g2.setPowerSave(0);
    screenOff = false;
    lastDrawMs = 0;
  }

  bool redraw = false;
  if (online && (lastFetchMs == 0 || jf::olderThan(now, lastFetchMs, kPollMs))) {
    lastFetchMs = now;
    if (flightCount == 0) message("Fetching...", "nearby aircraft");  // the plane on screen stays while it refreshes
    fetchFailed = !fetchFlights();
    if (fetchFailed) Serial.printf("fetch failed (%s); keeping last data\n", lastError);
    else Serial.printf("fetched %d aircraft\n", flightCount);
    redraw = true;
  }
  static bool wasDown = false;
  if (online == wasDown) {  // the link came up or went down: show it
    wasDown = !online;
    redraw = true;
  }

  static uint32_t pressedAt = 0;  // a tap on the FLASH button shows the next plane
  if (button && pressedAt == 0) pressedAt = now;
  if (!button && pressedAt != 0) {
    if (jf::elapsedMs(now, pressedAt) >= 40 && flightCount > 1) {
      selected = (selected + 1) % flightCount;
      lastRotateMs = now;
      redraw = true;
    }
    pressedAt = 0;
  }
  if (flightCount > 1 && jf::olderThan(now, lastRotateMs, static_cast<uint32_t>(cfg.plane_sec) * 1000UL)) {
    selected = (selected + 1) % flightCount;
    lastRotateMs = now;
    redraw = true;
  }
  if (redraw || lastDrawMs == 0) drawScreen();

  // Nothing fetched for 10 minutes although WiFi is configured: restart.
  if (jf::configWifiReady(cfg) && jf::olderThan(millis(), lastGoodMs, kStuckRestartMs)) {
    Serial.println("restarting: no successful fetch for 10 minutes");
    Serial.flush();
    delay(200);
    ESP.restart();
  }
  delay(20);
}
