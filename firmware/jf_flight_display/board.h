// Single source of truth for the hardware pinout. Two boards are supported:
//   * classic ESP32: the ESP32-2432S028 "CYD" family (and bigger 3.5" cousins, experimental)
//   * ESP32-S3: the Waveshare ESP32-S3-Touch-AMOLED-1.8 (V2: CO5300 + CST820; V1: SH8601 + FT3168).
//     TESTED IN THE SIMULATOR ONLY (esp32sim), never on the real board.
// Plain constants only: no Arduino headers, so host tests may include it. CONFIG_IDF_TARGET_ESP32S3
// comes from the build (it is not defined for the host tests, which use the classic branch).
#pragma once
#include <stdint.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
namespace board {
constexpr bool kIsAmoled = true;
constexpr int kScreenW = 368;  // native (portrait) size
constexpr int kScreenH = 448;

// QSPI AMOLED (CO5300 on V2, SH8601 on V1).
constexpr int kLcdSclk = 11;
constexpr int kLcdIo0 = 4;
constexpr int kLcdIo1 = 5;
constexpr int kLcdIo2 = 6;
constexpr int kLcdIo3 = 7;
constexpr int kLcdCs = 12;
constexpr int kLcdTe = 13;  // tear signal, input
constexpr int kLcdColumnOffset = 16;  // CO5300: the glass starts at RAM column 16

// I2C0: touch (CST820 at 0x15 on V2, FT3168 at 0x38 on V1), TCA9554 at 0x20, AXP2101 at 0x34, RTC, IMU.
constexpr int kI2cSda = 15;
constexpr int kI2cScl = 14;
constexpr int kTouchInt = 21;
constexpr int kTouchAddrV2 = 0x15;
constexpr int kTouchAddrV1 = 0x38;

// No RGB LED and no light sensor on this board.
constexpr bool kHasLed = false;
}  // namespace board
#else
namespace board {
constexpr bool kIsAmoled = false;
constexpr bool kHasLed = true;
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
#endif
