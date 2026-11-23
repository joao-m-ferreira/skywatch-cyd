#include "plate.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "airline.h"
#include <TJpg_Decoder.h>

#include "livery.h"
#include "board.h"
#include "layout.h"
#include "model3d.h"

namespace {
struct Colors {
  uint16_t paper, ink, faint, accent;
};
Colors colors(lgfx::LovyanGFX& g) {
  return {static_cast<uint16_t>(g.color565(244, 234, 208)), static_cast<uint16_t>(g.color565(44, 32, 22)),
          static_cast<uint16_t>(g.color565(176, 160, 130)), static_cast<uint16_t>(g.color565(150, 40, 30))};
}

void frame(lgfx::LovyanGFX& g, const Colors& c, const jf::Layout& L, const char* left, const char* right) {
  g.fillScreen(c.paper);
  g.drawRect(3, 3, L.w - 6, L.h - 6, c.ink);  // the ruled border follows the real screen edges
  g.drawRect(6, 6, L.w - 12, L.h - 12, c.faint);
  g.setTextSize(L.s);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.ink);
  g.setTextDatum(lgfx::top_left);
  g.drawString(left, L.X(L.header_left.x), L.Y(L.header_left.y));
  g.setTextDatum(lgfx::top_right);
  g.setTextColor(c.faint);
  g.drawString(right, L.X(L.header_right.x), L.Y(L.header_right.y));
  g.drawFastHLine(L.X(10), L.Y(L.header_rule_y), L.w - 2 * L.X(10), c.ink);
}

lgfx::LovyanGFX* gPhotoGfx = nullptr;
int gPhotoX = 0, gPhotoY = 0;
float gPhotoS = 1.0f;  // layout scale

// Decoder callback: draws a block straight to the screen, dropping every 4th row and
// column (nearest-neighbour 3/4 downscale: a 200 px thumbnail becomes 150 px).
bool photoBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (uint16_t r = 0; r < h; ++r) {
    const int sy = y + r;
    if (sy % 4 == 3) continue;
    const int dy = sy - sy / 4;
    for (uint16_t cc = 0; cc < w; ++cc) {
      const int sx = x + cc;
      if (sx % 4 == 3) continue;
      const int dx = sx - sx / 4;
      if (gPhotoS <= 1.01f) {
        gPhotoGfx->drawPixel(gPhotoX + dx, gPhotoY + dy, bitmap[r * w + cc]);
      } else {  // larger panel: each kept pixel becomes a small block, edges rounded so there are no gaps
        const int x0 = static_cast<int>(lroundf(static_cast<float>(dx) * gPhotoS));
        const int x1 = static_cast<int>(lroundf(static_cast<float>(dx + 1) * gPhotoS));
        const int y0 = static_cast<int>(lroundf(static_cast<float>(dy) * gPhotoS));
        const int y1 = static_cast<int>(lroundf(static_cast<float>(dy + 1) * gPhotoS));
        gPhotoGfx->fillRect(gPhotoX + x0, gPhotoY + y0, x1 - x0, y1 - y0, bitmap[r * w + cc]);
      }
    }
  }
  return true;
}

// Decodes the JPEG into the plane's area (centred, scaled to the panel). False on failure.
bool drawPhoto(lgfx::LovyanGFX& g, const jf::Photo& ph, const Colors& c, const jf::Layout& L) {
  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, ph.jpg, ph.len) != 0 || jw == 0 || jw > 220 || jh > 160) return false;
  const int ow = jw - jw / 4, oh = jh - jh / 4;
  // The largest scale at which the photo fits the plane's area, capped at the panel's scale.
  float k = L.s;
  const float kw = static_cast<float>(L.D(L.model.w)) / static_cast<float>(ow),
              kh = static_cast<float>(L.D(L.model.h)) / static_cast<float>(oh);
  if (kw < k) k = kw;
  if (kh < k) k = kh;
  if (k < 0.5f) return false;
  const int sw = static_cast<int>(lroundf(static_cast<float>(ow) * k)),
            sh = static_cast<int>(lroundf(static_cast<float>(oh) * k));
  gPhotoGfx = &g;
  gPhotoS = k;
  gPhotoX = L.X(L.model.x) + (L.D(L.model.w) - sw) / 2;
  gPhotoY = L.Y(L.model.y) + (L.D(L.model.h) - sh) / 2;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(photoBlock);
  const bool ok = TJpgDec.drawJpg(0, 0, ph.jpg, ph.len) == 0;
  if (ok) g.drawRect(gPhotoX - 1, gPhotoY - 1, sw + 2, sh + 2, c.faint);
  gPhotoGfx = nullptr;
  return ok;
}

