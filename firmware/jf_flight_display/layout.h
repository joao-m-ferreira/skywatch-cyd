// Screen layout for different panels and orientations. No Arduino headers: host-testable.
//
// The plate is laid out on a "design canvas" measured in design units. The canvas always fills the
// screen exactly (no letterboxing): `s` pixels per design unit.
//   Side / SideFlip : canvas height 240, width grows with the panel (plane on one side, text on the
//                     other; Side = plane left, as originally designed; SideFlip = plane right).
//   Stacked         : canvas width 320, height grows with the panel (plane on top, text below);
//                     suits portrait and square panels.
// Auto picks Stacked for portrait and near-square panels and Side otherwise.
#pragma once
#include <math.h>

namespace jf {

enum class LayoutMode { Auto = 0, Side = 1, SideFlip = 2, Stacked = 3 };

struct Rect {
  int x, y, w, h;
};
struct Pt {
  int x, y;
};

struct Layout {
  // --- the screen and the canvas ---
  int w = 320, h = 240;          // pixels
  float s = 1.0f;                // pixels per design unit
  int cw = 320, ch = 240;        // canvas size in design units
  LayoutMode mode = LayoutMode::Side;  // resolved: never Auto
  // --- elements, in design units ---
  Rect model = {14, 38, 156, 128};     // the 3D model or the photo
  Pt compass = {40, 192};              // centre of the compass; radius 12
  Pt heading = {62, 192};              // text, vertically centred
  Pt dots = {92, 216};                 // centre of the page dots
  Rect info = {176, 34, 134, 192};     // the text area (for tests)
  Pt bars = {186, 34};                 // airline colour bars (78 + 40 wide)
  int divider_x = 176, divider_y = 36, divider_len = 190;  // separator line (vertical when side)
  bool divider_vertical = true;
  Pt call = {186, 40}, type = {186, 68}, airline = {186, 90};
  Pt rule = {186, 108};                // horizontal rule under the airline; width rule_w
  int rule_w = 118;
  Pt altitude = {186, 112}, speed = {186, 150}, distance = {186, 188};
  Pt header_left = {14, 11}, header_right = {306, 11};  // header texts (right one right-aligned)
  int header_rule_y = 30;

  int X(int v) const { return static_cast<int>(lroundf(static_cast<float>(v) * s)); }
  int Y(int v) const { return static_cast<int>(lroundf(static_cast<float>(v) * s)); }
  int D(int v) const {  // a length (never collapses to 0)
    const int r = static_cast<int>(lroundf(static_cast<float>(v) * s));
    return (r < 1 && v > 0) ? 1 : r;
  }
  int thick() const { return s >= 1.75f ? 2 : 1; }  // width of the 3D outline in pixels
};

inline Layout layoutFor(int w, int h, LayoutMode want = LayoutMode::Auto) {
  Layout l;
  l.w = w;
  l.h = h;
  const float aspect = static_cast<float>(w) / static_cast<float>(h);
  LayoutMode m = want;
  if (m == LayoutMode::Auto) m = aspect >= 1.2f ? LayoutMode::Side : LayoutMode::Stacked;
  // A mode the panel's shape cannot hold falls back: a stack on a wide panel is too short, a
  // side-by-side layout on a portrait or square panel leaves the plane no room.
  if (m == LayoutMode::Stacked && aspect >= 1.2f) m = LayoutMode::Side;
  if ((m == LayoutMode::Side || m == LayoutMode::SideFlip) && aspect < 1.2f) m = LayoutMode::Stacked;
  l.mode = m;

  if (m == LayoutMode::Stacked) {
    l.s = static_cast<float>(w) / 320.0f;
    l.cw = 320;
    l.ch = static_cast<int>(lroundf(static_cast<float>(h) / l.s));
    constexpr int kInfoH = 122, kStrip = 24, kHead = 36;
    const int info_y = l.ch - kInfoH - 10;
    const int strip_y = info_y - kStrip - 2;
    l.model = {10, kHead + 4, 300, strip_y - kHead - 8};
    l.compass = {34, strip_y + 12};
    l.heading = {54, strip_y + 12};
    l.dots = {240, strip_y + 12};
    l.info = {10, info_y, 300, kInfoH};
    l.bars = {14, info_y - 3};
    l.divider_vertical = false;
    l.divider_x = 10;
    l.divider_y = info_y - 6;
    l.divider_len = 300;
    l.call = {14, info_y + 2};
    l.type = {150, info_y + 6};
    l.airline = {14, info_y + 30};
    l.rule = {14, info_y + 48};
    l.rule_w = 292;
    l.altitude = {14, info_y + 54};
    l.speed = {166, info_y + 54};
    l.distance = {14, info_y + 88};
  } else {
    l.s = static_cast<float>(h) / 240.0f;
    l.ch = 240;
    l.cw = static_cast<int>(lroundf(static_cast<float>(w) / l.s));
    constexpr int kInfoW = 134;
    const bool flip = m == LayoutMode::SideFlip;
    const int info_x = flip ? 10 : l.cw - kInfoW - 10;  // left edge of the text column
    const int mod_x = flip ? info_x + kInfoW + 12 : 14;  // the plane's area, beside the divider
    const int mod_w = flip ? l.cw - mod_x - 14 : info_x - 6 - 14;
    l.model = {mod_x, 38, mod_w, 128};
    const int mcx = mod_x + mod_w / 2;
    l.compass = {mod_x + 26, 192};
    l.heading = {mod_x + 48, 192};
    l.dots = {mcx, 216};
    l.info = {info_x, 34, kInfoW, 192};
    l.bars = {info_x + 10, 34};
    l.divider_vertical = true;
    l.divider_x = flip ? info_x + kInfoW + 6 : info_x;
    l.divider_y = 36;
    l.divider_len = 190;
    l.call = {info_x + 10, 40};
    l.type = {info_x + 10, 68};
    l.airline = {info_x + 10, 90};
    l.rule = {info_x + 10, 108};
    l.rule_w = 118;
    l.altitude = {info_x + 10, 112};
    l.speed = {info_x + 10, 150};
    l.distance = {info_x + 10, 188};
    l.header_right = {l.cw - 14, 11};
  }
  if (m == LayoutMode::Stacked) l.header_right = {306, 11};
  return l;
}

}  // namespace jf
