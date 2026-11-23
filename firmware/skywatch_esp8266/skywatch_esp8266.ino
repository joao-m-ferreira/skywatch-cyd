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
#include "crc_util.h"
#include "flights.h"
#include "net_compat.h"
#include "portal_util.h"
#include "timing_util.h"
#include "version.h"
#include "version_util.h"
#include "webapi.h"

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
// buffer must hold the largest record they send; 6 KB worked in testing for ~10 planes (8 KB leaves too little next
// to the web server). The rest of the handshake needs about 7 KB more: with less free than that, say so instead of crashing the TLS
// library (it writes through a null pointer when an allocation fails).
constexpr int kTlsRx = 6144, kTlsTx = 512;
constexpr uint32_t kTlsOverhead = 7000;
constexpr uint32_t kStuckRestartMs = 10UL * 60UL * 1000UL;  // no good fetch for this long: restart

// Bit-banged I2C: works on any two pins (the hardware I2C pins of an ESP8266 are not fixed anyway).
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /*clock=*/SKYWATCH_I2C_SCL, /*data=*/SKYWATCH_I2C_SDA, U8X8_PIN_NONE);

jf::Config cfg;
jf::Flight flights[jf::kMaxFlights];
int flightCount = 0;
int selected = 0;
// Routes found so far (adsbdb.com, by callsign): a few slots, replaced round-robin. `text` is "LIS > YYZ", empty when the
// service did not know the flight (kept so it is not asked again).
struct RouteSlot {
  char callsign[12];
  char text[12];
};
constexpr size_t kRouteSlots = 8;
RouteSlot routes[kRouteSlots];
size_t nextRouteSlot = 0;
char pageCall[12] = "";  // the plane the airline/route alternation is timed for
uint32_t pageStartMs = 0;
bool shownRoutePage = false;
uint32_t lastFetchMs = 0, lastGoodMs = 0, lastRotateMs = 0, lastDrawMs = 0, wifiStartMs = 0, wakeUntilMs = 0;
bool fetchedOnce = false, fetchFailed = false, geoDone = false, clockStarted = false, screenOff = false, netChanged = true;
char lastError[48] = "";
bool memoryShort = false;  // the last fetch was refused for lack of memory (recovery below)
uint8_t memoryFailures = 0;  // in a row; a board stuck short of memory restarts to defragment the heap
uint32_t webHoldUntilMs = 0;  // the web server stays off until then so a fetch can have the heap

// ---------------------------------------------------------------- settings in flash (EEPROM emulation)
struct Stored {
  uint32_t magic;  // changes with the Config layout: after an update that changes it the settings are reset
  jf::Config cfg;
  uint32_t crc;    // over magic and cfg: a flash sector that decayed or was half-written is detected, not used
};
constexpr uint32_t kMagic = 0x534B5701u ^ static_cast<uint32_t>(sizeof(jf::Config));  // ...01: layout with a checksum

// The factory settings of this build (no web page, MCP server or announcement: they do not exist on this chip).
void defaults() {
  jf::configDefaults(&cfg, "", "", 38.7223, -9.1393, 40, "you@example.com");
  cfg.panel = 6;
  // The web page, MCP server and mDNS announcement are opt-in here: with all three on a fetch has only ~14 KB of
  // contiguous heap (the mDNS responder alone takes ~4.7 KB), which is too little whenever memory is fragmented.
  cfg.web_on = cfg.mcp_on = cfg.adv_on = false;
  cfg.i2c_sda = SKYWATCH_I2C_SDA;
  cfg.i2c_scl = SKYWATCH_I2C_SCL;
}

// Loads the settings from the flash sector the EEPROM library keeps: used only when the layout marker and the
// checksum both match, then sanity-checked. Anything else (first start, an update that changed the layout, a
// damaged sector) means the defaults.
void loadConfig() {
  defaults();
  EEPROM.begin(sizeof(Stored));
  Stored s;
  EEPROM.get(0, s);
  if (s.magic == kMagic && s.crc == jf::crc32(&s, offsetof(Stored, crc))) {
    cfg = s.cfg;
  } else if (s.magic == kMagic) {
    Serial.println("warning: the stored settings are corrupt (checksum); using defaults");
  }  // else: first start, or an update that changed the layout: defaults
  const int fixed = jf::configSanitize(&cfg);
  if (fixed > 0) Serial.printf("warning: %d stored setting(s) were out of range and were reset to defaults\n", fixed);
}

