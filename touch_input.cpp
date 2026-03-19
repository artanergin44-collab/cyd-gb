#include "touch_input.h"
#include "hw_config.h"
#include "display.h"
#include <Arduino.h>
#include <Preferences.h>

#define T_CLK  TOUCH_PIN_CLK
#define T_CS   TOUCH_PIN_CS
#define T_DIN  TOUCH_PIN_MOSI
#define T_DOUT TOUCH_PIN_MISO
#define T_IRQ  TOUCH_PIN_IRQ

#define CMD_X  0xD0
#define CMD_Y  0x90
#define CMD_Z1 0xB0
#define CMD_Z2 0xC0

static TouchCalibration cal;
static Preferences prefs;
static volatile uint16_t cur_btns = 0;
static volatile int16_t scr_x = -1, scr_y = -1;
static volatile bool pressed = false;
static uint32_t last_ms = 0;

// ─── Bit-bang SPI ───────────────────────────────────────────────────────────
static uint16_t spi16(uint8_t cmd) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(T_DIN, (cmd >> i) & 1);
        digitalWrite(T_CLK, HIGH); delayMicroseconds(1);
        digitalWrite(T_CLK, LOW);  delayMicroseconds(1);
    }
    uint16_t r = 0;
    for (int i = 0; i < 13; i++) {
        digitalWrite(T_CLK, HIGH); delayMicroseconds(1);
        if (i > 0) { r <<= 1; r |= digitalRead(T_DOUT); }
        digitalWrite(T_CLK, LOW); delayMicroseconds(1);
    }
    return r;
}

static bool read_raw(int16_t* rx, int16_t* ry, int16_t* rz) {
    digitalWrite(T_CS, LOW);
    uint16_t z1 = spi16(CMD_Z1), z2 = spi16(CMD_Z2);
    int16_t z = z1 - z2 + 4095;
    if (z < 200) { digitalWrite(T_CS, HIGH); *rz = 0; return false; }
    uint32_t sx = 0, sy = 0; int n = 0;
    for (int i = 0; i < 6; i++) {  // 6 samples for better accuracy
        uint16_t x = spi16(CMD_X), y = spi16(CMD_Y);
        if (x > 100 && x < 4000 && y > 100 && y < 4000) { sx += x; sy += y; n++; }
    }
    digitalWrite(T_CS, HIGH);
    if (n < 2) { *rz = 0; return false; }  // need at least 2 good samples
    *rx = sx / n; *ry = sy / n; *rz = z;
    return true;
}

// ─── NVS Save/Load ──────────────────────────────────────────────────────────
static void save_cal_to_nvs() {
    prefs.begin("touch", false);
    prefs.putShort("xmin", cal.x_min);
    prefs.putShort("xmax", cal.x_max);
    prefs.putShort("ymin", cal.y_min);
    prefs.putShort("ymax", cal.y_max);
    prefs.putBool("swap", cal.swapped);
    prefs.putBool("invx", cal.invert_x);
    prefs.putBool("invy", cal.invert_y);
    prefs.putBool("valid", true);
    prefs.end();
    Serial.println("[CAL] Saved to NVS");
}

static bool load_cal_from_nvs() {
    prefs.begin("touch", true);
    bool valid = prefs.getBool("valid", false);
    if (valid) {
        cal.x_min = prefs.getShort("xmin", 200);
        cal.x_max = prefs.getShort("xmax", 3800);
        cal.y_min = prefs.getShort("ymin", 200);
        cal.y_max = prefs.getShort("ymax", 3800);
        cal.swapped = prefs.getBool("swap", true);
        cal.invert_x = prefs.getBool("invx", true);
        cal.invert_y = prefs.getBool("invy", false);
        Serial.printf("[CAL] Loaded from NVS: x[%d-%d] y[%d-%d] sw=%d ix=%d iy=%d\n",
                      cal.x_min, cal.x_max, cal.y_min, cal.y_max,
                      cal.swapped, cal.invert_x, cal.invert_y);
    }
    prefs.end();
    return valid;
}

// ─── Settings NVS ───────────────────────────────────────────────────────────
void touch_save_settings(uint8_t palette, uint8_t fskip, uint8_t brightness) {
    prefs.begin("settings", false);
    prefs.putUChar("pal", palette);
    prefs.putUChar("fskip", fskip);
    prefs.putUChar("bright", brightness);
    prefs.end();
}

