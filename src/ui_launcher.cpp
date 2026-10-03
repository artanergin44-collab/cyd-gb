#include "ui_launcher.h"
#include "display.h"
#include "touch_input.h"
#include "button_input.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include <Arduino.h>

#define ITEMS_PP 5
#define ITEM_H   34
#define ITEM_Y0  44
#define ITEM_X   8

// ─── Helpers ────────────────────────────────────────────────────────────────
static void wait_release() { 
	// wait until no button is pressed
	button_update();
	touch_update();
	while(button_get_buttons() || touch_is_pressed()) {
		button_update(); touch_update(); delay(10);
	}
	delay(100);
}

static bool touch_press(int* x, int* y, bool* was_pressed) {
	touch_update();
	bool pressed = touch_is_pressed();
	bool edge = pressed && !*was_pressed;
	*was_pressed = pressed;
	if (edge) { *x = touch_get_x(); *y = touch_get_y(); }
	return edge;
}

static void draw_header(const char* t) {
	tft.fillRect(0,0,SCREEN_W,36,0x18C3);
	tft.setTextColor(TFT_WHITE,0x18C3); tft.setTextDatum(ML_DATUM);
	tft.drawString(t,10,18,2);
	tft.setTextDatum(MR_DATUM); tft.setTextColor(0x7BEF,0x18C3);
	tft.drawString("CYD-GB",SCREEN_W-10,18,1);
}

// ─── ROM List ───────────────────────────────────────────────────────────────
static void draw_list(RomEntry* r, int cnt, int pg, int sel) {
	int total = cnt + 1; // extra entry: BT scanner
	int s = pg*ITEMS_PP, e = min(s+ITEMS_PP, total);
	tft.fillRect(0,38,SCREEN_W,202,TFT_BLACK);

	for (int i=s; i<e; i++) {
		int y = ITEM_Y0 + (i-s)*ITEM_H;
		uint16_t bg = (i==sel) ? 0x0014 : 0x0000;
		uint16_t fg = (i==sel) ? 0xFFE0 : TFT_WHITE;
		tft.fillRoundRect(ITEM_X,y,SCREEN_W-ITEM_X-8,ITEM_H-4,4,bg);

		if (i < cnt) {
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
			tft.drawString(sz,SCREEN_W-12,y+ITEM_H/2-2,1);
		} else {
			// Virtual app entry: BT scanner
			tft.fillRoundRect(ITEM_X+3,y+5,26,18,3,0x07FF);
			tft.setTextColor(TFT_BLACK,0x07FF); tft.setTextDatum(MC_DATUM);
			tft.drawString("BT",ITEM_X+16,y+14,1);

			tft.setTextColor(fg,bg); tft.setTextDatum(ML_DATUM);
			tft.drawString("Beacon Scanner",ITEM_X+34,y+ITEM_H/2-2,2);

		tft.setTextColor(0x7BEF,bg); tft.setTextDatum(MR_DATUM);
		tft.drawString("APP",SCREEN_W-12,y+ITEM_H/2-2,1);
		}
	}

	// Nav bar
	tft.fillRect(0,SCREEN_H-20,SCREEN_W,20,0x18C3);
	int tp = (total+ITEMS_PP-1)/ITEMS_PP;
	if (tp>1) {
		tft.setTextColor(TFT_WHITE,0x18C3); tft.setTextDatum(MC_DATUM);
		char ps[16]; snprintf(ps,16,"< %d/%d >",pg+1,tp);
		tft.drawString(ps,SCREEN_W/2,SCREEN_H-10,1);
	}
	tft.setTextColor(0xFFE0,0x18C3); tft.setTextDatum(MR_DATUM);
	tft.drawString("[CAL]",SCREEN_W-5,SCREEN_H-10,1);
}

