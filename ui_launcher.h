#pragma once
#include "sd_manager.h"

int launcher_show(RomEntry* roms, int count);
int launcher_ingame_menu();   // 0=resume 1=save 2=load 3=quit 4=calibrate 5=settings
void launcher_settings_menu(); // palette, frameskip, brightness
