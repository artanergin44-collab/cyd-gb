#include "ui_launcher.h"
#include "display.h"
#include "touch_input.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include <Arduino.h>

#define ITEMS_PP 5
#define ITEM_H   34
#define ITEM_Y0  44
#define ITEM_X   8

// ─── Helpers ────────────────────────────────────────────────────────────────
static void wait_release() { while(touch_is_pressed()){touch_update();delay(10);} delay(100); }

static void draw_header(const char* t) {
    tft.fillRect(0,0,320,36,0x18C3);
    tft.setTextColor(TFT_WHITE,0x18C3); tft.setTextDatum(ML_DATUM);
    tft.drawString(t,10,18,2);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(0x7BEF,0x18C3);
    tft.drawString("CYD-GB",310,18,1);
}

// ─── ROM List ───────────────────────────────────────────────────────────────
static void draw_list(RomEntry* r, int cnt, int pg, int sel) {
    int s = pg*ITEMS_PP, e = min(s+ITEMS_PP, cnt);
    tft.fillRect(0,38,320,202,TFT_BLACK);

    for (int i=s; i<e; i++) {
        int y = ITEM_Y0 + (i-s)*ITEM_H;
        uint16_t bg = (i==sel) ? 0x0014 : 0x0000;
        uint16_t fg = (i==sel) ? 0xFFE0 : TFT_WHITE;
        tft.fillRoundRect(ITEM_X,y,304,ITEM_H-4,4,bg);

        // Badge
        uint16_t bc = r[i].is_gbc ? 0x07E0 : 0x7BEF;
        const char* bt = r[i].is_gbc ? "GBC" : "GB";
        tft.fillRoundRect(ITEM_X+3,y+5,26,18,3,bc);
        tft.setTextColor(TFT_BLACK,bc); tft.setTextDatum(MC_DATUM);
        tft.drawString(bt,ITEM_X+16,y+14,1);

        // Name (truncated, readable)
        char nm[30]; strncpy(nm,r[i].filename,28); nm[28]=0;
        char* dot=strrchr(nm,'.'); if(dot)*dot=0;
        tft.setTextColor(fg,bg); tft.setTextDatum(ML_DATUM);
        tft.drawString(nm,ITEM_X+34,y+ITEM_H/2-2,2);

        // Size
        char sz[12]; snprintf(sz,12,"%uK",r[i].size/1024);
        tft.setTextColor(0x7BEF,bg); tft.setTextDatum(MR_DATUM);
        tft.drawString(sz,308,y+ITEM_H/2-2,1);
    }

    // Nav bar
    tft.fillRect(0,SCREEN_H-20,320,20,0x18C3);
    int tp = (cnt+ITEMS_PP-1)/ITEMS_PP;
    if (tp>1) {
        tft.setTextColor(TFT_WHITE,0x18C3); tft.setTextDatum(MC_DATUM);
        char ps[16]; snprintf(ps,16,"< %d/%d >",pg+1,tp);
        tft.drawString(ps,160,SCREEN_H-10,1);
    }
    tft.setTextColor(0xFFE0,0x18C3); tft.setTextDatum(MR_DATUM);
    tft.drawString("[CAL]",315,SCREEN_H-10,1);
}

int launcher_show(RomEntry* roms, int cnt) {
    int pg=0, sel=-1;
    tft.fillScreen(TFT_BLACK);
    draw_header("Game Boy ROMs");

    if (cnt==0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.drawString("No ROMs found!",160,80,4);
        tft.setTextColor(0x7BEF); tft.drawString("Put .gb files in /roms/gb/",160,120,2);
        tft.drawString("on your SD card",160,145,2);
        while(true) delay(1000);
    }

    draw_list(roms,cnt,pg,sel);
    uint32_t rel_t=0, dbg_t=0;

    while (true) {
        touch_update();
        if (touch_is_pressed()) {
            int16_t tx=touch_get_x(), ty=touch_get_y();
            // ROM items
            if (ty>=ITEM_Y0 && ty<ITEM_Y0+ITEMS_PP*ITEM_H) {
                int idx=pg*ITEMS_PP+(ty-ITEM_Y0)/ITEM_H;
                if (idx<cnt && idx!=sel) { sel=idx; draw_list(roms,cnt,pg,sel); }
            }
            // Nav
            if (ty>=SCREEN_H-20) {
                int tp=(cnt+ITEMS_PP-1)/ITEMS_PP;
                if (tx<80 && pg>0) { pg--; sel=-1; draw_list(roms,cnt,pg,sel); delay(300); }
                else if (tx>240 && tx<320) return -2; // CAL
                else if (tx>SCREEN_W-80 && pg<tp-1) { pg++; sel=-1; draw_list(roms,cnt,pg,sel); delay(300); }
            }
            rel_t=millis();
        } else {
            if (sel>=0 && millis()-rel_t>150 && millis()-rel_t<800) return sel;
        }
        // Debug
        if (millis()-dbg_t>3000) { dbg_t=millis(); Serial.printf("[LAUNCH] pg=%d sel=%d\n",pg,sel); }
        delay(20);
    }
}

