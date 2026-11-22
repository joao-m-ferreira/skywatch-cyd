#include "plate.h"

#include <stdio.h>

#include "airline.h"
#include "board.h"
#include "silhouette.h"

namespace {
struct Colors {
  uint16_t paper, ink, faint, accent;
};
Colors colors(lgfx::LovyanGFX& g) {
  return {static_cast<uint16_t>(g.color565(244, 234, 208)), static_cast<uint16_t>(g.color565(44, 32, 22)),
          static_cast<uint16_t>(g.color565(176, 160, 130)), static_cast<uint16_t>(g.color565(150, 40, 30))};
}

void frame(lgfx::LovyanGFX& g, const Colors& c, const char* left, const char* right) {
  g.fillScreen(c.paper);
  g.drawRect(3, 3, board::kScreenW - 6, board::kScreenH - 6, c.ink);
  g.drawRect(6, 6, board::kScreenW - 12, board::kScreenH - 12, c.faint);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.ink);
  g.setTextDatum(lgfx::top_left);
  g.drawString(left, 14, 11);
  g.setTextDatum(lgfx::top_right);
  g.setTextColor(c.faint);
  g.drawString(right, board::kScreenW - 14, 11);
  g.drawFastHLine(10, 30, board::kScreenW - 20, c.ink);
}

void drawShape(lgfx::LovyanGFX& g, const jf::Shape& sh, float heading, int cx, int cy,
               uint16_t hull, uint16_t primary, uint16_t secondary, uint16_t ink) {
  const float scale = sh.scale;  // pixels per metre
  for (size_t p = 0; p < sh.count; ++p) {
    const jf::Poly& poly = sh.polys[p];
    const uint16_t fill = poly.part == 2 ? primary : poly.part == 3 ? secondary : hull;
    for (int m = 0; m < (poly.mirror ? 2 : 1); ++m) {
      int px[jf::kMaxPolyPoints], py[jf::kMaxPolyPoints];
      for (size_t i = 0; i < poly.n; ++i) {
        jf::transformPoint(m ? -poly.x[i] : poly.x[i], poly.y[i], heading, scale, cx, cy,
                           &px[i], &py[i]);
      }
      for (size_t i = 1; i + 1 < poly.n; ++i) {  // convex fan
        g.fillTriangle(px[0], py[0], px[i], py[i], px[i + 1], py[i + 1], fill);
      }
      for (size_t i = 0; i < poly.n; ++i) {  // outline, skipping the mirror seam on x = 0
        const size_t j = (i + 1) % poly.n;
        if (poly.x[i] == 0.0f && poly.x[j] == 0.0f) continue;
        g.drawLine(px[i], py[i], px[j], py[j], ink);
      }
    }
  }
  if (sh.rotor_radius > 0.0f) {
    g.drawCircle(cx, cy, static_cast<int>(scale * sh.rotor_radius), ink);
    g.drawCircle(cx, cy, 3, ink);
  }
}

void field(lgfx::LovyanGFX& g, const Colors& c, int y, const char* label, const char* value) {
  g.setTextDatum(lgfx::top_left);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.faint);
  g.drawString(label, 186, y);
  g.setFont(&lgfx::fonts::FreeSans9pt7b);
  g.setTextColor(c.ink);
  g.drawString(value, 186, y + 16);
}
}  // namespace

void drawPlate(lgfx::LovyanGFX& g, const jf::Flight& f, int index, int count,
               const char* header_left, const char* header_right) {
  const Colors c = colors(g);
  g.startWrite();
  frame(g, c, header_left, header_right);

  // Left: the aircraft, pointing along its track, inside a heading ring.
  const int cx = 93, cy = 118;
  g.drawCircle(cx, cy, 70, c.faint);
  const jf::Airline* al = jf::airlineFor(f.callsign);
  uint16_t pri = c.ink, sec = c.faint;
  if (al != nullptr) {
    pri = g.color565(al->r1, al->g1, al->b1);
    sec = g.color565(al->r2, al->g2, al->b2);
  }
  const uint16_t hull = g.color565(252, 250, 242);
  drawShape(g, jf::shapeForType(f.type), static_cast<float>(f.track), cx, cy, hull, pri, sec,
            c.ink);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(c.accent);
  g.drawString("N", cx, cy - 78);

  // Page dots.
  const int dots = count > 8 ? 8 : count;
  for (int i = 0; i < dots; ++i) {
    int x = cx - (dots - 1) * 6 + i * 12;
    if (i == index) g.fillCircle(x, 213, 3, c.ink);
    else g.drawCircle(x, 213, 3, c.faint);
  }

  // Divider + particulars (rows spaced so no line overlaps another).
  g.drawFastVLine(176, 36, 190, c.faint);
  g.setTextDatum(lgfx::top_left);
  g.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  g.setTextColor(c.ink);
  g.drawString(f.callsign, 186, 40);
  g.setFont(&lgfx::fonts::FreeSansOblique9pt7b);
  g.setTextColor(c.accent);
  g.drawString(f.type[0] ? f.type : "type n/a", 186, 68);
  if (al != nullptr) {
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.ink);
    g.drawString(al->name, 186, 90);
    g.fillRect(186, 34, 78, 3, pri);
    g.fillRect(264, 34, 40, 3, sec);
  } else {  // unknown operator: show its code; registrations have no airline
    char op[24];
    if (jf::looksLikeAirlineCallsign(f.callsign)) {
      snprintf(op, sizeof op, "Operator %.3s", f.callsign);
    } else {
      snprintf(op, sizeof op, "Private / other");
    }
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.faint);
    g.drawString(op, 186, 90);
  }
  g.drawFastHLine(186, 108, 118, c.ink);

  char buf[24], d[16];
  jf::formatAltitude(f.alt_ft, buf, sizeof buf);
  field(g, c, 112, "ALTITUDE", buf);
  snprintf(buf, sizeof buf, "%d kt  trk %03d", f.speed_kt, f.track);
  field(g, c, 150, "SPEED / TRACK", buf);
  jf::formatDistance(f.dist_km, d, sizeof d);
  snprintf(buf, sizeof buf, "%s %s", d, jf::compass(f.bearing));
  field(g, c, 188, "DISTANCE", buf);
  g.endWrite();
}

void drawPlateMessage(lgfx::LovyanGFX& g, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error) {
  const Colors c = colors(g);
  g.startWrite();
  frame(g, c, header_left, header_right);
  g.setFont(&lgfx::fonts::FreeSansOblique12pt7b);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(error ? c.accent : c.ink);
  g.drawString(msg, board::kScreenW / 2, board::kScreenH / 2 - 8);
  if (detail != nullptr && detail[0] != '\0') {
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.ink);
    g.drawString(detail, board::kScreenW / 2, board::kScreenH / 2 + 24);
  }
  g.endWrite();
}
