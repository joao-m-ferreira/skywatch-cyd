// "Plate" screen: one aircraft on a paper-coloured, ruled plate with its particulars.
#pragma once
#include <LovyanGFX.hpp>

#include "flights.h"

// index/count place the plane in the list (1-based on screen); status_note is shown
// small in the header (e.g. "LIVE").
void drawPlate(lgfx::LovyanGFX& gfx, const jf::Flight& f, int index, int count,
               const char* status_note);
// Message plate when there is nothing to show (error=true draws it in red).
void drawPlateMessage(lgfx::LovyanGFX& gfx, const char* msg, const char* detail,
                      const char* status_note, bool error);
