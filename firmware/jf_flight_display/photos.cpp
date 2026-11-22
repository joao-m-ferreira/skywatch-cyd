#include "photos.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>
#include <string.h>

#include "secrets.h"  // PLANESPOTTERS_CONTACT

namespace jf {
namespace {

constexpr size_t kSlots = 4;      // photos kept in RAM
constexpr size_t kMaxJpg = 24000;  // thumbnails are ~5-15 KB
Photo slots[kSlots];
size_t nextSlot = 0;

Photo* find(const char* reg) {
  for (Photo& p : slots) {
    if (p.reg[0] != '\0' && strcmp(p.reg, reg) == 0) return &p;
  }
  return nullptr;
}

Photo* claim(const char* reg) {
  Photo* p = &slots[nextSlot];
  nextSlot = (nextSlot + 1) % kSlots;
  free(p->jpg);
  *p = Photo{};
  strncpy(p->reg, reg, sizeof p->reg - 1);
  return p;
}

char contactBuf[64] = PLANESPOTTERS_CONTACT;

void userAgent(HTTPClient& http) {
  // planespotters.net requires a contact in the User-Agent.
  static char ua[112];
  snprintf(ua, sizeof ua, "jf-flight-display/1.0 (+mailto:%s)", contactBuf);
  http.setUserAgent(ua);
}

// Public read-only data: certificate validation is skipped (see the adsb.fi fetch).
bool getText(const char* url, String* out) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  userAgent(http);
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code == HTTP_CODE_OK) *out = http.getString();
  http.end();
  return code == HTTP_CODE_OK;
}

uint8_t* getBytes(const char* url, size_t* len) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding
  http.setTimeout(8000);
  userAgent(http);
  if (!http.begin(client, url)) return nullptr;
  uint8_t* buf = nullptr;
  if (http.GET() == HTTP_CODE_OK) {
    const int size = http.getSize();
    const size_t cap = size > 0 ? static_cast<size_t>(size) : kMaxJpg;
    if (cap <= kMaxJpg && (buf = static_cast<uint8_t*>(malloc(cap))) != nullptr) {
      WiFiClient* s = http.getStreamPtr();
      size_t got = 0;
      const uint32_t t0 = millis();
      while (got < cap && millis() - t0 < 10000 && (http.connected() || s->available())) {
        const int n = s->readBytes(buf + got, cap - got);
        if (n <= 0) delay(5);
        got += n > 0 ? static_cast<size_t>(n) : 0;
      }
      if (got < 1000 || (size > 0 && got != cap)) {  // truncated or error page
        free(buf);
        buf = nullptr;
      } else {
        *len = got;
      }
    }
  }
  http.end();
  return buf;
}

}  // namespace

void photoSetContact(const char* contact) {
  strncpy(contactBuf, contact, sizeof contactBuf - 1);
  contactBuf[sizeof contactBuf - 1] = '\0';
}

const Photo* photoFor(const char* reg) {
  if (reg == nullptr || reg[0] == '\0') return nullptr;
  const Photo* p = find(reg);
  return (p != nullptr && p->jpg != nullptr) ? p : nullptr;
}

bool photoPrefetch(const char* reg) {
  if (reg == nullptr || reg[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;
  if (find(reg) != nullptr) return false;  // already tried
  Photo* p = claim(reg);
  p->tried = true;

  char url[96];
  snprintf(url, sizeof url, "https://api.planespotters.net/pub/photos/reg/%s", reg);
  String body;
  if (!getText(url, &body)) {
    Serial.printf("photo %s: lookup failed\n", reg);
    return false;
  }
  JsonDocument filter, doc;
  filter["photos"][0]["thumbnail"]["src"] = true;
  filter["photos"][0]["photographer"] = true;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  const char* src = doc["photos"][0]["thumbnail"]["src"] | "";
  if (strncmp(src, "https://", 8) != 0) {
    Serial.printf("photo %s: none available\n", reg);
    return false;
  }
  strncpy(p->credit, doc["photos"][0]["photographer"] | "", sizeof p->credit - 1);
  size_t len = 0;
  uint8_t* jpg = getBytes(src, &len);
  if (jpg == nullptr) {
    Serial.printf("photo %s: download failed\n", reg);
    return false;
  }
  p->jpg = jpg;
  p->len = len;
  Serial.printf("photo %s: %u bytes by %s\n", reg, static_cast<unsigned>(len), p->credit);
  return true;
}

}  // namespace jf