// Writes the settings with a checksum; the whole struct is cleared first so padding never differs between writes.
void saveConfig() {
  Stored s;
  memset(&s, 0, sizeof s);  // no uninitialised padding in the checksum
  s.magic = kMagic;
  s.cfg = cfg;
  s.crc = jf::crc32(&s, offsetof(Stored, crc));
  EEPROM.put(0, s);
  if (!EEPROM.commit()) Serial.println("error: could not write the settings to flash");
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

// The plane on screen as four rows of text: call sign and type, airline, altitude and speed, distance and bearing.
void drawPlane(const char* route) {
  const jf::Flight& f = flights[selected];
  const jf::Airline* al = jf::airlineFor(f.callsign);
  jf::OledText t;
  jf::formatOledText(f, al != nullptr ? al->name : nullptr, jf::looksLikeAirlineCallsign(f.callsign), &t);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.drawStr(0, 14, t.call);
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.drawStr(128 - u8g2.getStrWidth(t.type), 13, t.type);
  u8g2.drawStr(0, 30, route != nullptr ? route : t.airline);  // the route replaces the airline row on alternate pages
  u8g2.drawStr(0, 46, t.alt_speed);
  u8g2.drawStr(0, 62, t.dist_dir);
  u8g2.sendBuffer();
}

// Draws a status message (not set up, connecting, fetching, no aircraft, fetch failed) or the selected plane.
// The route text of a callsign, or nullptr when unknown.
const char* routeText(const char* callsign) {
  for (const RouteSlot& r : routes) {
    if (r.text[0] != '\0' && strcmp(r.callsign, callsign) == 0) return r.text;
  }
  return nullptr;
}

// True when the callsign has been looked up before (whether or not the service knew it).
bool routeTried(const char* callsign) {
  for (const RouteSlot& r : routes) {
    if (r.callsign[0] != '\0' && strcmp(r.callsign, callsign) == 0) return true;
  }
  return false;
}

// Whether the row that alternates between the airline and the route should show the route now: the time per plane is
// shared between the two, each for at least a second, and the alternation restarts with every new plane.
bool routePageNow() {
  if (flightCount == 0 || selected >= flightCount) return false;
  const char* cs = flights[selected].callsign;
  if (strcmp(pageCall, cs) != 0) {
    snprintf(pageCall, sizeof pageCall, "%s", cs);
    pageStartMs = millis();
  }
  if (routeText(cs) == nullptr) return false;
  uint32_t half = static_cast<uint32_t>(cfg.plane_sec) * 500UL;
  if (half < 1000) half = 1000;
  return ((millis() - pageStartMs) / half) % 2 == 1;
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
    drawPlane(routePageNow() ? routeText(flights[selected].callsign) : nullptr);
    shownRoutePage = routePageNow();
  }
  lastDrawMs = millis();
}

// ---------------------------------------------------------------- network
bool timeValid() { return time(nullptr) > 1700000000; }

// Starts the NTP clock in the configured timezone.
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
    if (started_ == 0) started_ = millis() | 1u;  // never 0, which means "not started"
    // One document ends whatever the server does: after kMaxBytes or kMaxMs the stream reports its end and the parser
    // fails cleanly (the fetch is retried later).
    if (total_ >= kMaxBytes || jf::olderThan(millis(), started_, kMaxMs)) return -1;
    const uint32_t t0 = millis();
    while (c_->available() == 0) {
      if (!c_->connected() || jf::olderThan(millis(), t0, 8000)) return -1;
      delay(1);
      ESP.wdtFeed();
    }
    ++total_;
    return c_->read();
  }

 private:
  static constexpr size_t kMaxBytes = 400000;  // far above any aircraft list (about 1 KB per plane)
  static constexpr uint32_t kMaxMs = 30000;    // for the whole document
  WiFiClient* c_;
  size_t total_ = 0;
  uint32_t started_ = 0;
};

