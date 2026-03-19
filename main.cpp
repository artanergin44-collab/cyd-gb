#include <Arduino.h>
#include "hw_config.h"
#include "display.h"
#include "touch_input.h"
#include "sd_manager.h"
#include "ui_launcher.h"
#include "emulator_bridge.h"

static RomEntry roms[64];
static int rcnt = 0;
static char cur_path[80] = {0};
static TaskHandle_t ttask = nullptr;
static volatile bool emu_on = false, menu_req = false;

void touch_task(void* p) {
    (void)p;
    for(;;) {
        touch_update();
        if (emu_on) {
            uint16_t b = touch_get_buttons();
            if (b & GB_BTN_MENU) menu_req = true;
            emu_set_joypad(b & 0xFF);
        }
        vTaskDelay(pdMS_TO_TICKS(12));
    }
}

static void tt_start() {
    if(!ttask) xTaskCreatePinnedToCore(touch_task,"t",4096,0,2,&ttask,0);
    else vTaskResume(ttask);
}
static void tt_stop() { if(ttask) vTaskSuspend(ttask); }

static void save_ram() {
    if(!cur_path[0]) return;
    uint32_t sz=0; uint8_t* r=emu_get_cart_ram(&sz);
    if(sz>0) { sd_save_state(cur_path,r,sz); Serial.printf("[SAVE] %u bytes\n",sz); }
}
static void load_ram() {
    if(!cur_path[0]) return;
    uint32_t sz=0; emu_get_cart_ram(&sz);
    if(sz>0) { uint8_t* t=(uint8_t*)malloc(sz);
        if(t){if(sd_load_state(cur_path,t,sz))emu_set_cart_ram(t,sz);free(t);} }
}

// ─── Emulation loop ─────────────────────────────────────────────────────────
void run_emu() {
    emu_on = true; menu_req = false;
    tt_start();
    display_clear(TFT_BLACK);
    display_draw_controls();

    uint32_t ft=0;
    while(emu_on) {
        emu_run_frame();

        if (menu_req) {
            menu_req = false;
            tt_stop();

            int c = launcher_ingame_menu();
            switch(c) {
                case 0: break;  // resume
                case 1:  // save
                    save_ram();
                    tft.fillRect(80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_GREEN);
                    tft.drawString("SAVED!",160,100,4);
                    delay(700);
                    break;
                case 2:  // load
                    load_ram(); emu_reset(); load_ram();
                    tft.fillRect(80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(0x07FF);
                    tft.drawString("LOADED!",160,100,4);
                    delay(700);
                    break;
                case 3:  // quit
                    emu_on=false; save_ram(); tt_stop(); return;
                case 4:  // calibrate
                    touch_run_calibration(); break;
                case 5:  // settings
                    launcher_settings_menu(); break;
            }
            display_clear(TFT_BLACK);
            display_draw_controls();
            tt_start();
        }

        // FPS log
        uint32_t n=millis();
        if(n-ft>3000){ft=n;Serial.printf("[G] FPS:%u btn:0x%02X\n",emu_get_fps(),touch_get_buttons());}
        taskYIELD();
    }
}

// ─── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\n=== CYD-GB ===");
    pinMode(LED_R_PIN,OUTPUT); pinMode(LED_G_PIN,OUTPUT); pinMode(LED_B_PIN,OUTPUT);
    digitalWrite(LED_R_PIN,HIGH); digitalWrite(LED_G_PIN,HIGH); digitalWrite(LED_B_PIN,HIGH);

    display_init();
    touch_init();

    if(!sd_init()) {
        tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.drawString("SD Card Error!",160,100,4);
        tft.setTextColor(0x7BEF); tft.drawString("Insert FAT32 SD & reset",160,140,2);
        while(true) delay(1000);
    }

    // Splash
    tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x07E0); tft.drawString("CYD-GB",160,70,4);
    tft.setTextColor(0x7BEF); tft.drawString("Game Boy Emulator",160,110,2);
    delay(1200);

    // Load saved settings from NVS
    uint8_t s_pal, s_fs, s_bl;
    if (touch_load_settings(&s_pal, &s_fs, &s_bl)) {
        emu_set_palette(s_pal);
        emu_set_frame_skip(s_fs);
        display_set_backlight(s_bl);
        Serial.printf("[INIT] Loaded settings: pal=%d fs=%d bl=%d\n", s_pal, s_fs, s_bl);
    }

    Serial.printf("[INIT] Heap: %u\n",ESP.getFreeHeap());
}

// ─── Loop ───────────────────────────────────────────────────────────────────
void loop() {
    rcnt = sd_scan_roms(roms, 64);
    int sel = launcher_show(roms, rcnt);
    if(sel==-2){touch_run_calibration();return;}
    if(sel<0||sel>=rcnt) return;

    strncpy(cur_path,roms[sel].full_path,79);

    // Loading screen
    tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x07E0); tft.drawString("Loading...",160,90,4);
    char nm[30]; strncpy(nm,roms[sel].filename,28); nm[28]=0;
    char* d=strrchr(nm,'.'); if(d)*d=0;
    tft.setTextColor(TFT_WHITE); tft.drawString(nm,160,130,2);

    if(!emu_open_rom(cur_path)){
        tft.setTextColor(TFT_RED); tft.drawString("Open failed!",160,170,2); delay(2000); return;
    }
    if(!emu_init(0,0)){
        tft.setTextColor(TFT_RED); tft.drawString("Init failed!",160,170,2); delay(2000); emu_close_rom(); return;
    }

    load_ram();
    emu_set_frame_skip(0);
    digitalWrite(LED_G_PIN,LOW);
    run_emu();
    digitalWrite(LED_G_PIN,HIGH);
    emu_close_rom();
}
