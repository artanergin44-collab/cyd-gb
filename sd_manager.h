// =============================================================================
// sd_manager.h - SD Card ROM management
// =============================================================================
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_ROMS        64
#define MAX_FILENAME    48
#define ROM_PATH_GB     "/roms/gb"
#define ROM_PATH_GBC    "/roms/gbc"
#define SAVE_PATH       "/saves"

struct RomEntry {
    char     filename[MAX_FILENAME];
    char     full_path[80];
    uint32_t size;
    bool     is_gbc;  // true if from /roms/gbc
};

bool sd_init();
int  sd_scan_roms(RomEntry* list, int max_entries);  // returns count
bool sd_load_rom(const char* path, uint8_t** out_buf, uint32_t* out_size);
void sd_free_rom(uint8_t* buf);

// Save/Load game state
bool sd_save_state(const char* rom_path, const uint8_t* sram, uint32_t size);
bool sd_load_state(const char* rom_path, uint8_t* sram, uint32_t size);

// Save file path helper
void sd_get_save_path(const char* rom_path, char* save_path, int max_len);
