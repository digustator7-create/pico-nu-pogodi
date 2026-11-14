#include "system.h"
#include "../cpu/core.h"
#include "graphic.h"
#include "ili9341.h"
#include "input.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "rom.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sound.h"

#include "../data/o.h"

extern volatile bool emulate;
extern volatile bool draw;
extern volatile uint8 *buffer_start;

int button;
#define BACKLIGHT_PIN 14
#define SCREEN_TIMEOUT_MS 30000
uint32_t last_activity_ms = 0;

void writeR(u8 data) {
  if (data & 0x01) {
    *(current_buff++) = 255;
  } else {
    *(current_buff++) = 0;
  }
};

u32 get_buttons() { return button; }
u8 readB() { return B; }
u8 readBA() { return BA; }
u8 readK(u8 io_S) {
  if ((button == 5) && (io_S == 4)) {
    return 8;
  } else if ((button == 6) && (io_S == 4)) {
    return 4;
  } else if ((button == 7) && (io_S == 4)) {
    return 2;
  } else if ((button == 8) && (io_S == 4)) {
    return 1;
  } else if ((button == 1) && (io_S == 8)) {
    return 8;
  } else if ((button == 2) && (io_S == 8)) {
    return 4;
  } else if ((button == 3) && (io_S == 8)) {
    return 2;
  } else if ((button == 4) && (io_S == 8)) {
    return 1;
  } else {
    return 0;
  }
}

uint16_t fb[GW_LCD_WIDTH * GW_LCD_HEIGHT];

void render() {
  uint32_t last_bat_check = 0;
  bool low_battery = false;

  while (true) {
    if (draw) {
      uint32_t now = to_ms_since_boot(get_absolute_time());
      if (now - last_bat_check > 5000 || last_bat_check == 0) {
        last_bat_check = now;
        uint16_t raw = adc_read();
        // Делитель на плате Pico 1:3, опорное 3.3В, 12 бит (4095)
        float vsys = raw * 3.3f * 3.0f / 4095.0f;
        low_battery = (vsys < 3.3f && vsys > 1.5f);
      }

      render_to_framebuffer(fb);

      if (low_battery) {
        uint16_t red = 0xF800; // Красный цвет RGB565
        // Корпус батарейки (рамка 20x10 пикселей в левом верхнем углу)
        for (int x = 6; x < 26; x++) {
          fb[6 * GW_LCD_WIDTH + x] = red;
          fb[15 * GW_LCD_WIDTH + x] = red;
        }
        for (int y = 6; y <= 15; y++) {
          fb[y * GW_LCD_WIDTH + 6] = red;
          fb[y * GW_LCD_WIDTH + 25] = red;
        }
        // Плюсовой контакт батарейки справа
        for (int y = 9; y <= 12; y++) {
          fb[y * GW_LCD_WIDTH + 26] = red;
          fb[y * GW_LCD_WIDTH + 27] = red;
        }
        // Красное деление внутри (остаток заряда)
        for (int y = 8; y <= 13; y++) {
          for (int x = 8; x <= 11; x++) {
            fb[y * GW_LCD_WIDTH + x] = red;
          }
        }
      }

      LCD_WriteBitmap(0, 0, GW_LCD_WIDTH, GW_LCD_HEIGHT, fb);
      draw = false;
    }
  }
}

int system_run(int clock_cycles) {
  m_k_active = (get_buttons() != 0);
  m_icount += (clock_cycles / 2);

  current_buff = buffer_start;
  device_run();
  return m_icount * m_clk_div;
}
void mainloop() {

  device_start();
  device_reset();

  for (int i = 6; i < 14; i++) {
    gpio_init(i);
  }

  for (int i = 6; i < 8; i++) {
    gpio_set_dir(i, GPIO_OUT);
  }

  gpio_set_dir(8, GPIO_IN);
  gpio_pull_down(8);

  gpio_set_dir(9, GPIO_IN);
  gpio_pull_down(9);

  for (int i = 10; i < 14; i++) {
    gpio_set_dir(i, GPIO_IN);
    gpio_pull_down(i);
  }
    gpio_init(BACKLIGHT_PIN);
    gpio_set_dir(BACKLIGHT_PIN, GPIO_OUT);
    gpio_put(BACKLIGHT_PIN, 1);
    last_activity_ms = to_ms_since_boot(get_absolute_time());

  adc_init();
    adc_gpio_init(29);
    adc_select_input(3);
  
  multicore_launch_core1(render);
  init_sound();

 while ( 1) {
        if ( emulate) {
            emulate = false;
            button = process_input();
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (button != 0) {
                last_activity_ms = now;
                gpio_put(BACKLIGHT_PIN, 1);
            } else if (now - last_activity_ms > SCREEN_TIMEOUT_MS) {
                gpio_put(BACKLIGHT_PIN, 0);
            }
            system_run( SYS_FREQ / 128);
           }
        }
    }
int init(int argc, char **argv) {
  u8 *rom = NULL;
  bool romflg = false;
  int size = 0;

  size = 179348;
  romflg = romloader(o_data, size);

  if (!romflg)
    return 1;
  mainloop();

  return 0;
}
