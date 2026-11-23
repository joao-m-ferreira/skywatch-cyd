// "Plate" screen: one aircraft on a paper-coloured, ruled plate with its particulars.
#pragma once
#include "lgfx_setup.h"
#include <LovyanGFX.hpp>

#include "flights.h"
#include "photos.h"

// photo (optional) replaces the 3D model with a photo of the actual aircraft.
// index/count place the plane in the list (dots); header_left/right are small texts in
// the header line (e.g. "8 aircraft within 40 km" / "2/8  12s ago").
void drawPlate(lgfx::LovyanGFX& gfx, const jf::Flight& f, int index, int count,
               const char* header_left, const char* header_right, const jf::Photo* photo,
               bool lite = false,  // lite: no photo, no 3D model (safe mode)
               int layout_mode = 0,  // jf::LayoutMode: 0 auto, 1 side, 2 side-flipped, 3 stacked
               int page = 0,  // compact (short) screens show one field at a time: 0 distance, 1 altitude, 2 speed, 3 route
               const char* route = nullptr);  // the route text for page 3 (nullptr when unknown: three pages)
// Compact screens only: repaints just the rotating data field (no clear, no 3D model, no photo).
// Returns false, drawing nothing, on a layout that has no such field.
bool drawPlateField(lgfx::LovyanGFX& gfx, const jf::Flight& f, int page, int layout_mode = 0, const char* route = nullptr);
// Message plate when there is nothing to show (error=true draws it in red).
void drawPlateMessage(lgfx::LovyanGFX& gfx, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error, int layout_mode = 0);

// Setup mode screen: the board's own WiFi (QR code, name, one-time password) and the settings page address.
// `detail` (may be empty) is a short note such as "Could not join MyWiFi".
void drawSetupScreen(lgfx::LovyanGFX& gfx, const char* ap, const char* pass, const char* url, const char* detail,
                     int layout_mode = 0);
void drawSetupOled(lgfx::LovyanGFX& gfx, const char* ap, const char* pass, const char* url, const char* detail);

// Text-only screens for a 128x64 monochrome OLED (no 3D model, no photo, no compass).
// `route`, when given, takes the place of the airline row (the caller alternates them).
void drawPlateOled(lgfx::LovyanGFX& gfx, const jf::Flight& f, const char* route = nullptr);
void drawOledMessage(lgfx::LovyanGFX& gfx, const char* msg, const char* detail);
