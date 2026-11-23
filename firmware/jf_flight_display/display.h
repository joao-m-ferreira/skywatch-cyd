// LovyanGFX configuration for the ESP32-2432S028 family and its bigger cousins. The pinout comes from
// board.h. The same 2.8" board is sold with different panels (ILI9341 on the single-USB board, ST7789
// on the two-USB one, some needing inverted colours), and the 3.5" boards use an ST7796 or ILI9488
// (320x480) with the backlight on another pin and the touch controller on the display's SPI bus, so
// all of that is chosen at start-up from the saved settings. The 3.5" setups are experimental.
#pragma once
#include "lgfx_setup.h"
#include <LovyanGFX.hpp>

#include "board.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ---- ESP32-S3: Waveshare ESP32-S3-Touch-AMOLED-1.8 (tested in the simulator only) ----
// V2 has a CO5300 panel and a CST820 touch controller, V1 an SH8601 and an FT3168 (panel 5).
// Power rails (AXP2101) and the reset expander (TCA9554) are left as the board powers up.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_CO5300 co5300_;
  lgfx::Panel_SH8601Z sh8601_;
  lgfx::Bus_SPI bus_;
  lgfx::Touch_CST816S cst820_;  // the CST820 is register-compatible
  lgfx::Touch_FT5x06 ft3168_;

  template <typename P>
  void setupPanel(P& panel) {
    auto c = panel.config();
    c.pin_cs = board::kLcdCs;
    c.pin_rst = -1;
    c.panel_width = board::kScreenW;
    c.panel_height = board::kScreenH;
    c.memory_width = board::kScreenW + board::kLcdColumnOffset;
    c.memory_height = board::kScreenH;
    c.offset_x = board::kLcdColumnOffset;
    c.offset_y = 0;
    c.readable = false;
    c.invert = false;
    c.rgb_order = false;
    panel.config(c);
    panel.setBus(&bus_);
  }
  template <typename T>
  void setupTouch(T& touch, int addr) {
    auto c = touch.config();
    c.x_min = 0;
    c.x_max = board::kScreenW - 1;
    c.y_min = 0;
    c.y_max = board::kScreenH - 1;
    c.pin_int = board::kTouchInt;
    c.pin_rst = -1;
    c.bus_shared = true;
    c.i2c_port = 0;
    c.i2c_addr = addr;
    c.pin_sda = board::kI2cSda;
    c.pin_scl = board::kI2cScl;
    c.freq = 400000;
    touch.config(c);
  }

 public:
  LGFX() {
    auto c = bus_.config();
    c.spi_host = SPI2_HOST;
    c.spi_mode = 0;
    c.freq_write = 40000000;
    c.freq_read = 16000000;
    c.pin_sclk = board::kLcdSclk;
    c.pin_io0 = board::kLcdIo0;
    c.pin_io1 = board::kLcdIo1;
    c.pin_io2 = board::kLcdIo2;
    c.pin_io3 = board::kLcdIo3;
    c.spi_3wire = false;
    bus_.config(c);
    useVariant(4, false, false, 0, false, 0, 0);
  }

  // panel 5 selects the V1 board (SH8601 + FT3168); anything else the V2 (CO5300 + CST820).
  // Inversion, red/blue order, backlight pin and touch bus do not apply to this board.
  void useVariant(int panel, bool invert, bool rgb_swap, int, bool, int, int) {
    if (panel == 5) {
      setupPanel(sh8601_);
      auto pc = sh8601_.config();
      pc.invert = invert;
      pc.rgb_order = rgb_swap;
      sh8601_.config(pc);
      setupTouch(ft3168_, board::kTouchAddrV1);
      sh8601_.setTouch(&ft3168_);
      setPanel(&sh8601_);
    } else {
      setupPanel(co5300_);
      auto pc = co5300_.config();
      pc.invert = invert;
      pc.rgb_order = rgb_swap;
      co5300_.config(pc);
      setupTouch(cst820_, board::kTouchAddrV2);
      co5300_.setTouch(&cst820_);
      setPanel(&co5300_);
    }
  }
};
#else
// ---- classic ESP32: the CYD family ----
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 ili9341_;
  lgfx::Panel_ST7789 st7789_;
  lgfx::Panel_ST7796 st7796_;
  lgfx::Panel_ILI9488 ili9488_;
  lgfx::Panel_SSD1306 ssd1306_;  // 0.96" OLED, 128x64, 1 bit, text only
  lgfx::Bus_I2C i2c_;
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
    useVariant(0, false, false, board::kTftBacklight, false, 21, 22);
  }

  // Selects the panel (0 ILI9341, 1 ST7789, 2 ST7796, 3 ILI9488), colour inversion, red/blue order,
  // the backlight pin and where the touch controller sits. Call before init().
  void useVariant(int panel, bool invert, bool rgb_swap, int bl_pin, bool touch_shared, int sda, int scl) {
    if (panel == 6) {  // 0.96" SSD1306 OLED on I2C: no backlight pin, no touch
      auto bc = i2c_.config();
      bc.i2c_port = 0;
      bc.i2c_addr = 0x3C;
      bc.freq_write = 400000;
      bc.freq_read = 400000;
      bc.pin_sda = sda;
      bc.pin_scl = scl;
      i2c_.config(bc);
      auto pc = ssd1306_.config();
      pc.pin_cs = -1;
      pc.pin_rst = -1;
      pc.panel_width = 128;
      pc.panel_height = 64;
      pc.memory_width = 128;
      pc.memory_height = 64;
      pc.offset_x = 0;
      pc.offset_y = 0;
      pc.invert = invert;
      pc.readable = false;
      ssd1306_.config(pc);
      ssd1306_.setBus(&i2c_);
      setPanel(&ssd1306_);
      return;
    }
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
#endif