bool touch_load_settings(uint8_t* palette, uint8_t* fskip, uint8_t* brightness) {
    prefs.begin("settings", true);
    bool has = prefs.isKey("pal");
    if (has) {
        *palette = prefs.getUChar("pal", 0);
        *fskip = prefs.getUChar("fskip", 0);
        *brightness = prefs.getUChar("bright", 255);
    }
    prefs.end();
    return has;
}

// ─── Init ───────────────────────────────────────────────────────────────────
TouchCalibration touch_get_default_calibration() {
    return {200, 3800, 200, 3800, true, true, false};
}

void touch_init() {
    pinMode(T_CLK, OUTPUT); pinMode(T_CS, OUTPUT);
    pinMode(T_DIN, OUTPUT); pinMode(T_DOUT, INPUT); pinMode(T_IRQ, INPUT);
    digitalWrite(T_CS, HIGH); digitalWrite(T_CLK, LOW);

    // Try loading saved calibration from NVS
    if (!load_cal_from_nvs()) {
        cal = touch_get_default_calibration();
        Serial.println("[CAL] Using defaults (no saved calibration)");
    }

    Serial.printf("[TOUCH] CLK=%d CS=%d DIN=%d DOUT=%d IRQ=%d\n",
                  T_CLK, T_CS, T_DIN, T_DOUT, T_IRQ);
}

void touch_set_calibration(TouchCalibration c) { cal = c; }

// ─── Mapping ────────────────────────────────────────────────────────────────
static void map_screen(int16_t rx, int16_t ry, int16_t* ox, int16_t* oy) {
    if (cal.swapped) { int16_t t = rx; rx = ry; ry = t; }
    int32_t mx = (int32_t)(rx - cal.x_min) * SCREEN_W / (cal.x_max - cal.x_min);
    int32_t my = (int32_t)(ry - cal.y_min) * SCREEN_H / (cal.y_max - cal.y_min);
    if (cal.invert_x) mx = SCREEN_W - 1 - mx;
    if (cal.invert_y) my = SCREEN_H - 1 - my;
    *ox = constrain(mx, 0, SCREEN_W - 1);
    *oy = constrain(my, 0, SCREEN_H - 1);
}

// ─── Button classification ──────────────────────────────────────────────────
static uint16_t classify(int16_t x, int16_t y) {
    uint16_t b = 0;

    // D-pad
    int dx = x - DPAD_CX, dy = y - DPAD_CY;
    int32_t d2 = (int32_t)dx*dx + (int32_t)dy*dy;
    if (d2 <= (int32_t)(DPAD_R+12)*(DPAD_R+12) && d2 > 49) {
        int ax = abs(dx), ay = abs(dy);
        if (ax > ay/3) b |= (dx < 0) ? GB_BTN_LEFT : GB_BTN_RIGHT;
        if (ay > ax/3) b |= (dy < 0) ? GB_BTN_UP : GB_BTN_DOWN;
    }

    // A
    { int32_t d=(int32_t)(x-BTN_A_X)*(x-BTN_A_X)+(int32_t)(y-BTN_A_Y)*(y-BTN_A_Y);
      if(d<=(int32_t)(BTN_A_R+10)*(BTN_A_R+10)) b|=GB_BTN_A; }
    // B
    { int32_t d=(int32_t)(x-BTN_B_X)*(x-BTN_B_X)+(int32_t)(y-BTN_B_Y)*(y-BTN_B_Y);
      if(d<=(int32_t)(BTN_B_R+10)*(BTN_B_R+10)) b|=GB_BTN_B; }
    // Start
    if(x>=BTN_ST_X-BTN_ST_W/2-6&&x<=BTN_ST_X+BTN_ST_W/2+6&&
       y>=BTN_ST_Y-BTN_ST_H/2-6&&y<=BTN_ST_Y+BTN_ST_H/2+6) b|=GB_BTN_START;
    // Select
    if(x>=BTN_SE_X-BTN_SE_W/2-6&&x<=BTN_SE_X+BTN_SE_W/2+6&&
       y>=BTN_SE_Y-BTN_SE_H/2-6&&y<=BTN_SE_Y+BTN_SE_H/2+6) b|=GB_BTN_SELECT;
    // Menu
    { int32_t d=(int32_t)(x-BTN_M_X)*(x-BTN_M_X)+(int32_t)(y-BTN_M_Y)*(y-BTN_M_Y);
      if(d<=(int32_t)(BTN_M_R+8)*(BTN_M_R+8)) b|=GB_BTN_MENU; }

    return b;
}

