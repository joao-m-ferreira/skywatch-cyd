// Single source of truth for the ESP32-2432S028 ("CYD") hardware pinout.
// Plain constants only: no Arduino headers, so host tests may include it.
#pragma once
#include <stdint.h>

namespace board {
constexpr int kScreenW = 320;
constexpr int kScreenH = 240;

// ILI9341 on HSPI.
constexpr int kTftSck = 14;
constexpr int kTftMosi = 13;
constexpr int kTftMiso = 12;
constexpr int kTftDc = 2;
constexpr int kTftCs = 15;
constexpr int kTftBacklight = 21;  // active high

// XPT2046 touch on its own bus.
constexpr int kTouchSck = 25;
constexpr int kTouchMosi = 32;
constexpr int kTouchMiso = 39;
constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;

constexpr int kLdr = 34;  // ADC1, light sensor
constexpr int kLedR = 4;  // active low
constexpr int kLedG = 16;
constexpr int kLedB = 17;
}  // namespace board
