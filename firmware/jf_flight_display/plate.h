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
               int layout_mode = 0);  // jf::LayoutMode: 0 auto, 1 side, 2 side-flipped, 3 stacked
// Message plate when there is nothing to show (error=true draws it in red).
void drawPlateMessage(lgfx::LovyanGFX& gfx, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error, int layout_mode = 0);
