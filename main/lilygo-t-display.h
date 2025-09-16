#pragma once
#include "esp_err.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "lvgl.h"


#ifdef __cplusplus
extern "C" {
#endif

void display_init();
bool lvgl_lock(int timeout_ms);
void lvgl_unlock();
void set_backlight_brightness(uint16_t brightness);
void show_message(const char *message, bool is_error);
void show_time(int stunde, int minute);

#ifdef __cplusplus
}
#endif












