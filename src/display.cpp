#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

TFT_eSPI tft = TFT_eSPI();
static uint16_t scaled[SCREEN_W];

void display_init() {
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
    tft.init();
    tft.invertDisplay(true);
    // Some ST77xx displays expect swapped byte order (RGB/BGR). Enable
    // swap here to match palette byte-order when needed.
    tft.setSwapBytes(true);
    // Use rotation 2 so the USB connector is at the top in portrait mode.
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_PIN_BL, 0);
    ledcWrite(0, 255);
    Serial.printf("[TFT] %dx%d OK\n", tft.width(), tft.height());
}

void display_set_backlight(uint8_t level) { ledcWrite(0, level); }
void display_clear(uint16_t color) { tft.fillScreen(color); }

// Game scanline -> top 256px (1.5x horizontal, variable vertical scaling)
void display_push_gb_line(uint8_t y, uint16_t* buf) {
    if (y >= GB_SCREEN_H) return;
    // Scale 160 -> SCREEN_W (240) horizontally. We approximate 1.5x scaling
    // by duplicating every even pixel (pattern: 2,1,2,1...) to reach 240.
    int idx = 0;
    for (int x = 0; x < GB_SCREEN_W && idx < SCREEN_W; x++) {
        scaled[idx++] = buf[x];
        if ((x & 1) == 0 && idx < SCREEN_W) scaled[idx++] = buf[x];
    }
    while (idx < SCREEN_W) scaled[idx++] = buf[GB_SCREEN_W-1];
    int y0 = y * GAME_H / GB_SCREEN_H;
    int y1 = (y+1) * GAME_H / GB_SCREEN_H;
    if (y1 == y0) y1 = y0 + 1;

    for (int sy = y0; sy < y1 && sy < GAME_H; sy++)
        tft.pushImage(0, sy, SCREEN_W, 1, scaled);
}

// ─── Touch controls ─────────────────────────────────────────────────────────
void display_draw_controls() {
    tft.fillRect(0, CTRL_Y, SCREEN_W, CTRL_H, 0x1082);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, 0x2945);
    tft.fillRoundRect(7, 275, 62, 26, 5, 0x2945);
    tft.drawString("+", DPAD_CX, DPAD_CY, 4);
    tft.fillRoundRect(71, 274, 30, 29, 5, 0x2945);
    tft.drawString("SE", BTN_SE_X, BTN_SE_Y, 1);
    tft.fillRoundRect(109, 274, 30, 29, 5, 0x2945);
    tft.drawString("ST", BTN_ST_X, BTN_ST_Y, 1);
    tft.fillCircle(BTN_A_X, BTN_A_Y, BTN_A_R, 0xF800);
    tft.setTextColor(TFT_WHITE, 0xF800);
    tft.drawString("A", BTN_A_X, BTN_A_Y, 2);
    tft.fillCircle(BTN_B_X, BTN_B_Y, BTN_B_R, 0x001F);
    tft.setTextColor(TFT_WHITE, 0x001F);
    tft.drawString("B", BTN_B_X, BTN_B_Y, 2);
    display_draw_menu_icon();
}

void display_draw_menu_icon() {
    tft.fillCircle(BTN_M_X, BTN_M_Y, BTN_M_R, 0x528A);
    tft.setTextColor(TFT_WHITE, 0x528A);
    tft.drawString("II", BTN_M_X, BTN_M_Y, 1);
}

void display_draw_pixel_logo(int x, int y, int scale) {
    static const uint8_t pixels[8] = {
        0xFF, 0x81, 0xBD, 0xA5, 0xBD, 0x99, 0x99, 0xFF
    };
    tft.fillRect(x,y,8*scale,8*scale,0x1082);
    for (int row=0; row<8; ++row) {
        for (int col=0; col<8; ++col) {
            if (pixels[row] & (0x80 >> col))
                tft.fillRect(x+col*scale,y+row*scale,scale,scale,0xBFE0);
        }
    }
}

void display_show_boot_animation() {
    tft.fillScreen(0x0842);
    display_draw_pixel_logo(88,70,8);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE,0x0842);
    tft.drawString("CYD-GB",SCREEN_W/2,164,4);
    tft.setTextColor(0x7BEF,0x0842);
    tft.drawString("GAME BOY / BLE HUB",SCREEN_W/2,190,2);
    for (int i=0; i<8; ++i) {
        tft.fillRoundRect(32+i*23,230,17,6,2,0xBFE0);
        delay(55);
    }
    tft.setTextColor(0xBFE0,0x0842);
    tft.drawString("READY TO PLAY",SCREEN_W/2,262,2);
    delay(350);
}
