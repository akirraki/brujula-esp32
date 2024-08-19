#ifndef ILI9341_LVGL_SOLUTION_H
#define ILI9341_LVGL_SOLUTION_H

#include <stdio.h>
#include "lvgl.h"
// Using SPI2
#define LCD_HOST SPI2_HOST
// LCD Config defines
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK 18
#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 19
#define PIN_NUM_LCD_DC 4
#define PIN_NUM_LCD_RST 0 // esta conectado al 0 pero en la placa anterior andaba igual. que onda?
#define PIN_NUM_LCD_CS 5
#define PIN_NUM_BK_LIGHT 3
#define PIN_NUM_TOUCH_CS 15
// The pixel number in horizontal and vertical
#define LCD_H_RES 240
#define LCD_V_RES 320
// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE (4 * 1024)
#define LVGL_TASK_PRIORITY 2

lv_disp_t *setup_display(void);
bool lvgl_lock(int timeout_ms);
void lvgl_unlock(void);

#endif