int launcher_show(RomEntry* roms, int cnt) {
	int pg=0, sel=0;
	tft.fillScreen(TFT_BLACK);
	draw_header("Game Boy ROMs");

	draw_list(roms,cnt,pg,sel);
	wait_release();
	uint16_t prev = 0;
	bool touch_was_pressed = false;
	uint32_t dbg_t = 0;
	int total = cnt + 1;
	int tp = (total+ITEMS_PP-1)/ITEMS_PP;
	while (true) {
		button_update();
		uint16_t b = button_get_buttons();
		int tx = -1, ty = -1;
		if (touch_press(&tx, &ty, &touch_was_pressed)) {
			if (ty >= SCREEN_H - 20) {
				if (tx >= SCREEN_W - 45) {
					touch_run_calibration();
					tft.fillScreen(TFT_BLACK); draw_header("Game Boy ROMs");
					draw_list(roms,cnt,pg,sel);
				} else if (tx < SCREEN_W / 2 && pg > 0) {
					pg--; sel = pg * ITEMS_PP; draw_list(roms,cnt,pg,sel);
				} else if (tx >= SCREEN_W / 2 && pg < tp - 1) {
					pg++; sel = pg * ITEMS_PP; draw_list(roms,cnt,pg,sel);
				}
			} else if (ty >= ITEM_Y0 && ty < ITEM_Y0 + ITEMS_PP * ITEM_H) {
				int chosen = pg * ITEMS_PP + (ty - ITEM_Y0) / ITEM_H;
				if (chosen < total) {
					if (chosen == cnt) return LAUNCHER_SEL_BT_SCANNER;
					return chosen;
				}
			}
		}
		// Up
		if ((b & GB_BTN_UP) && !(prev & GB_BTN_UP)) {
			if (sel > 0) sel--; else if (pg>0) { pg--; sel = min(total-1, pg*ITEMS_PP+ITEMS_PP-1); }
			draw_list(roms,cnt,pg,sel);
		}
		// Down
		if ((b & GB_BTN_DOWN) && !(prev & GB_BTN_DOWN)) {
			if (sel < total-1 && sel < (pg+1)*ITEMS_PP-1) sel++; else if (pg<tp-1) { pg++; sel = pg*ITEMS_PP; }
			draw_list(roms,cnt,pg,sel);
		}
		// Left = prev page
		if ((b & GB_BTN_LEFT) && !(prev & GB_BTN_LEFT)) {
			if (pg>0) { pg--; sel = pg*ITEMS_PP; draw_list(roms,cnt,pg,sel); }
		}
		// Right = next page
		if ((b & GB_BTN_RIGHT) && !(prev & GB_BTN_RIGHT)) {
			if (pg<tp-1) { pg++; sel = pg*ITEMS_PP; draw_list(roms,cnt,pg,sel); }
		}
		// A = select
		if ((b & GB_BTN_A) && !(prev & GB_BTN_A)) {
			if (sel == cnt) return LAUNCHER_SEL_BT_SCANNER;
			return sel;
		}

		prev = b;
		if (millis()-dbg_t>3000) { dbg_t=millis(); Serial.printf("[LAUNCH] pg=%d sel=%d\n",pg,sel); }
		delay(20);
	}
}

// ─── In-game menu ───────────────────────────────────────────────────────────
static void mbtn(int x, int w, int y, const char* t, uint16_t fg, bool hl) {
	uint16_t bg = hl ? 0x2945 : 0x1082;
	tft.fillRoundRect(x,y,w,26,5,bg);
	tft.drawRoundRect(x,y,w,26,5,0x528A);
	tft.setTextColor(fg,bg); tft.setTextDatum(MC_DATUM);
	tft.drawString(t,SCREEN_W/2,y+13,2);
}