// GET `url` over HTTPS and parse the JSON straight off the stream through `filter` into `doc`. The body of the
// aircraft list is about 1 KB per plane and has no Content-Length; collecting it in a String needs a block that
// this heap cannot always give (growth failed silently and dropped chunks), while the filtered document is small.
bool httpsJson(const char* host, const char* url, JsonDocument& filter, JsonDocument& doc) {
  // With the web server and mDNS up, memory can be short: close them for this fetch (nothing is served while it runs
  // anyway) and let the main loop start them again afterwards.
  if (jf::webActive() && (ESP.getFreeHeap() < kTlsRx + kTlsTx + kTlsOverhead + 3000 || ESP.getMaxFreeBlockSize() < kTlsRx + 5000)) {
    jf::webStop();
  }
  BearSSL::WiFiClientSecure client;
  client.setInsecure();  // public read-only data, nothing private is sent
  // TLS wants a 16 KB receive buffer unless the server agrees to small records (MFLN). If it does not, and there is
  // no such block free, refuse: the TLS library dereferences a null pointer when its allocation fails.
  if (client.probeMaxFragmentLength(host, 443, 1024)) {
    client.setBufferSizes(1024, 1024);  // a server that allows small records: nearly free
  } else if (ESP.getFreeHeap() < kTlsRx + kTlsTx + kTlsOverhead || ESP.getMaxFreeBlockSize() < kTlsRx + 2000) {
    snprintf(lastError, sizeof lastError, "Not enough memory for TLS");
    memoryShort = true;
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

// One attempt at the aircraft list: parsed straight off the connection, keeping the same plane on screen across
// refreshes while it stays in range.
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

// The aircraft list, with one retry (a connection can drop mid-body).
bool fetchFlights() {
  memoryShort = false;
  if (fetchOnce()) {
    memoryFailures = 0;
    return true;
  }
  if (memoryShort) {
    // Not a network problem: free what can be freed (the web server and its buffers) and try again at once.
    if (jf::webActive()) jf::webStop();
    webHoldUntilMs = millis() + 60000;  // keep it off for a minute so the next fetches are not starved
    yield();
    delay(200);
    memoryShort = false;
  } else {
    delay(500);  // a connection can drop mid-body: one retry fixes it
  }
  if (fetchOnce()) {
    memoryFailures = 0;
    return true;
  }
  if (memoryShort && ++memoryFailures >= 4) {
    Serial.println("memory stayed short for 4 fetches; restarting to defragment the heap");
    delay(100);
    ESP.restart();
  }
  return false;
}

// Looks up where a flight is going (adsbdb.com, by callsign) and remembers the answer, also when there is none. Only the
// airport codes are kept: they are what fits a 128-pixel row ("LIS > YYZ"). Blocking, about 1-3 s.
bool fetchRoute(const char* callsign) {
  for (const char* c = callsign; *c != '\0'; ++c) {
    if (!isalnum(static_cast<unsigned char>(*c))) return false;  // the callsign goes into a URL: letters and digits only
  }
  RouteSlot* r = &routes[nextRouteSlot];
  nextRouteSlot = (nextRouteSlot + 1) % kRouteSlots;
  memset(r, 0, sizeof *r);
  snprintf(r->callsign, sizeof r->callsign, "%s", callsign);
  char url[64];
  snprintf(url, sizeof url, "https://api.adsbdb.com/v0/callsign/%s", callsign);
  JsonDocument filter, doc;
  filter["response"]["flightroute"]["origin"]["iata_code"] = true;
  filter["response"]["flightroute"]["destination"]["iata_code"] = true;
  if (!httpsJson("api.adsbdb.com", url, filter, doc)) return false;
  JsonObjectConst fr = doc["response"]["flightroute"];  // an unknown callsign answers with a plain string instead
  const char* o = fr["origin"]["iata_code"] | "";
  const char* d = fr["destination"]["iata_code"] | "";
  if (o[0] != '\0' || d[0] != '\0') snprintf(r->text, sizeof r->text, "%.3s > %.3s", o[0] != '\0' ? o : "?", d[0] != '\0' ? d : "?");
  Serial.printf("route %s: %s\n", callsign, r->text[0] != '\0' ? r->text : "none");
  return r->text[0] != '\0';
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
    snprintf(cfg.tz, sizeof cfg.tz, "%s", tz);
    startClock();
  }
  saveConfig();
  Serial.printf("location from IP (approximate): %s %.4f, %.4f\n", static_cast<const char*>(doc["city"] | "?"), la, lo);
  return true;
}

// Starts (or restarts) joining the saved WiFi without waiting for the result.
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

// The `version` command.
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

// The `flights` command: the planes found, nearest first.
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

// Runs one serial command line: the shared parser edits the settings and says what to do next; this applies it.
// ---------------------------------------------------------------- web page, JSON API and MCP server (webapi.cpp)

// What the web page and the MCP server may see, copied per request (they never read the globals directly).
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
  c.last_fetch_age_s = fetchedOnce ? static_cast<int>((millis() - lastGoodMs) / 1000) : -1;
  c.radius_km = cfg.radius_km;
  c.max_planes = cfg.max_planes;
  c.panel = "ESP8266 OLED";
  return c;
}

