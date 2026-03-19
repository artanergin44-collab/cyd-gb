#pragma once
#include <TFT_eSPI.h>
extern TFT_eSPI tft;

void display_init();
void display_set_backlight(uint8_t level);
void display_clear(uint16_t color = TFT_BLACK);
void display_push_gb_line(uint8_t line_y, uint16_t* buf160);
void display_draw_controls();