int launcher_ingame_menu() {
	const int panel_w = SCREEN_W - 24;
	const int panel_x = (SCREEN_W - panel_w) / 2;
	const int panel_y = 10;
	const int panel_h = 220;
	const int btn_w = panel_w - 24;
	const int btn_x = panel_x + 12;

	tft.fillRect(panel_x,panel_y,panel_w,panel_h,TFT_BLACK);
	tft.drawRoundRect(panel_x,panel_y,panel_w,panel_h,6,0x528A);
	tft.setTextColor(0xFFE0,TFT_BLACK); tft.setTextDatum(MC_DATUM);
	tft.drawString("PAUSED",SCREEN_W/2,28,4);

	#define MI 5
	int yp[MI]={58,88,118,148,178};
	const char* lb[MI]={"Resume","Save Game","Load Save","Settings","Quit"};
	uint16_t fc[MI]={TFT_GREEN,0x07FF,0x07FF,0xFFE0,TFT_RED};
	for(int i=0;i<MI;i++) mbtn(btn_x,btn_w,yp[i],lb[i],fc[i],false);
	wait_release();

	int hl=0;
	mbtn(btn_x,btn_w,yp[hl],lb[hl],fc[hl],true);
	uint16_t prev = 0;
	bool touch_was_pressed = false;
	while(true) {
		button_update();
		uint16_t b = button_get_buttons();
		int tx = -1, ty = -1;
		if (touch_press(&tx, &ty, &touch_was_pressed) && tx >= btn_x && tx < btn_x + btn_w) {
			for (int i = 0; i < MI; ++i) {
				if (ty >= yp[i] && ty < yp[i] + 26) {
					static const int actions[MI] = {0, 1, 2, 5, 3};
					return actions[i];
				}
			}
		}
		if ((b & GB_BTN_UP) && !(prev & GB_BTN_UP)) {
			mbtn(btn_x,btn_w,yp[hl],lb[hl],fc[hl],false);
			hl = (hl==0)?MI-1:hl-1;
			mbtn(btn_x,btn_w,yp[hl],lb[hl],fc[hl],true);
		}
		if ((b & GB_BTN_DOWN) && !(prev & GB_BTN_DOWN)) {
			mbtn(btn_x,btn_w,yp[hl],lb[hl],fc[hl],false);
			hl = (hl+1)%MI;
			mbtn(btn_x,btn_w,yp[hl],lb[hl],fc[hl],true);
		}
		// A = select
		if ((b & GB_BTN_A) && !(prev & GB_BTN_A)) {
			int s = hl;
			// 0=resume 1=save 2=load 3=settings 4=quit
			switch(s){case 0:return 0;case 1:return 1;case 2:return 2;case 3:return 5;case 4:return 3;}
		}
		// B = cancel -> resume
		if ((b & GB_BTN_B) && !(prev & GB_BTN_B)) return 0;

		prev = b;
		delay(15);
	}
}

