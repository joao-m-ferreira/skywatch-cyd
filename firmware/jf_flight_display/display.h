// LovyanGFX configuration for the ESP32-2432S028. Pins come from board.h.
#pragma once
#include <LovyanGFX.hpp>

#include "board.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

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
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = board::kTftCs;
      c.pin_rst = -1;
      c.panel_width = 240;
      c.panel_height = 320;
      c.invert = false;
      c.rgb_order = false;
      panel_.config(c);
    }
    {
      auto c = light_.config();
      c.pin_bl = board::kTftBacklight;
      c.invert = false;
      c.freq = 12000;
      c.pwm_channel = 7;
      light_.config(c);
      panel_.setLight(&light_);
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
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};