// Makes a new random access token (120 bits, no look-alike characters) and saves it.
void makeToken() {
  uint32_t words[4];
  for (uint32_t& w : words) w = platformRandom();
  jf::randomToken(words, cfg.token);
  saveConfig();
}

// The web page and the MCP server both need a token: made the first time either is switched on.
void ensureToken() {
  if ((cfg.web_on || cfg.mcp_on) && cfg.token[0] == '\0') {
    makeToken();
    Serial.printf("access token made: %s\n(keep it private; 'token' shows it again, 'token new' replaces it)\n", cfg.token);
  }
}

// The settings page on the board's address saved something: apply it like a serial 'set'.
void onWebSaved() {
  saveConfig();
  netChanged = true;
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
  switch (action) {
    case jf::CmdAction::Changed:
      saveConfig();
      ensureToken();
      netChanged = true;  // pick the new WiFi / location / radius up
      break;
    case jf::CmdAction::Redraw:
      saveConfig();
      lastDrawMs = 0;
      jf::webLogAddress();  // prints the server's address when 'debug' has just been switched on
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
      Serial.println("the WiFi setup page is not available on the ESP8266 build; use the serial commands or the web setup at the project's page");
      break;
    case jf::CmdAction::Token:
      if (cfg.token[0] != '\0') Serial.printf("access token: %s\n", cfg.token);
      else Serial.println("no access token yet: it is made when the web page or MCP server is switched on");
      break;
    case jf::CmdAction::NewToken:
      makeToken();
      Serial.printf("new access token: %s\n(clients using the old one stop working)\n", cfg.token);
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
    const int c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (overflow) {  // never run the first part of a line that did not fit
        Serial.println("error: line too long (at most 199 characters); nothing was changed");
        overflow = false;
        len = 0;
      } else if (len > 0) {
        buf[len] = '\0';
        len = 0;
        handleLine(buf);
      }
    } else if (len < sizeof buf - 1) {
      buf[len++] = static_cast<char>(c);
    } else {
      overflow = true;
    }
  }
}

// True inside the night window once the clock is valid.
bool isNightNow() {
  if (!cfg.night_on || !timeValid()) return false;
  const time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return jf::isNightHour(cfg, tmv.tm_hour);
}
}  // namespace

// Arduino entry point: serial, settings, display, then start joining the WiFi.
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

// Arduino main loop: serial, (re)connecting, location, fetching, night mode, the FLASH button, the carousel, drawing
// and the stuck-fetch watchdog.
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
  // The optional web page / JSON API / MCP server on the board's address (webapi.cpp), announced over mDNS. Each
  // request is answered between the loop's other work; a fetch or route lookup (a few seconds) delays it.
  if (cfg.web_on || cfg.mcp_on) {
    if (!jf::webActive() && online && jf::olderThan(now, webHoldUntilMs, 0) && ESP.getFreeHeap() > 18000) {  // a fetch stops it again if it needs the room
      ensureToken();
      jf::webStart(&cfg, webSnapshot, onWebSaved);
    }
    jf::webLoop();
  } else if (jf::webActive()) {
    jf::webStop();
  }
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
    if (fetchFailed && memoryShort) lastFetchMs = now - kPollMs + 8000;  // not a network fault: try again in 8 s, not 20
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
  if (online && flightCount > 0 && selected < flightCount && !routeTried(flights[selected].callsign)) {
    if (fetchRoute(flights[selected].callsign)) redraw = true;  // each callsign is asked once
    pageStartMs = millis();                                      // the lookup took a while: restart the alternation
  }
  if (flightCount > 0 && routePageNow() != shownRoutePage) redraw = true;  // the airline / route row flips
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
