#include "button_input.h"
#include "touch_input.h"
#include "hw_config.h"
#include <Arduino.h>
#include <Wire.h>

static volatile uint16_t cur_btns = 0;
static bool button_board_present = false;

void button_init() {
    Wire.begin(BUTTON_I2C_SDA, BUTTON_I2C_SCL);
    Wire.setClock(100000);
    Wire.beginTransmission(BUTTON_I2C_ADDR);
    button_board_present = Wire.endTransmission() == 0;
    Serial.printf("[BUTTON] PCF8574 %s\n", button_board_present ? "ready" : "not detected; touch only");
}

static uint16_t read_pcf_buttons() {
    Wire.requestFrom((uint8_t)BUTTON_I2C_ADDR, (uint8_t)1);
    if (Wire.available() < 1) return 0;
    uint8_t raw = Wire.read();
    raw = ~raw;  // PCF8574 inputs are pulled high; pressed is low.

    uint16_t buttons = 0;
    if (raw & (1 << 0)) buttons |= GB_BTN_UP;
    if (raw & (1 << 1)) buttons |= GB_BTN_DOWN;
    if (raw & (1 << 2)) buttons |= GB_BTN_LEFT;
    if (raw & (1 << 3)) buttons |= GB_BTN_RIGHT;
    if (raw & (1 << 4)) buttons |= GB_BTN_A;
    if (raw & (1 << 5)) buttons |= GB_BTN_B;
    if (raw & (1 << 6)) buttons |= GB_BTN_START;
    if (raw & (1 << 7)) buttons |= GB_BTN_SELECT;
    return buttons;
}

void button_update() {
    cur_btns = button_board_present ? read_pcf_buttons() : 0;
}

uint16_t button_get_buttons() {
    return cur_btns;
}