// ─── In-game menu ───────────────────────────────────────────────────────────
static void mbtn(int y, const char* t, uint16_t fg, bool hl) {
    uint16_t bg = hl ? 0x2945 : 0x1082;
    tft.fillRoundRect(65,y,190,26,5,bg);
    tft.drawRoundRect(65,y,190,26,5,0x528A);
    tft.setTextColor(fg,bg); tft.setTextDatum(MC_DATUM);
    tft.drawString(t,160,y+13,2);
}

int launcher_ingame_menu() {
    tft.fillRect(45,10,230,220,TFT_BLACK);
    tft.drawRoundRect(45,10,230,220,6,0x528A);
    tft.setTextColor(0xFFE0,TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.drawString("PAUSED",160,28,4);

    #define MI 6
    int yp[MI]={48,78,108,138,168,198};
    const char* lb[MI]={"Resume","Save Game","Load Save","Settings","Calibrate","Quit"};
    uint16_t fc[MI]={TFT_GREEN,0x07FF,0x07FF,0xFFE0,0xFFE0,TFT_RED};
    for(int i=0;i<MI;i++) mbtn(yp[i],lb[i],fc[i],false);
    wait_release();

    int hl=-1;
    while(true) {
        touch_update();
        if (touch_is_pressed()) {
            int16_t tx=touch_get_x(),ty=touch_get_y();
            if (tx>=65&&tx<=255) {
                for(int i=0;i<MI;i++) if(ty>=yp[i]&&ty<yp[i]+26) {
                    if(hl!=i){if(hl>=0)mbtn(yp[hl],lb[hl],fc[hl],false);mbtn(yp[i],lb[i],fc[i],true);hl=i;}
                    break;
                }
            }
        } else if (hl>=0) {
            int s=hl; hl=-1;
            // 0=resume 1=save 2=load 3=settings 4=cal 5=quit
            switch(s){case 0:return 0;case 1:return 1;case 2:return 2;case 3:return 5;case 4:return 4;case 5:return 3;}
        }
        delay(15);
    }
}

// ─── Settings menu ──────────────────────────────────────────────────────────
void launcher_settings_menu() {
    uint8_t pal = emu_get_palette();
    uint8_t fs = emu_get_frame_skip();
    uint8_t bl = 255; // brightness

    auto draw_settings = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(0xFFE0); tft.drawString("SETTINGS",160,15,4);

        // Palette
        tft.setTextColor(TFT_WHITE); tft.drawString("Color Palette:",160,50,2);
        tft.fillRoundRect(40,65,240,28,5,0x1082);
        char palstr[40]; snprintf(palstr,40,"%d/%d %s",pal+1,NUM_PALETTES,emu_get_palette_name(pal));
        tft.setTextColor(0x07E0,0x1082);
        tft.drawString(palstr,160,79,2);
        tft.setTextColor(0x7BEF,0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<<",48,79,2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">>",272,79,2);

        // Frame skip
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE); tft.drawString("Frame Skip:",160,105,2);
        tft.fillRoundRect(40,120,240,28,5,0x1082);
        char fss[16]; snprintf(fss,16,"%d (FPS ~%d)",fs, fs==0?60:60/(fs+1));
        tft.setTextColor(0x07E0,0x1082); tft.drawString(fss,160,134,2);
        tft.setTextColor(0x7BEF,0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<",50,134,2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">",270,134,2);

        // Brightness
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE); tft.drawString("Brightness:",160,160,2);
        tft.fillRoundRect(40,175,240,28,5,0x1082);
        char bls[16]; snprintf(bls,16,"%d%%",bl*100/255);
        tft.setTextColor(0x07E0,0x1082); tft.drawString(bls,160,189,2);
        tft.setTextColor(0x7BEF,0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<",50,189,2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">",270,189,2);

        // Done button
        tft.fillRoundRect(100,215,120,22,5,0x07E0);
        tft.setTextColor(TFT_BLACK,0x07E0); tft.setTextDatum(MC_DATUM);
        tft.drawString("DONE",160,226,2);
    };

    draw_settings();
    wait_release();

    while(true) {
        touch_update();
        if (touch_is_pressed()) {
            int16_t tx=touch_get_x(), ty=touch_get_y();
            bool changed = false;

            // Palette row (y=65..93)
            if (ty>=65 && ty<93) {
                if (tx<120) { pal = (pal+NUM_PALETTES-1)%NUM_PALETTES; changed=true; }
                else if (tx>200) { pal = (pal+1)%NUM_PALETTES; changed=true; }
            }
            // Frame skip row (y=120..148)
            if (ty>=120 && ty<148) {
                if (tx<120 && fs>0) { fs--; changed=true; }
                else if (tx>200 && fs<4) { fs++; changed=true; }
            }
            // Brightness row (y=175..203)
            if (ty>=175 && ty<203) {
                if (tx<120 && bl>30) { bl-=25; changed=true; }
                else if (tx>200 && bl<255) { bl=min(255,bl+25); changed=true; }
            }
            // Done
            if (ty>=215 && ty<237 && tx>=100 && tx<=220) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                touch_save_settings(pal, fs, bl);  // Save to NVS
                wait_release();
                return;
            }

            if (changed) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                draw_settings();
                delay(200);
            }
        }
        delay(20);
    }
}