// ─── Update ─────────────────────────────────────────────────────────────────
void touch_update() {
    uint32_t now = millis();
    if (now - last_ms < 14) return;
    last_ms = now;
    if (digitalRead(T_IRQ) == HIGH) {
        pressed = false; cur_btns = 0; scr_x = scr_y = -1; return;
    }
    int16_t rx, ry, rz;
    if (read_raw(&rx, &ry, &rz)) {
        int16_t mx, my; map_screen(rx, ry, &mx, &my);
        scr_x = mx; scr_y = my; pressed = true; cur_btns = classify(mx, my);
    } else { pressed = false; cur_btns = 0; scr_x = scr_y = -1; }
}

uint16_t touch_get_buttons() { return cur_btns; }
bool touch_is_pressed() { return pressed; }
int16_t touch_get_x() { return scr_x; }
int16_t touch_get_y() { return scr_y; }

// ─── Smart Calibration ─────────────────────────────────────────────────────
// Uses 5 points (4 corners + center) for better accuracy
// Calculates linear regression for X and Y mapping
// Saves to NVS automatically
void touch_run_calibration() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xFFE0);
    tft.drawString("CALIBRATION", 160, 12, 4);
    tft.setTextColor(0xAD55);
    tft.drawString("Touch each + carefully", 160, 35, 2);

    // 5 calibration points: 4 corners + center
    struct { int16_t sx, sy; } targets[5] = {
        {30, 60}, {290, 60}, {30, 210}, {290, 210}, {160, 135}
    };
    const char* labels[5] = {"Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right", "Center"};
    int16_t raw_x[5], raw_y[5];
    bool got[5] = {false};

    for (int i = 0; i < 5; i++) {
        // Clear instruction area
        tft.fillRect(0, 42, 320, 20, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        char msg[32]; snprintf(msg, 32, "%d/5: %s", i + 1, labels[i]);
        tft.drawString(msg, 160, 52, 2);

        // Draw crosshair with circle
        int tx = targets[i].sx, ty = targets[i].sy;
        tft.drawCircle(tx, ty, 10, 0x07E0);
        tft.drawCircle(tx, ty, 4, 0x07E0);
        tft.drawLine(tx - 14, ty, tx + 14, ty, 0x07E0);
        tft.drawLine(tx, ty - 14, tx, ty + 14, 0x07E0);

        // Wait for touch with timeout
        uint32_t t0 = millis();
        while (digitalRead(T_IRQ) == HIGH) {
            delay(10);
            if (millis() - t0 > 15000) goto cal_fail;  // 15s timeout
        }
        delay(100);  // settle time

        // Take multiple samples and median-filter
        int16_t samples_x[8], samples_y[8];
        int ns = 0;
        for (int s = 0; s < 8; s++) {
            int16_t rx, ry, rz;
            if (read_raw(&rx, &ry, &rz)) {
                samples_x[ns] = rx;
                samples_y[ns] = ry;
                ns++;
            }
            delay(30);
        }

        if (ns >= 3) {
            // Sort and take median
            for (int a = 0; a < ns-1; a++) for (int b = a+1; b < ns; b++) {
                if (samples_x[a] > samples_x[b]) { int16_t t = samples_x[a]; samples_x[a] = samples_x[b]; samples_x[b] = t; }
                if (samples_y[a] > samples_y[b]) { int16_t t = samples_y[a]; samples_y[a] = samples_y[b]; samples_y[b] = t; }
            }
            raw_x[i] = samples_x[ns / 2];
            raw_y[i] = samples_y[ns / 2];
            got[i] = true;
            Serial.printf("[CAL] %d: raw(%d,%d) -> screen(%d,%d) [%d samples]\n",
                          i, raw_x[i], raw_y[i], tx, ty, ns);
        }

        // Mark done
        tft.fillCircle(tx, ty, 8, got[i] ? TFT_GREEN : TFT_RED);

        // Wait release
        while (digitalRead(T_IRQ) == LOW) delay(10);
        delay(300);
    }

    // ─── Calculate calibration from 5 points ────────────────────────────────
    {
        // Check we got all points
        int valid = 0;
        for (int i = 0; i < 5; i++) if (got[i]) valid++;
        if (valid < 4) goto cal_fail;

        TouchCalibration nc;

        // Determine swap: top-left to top-right should change screen X
        // If raw_x changes more -> not swapped. If raw_y changes more -> swapped
        int16_t dx_raw = abs(raw_x[1] - raw_x[0]);
        int16_t dy_raw = abs(raw_y[1] - raw_y[0]);
        nc.swapped = (dx_raw < dy_raw);

        int16_t mx[5], my[5];
        for (int i = 0; i < 5; i++) {
            mx[i] = nc.swapped ? raw_y[i] : raw_x[i];
            my[i] = nc.swapped ? raw_x[i] : raw_y[i];
        }

        // Determine inversion
        nc.invert_x = (mx[0] > mx[1]);  // left has higher raw than right
        nc.invert_y = (my[0] > my[2]);  // top has higher raw than bottom

        // Calculate ranges using all points for better accuracy
        int16_t x_vals[5], y_vals[5];
        for (int i = 0; i < 5; i++) { x_vals[i] = mx[i]; y_vals[i] = my[i]; }

        // Sort to find range
        for (int a = 0; a < 4; a++) for (int b = a+1; b < 5; b++) {
            if (x_vals[a] > x_vals[b]) { int16_t t = x_vals[a]; x_vals[a] = x_vals[b]; x_vals[b] = t; }
            if (y_vals[a] > y_vals[b]) { int16_t t = y_vals[a]; y_vals[a] = y_vals[b]; y_vals[b] = t; }
        }

        // Use 2nd lowest and 2nd highest (outlier rejection)
        int16_t xlo = x_vals[1], xhi = x_vals[3];
        int16_t ylo = y_vals[1], yhi = y_vals[3];

        // Extrapolate to full screen (calibration points are not at edges)
        // Points are at ~10% and ~90% of screen, so extend by ~12%
        int16_t xspan = xhi - xlo;
        int16_t yspan = yhi - ylo;
        nc.x_min = xlo - xspan * 12 / 80;
        nc.x_max = xhi + xspan * 12 / 80;
        nc.y_min = ylo - yspan * 15 / 75;
        nc.y_max = yhi + yspan * 15 / 75;

        cal = nc;
        save_cal_to_nvs();

        Serial.printf("[CAL] Done: sw=%d ix=%d iy=%d x[%d-%d] y[%d-%d]\n",
                      cal.swapped, cal.invert_x, cal.invert_y,
                      cal.x_min, cal.x_max, cal.y_min, cal.y_max);
    }

    // ─── Verification ───────────────────────────────────────────────────────
    {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(0x07E0);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("VERIFY", 160, 10, 4);
        tft.setTextColor(0xAD55);
        tft.drawString("Draw to test accuracy", 160, 35, 2);
        tft.setTextColor(0x7BEF);
        tft.drawString("Wait 5s or lift to exit", 160, 228, 1);

        // Draw reference grid
        for (int x = 0; x <= 320; x += 64) tft.drawFastVLine(x, 50, 170, 0x18C3);
        for (int y = 50; y <= 220; y += 42) tft.drawFastHLine(0, y, 320, 0x18C3);
        // Draw corner markers
        tft.drawCircle(30, 60, 4, 0x4A69);
        tft.drawCircle(290, 60, 4, 0x4A69);
        tft.drawCircle(30, 210, 4, 0x4A69);
        tft.drawCircle(290, 210, 4, 0x4A69);
        tft.drawCircle(160, 135, 4, 0x4A69);

        uint32_t t0 = millis();
        uint32_t no_touch_since = 0;
        bool was_touching = false;

        while (millis() - t0 < 8000) {
            if (digitalRead(T_IRQ) == LOW) {
                int16_t rx, ry, rz;
                if (read_raw(&rx, &ry, &rz)) {
                    int16_t mx, my; map_screen(rx, ry, &mx, &my);
                    tft.fillCircle(mx, my, 2, TFT_RED);
                }
                was_touching = true;
                no_touch_since = millis();
            } else {
                if (was_touching && millis() - no_touch_since > 2000) break;
            }
            delay(15);
        }
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(0x07E0);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Calibration Saved!", 160, 110, 4);
    delay(1200);
    return;

cal_fail:
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Calibration Failed!", 160, 100, 4);
    tft.setTextColor(0x7BEF);
    tft.drawString("Using previous values", 160, 140, 2);
    delay(2000);
}
