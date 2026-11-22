#include "plate.h"

#include <math.h>
#include <stdio.h>

#include "airline.h"
#include <TJpg_Decoder.h>

#include "livery.h"
#include "board.h"
#include "model3d.h"

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

lgfx::LovyanGFX* gPhotoGfx = nullptr;
int gPhotoX = 0, gPhotoY = 0;

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
      gPhotoGfx->drawPixel(gPhotoX + sx - sx / 4, gPhotoY + dy, bitmap[r * w + cc]);
    }
  }
  return true;
}

// Decodes the JPEG straight into the left panel. False on failure.
bool drawPhoto(lgfx::LovyanGFX& g, const jf::Photo& ph, const Colors& c) {
  uint16_t jw = 0, jh = 0;
  if (TJpgDec.getJpgSize(&jw, &jh, ph.jpg, ph.len) != 0 || jw == 0 || jw > 220 || jh > 160) return false;
  const int ow = jw - jw / 4, oh = jh - jh / 4;
  gPhotoGfx = &g;
  gPhotoX = 92 - ow / 2;
  gPhotoY = 46 + (116 - oh) / 2;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(photoBlock);
  const bool ok = TJpgDec.drawJpg(0, 0, ph.jpg, ph.len) == 0;
  if (ok) g.drawRect(gPhotoX - 1, gPhotoY - 1, ow + 2, oh + 2, c.faint);
  gPhotoGfx = nullptr;
  return ok;
}

// Compass dial: red north marker at the top, an arrow along the direction of travel.
void drawCompass(lgfx::LovyanGFX& g, const Colors& c, int cx, int cy, int r, int track) {
  g.drawCircle(cx, cy, r, c.faint);
  g.fillTriangle(cx - 3, cy - r - 1, cx + 3, cy - r - 1, cx, cy - r - 6, c.accent);  // north
  const float a = static_cast<float>(track) * static_cast<float>(M_PI) / 180.0f;
  const float dx = sinf(a), dy = -cosf(a), px = -dy, py = dx;  // direction and its normal
  const float len = static_cast<float>(r) - 2.0f, w = 0.42f * static_cast<float>(r);
  const float bx = cx - 0.45f * len * dx, by = cy - 0.45f * len * dy;  // arrow base
  g.fillTriangle(static_cast<int>(lroundf(cx + len * dx)), static_cast<int>(lroundf(cy + len * dy)),
                 static_cast<int>(lroundf(bx + w * px)), static_cast<int>(lroundf(by + w * py)),
                 static_cast<int>(lroundf(bx - w * px)), static_cast<int>(lroundf(by - w * py)), c.ink);
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
               const char* header_left, const char* header_right, const jf::Photo* photo) {
  const Colors c = colors(g);
  g.startWrite();
  frame(g, c, header_left, header_right);

  // Left: the aircraft as a small shaded 3D model (dark outline), nose pointing the way it
  // is flying, in the airline's livery.
  const jf::Airline* al = jf::airlineFor(f.callsign);
  const jf::Livery lv = jf::liveryFor(al);
  uint16_t pri = g.color565(120, 130, 150), sec = g.color565(90, 95, 105);
  jf::Palette pal = {{252, 250, 242}, {222, 222, 228}, {120, 130, 150}, {90, 95, 105},
                     {150, 190, 220}, {44, 32, 22},    {255, 255, 255}, {205, 205, 210},
                     {120, 130, 150}, lv.nose_red,     0,       lv.tail, lv.body};
  if (al != nullptr) {
    pal.primary = {al->r1, al->g1, al->b1};
    pal.secondary = {al->r2, al->g2, al->b2};
    pri = g.color565(al->r1, al->g1, al->b1);
    sec = g.color565(al->r2, al->g2, al->b2);
  }
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
  if (photo != nullptr && photo->jpg != nullptr) shown_photo = drawPhoto(g, *photo, c);
  if (!shown_photo) {
    const jf::Mesh& mesh = jf::buildAircraft(f.type, pal);
    static jf::ScreenTri tris[jf::kMaxTris];  // fixed buffer: no per-draw heap allocation
    {
      const size_t n = jf::projectMesh(mesh, right, 92, 112, 146, 128, tris, jf::kMaxTris);
      const uint16_t outline = g.color565(20, 24, 32);
      for (int o = 0; o < 4; ++o) {  // 1 px dark outline around the silhouette
        const int dx = (o == 0) - (o == 1), dy = (o == 2) - (o == 3);
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
  if (shown_photo) {  // credit the photographer
    char credit[40];
    snprintf(credit, sizeof credit, "Photo: %.26s", photo->credit);
    g.setFont(&lgfx::fonts::Font2);
    g.setTextDatum(lgfx::middle_center);
    g.setTextColor(c.faint);
    g.drawString(credit, 92, 170);
  }
  // Compass with the direction of travel.
  drawCompass(g, c, 40, 192, 12, f.track);
  char hdg[24];
  snprintf(hdg, sizeof hdg, "%s  %03d", jf::compass(f.track), f.track);
  g.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  g.setTextDatum(lgfx::middle_left);
  g.setTextColor(c.ink);
  g.drawString(hdg, 62, 192);

  // Page dots.
  const int dots = count > 8 ? 8 : count;
  for (int i = 0; i < dots; ++i) {
    int x = 92 - (dots - 1) * 6 + i * 12;
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
