// Must be included before <LovyanGFX.hpp> everywhere: the ESP32-S3 board's AMOLED is driven over
// quad SPI, which LovyanGFX only compiles in when LGFX_USE_QSPI is defined, and the definition has to
// be the same in every file (the library's classes change shape with it).
#pragma once
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(LGFX_USE_QSPI)
#define LGFX_USE_QSPI
#endif
