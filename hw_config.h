#pragma once
#include <stdint.h>

// ─── Pins ───────────────────────────────────────────────────────────────────
#define TFT_PIN_BL     21
#define SCREEN_W       320
#define SCREEN_H       240

#define TOUCH_PIN_CS    33
#define TOUCH_PIN_IRQ   36
#define TOUCH_PIN_MOSI  32
#define TOUCH_PIN_MISO  39
#define TOUCH_PIN_CLK   25

#define SD_PIN_CS 5
#define SD_PIN_MOSI 23
#define SD_PIN_MISO 19
#define SD_PIN_SCK 18

#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

// ─── GameBoy ────────────────────────────────────────────────────────────────
#define GB_SCREEN_W 160
#define GB_SCREEN_H 144

// Game area: top 192px. Control bar: bottom 48px (y=192..240)
#define GAME_H 192
#define CTRL_Y 192
#define CTRL_H 48

// ─── Touch Zones (y=192..240 control bar) ───────────────────────────────────
// D-pad left
#define DPAD_CX    50
#define DPAD_CY   216
#define DPAD_R     22

// A = right-upper, B = right-lower (FIXED - was swapped)
#define BTN_A_X   285
#define BTN_A_Y   206
#define BTN_A_R    20

#define BTN_B_X   245
#define BTN_B_Y   226
#define BTN_B_R    20

// Start / Select center
#define BTN_ST_X  165
#define BTN_ST_Y  216
#define BTN_ST_W   38
#define BTN_ST_H   20

#define BTN_SE_X  115
#define BTN_SE_Y  216
#define BTN_SE_W   38
#define BTN_SE_H   20

// Menu top-right
#define BTN_M_X   305
#define BTN_M_Y    12
#define BTN_M_R    14
