// LovyanGFX configuration for the ESP32-2432S028 family. Pins come from board.h. The same pinout is
// sold with different panels (ILI9341 on the single-USB board, ST7789 on the two-USB one, some of
// them needing inverted colours), so the panel is chosen at start-up from the saved settings.
#pragma once
#include <LovyanGFX.hpp>

#include "board.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 ili9341_;
  lgfx::Panel_ST7789 st7789_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

  template <typename P>
  void setupPanel(P& panel, bool invert, bool rgb_swap) {
    auto c = panel.config();
    c.pin_cs = board::kTftCs;
    c.pin_rst = -1;
    c.panel_width = 240;
    c.panel_height = 320;
    c.invert = invert;
    c.rgb_order = rgb_swap;
    panel.config(c);
    panel.setBus(&bus_);
    panel.setLight(&light_);
    panel.setTouch(&touch_);
  }

 public:
  LGFX() {
    {
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
    }
    {
      auto c = light_.config();
      c.pin_bl = board::kTftBacklight;
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
      c.bus_shared = false;
      c.spi_host = SPI3_HOST;
      c.freq = 1000000;
      c.pin_sclk = board::kTouchSck;
      c.pin_mosi = board::kTouchMosi;
      c.pin_miso = board::kTouchMiso;
      c.pin_cs = board::kTouchCs;
      touch_.config(c);
    }
    useVariant(0, false, false);
  }

  // Selects the panel (0 = ILI9341, 1 = ST7789), colour inversion and red/blue order. Call before init().
  void useVariant(int panel, bool invert, bool rgb_swap) {
    if (panel == 1) {
      setupPanel(st7789_, invert, rgb_swap);
      setPanel(&st7789_);
    } else {
      setupPanel(ili9341_, invert, rgb_swap);
      setPanel(&ili9341_);
    }
  }
};
