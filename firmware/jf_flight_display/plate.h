// "Plate" screen: one aircraft on a paper-coloured, ruled plate with its particulars.
#pragma once
#include <LovyanGFX.hpp>

#include "flights.h"

// index/count place the plane in the list (dots); header_left/right are small texts in
// the header line (e.g. "8 aircraft within 40 km" / "2/8  12s ago").
void drawPlate(lgfx::LovyanGFX& gfx, const jf::Flight& f, int index, int count,
               const char* header_left, const char* header_right);
// Message plate when there is nothing to show (error=true draws it in red).
void drawPlateMessage(lgfx::LovyanGFX& gfx, const char* msg, const char* detail,
                      const char* header_left, const char* header_right, bool error);
