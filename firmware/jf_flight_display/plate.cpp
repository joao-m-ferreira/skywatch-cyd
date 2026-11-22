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

void drawShape(lgfx::LovyanGFX& g, const jf::Shape& sh, bool facing_right, int cx, int cy,
               uint16_t hull, uint16_t primary, uint16_t secondary, uint16_t ink,
               bool gear_down) {
  const uint16_t glass = g.color565(170, 200, 222), window = g.color565(60, 70, 90);
  for (size_t p = 0; p < sh.count; ++p) {
    const jf::Poly& poly = sh.polys[p];
    if (poly.part == jf::kGear && !gear_down) continue;
    int px[jf::kMaxPolyPoints], py[jf::kMaxPolyPoints];
    for (size_t i = 0; i < poly.n; ++i) {
      jf::transformSide(poly.x[i], poly.y[i], facing_right, sh.scale, cx, cy, &px[i], &py[i]);
    }
    if (poly.part == jf::kWindows) {  // dashed row from point 0 to point 1
      const int dx = px[1] - px[0], dy = py[1] - py[0];
      const int steps = (dx < 0 ? -dx : dx) / 4;
      for (int k = 0; k < steps; k += 1) {
        g.fillRect(px[0] + dx * k / steps, py[0] + dy * k / steps, 2, 2, window);
      }
      continue;
    }
    uint16_t fill = hull;
    bool outline = true;
    switch (poly.part) {
      case jf::kFin: fill = primary; break;
      case jf::kEngine: fill = secondary; break;
      case jf::kStripe: fill = primary; outline = false; break;
      case jf::kGlass: fill = glass; outline = false; break;
      case jf::kGear:
      case jf::kDark: fill = ink; outline = false; break;
      default: break;
    }
    for (size_t i = 1; i + 1 < poly.n; ++i) {  // convex fan
      g.fillTriangle(px[0], py[0], px[i], py[i], px[i + 1], py[i + 1], fill);
    }
    if (outline) {
      for (size_t i = 0; i < poly.n; ++i) {
        const size_t j = (i + 1) % poly.n;
        g.drawLine(px[i], py[i], px[j], py[j], ink);
      }
    }
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

  // Left: the aircraft in side view, nose pointing the way it is flying.
  const int cx = 93, cy = 112;
  const jf::Airline* al = jf::airlineFor(f.callsign);
  uint16_t pri = g.color565(120, 130, 150), sec = g.color565(90, 95, 105);
  if (al != nullptr) {
    pri = g.color565(al->r1, al->g1, al->b1);
    sec = g.color565(al->r2, al->g2, al->b2);
  }
  const uint16_t hull = g.color565(252, 250, 242);
  const bool right = f.track > 0 && f.track < 180;  // eastbound: nose to the right
  const bool gear = f.alt_ft < 1500 && f.speed_kt < 200;
  const jf::Shape& sh = jf::shapeForType(f.type);
  // Centre the drawing vertically on the middle of its own extent.
  const int cyc = cy - static_cast<int>(sh.scale * (sh.top + sh.bottom) / 2.0f);
  g.drawFastHLine(14, 200, 156, c.faint);  // ground reference line
  drawShape(g, sh, right, cx, cyc, hull, pri, sec, c.ink, gear);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(c.faint);
  char hdg[16];
  snprintf(hdg, sizeof hdg, right ? "heading E >" : "< heading W");
  g.drawString(hdg, cx, 186);

  // Page dots.
  const int dots = count > 8 ? 8 : count;
  for (int i = 0; i < dots; ++i) {
    int x = cx - (dots - 1) * 6 + i * 12;
    if (i == index) g.fillCircle(x, 216, 3, c.ink);
    else g.drawCircle(x, 216, 3, c.faint);
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