// Compass dial: red north marker at the top, an arrow along the direction of travel.
void drawCompass(lgfx::LovyanGFX& g, const Colors& c, const jf::Layout& L, int cx, int cy, int r, int track) {
  g.drawCircle(cx, cy, r, c.faint);
  const int nw = L.D(3), nh = L.D(6);
  g.fillTriangle(cx - nw, cy - r - 1, cx + nw, cy - r - 1, cx, cy - r - nh, c.accent);  // north
  const float a = static_cast<float>(track) * static_cast<float>(M_PI) / 180.0f;
  const float dx = sinf(a), dy = -cosf(a), px = -dy, py = dx;  // direction and its normal
  const float len = static_cast<float>(r) - 2.0f, w = 0.42f * static_cast<float>(r);
  const float bx = cx - 0.45f * len * dx, by = cy - 0.45f * len * dy;  // arrow base
  g.fillTriangle(static_cast<int>(lroundf(cx + len * dx)), static_cast<int>(lroundf(cy + len * dy)),
                 static_cast<int>(lroundf(bx + w * px)), static_cast<int>(lroundf(by + w * py)),
                 static_cast<int>(lroundf(bx - w * px)), static_cast<int>(lroundf(by - w * py)), c.ink);
}

void field(lgfx::LovyanGFX& g, const Colors& c, const jf::Layout& L, jf::Pt at, const char* label, const char* value) {
  g.setTextSize(L.s);
  g.setTextDatum(lgfx::top_left);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.faint);
  g.drawString(label, L.X(at.x), L.Y(at.y));
  g.setFont(&lgfx::fonts::FreeSans9pt7b);
  g.setTextColor(c.ink);
  g.drawString(value, L.X(at.x), L.Y(at.y + 16));
}
}  // namespace

