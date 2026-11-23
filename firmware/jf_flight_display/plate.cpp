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
#include "portal_util.h"

namespace {
struct Colors {
  uint16_t paper, ink, faint, accent;
};
Colors colors(lgfx::LovyanGFX& g) {
  return {static_cast<uint16_t>(g.color565(244, 234, 208)), static_cast<uint16_t>(g.color565(44, 32, 22)),
          static_cast<uint16_t>(g.color565(176, 160, 130)), static_cast<uint16_t>(g.color565(150, 40, 30))};
}

void frame(lgfx::LovyanGFX& g, const Colors& c, const jf::Layout& L, const char* left, const char* right,
           bool header = true) {
  g.fillScreen(c.paper);
  if (L.micro) {  // no room for a double border
    g.drawRect(1, 1, L.w - 2, L.h - 2, c.ink);
  } else {
    g.drawRect(3, 3, L.w - 6, L.h - 6, c.ink);  // the ruled border follows the real screen edges
    g.drawRect(6, 6, L.w - 12, L.h - 12, c.faint);
  }
  if (!header) return;  // compact plate: no room for the header line
  g.setTextSize(L.s);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.ink);
  g.setTextDatum(lgfx::top_left);
  g.drawString(left, L.X(L.header_left.x), L.Y(L.header_left.y));
  if (!L.compact) {  // the right-hand text would run into the left one on a narrow screen
    g.setTextDatum(lgfx::top_right);
    g.setTextColor(c.faint);
    g.drawString(right, L.X(L.header_right.x), L.Y(L.header_right.y));
  }
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

// The compact plate's rotating field (altitude / speed / distance). Text is drawn with the paper as its
// background and a fixed padding, so a page change repaints just this area without clearing it first.
void compactField(lgfx::LovyanGFX& g, const Colors& c, const jf::Layout& L, const jf::Flight& f, int page, const char* route) {
  char buf[24], d[16];
  const int pages = (route != nullptr && route[0] != '\0') ? 4 : 3;
  const int p = ((page % pages) + pages) % pages;
  const char* label = "DISTANCE";  // order: distance, altitude, speed
  if (p == 0) {
    jf::formatDistance(f.dist_km, d, sizeof d);
    snprintf(buf, sizeof buf, "%s %s", d, jf::compass(f.bearing));
  } else if (p == 1) {
    label = "ALTITUDE";
    jf::formatAltitude(f.alt_ft, buf, sizeof buf);
  } else if (p == 2) {
    label = "SPEED";
    snprintf(buf, sizeof buf, "%d kt", f.speed_kt);
  } else {
    label = "ROUTE";
    snprintf(buf, sizeof buf, "%s", route);
  }
  // Clear the whole field first (inside the ruled border), so no trace of the previous value is left.
  g.fillRect(L.data.x, L.data.y, L.rule_w + 1, L.info.y + L.info.h - 2 - L.data.y, c.paper);
  g.setTextSize(1.0f);
  g.setTextDatum(lgfx::top_left);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextColor(c.faint);
  g.drawString(label, L.data.x, L.data.y);
  g.setFont(L.micro ? static_cast<const lgfx::IFont*>(&lgfx::fonts::Font2) : static_cast<const lgfx::IFont*>(&lgfx::fonts::FreeSansBold9pt7b));
  if (L.micro) {  // 72 px column: drop the direction (then the unit) rather than cut the text
    while (g.textWidth(buf) > L.rule_w) {
      char* sp = strrchr(buf, ' ');
      if (sp == nullptr) break;
      *sp = '\0';
    }
  }
  g.setTextColor(c.ink);
  g.drawString(buf, L.data.x, L.data.y + (L.micro ? 16 : 17));
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
               int layout_mode, int page, const char* route) {
  const Colors c = colors(g);
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  g.startWrite();
  frame(g, c, L, header_left, header_right, !L.compact);

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
  drawCompass(g, c, L, L.X(L.compass.x), L.Y(L.compass.y), L.micro ? 7 : L.compact ? 10 : L.D(12), f.track);
  char hdg[24];
  snprintf(hdg, sizeof hdg, L.micro ? "%s %03d" : "%s  %03d", jf::compass(f.track), f.track);
  g.setTextSize(L.s);
  g.setFont(L.micro ? static_cast<const lgfx::IFont*>(&lgfx::fonts::Font2) : static_cast<const lgfx::IFont*>(&lgfx::fonts::FreeSansBold9pt7b));
  g.setTextDatum(lgfx::middle_left);
  g.setTextColor(c.ink);
  g.drawString(hdg, L.X(L.heading.x), L.Y(L.heading.y));

  // Page dots.
  const int dots = L.micro ? 0 : count;
  const int sp = dots <= 8 ? 12 : dots <= 12 ? 9 : 5;  // spacing, in design units
  const int dr = sp >= 9 ? 3 : 2;                      // dot radius
  for (int i = 0; i < dots; ++i) {
    const int x = L.X(L.dots.x - (dots - 1) * sp / 2 + i * sp);
    if (i == index) g.fillCircle(x, L.Y(L.dots.y), L.D(dr), c.ink);
    else g.drawCircle(x, L.Y(L.dots.y), L.D(dr), c.faint);
  }

  // Divider + particulars (rows spaced so no line overlaps another).
  if (L.divider_vertical) g.drawFastVLine(L.X(L.divider_x), L.Y(L.divider_y), L.D(L.divider_len), c.faint);
  else g.drawFastHLine(L.X(L.divider_x), L.Y(L.divider_y), L.D(L.divider_len), c.faint);
  g.setTextSize(L.s);
  g.setTextDatum(lgfx::top_left);
  g.setFont(L.compact ? static_cast<const lgfx::IFont*>(&lgfx::fonts::FreeSansBold9pt7b)
                      : static_cast<const lgfx::IFont*>(&lgfx::fonts::FreeSansBold12pt7b));
  if (L.micro && g.textWidth(f.callsign) > L.rule_w) g.setFont(&lgfx::fonts::Font2);  // a long callsign: smaller type
  g.setTextColor(c.ink);
  g.drawString(f.callsign, L.X(L.call.x), L.Y(L.call.y));
  g.setFont(&lgfx::fonts::FreeSansOblique9pt7b);
  g.setTextColor(c.accent);
  g.drawString(f.type[0] ? f.type : "type n/a", L.X(L.type.x), L.Y(L.type.y));
  if (al != nullptr) {
    g.setFont(&lgfx::fonts::Font2);
    g.setTextColor(c.ink);
    char name[24];
    snprintf(name, sizeof name, L.compact ? "%.13s" : "%.23s", al->name);
    if (!L.micro) g.drawString(name, L.X(L.airline.x), L.Y(L.airline.y));
    const int b1 = L.micro ? 44 : L.compact ? 56 : 78, b2 = L.micro ? 26 : L.compact ? 30 : 40;
    g.fillRect(L.X(L.bars.x), L.Y(L.bars.y), L.D(b1), L.D(3), pri);
    g.fillRect(L.X(L.bars.x + b1), L.Y(L.bars.y), L.D(b2), L.D(3), sec);
  } else if (!L.micro) {  // unknown operator: show its code; registrations have no airline
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
  if (!L.micro) g.drawFastHLine(L.X(L.rule.x), L.Y(L.rule.y), L.D(L.rule_w), c.ink);

  char buf[24], d[16];
  if (L.compact) {  // one field at a time, large; the caller rotates `page`
    compactField(g, c, L, f, page, route);
  } else {
    jf::formatAltitude(f.alt_ft, buf, sizeof buf);
    field(g, c, L, L.altitude, "ALTITUDE", buf);
    snprintf(buf, sizeof buf, "%d kt  trk %03d", f.speed_kt, f.track);
    field(g, c, L, L.speed, "SPEED / TRACK", buf);
    jf::formatDistance(f.dist_km, d, sizeof d);
    snprintf(buf, sizeof buf, "%s %s", d, jf::compass(f.bearing));
    field(g, c, L, L.distance, "DISTANCE", buf);
  }
  g.setTextSize(1.0f);  // leave the text scale as found
  g.endWrite();
}

bool drawPlateField(lgfx::LovyanGFX& g, const jf::Flight& f, int page, int layout_mode, const char* route) {
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  if (!L.compact) return false;
  g.startWrite();
  compactField(g, colors(g), L, f, page, route);
  g.endWrite();
  return true;
}

void drawPlateMessage(lgfx::LovyanGFX& g, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error, int layout_mode) {
  const Colors c = colors(g);
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  g.startWrite();
  frame(g, c, L, header_left, header_right);
  g.setTextSize(L.s);
  g.setFont(&lgfx::fonts::FreeSansOblique12pt7b);
  if (g.textWidth(msg) > g.width() - 16) g.setFont(&lgfx::fonts::FreeSansOblique9pt7b);  // narrow screens
  if (g.textWidth(msg) > g.width() - 16) g.setFont(&lgfx::fonts::Font2);
  g.setTextDatum(lgfx::middle_center);
  g.setTextColor(error ? c.accent : c.ink);
  g.drawString(msg, g.width() / 2, g.height() / 2 - L.D(8));
  if (detail != nullptr && detail[0] != '\0') {
    g.setFont(&lgfx::fonts::Font2);
    if (g.textWidth(detail) > g.width() - 12) g.setFont(&lgfx::fonts::Font0);
    g.setTextColor(c.ink);
    g.drawString(detail, g.width() / 2, g.height() / 2 + L.D(24));
  }
  g.setTextSize(1.0f);
  g.endWrite();
}

// Setup mode: one screen that says how to reach the settings page (QR code to join the board's WiFi, its
// name and one-time password, and the address). Short and portrait panels get their own arrangements.
void drawSetupScreen(lgfx::LovyanGFX& g, const char* ap, const char* pass, const char* url, const char* detail,
                     int layout_mode) {
  const Colors c = colors(g);
  const jf::Layout L = jf::layoutFor(g.width(), g.height(), static_cast<jf::LayoutMode>(layout_mode));
  const lgfx::IFont* small = &lgfx::fonts::Font2;
  const lgfx::IFont* bold = &lgfx::fonts::FreeSansBold9pt7b;
  const bool has_detail = detail != nullptr && detail[0] != '\0';
  g.startWrite();
  frame(g, c, L, "WiFi setup", "", !L.compact);  // a compact screen has no room for the header line
  char text[80];
  if (L.micro) {  // 160x80: no room for a QR code beside the text, so four short rows
    g.setTextSize(1.0f);
    g.setTextDatum(lgfx::top_left);
    g.setFont(bold);
    g.setTextColor(has_detail ? c.accent : c.ink);
    g.drawString(has_detail ? detail : "WiFi setup", 8, 3);
    g.setFont(small);
    g.setTextColor(c.ink);
    g.drawString(ap, 8, 22);
    snprintf(text, sizeof text, "Pass %s", pass);
    g.setFont(bold);
    g.setTextColor(c.accent);
    g.drawString(text, 8, 38);
    snprintf(text, sizeof text, "Open %s", url);
    g.setFont(small);
    g.setTextColor(c.ink);
    g.drawString(text, 8, 58);
    g.endWrite();
    return;
  }
  jf::wifiQrText(ap, pass, text, sizeof text);
  const int t = L.compact ? 3 : (L.D(3) < 2 ? 2 : L.D(3));
  const int qw = 33 * t;  // the code is 29 modules wide and the library centres it in `qw`
  auto put = [&](int x, int y, const lgfx::IFont* font, uint16_t color, const char* s, uint8_t datum = lgfx::top_left) {
    g.setFont(font);
    g.setTextColor(color);
    g.setTextDatum(datum);
    g.drawString(s, x, y);
  };
  g.setTextSize(L.s);
  if (L.mode == jf::LayoutMode::Stacked) {
    const int qx = (L.w - qw) / 2, qy = L.Y(L.header_rule_y) + L.D(8);
    g.qrcode(text, qx, qy, qw);
    const int cx = L.w / 2, lh = L.D(20);
    int y = qy + qw + L.D(8);
    snprintf(text, sizeof text, "WiFi  %s", ap);
    put(cx, y, small, c.ink, text, lgfx::top_center);
    snprintf(text, sizeof text, "Password  %s", pass);
    put(cx, y + lh, bold, c.accent, text, lgfx::top_center);
    snprintf(text, sizeof text, "Open  %s", url);
    put(cx, y + 2 * lh + L.D(4), small, c.ink, text, lgfx::top_center);
    if (has_detail) put(cx, y + 3 * lh + L.D(8), small, c.accent, detail, lgfx::top_center);
  } else {
    const int top = L.compact ? 9 : L.Y(L.header_rule_y) + 4;
    const int area = L.h - (L.compact ? 9 : 6) - top;
    const int qx = L.compact ? 10 : L.X(14), qy = top + (area - qw) / 2;
    g.qrcode(text, qx, qy, qw);
    const int tx = qx + qw + L.X(L.compact ? 8 : 12);
    if (L.compact) {  // 240x135: rows at fixed pixel positions
      put(tx, 11, bold, has_detail ? c.accent : c.ink, has_detail ? detail : "WiFi setup");
      put(tx, 33, small, c.faint, "Join WiFi");
      put(tx, 49, small, c.ink, ap);
      put(tx, 69, small, c.faint, "Password");
      put(tx, 85, bold, c.accent, pass);
      put(tx, 107, small, c.ink, url);
    } else {
      const int y0 = top + L.D(6);
      put(tx, y0, small, c.faint, "Join WiFi");
      put(tx, y0 + L.D(18), small, c.ink, ap);
      put(tx, y0 + L.D(50), small, c.faint, "Password");
      put(tx, y0 + L.D(68), bold, c.accent, pass);
      put(tx, y0 + L.D(100), small, c.faint, "Then open");
      put(tx, y0 + L.D(118), small, c.ink, url);
      if (has_detail) put(tx, y0 + L.D(148), small, c.accent, detail);
    }
  }
  g.setTextSize(1.0f);
  g.endWrite();
}

// ---- 128x64 monochrome OLED: four text rows, the same data as the colour plate ----
void drawPlateOled(lgfx::LovyanGFX& g, const jf::Flight& f, const char* route) {
  const jf::Airline* al = jf::airlineFor(f.callsign);
  jf::OledText t;
  jf::formatOledText(f, al != nullptr ? al->name : nullptr, jf::looksLikeAirlineCallsign(f.callsign), &t);
  g.startWrite();
  g.fillScreen(0);
  g.setTextColor(0xFFFF, 0);  // white on black: the panel has one bit per pixel
  g.setTextSize(1.0f);
  g.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  g.setTextDatum(lgfx::top_left);
  g.drawString(t.call, 0, 0);
  g.setFont(&lgfx::fonts::Font2);
  g.setTextDatum(lgfx::top_right);
  g.drawString(t.type, g.width() - 1, 0);
  g.setTextDatum(lgfx::top_left);
  g.drawString(route != nullptr && route[0] != '\0' ? route : t.airline, 0, 16);
  g.drawString(t.alt_speed, 0, 32);
  g.drawString(t.dist_dir, 0, 48);
  g.endWrite();
  g.display();  // a 1-bit OLED is drawn into a buffer and sent here
}

void drawOledMessage(lgfx::LovyanGFX& g, const char* msg, const char* detail) {
  g.startWrite();
  g.fillScreen(0);
  g.setTextColor(0xFFFF, 0);
  g.setTextSize(1.0f);
  g.setTextDatum(lgfx::middle_center);
  g.setFont(strlen(msg) <= 15 ? static_cast<const lgfx::IFont*>(&lgfx::fonts::Font2) : static_cast<const lgfx::IFont*>(&lgfx::fonts::Font0));
  g.drawString(msg, g.width() / 2, g.height() / 2 - 10);
  if (detail != nullptr && detail[0] != '\0') {
    char buf[22];
    snprintf(buf, sizeof buf, "%.21s", detail);
    g.setFont(&lgfx::fonts::Font0);
    g.drawString(buf, g.width() / 2, g.height() / 2 + 12);
  }
  g.endWrite();
  g.display();
}

void drawSetupOled(lgfx::LovyanGFX& g, const char* ap, const char* pass, const char* url, const char* detail) {
  g.startWrite();
  g.fillScreen(0);
  g.setTextColor(0xFFFF, 0);
  g.setTextSize(1.0f);
  g.setFont(&lgfx::fonts::Font0);
  g.setTextDatum(lgfx::top_left);
  char line[24];
  g.drawString(detail != nullptr && detail[0] != '\0' ? detail : "SETUP: join the WiFi", 0, 0);
  snprintf(line, sizeof line, "%.21s", ap);
  g.drawString(line, 0, 11);
  g.drawString("password:", 0, 24);
  snprintf(line, sizeof line, "%.21s", pass);
  g.drawString(line, 0, 35);
  snprintf(line, sizeof line, "then open %.11s", url);
  g.drawString(line, 0, 52);
  g.endWrite();
  g.display();
}
