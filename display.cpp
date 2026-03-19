#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

TFT_eSPI tft = TFT_eSPI();
static uint16_t scaled[320];

void display_init() {
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_PIN_BL, 0);
    ledcWrite(0, 255);
    Serial.printf("[TFT] %dx%d OK\n", tft.width(), tft.height());
}

void display_set_backlight(uint8_t level) { ledcWrite(0, level); }
void display_clear(uint16_t color) { tft.fillScreen(color); }

// Game scanline -> top 192px (2x horiz, ~1.33x vert)
void display_push_gb_line(uint8_t y, uint16_t* buf) {
    if (y >= GB_SCREEN_H) return;
    for (int x = 0; x < 160; x++) {
        scaled[x*2] = scaled[x*2+1] = buf[x];
    }
    int y0 = y * GAME_H / GB_SCREEN_H;
    int y1 = (y+1) * GAME_H / GB_SCREEN_H;
    if (y1 == y0) y1 = y0 + 1;

    // No setSwapBytes - palette values are pre-swapped in emulator_bridge
    for (int sy = y0; sy < y1 && sy < GAME_H; sy++)
        tft.pushImage(0, sy, 320, 1, scaled);
}

// ─── Control bar (y=192..240) ───────────────────────────────────────────────
void display_draw_controls() {
    tft.fillRect(0, CTRL_Y, 320, CTRL_H, 0x18C3);
    tft.drawFastHLine(0, CTRL_Y, 320, 0x528A);

    // D-pad
    int cx = DPAD_CX, cy = DPAD_CY;
    tft.fillRoundRect(cx-10, cy-22, 20, 44, 3, 0x4A69);
    tft.fillRoundRect(cx-22, cy-10, 44, 20, 3, 0x4A69);
    tft.fillCircle(cx, cy, 3, 0x2104);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, 0x4A69);
    tft.drawString("^", cx, cy-14, 1);
    tft.drawString("v", cx, cy+14, 1);
    tft.drawString("<", cx-14, cy, 1);
    tft.drawString(">", cx+14, cy, 1);

    // A (red, upper-right)
    tft.fillCircle(BTN_A_X, BTN_A_Y, BTN_A_R, 0xC000);
    tft.setTextColor(TFT_WHITE, 0xC000);
    tft.drawString("A", BTN_A_X, BTN_A_Y, 2);

    // B (blue, lower-right)
    tft.fillCircle(BTN_B_X, BTN_B_Y, BTN_B_R, 0x0018);
    tft.setTextColor(TFT_WHITE, 0x0018);
    tft.drawString("B", BTN_B_X, BTN_B_Y, 2);

    // START
    tft.fillRoundRect(BTN_ST_X - BTN_ST_W/2, BTN_ST_Y - BTN_ST_H/2, BTN_ST_W, BTN_ST_H, 3, 0x528A);
    tft.setTextColor(TFT_WHITE, 0x528A);
    tft.drawString("STA", BTN_ST_X, BTN_ST_Y, 1);

    // SELECT
    tft.fillRoundRect(BTN_SE_X - BTN_SE_W/2, BTN_SE_Y - BTN_SE_H/2, BTN_SE_W, BTN_SE_H, 3, 0x528A);
    tft.setTextColor(TFT_WHITE, 0x528A);
    tft.drawString("SEL", BTN_SE_X, BTN_SE_Y, 1);

    // MENU (top-right)
    tft.fillCircle(BTN_M_X, BTN_M_Y, BTN_M_R, 0x7BE0);
    tft.setTextColor(TFT_BLACK, 0x7BE0);
    tft.drawString("||", BTN_M_X, BTN_M_Y, 2);
}
