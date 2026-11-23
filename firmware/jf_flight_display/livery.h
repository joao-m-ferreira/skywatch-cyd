// Livery design codes shared by the airline table and the 3D model. No Arduino headers.
#pragma once
#include <stdint.h>

namespace jf {

// What is painted on the vertical tail (motif colour = the airline's secondary colour,
// or white when Livery::white_motif is set).
enum Tail : uint8_t {
  kTailBand = 0,      // primary fin with a motif-coloured band at the bottom (default)
  kTailDisc = 1,      // round emblem
  kTailDiagonal = 2,  // diagonal band
  kTailStripes = 3,   // horizontal stripes
  kTailSplit = 4,     // two vertical halves
  kTailSolid = 5,     // plain
  kTailCross = 6,     // cross
  kTailChecker = 7,   // checkerboard (Volotea, Azul mosaic)
  kTailDiagSplit = 8, // everything behind a diagonal line in the motif colour
  kTailTap = 9,       // TAP: red fin, green wedge on the leading edge, white flash, forward logo block
  kTailFlag = 10      // Emirates: white fin with the UAE flag stripes sweeping up to the top
};

// How the fuselage is painted.
enum Body : uint8_t {
  kBodyCheat = 0,  // white with a cheatline in the primary colour
  kBodyBelly = 1,  // primary colour over the lower half
  kBodyFull = 2,   // primary colour all over
  kBodyPlain = 3,  // all white, no cheatline
  kBodyEasy = 4,   // easyJet: orange crown on the forward fuselage, orange rear fuselage
  kBodyTitle = 5,  // white with the airline name as a primary-colour band on the forward fuselage
  kBodyMaersk = 6, // white; primary-colour lower fuselage widening to the tail; name in the secondary colour
  kBodyEmirates = 7 // white; name in the secondary colour (gold); red/black streak on the rear fuselage
};

enum FinBase : uint8_t { kFinPrimary = 0, kFinWhite = 1, kFinSecondary = 2 };
// Fuselage paint: most airlines are white, a few are tinted.
enum HullTint : uint8_t { kHullWhite = 0, kHullPearl = 1, kHullChampagne = 2, kHullSky = 3 };

struct Livery {
  uint8_t tail;         // Tail
  uint8_t body;         // Body
  uint8_t engines;      // 0 = grey/white, 1 = primary colour, 2 = secondary colour
  uint8_t white_motif;  // 1 = tail motif is white instead of the secondary colour
  uint8_t fin_base;     // FinBase: colour of the fin behind the motif (motif = primary when not primary)
  uint8_t hull;         // HullTint
  uint8_t nose_red;     // 1 = nose cone in the primary colour (Norwegian)
};

}  // namespace jf
