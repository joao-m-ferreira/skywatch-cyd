#include "routes.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <string.h>

namespace jf {
namespace {

constexpr size_t kSlots = 8;
Route slots[kSlots];
size_t nextSlot = 0;

Route* find(const char* cs) {
  for (Route& r : slots) {
    if (r.callsign[0] != '\0' && strcmp(r.callsign, cs) == 0) return &r;
  }
  return nullptr;
}

void copy(char* dst, size_t n, const char* src) {
  strncpy(dst, src != nullptr ? src : "", n - 1);
  dst[n - 1] = '\0';
}

}  // namespace

const Route* routeFor(const char* cs) {
  if (cs == nullptr || cs[0] == '\0') return nullptr;
  const Route* r = find(cs);
  return (r != nullptr && r->ok) ? r : nullptr;
}

bool routePrefetch(const char* cs) {
  if (cs == nullptr || cs[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;
  if (find(cs) != nullptr) return false;  // already tried
  Route* r = &slots[nextSlot];
  nextSlot = (nextSlot + 1) % kSlots;
  *r = Route{};
  copy(r->callsign, sizeof r->callsign, cs);
  r->tried = true;

  char url[64];
  snprintf(url, sizeof url, "https://api.adsbdb.com/v0/callsign/%s", cs);
  WiFiClientSecure client;
  client.setInsecure();  // public read-only data, nothing private is sent
  HTTPClient http;
  http.setTimeout(8000);
  http.setUserAgent("jf-flight-display/1.0");
  if (!http.begin(client, url)) return false;
  bool got = false;
  const int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    JsonDocument filter, doc;
    for (const char* end : {"origin", "destination"}) {
      filter["response"]["flightroute"][end]["iata_code"] = true;
      filter["response"]["flightroute"][end]["municipality"] = true;
    }
    if (!deserializeJson(doc, body, DeserializationOption::Filter(filter))) {
      JsonObjectConst fr = doc["response"]["flightroute"];
      if (!fr.isNull()) {  // unknown callsigns answer with a plain string instead
        copy(r->o_iata, sizeof r->o_iata, fr["origin"]["iata_code"] | "");
        copy(r->o_city, sizeof r->o_city, fr["origin"]["municipality"] | "");
        copy(r->d_iata, sizeof r->d_iata, fr["destination"]["iata_code"] | "");
        copy(r->d_city, sizeof r->d_city, fr["destination"]["municipality"] | "");
        r->ok = r->o_iata[0] != '\0' || r->d_iata[0] != '\0';
        got = r->ok;
      }
    }
  }
  http.end();
  Serial.printf("route %s: %s%s%s\n", cs, got ? r->o_iata : "none", got ? " > " : "", got ? r->d_iata : "");
  return got;
}

}  // namespace jf
