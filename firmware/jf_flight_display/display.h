// LovyanGFX configuration for the ESP32-2432S028 family and its bigger cousins. The pinout comes from
// board.h. The same 2.8" board is sold with different panels (ILI9341 on the single-USB board, ST7789
// on the two-USB one, some needing inverted colours), and the 3.5" boards use an ST7796 or ILI9488
// (320x480) with the backlight on another pin and the touch controller on the display's SPI bus, so
// all of that is chosen at start-up from the saved settings. The 3.5" setups are experimental.
#pragma once
#include <LovyanGFX.hpp>

#include "board.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 ili9341_;
  lgfx::Panel_ST7789 st7789_;
  lgfx::Panel_ST7796 st7796_;
  lgfx::Panel_ILI9488 ili9488_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

  template <typename P>
  void setupPanel(P& panel, int width, int height, bool invert, bool rgb_swap) {
    auto c = panel.config();
    c.pin_cs = board::kTftCs;
    c.pin_rst = -1;
    c.panel_width = width;
    c.panel_height = height;
    c.invert = invert;
    c.rgb_order = rgb_swap;
    panel.config(c);
    panel.setBus(&bus_);
    panel.setLight(&light_);
    panel.setTouch(&touch_);
  }

 public:
  LGFX() {
    auto c = bus_.config();
    c.spi_host = SPI2_HOST;
    c.spi_mode = 0;
    c.freq_write = 40000000;
    c.freq_read = 16000000;
    c.pin_sclk = board::kTftSck;
    c.pin_mosi = board::kTftMosi;
    c.pin_miso = board::kTftMiso;
    c.pin_dc = board::kTftDc;
    bus_.config(c);
    useVariant(0, false, false, board::kTftBacklight, false);
  }

  // Selects the panel (0 ILI9341, 1 ST7789, 2 ST7796, 3 ILI9488), colour inversion, red/blue order,
  // the backlight pin and where the touch controller sits. Call before init().
  void useVariant(int panel, bool invert, bool rgb_swap, int bl_pin, bool touch_shared) {
    {
      auto c = light_.config();
      c.pin_bl = bl_pin;
      c.invert = false;
      c.freq = 12000;
      c.pwm_channel = 7;
      light_.config(c);
    }
    {
      auto c = touch_.config();
      c.x_min = 300;
      c.x_max = 3900;
      c.y_min = 3700;
      c.y_max = 200;
      c.pin_int = board::kTouchIrq;
      c.pin_cs = board::kTouchCs;
      c.freq = 1000000;
      if (touch_shared) {  // 3.5" boards: the touch chip shares the display's SPI bus
        c.bus_shared = true;
        c.spi_host = SPI2_HOST;
        c.pin_sclk = board::kTftSck;
        c.pin_mosi = board::kTftMosi;
        c.pin_miso = board::kTftMiso;
      } else {  // 2.8" boards: its own bus
        c.bus_shared = false;
        c.spi_host = SPI3_HOST;
        c.pin_sclk = board::kTouchSck;
        c.pin_mosi = board::kTouchMosi;
        c.pin_miso = board::kTouchMiso;
      }
      touch_.config(c);
    }
    switch (panel) {
      case 1:
        setupPanel(st7789_, 240, 320, invert, rgb_swap);
        setPanel(&st7789_);
        break;
      case 2:
        setupPanel(st7796_, 320, 480, invert, rgb_swap);
        setPanel(&st7796_);
        break;
      case 3:
        setupPanel(ili9488_, 320, 480, invert, rgb_swap);
        setPanel(&ili9488_);
        break;
      default:
        setupPanel(ili9341_, 240, 320, invert, rgb_swap);
        setPanel(&ili9341_);
        break;
    }
  }
};
