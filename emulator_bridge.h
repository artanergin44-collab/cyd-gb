#pragma once
#include <stdint.h>
#include <stdbool.h>

bool emu_open_rom(const char* path);
void emu_close_rom();
bool emu_init(uint8_t* rom_data, uint32_t rom_size);
void emu_run_frame();
void emu_set_joypad(uint8_t buttons);
uint8_t* emu_get_cart_ram(uint32_t* size);
void emu_set_cart_ram(const uint8_t* data, uint32_t size);
void emu_set_frame_skip(uint8_t skip);
uint8_t emu_get_frame_skip();
uint32_t emu_get_fps();
void emu_reset();
uint16_t* emu_get_line_buffer();

// Palette
#define NUM_PALETTES 20
void emu_set_palette(uint8_t idx);
uint8_t emu_get_palette();
const char* emu_get_palette_name(uint8_t idx);
