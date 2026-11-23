// Small helpers for the WiFi setup mode (access point + settings page). No Arduino headers: host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace jf {

// Name of the setup access point: "skywatch-" plus the last two bytes of the board's MAC, so two boards
// set up side by side do not clash.
inline void apName(const uint8_t mac[6], char* out, size_t n) {
  snprintf(out, n, "skywatch-%02X%02X", static_cast<unsigned>(mac[4]), static_cast<unsigned>(mac[5]));
}

// A one-time access point password: 8 characters from an alphabet without look-alikes (no 0/O, 1/I),
// shown on the board's screen. `out` needs 9 bytes.
inline void apPassword(uint32_t r1, uint32_t r2, char* out) {
  static const char kAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";  // 32 symbols: 5 bits each
  uint64_t bits = (static_cast<uint64_t>(r1) << 32) | r2;
  for (int i = 0; i < 8; ++i) {
    out[i] = kAlphabet[bits & 31u];
    bits >>= 5;
  }
  out[8] = '\0';
}

// A 24-character access token from four random words (5 bits per character, 120 bits). `out` needs 25 bytes.
inline void randomToken(const uint32_t r[4], char* out) {
  static const char kAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
  for (int i = 0; i < 24; ++i) {
    const int bit = i * 5;  // 5-bit groups can straddle two words
    const int w = bit / 32, off = bit % 32;
    uint64_t v = r[w];
    if (w + 1 < 4) v |= static_cast<uint64_t>(r[w + 1]) << 32;
    out[i] = kAlphabet[(v >> off) & 31u];
  }
  out[24] = '\0';
}

// Compares two secrets in time that does not depend on where they first differ.
inline bool secretEquals(const char* a, const char* b) {
  size_t la = 0, lb = 0;
  while (a[la] != '\0') ++la;
  while (b[lb] != '\0') ++lb;
  unsigned diff = static_cast<unsigned>(la ^ lb);
  const size_t n = la < lb ? la : lb;
  for (size_t i = 0; i < n; ++i) diff |= static_cast<unsigned>(static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]));
  return diff == 0;
}

// The token in an "Authorization: Bearer <token>" header value; empty if the header has another form.
inline const char* bearerToken(const char* header) {
  static const char kPrefix[] = "Bearer ";
  for (size_t i = 0; kPrefix[i] != '\0'; ++i) {
    if (header[i] != kPrefix[i]) return "";
  }
  return header + 7;
}

// Escapes text for an HTML attribute or element (&, <, >, ", '). Truncates to fit `n` bytes.
inline void htmlEscape(const char* in, char* out, size_t n) {
  size_t o = 0;
  for (; *in != '\0'; ++in) {
    const char* rep = nullptr;
    switch (*in) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&#39;"; break;
      default: break;
    }
    if (rep != nullptr) {
      size_t len = 0;
      while (rep[len] != '\0') ++len;
      if (o + len + 1 > n) break;
      for (size_t i = 0; i < len; ++i) out[o++] = rep[i];
    } else {
      if (o + 2 > n) break;
      out[o++] = *in;
    }
  }
  if (n > 0) out[o < n ? o : n - 1] = '\0';
}

// A form value is acceptable only if it has no control characters (no line breaks: each value becomes
// one settings command).
inline bool formValueOk(const char* v) {
  for (; *v != '\0'; ++v) {
    if (static_cast<unsigned char>(*v) < 0x20 || *v == 0x7f) return false;
  }
  return true;
}

// The text a phone's camera reads to join the setup network ("WIFI:T:WPA;S:<ssid>;P:<password>;;").
// The special characters \ ; , : " in the values would need escaping; the generated names never have them.
inline void wifiQrText(const char* ssid, const char* pass, char* out, size_t n) {
  snprintf(out, n, "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
}

}  // namespace jf