void drawPlate(lgfx::LovyanGFX& g, const jf::Flight& f, int index, int count,
               const char* header_left, const char* header_right, const jf::Photo* photo, bool lite,
               int layout_mode) {
  const Colors c = colors(g);
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  g.startWrite();
  frame(g, c, L, header_left, header_right);

  // The aircraft as a small shaded 3D model (dark outline), nose pointing the way it is flying, in
  // the airline's livery.
  const jf::Airline* al = jf::airlineFor(f.callsign);
  const jf::Livery lv = jf::liveryFor(al);
  uint16_t pri = g.color565(120, 130, 150), sec = g.color565(90, 95, 105);
  jf::Palette pal = {{252, 250, 242}, {222, 222, 228}, {120, 130, 150}, {90, 95, 105},
                     {150, 190, 220}, {44, 32, 22},    {255, 255, 255}, {205, 205, 210},
                     {120, 130, 150}, lv.nose_red,     0,       lv.tail, lv.body,
                     {120, 130, 150}};
  if (al != nullptr) {
    pal.primary = {al->r1, al->g1, al->b1};
    pal.secondary = {al->r2, al->g2, al->b2};
    pal.tip = pal.primary;
    pri = g.color565(al->r1, al->g1, al->b1);
    sec = g.color565(al->r2, al->g2, al->b2);
  }
  if (lv.tail == jf::kTailTap) pal.tip = pal.secondary;  // TAP: red winglets
  if (lv.tail == jf::kTailFlag) pal.tip = {250, 250, 250};  // Emirates: white wingtips
  switch (lv.fin_base) {
    case jf::kFinWhite: pal.fin = {250, 250, 250}; break;
    case jf::kFinSecondary: pal.fin = pal.secondary; break;
    default: pal.fin = pal.primary; break;
  }
  pal.motif = lv.fin_base != jf::kFinPrimary ? pal.primary
              : lv.white_motif               ? jf::Rgb{255, 255, 255}
                                             : pal.secondary;
  if (lv.engines == 1) pal.engine = pal.primary;
  if (lv.engines == 2) pal.engine = pal.secondary;
  switch (lv.hull) {  // tinted fuselages
    case jf::kHullPearl: pal.hull = {214, 214, 212}; break;
    case jf::kHullChampagne: pal.hull = {240, 230, 206}; break;
    case jf::kHullSky: pal.hull = {150, 200, 235}; break;
    default: break;
  }
  pal.belly = {static_cast<uint8_t>(pal.hull.r * 0.88f), static_cast<uint8_t>(pal.hull.g * 0.88f),
               static_cast<uint8_t>(pal.hull.b * 0.9f)};
  pal.gear = (f.alt_ft < 1500 && f.speed_kt < 200) ? 1 : 0;  // landing gear down when low and slow
  const bool right = f.track > 0 && f.track < 180;  // eastbound: nose to the right
  bool shown_photo = false;
  if (!lite && photo != nullptr && photo->jpg != nullptr) shown_photo = drawPhoto(g, *photo, c, L);
  const int mcx = L.X(L.model.x) + L.D(L.model.w) / 2, mcy = L.Y(L.model.y) + L.D(L.model.h) / 2;
  if (!shown_photo && lite) {
    g.setTextSize(L.s);
    g.setFont(&lgfx::fonts::Font2);
    g.setTextDatum(lgfx::middle_center);
    g.setTextColor(c.faint);
    g.drawString("safe mode", mcx, mcy - L.D(10));
    g.drawString("(restarted repeatedly)", mcx, mcy + L.D(10));
  } else if (!shown_photo) {
    const jf::Mesh& mesh = jf::buildAircraft(f.type, pal);
    // Allocated once and kept (too big for static DRAM; per-draw malloc fragments the heap).
    static jf::ScreenTri* tris = static_cast<jf::ScreenTri*>(malloc(sizeof(jf::ScreenTri) * jf::kMaxTris));
    if (tris == nullptr) abort();
    {
      // The model fills the plane's area, centred (the projection fits it into the box).
      const size_t n = jf::projectMesh(mesh, right, mcx, mcy, L.D(L.model.w) - 6, L.D(L.model.h) - 4, tris, jf::kMaxTris);
      const uint16_t outline = g.color565(20, 24, 32);
      for (int o = 0; o < 4; ++o) {  // dark outline around the silhouette
        const int dx = ((o == 0) - (o == 1)) * L.thick(), dy = ((o == 2) - (o == 3)) * L.thick();
        for (size_t i = 0; i < n; ++i) {
          const jf::ScreenTri& t = tris[i];
          g.fillTriangle(t.x[0] + dx, t.y[0] + dy, t.x[1] + dx, t.y[1] + dy, t.x[2] + dx,
                         t.y[2] + dy, outline);
        }
      }
      for (size_t i = 0; i < n; ++i) {
        const jf::ScreenTri& t = tris[i];
        g.fillTriangle(t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2],
                       g.color565(t.color.r, t.color.g, t.color.b));
      }
    }
  }
  if (shown_photo) {  // credit the photographer, under the photo
    char credit[40];
    snprintf(credit, sizeof credit, "Photo: %.26s", photo->credit);
    g.setTextSize(L.s);
    g.setFont(&lgfx::fonts::Font2);
    g.setTextDatum(lgfx::middle_center);
    g.setTextColor(c.faint);
    g.drawString(credit, mcx, L.Y(L.model.y + L.model.h) + L.D(2));
  }
  // Compass with the direction of travel.
  drawCompass(g, c, L, L.X(L.compass.x), L.Y(L.compass.y), L.D(12), f.track);
  char hdg[24];
  snprintf(hdg, sizeof hdg, "%s  %03d", jf::compass(f.track), f.track);
  g.setTextSize(L.s);
  g.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  g.setTextDatum(lgfx::middle_left);
  g.setTextColor(c.ink);
  g.drawString(hdg, L.X(L.heading.x), L.Y(L.heading.y));

  // Page dots.
  const int dots = count > 8 ? 8 : count;
  for (int i = 0; i < dots; ++i) {
    const int x = L.X(L.dots.x - (dots - 1) * 6 + i * 12);
    if (i == index) g.fillCircle(x, L.Y(L.dots.y), L.D(3), c.ink);
    else g.drawCircle(x, L.Y(L.dots.y), L.D(3), c.faint);
  }

  // Divider + particulars (rows spaced so no line overlaps another).
  if (L.divider_vertical) g.drawFastVLine(L.X(L.divider_x), L.Y(L.divider_y), L.D(L.divider_len), c.faint);
  else g.drawFastHLine(L.X(L.divider_x), L.Y(L.divider_y), L.D(L.divider_len), c.faint);
  g.setTextSize(L.s);
  g.setTextDatum(lgfx::top_left);
  g.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  g.setTextColor(c.ink);
  g.drawString(f.callsign, L.X(L.call.x), L.Y(L.call.y));
  g.setFont(&lgfx::fonts::FreeSansOblique9pt7b);
  g.setTextColor(c.accent);
  g.drawString(f.type[0] ? f.type : "type n/a", L.X(L.type.x), L.Y(L.type.y));
  if (al != nullptr) {
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.ink);
    g.drawString(al->name, L.X(L.airline.x), L.Y(L.airline.y));
    g.fillRect(L.X(L.bars.x), L.Y(L.bars.y), L.D(78), L.D(3), pri);
    g.fillRect(L.X(L.bars.x + 78), L.Y(L.bars.y), L.D(40), L.D(3), sec);
  } else {  // unknown operator: show its code; registrations have no airline
    char op[24];
    if (jf::looksLikeAirlineCallsign(f.callsign)) {
      snprintf(op, sizeof op, "Operator %.3s", f.callsign);
    } else {
      snprintf(op, sizeof op, "Private / other");
    }
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.faint);
    g.drawString(op, L.X(L.airline.x), L.Y(L.airline.y));
  }
  g.drawFastHLine(L.X(L.rule.x), L.Y(L.rule.y), L.D(L.rule_w), c.ink);

  char buf[24], d[16];
  jf::formatAltitude(f.alt_ft, buf, sizeof buf);
  field(g, c, L, L.altitude, "ALTITUDE", buf);
  snprintf(buf, sizeof buf, "%d kt  trk %03d", f.speed_kt, f.track);
  field(g, c, L, L.speed, "SPEED / TRACK", buf);
  jf::formatDistance(f.dist_km, d, sizeof d);
  snprintf(buf, sizeof buf, "%s %s", d, jf::compass(f.bearing));
  field(g, c, L, L.distance, "DISTANCE", buf);
  g.setTextSize(1.0f);  // leave the text scale as found
  g.endWrite();
}

void drawPlateMessage(lgfx::LovyanGFX& g, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error, int layout_mode) {
  const Colors c = colors(g);
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  g.startWrite();
  frame(g, c, L, header_left, header_right);
  g.setTextSize(L.s);
  g.setFont(&lgfx::fonts::FreeSansOblique12pt7b);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(error ? c.accent : c.ink);
  g.drawString(msg, g.width() / 2, g.height() / 2 - L.D(8));
  if (detail != nullptr && detail[0] != '\0') {
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.ink);
    g.drawString(detail, g.width() / 2, g.height() / 2 + L.D(24));
  }
  g.setTextSize(1.0f);
  g.endWrite();
}
