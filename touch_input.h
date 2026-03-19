#pragma once
#include <stdint.h>
#include <stdbool.h>

#define GB_BTN_RIGHT   0x01
#define GB_BTN_LEFT    0x02
#define GB_BTN_UP      0x04
#define GB_BTN_DOWN    0x08
#define GB_BTN_A       0x10
#define GB_BTN_B       0x20
#define GB_BTN_SELECT  0x40
#define GB_BTN_START   0x80
#define GB_BTN_MENU    0x100

struct TouchCalibration {
    int16_t x_min, x_max, y_min, y_max;
    bool swapped, invert_x, invert_y;
};

void touch_init();
void touch_update();
uint16_t touch_get_buttons();
bool touch_is_pressed();
int16_t touch_get_x();
int16_t touch_get_y();
void touch_set_calibration(TouchCalibration cal);
TouchCalibration touch_get_default_calibration();
void touch_run_calibration();

// Settings persistence (NVS)
void touch_save_settings(uint8_t palette, uint8_t fskip, uint8_t brightness);
bool touch_load_settings(uint8_t* palette, uint8_t* fskip, uint8_t* brightness);