// ─── Settings menu ──────────────────────────────────────────────────────────
void launcher_settings_menu(bool* show_fps_overlay, bool* show_save_overlay) {
	uint8_t pal = emu_get_palette();
	uint8_t fs = emu_get_frame_skip();
	uint8_t bl = 255; // brightness
	uint8_t saved_pal = 0, saved_fs = 0, saved_bl = 255;
	touch_load_settings(&saved_pal, &saved_fs, &saved_bl);
	bl = saved_bl;

	// Selected row: 0=palette,1=frameskip,2=brightness,3=done
	int sel = 0;
	uint16_t prev = 0;

	auto draw_settings = [&](int selrow) {
		const int row_x = 12;
		const int row_w = SCREEN_W - 24;
		const int arrow_l = row_x + 10;
		const int arrow_r = row_x + row_w - 10;

		tft.fillScreen(TFT_BLACK);
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(0xFFE0); tft.drawString("SETTINGS",SCREEN_W/2,15,4);

		// Palette
		tft.setTextColor(TFT_WHITE); tft.drawString("Color Palette:",SCREEN_W/2,50,2);
		uint16_t palbg = (selrow==0)?0x2945:0x1082;
		tft.fillRoundRect(row_x,65,row_w,28,5,palbg);
		char palstr[40]; snprintf(palstr,40,"%d/%d %s",pal+1,NUM_PALETTES,emu_get_palette_name(pal));
		tft.setTextColor(0x07E0,palbg);
		tft.drawString(palstr,SCREEN_W/2,79,2);
		tft.setTextColor(0x7BEF,palbg);
		tft.setTextDatum(ML_DATUM); tft.drawString("<<",arrow_l,79,2);
		tft.setTextDatum(MR_DATUM); tft.drawString(">>",arrow_r,79,2);

		// Frame skip
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE); tft.drawString("Frame Skip:",SCREEN_W/2,105,2);
		uint16_t fsbg = (selrow==1)?0x2945:0x1082;
		tft.fillRoundRect(row_x,120,row_w,28,5,fsbg);
		char fss[16]; snprintf(fss,16,"%d (FPS ~%d)",fs, fs==0?60:60/(fs+1));
		tft.setTextColor(0x07E0,fsbg); tft.drawString(fss,SCREEN_W/2,134,2);
		tft.setTextColor(0x7BEF,fsbg);
		tft.setTextDatum(ML_DATUM); tft.drawString("<",arrow_l,134,2);
		tft.setTextDatum(MR_DATUM); tft.drawString(">",arrow_r,134,2);

		// Brightness
		tft.setTextDatum(MC_DATUM);
		tft.setTextColor(TFT_WHITE); tft.drawString("Brightness:",SCREEN_W/2,160,2);
		uint16_t blbg = (selrow==2)?0x2945:0x1082;
		tft.fillRoundRect(row_x,175,row_w,28,5,blbg);
		char bls[16]; snprintf(bls,16,"%d%%",bl*100/255);
		tft.setTextColor(0x07E0,blbg); tft.drawString(bls,SCREEN_W/2,189,2);
		tft.setTextColor(0x7BEF,blbg);
		tft.setTextDatum(ML_DATUM); tft.drawString("<",arrow_l,189,2);
		tft.setTextDatum(MR_DATUM); tft.drawString(">",arrow_r,189,2);

		// Done button
		uint16_t donebg = (selrow==3)?0x2945:0x07E0;
		tft.fillRoundRect(100,302,120,16,5,donebg);
		tft.setTextColor(TFT_BLACK,donebg); tft.setTextDatum(MC_DATUM);
		tft.drawString("DONE",SCREEN_W/2,310,1);
	};

	draw_settings(sel);
	wait_release();
	bool touch_was_pressed = false;

	while(true) {
		button_update();
		uint16_t b = button_get_buttons();
		int tx = -1, ty = -1;
		if (touch_press(&tx, &ty, &touch_was_pressed)) {
			if (ty >= 302) {
				touch_save_settings(pal, fs, bl, *show_fps_overlay, *show_save_overlay);
				wait_release(); return;
			}
			if (ty >= 65 && ty < 93) {
				pal = (pal + NUM_PALETTES + (tx < SCREEN_W / 2 ? -1 : 1)) % NUM_PALETTES;
				emu_set_palette(pal); sel = 0; draw_settings(sel);
			} else if (ty >= 120 && ty < 148) {
				if (tx < SCREEN_W / 2 && fs > 0) --fs;
				if (tx >= SCREEN_W / 2 && fs < 4) ++fs;
				emu_set_frame_skip(fs); sel = 1; draw_settings(sel);
			} else if (ty >= 175 && ty < 203) {
				if (tx < SCREEN_W / 2) bl = bl > 30 ? bl - 25 : bl;
				else bl = min(255, (int)bl + 25);
				display_set_backlight(bl); sel = 2; draw_settings(sel);
			}
		}
		// Navigation: up/down change selected row (edge detect)
		if ((b & GB_BTN_UP) && !(prev & GB_BTN_UP)) { sel = (sel==0)?3:sel-1; draw_settings(sel); }
		if ((b & GB_BTN_DOWN) && !(prev & GB_BTN_DOWN)) { sel = (sel+1)%4; draw_settings(sel); }

		// Row actions (use edge detection where appropriate)
		if (sel==0) {
			if ((b & GB_BTN_LEFT) && !(prev & GB_BTN_LEFT)) { pal = (pal+NUM_PALETTES-1)%NUM_PALETTES; emu_set_palette(pal); draw_settings(sel); }
			if ((b & GB_BTN_RIGHT) && !(prev & GB_BTN_RIGHT)) { pal = (pal+1)%NUM_PALETTES; emu_set_palette(pal); draw_settings(sel); }
		} else if (sel==1) {
			if ((b & GB_BTN_LEFT) && !(prev & GB_BTN_LEFT)) { if (fs>0) { fs--; emu_set_frame_skip(fs); draw_settings(sel); } }
			if ((b & GB_BTN_RIGHT) && !(prev & GB_BTN_RIGHT)) { if (fs<4) { fs++; emu_set_frame_skip(fs); draw_settings(sel); } }
		} else if (sel==2) {
			if ((b & GB_BTN_LEFT) && !(prev & GB_BTN_LEFT)) { if (bl>30) { bl-=25; display_set_backlight(bl); draw_settings(sel); } }
			if ((b & GB_BTN_RIGHT) && !(prev & GB_BTN_RIGHT)) { if (bl<255) { bl=min(255,bl+25); display_set_backlight(bl); draw_settings(sel); } }
		} else if (sel==3) {
			if ((b & GB_BTN_A) && !(prev & GB_BTN_A)) {
				touch_save_settings(pal, fs, bl, *show_fps_overlay, *show_save_overlay);
				wait_release();
				return;
			}
		}

		// A anywhere acts as confirm for done as well
		if ((b & GB_BTN_A) && !(prev & GB_BTN_A) && sel>=0 && sel<=2) {
			// if not on DONE, treat A as toggle to next row (optional)
			sel = (sel+1)%4; draw_settings(sel);
		}

		prev = b;
		delay(20);
	}
}
