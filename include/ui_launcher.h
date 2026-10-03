#pragma once
#include "sd_manager.h"

#define LAUNCHER_SEL_BT_SCANNER (-2)

int launcher_show(RomEntry* roms, int count);
int launcher_ingame_menu();   // 0=resume 1=save RAM 2=load RAM 3=quit 5=settings 6=quick save 7=quick load
void launcher_settings_menu(bool* show_fps_overlay, bool* show_save_overlay); // palette, frameskip, brightness, overlays
