#include <Arduino.h>
#include "hw_config.h"
#include "display.h"
#include "touch_input.h"
#include "button_input.h"
#include "sd_manager.h"
#include "ui_launcher.h"
#include "emulator_bridge.h"
#include "bt_scanner.h"

static RomEntry roms[64];
static int rcnt = 0;
static char cur_path[80] = {0};
static TaskHandle_t ttask = nullptr;
static volatile bool emu_on = false, menu_req = false;
static bool show_fps_overlay = false;
static bool show_sd_save_overlay = false;
static bool has_saved_settings = false;

void input_task(void* p) {
    (void)p;
    bool prev_menu_combo = false;
    bool prev_touch_menu = false;
    for(;;) {
        button_update();
        touch_update();
        if (emu_on) {
            uint16_t physical = button_get_buttons();
            uint16_t touch = touch_get_buttons();
            uint16_t b = physical | touch;
            bool menu_combo = (physical & (GB_BTN_START | GB_BTN_SELECT)) == (GB_BTN_START | GB_BTN_SELECT);
            bool touch_menu = (touch & GB_BTN_MENU) != 0;
            if ((menu_combo && !prev_menu_combo) || (touch_menu && !prev_touch_menu)) {
                menu_req = true;
            }
            prev_menu_combo = menu_combo;
            prev_touch_menu = touch_menu;
            if (menu_combo) {
                emu_set_joypad(b & ~(GB_BTN_START | GB_BTN_SELECT));
            } else {
                emu_set_joypad(b & 0xFF);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(12));
    }
}

static void tt_start() {
    if(!ttask) xTaskCreatePinnedToCore(input_task,"t",4096,0,2,&ttask,0);
    else vTaskResume(ttask);
}
static void tt_stop() { if(ttask) vTaskSuspend(ttask); }

static bool save_ram() {
    if(!cur_path[0]) return false;
    uint32_t sz=0; uint8_t* r=emu_get_cart_ram(&sz);
    if(sz>0) {
        bool ok = sd_save_state(cur_path,r,sz);
        Serial.printf("[SAVE] %u bytes (%s)\n",sz, ok ? "ok" : "fail");
        return ok;
    }
    return false;
}

static bool load_ram() {
    if(!cur_path[0]) return false;
    uint32_t sz=0;
    uint8_t* cart_ram = emu_get_cart_ram(&sz);
    if (sz == 0) {
        Serial.println("[SAVE] Load skipped: cart RAM size is 0");
        return false;
    }

    if (!cart_ram) {
        Serial.println("[SAVE] Load failed: cart RAM pointer is null");
        return false;
    }

    if (sd_load_state(cur_path, cart_ram, sz)) {
        Serial.printf("[SAVE] Loaded %u bytes\n", sz);
        return true;
    } else {
        Serial.printf("[SAVE] No load for %s\n", cur_path);
        return false;
    }
}

// ─── Emulation loop ─────────────────────────────────────────────────────────
void run_emu() {
    emu_on = true; menu_req = false;
    tt_start();
    display_clear(TFT_BLACK);
    display_draw_controls();

    while(emu_on) {
        emu_run_frame();
        display_draw_menu_icon();

        if (menu_req) {
            menu_req = false;
            tt_stop();

            int c = launcher_ingame_menu();
            switch(c) {
                case 0: break;  // resume
                case 1:  // save
                {
                    bool ok = save_ram();
                    tft.fillRect(80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
                    tft.drawString(ok ? "SAVED!" : "SAVE FAILED",SCREEN_W/2,100,2);
                    delay(700);
                    break;
                }
                case 2:  // load
                {
                    uint32_t sz = 0;
                    uint8_t* cart_ram = emu_get_cart_ram(&sz);
                    bool ok = sz > 0 && cart_ram && load_ram();
                    if (ok) { emu_reset(); load_ram(); }
                    tft.fillRect(80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(ok ? 0x07FF : TFT_RED);
                    tft.drawString(ok ? "LOADED!" : "NO SAVE",SCREEN_W/2,100,2);
                    delay(700);
                    break;
                }
                case 3:  // quit
                    emu_on=false; save_ram(); tt_stop(); return;
                case 4:
                    touch_run_calibration(); break;
                case 5:  // settings
                    launcher_settings_menu(&show_fps_overlay, &show_sd_save_overlay); break;
                case 6:  // complete emulator state
                case 7:
                {
                    bool ok = c == 6 ? emu_save_state(cur_path) : emu_load_state(cur_path);
                    tft.fillRect(36,80,SCREEN_W-72,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM);
                    tft.setTextColor(ok ? TFT_GREEN : TFT_RED);
                    tft.drawString(c == 6 ? (ok ? "STATE SAVED" : "SAVE FAILED")
                                           : (ok ? "STATE LOADED" : "NO VALID STATE"),
                                   SCREEN_W/2,100,2);
                    delay(700);
                    break;
                }
            }
            display_clear(TFT_BLACK);
            display_draw_controls();
            tt_start();
        }

        taskYIELD();
    }
}

static void run_bt_scanner() {
    bt_scanner_enter();
    while (true) {
        if (bt_scanner_loop()) break;
        delay(10);
        taskYIELD();
    }
    bt_scanner_shutdown();
    display_clear(TFT_BLACK);
}

// ─── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\n=== CYD-GB ===");
    pinMode(LED_R_PIN, OUTPUT);
    if (LED_G_PIN >= 0) pinMode(LED_G_PIN, OUTPUT);
    if (LED_B_PIN >= 0) pinMode(LED_B_PIN, OUTPUT);
    digitalWrite(LED_R_PIN, HIGH);
    if (LED_G_PIN >= 0) digitalWrite(LED_G_PIN, HIGH);
    if (LED_B_PIN >= 0) digitalWrite(LED_B_PIN, HIGH);

    display_init();
    touch_init();
    button_init();

    if(!sd_init()) {
        tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.drawString("SD Card Error!",SCREEN_W/2,100,4);
        tft.setTextColor(0x7BEF); tft.drawString("Insert FAT32 SD & reset",SCREEN_W/2,140,2);
        while(true) delay(1000);
    }

    display_show_boot_animation();

    // Load saved settings from NVS
    uint8_t s_pal, s_fs, s_bl;
    if (touch_load_settings(&s_pal, &s_fs, &s_bl, &show_fps_overlay, &show_sd_save_overlay)) {
        has_saved_settings = true;
        emu_set_palette(s_pal);
        emu_set_frame_skip(s_fs);
        display_set_backlight(s_bl);
        Serial.printf("[INIT] Loaded settings: pal=%d fs=%d bl=%d fps_ov=%d save_ov=%d\n",
                      s_pal, s_fs, s_bl, (int)show_fps_overlay, (int)show_sd_save_overlay);
    }

    Serial.printf("[INIT] Heap: %u\n",ESP.getFreeHeap());
}

// ─── Loop ───────────────────────────────────────────────────────────────────
void loop() {
    rcnt = sd_scan_roms(roms, 64);
    int sel = launcher_show(roms, rcnt);
    if (sel == LAUNCHER_SEL_BT_SCANNER) {
        run_bt_scanner();
        return;
    }
    if(sel<0||sel>=rcnt) return;

    strncpy(cur_path,roms[sel].full_path,79);
    cur_path[79] = 0;

    // Loading screen
    tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x07E0); tft.drawString("Loading...",SCREEN_W/2,90,4);
    char nm[30]; strncpy(nm,roms[sel].filename,28); nm[28]=0;
    char* d=strrchr(nm,'.'); if(d)*d=0;
    tft.setTextColor(TFT_WHITE); tft.drawString(nm,SCREEN_W/2,130,2);

    if(!emu_open_rom(cur_path)){
        tft.setTextColor(TFT_RED); tft.drawString("Open failed!",SCREEN_W/2,170,2); delay(2000); return;
    }
    if(!emu_init(0,0)){
        tft.setTextColor(TFT_RED); tft.drawString("Init failed!",SCREEN_W/2,170,2); delay(2000); emu_close_rom(); return;
    }

    load_ram();
    if (!has_saved_settings) emu_set_frame_skip(2);
    if (LED_G_PIN >= 0) digitalWrite(LED_G_PIN, LOW);
    run_emu();
    if (LED_G_PIN >= 0) digitalWrite(LED_G_PIN, HIGH);
    emu_close_rom();
}
