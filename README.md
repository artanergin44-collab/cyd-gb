# 🎮 CYD-GB

**Game Boy emulator for the $15 ESP32 Cheap Yellow Display — fully touchscreen controlled.**

The first GB emulator running on ESP32-2432S028R (CYD) without PSRAM and without any physical buttons. Just flash, insert SD card with ROMs, and play with your fingers.

## Features

- **No extra hardware** — runs on a stock CYD board ($15)
- **Touchscreen controls** — D-pad, A, B, Start, Select all on-screen
- **SD card ROM browser** — touch to select and play
- **20 color palettes** — Classic Green, DMG, Neon, Sepia and more
- **Save system** — battery saves persist on SD card
- **Smart calibration** — 5-point touch calibration saved to NVS
- **Settings persist** — palette, frame skip, brightness remembered across reboots
- **SPIFFS ROM cache** — copies ROM from SD to flash for faster reads

## Hardware

**Required:** [ESP32-2432S028R](https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/) + FAT32 microSD card. That's it.

## Quick Start

### 1. Clone or Download

```bash
git clone https://github.com/YOUR_USERNAME/cyd-gb.git
cd cyd-gb
```

### 2. File Structure

Make sure your project folder looks exactly like this:

```
cyd-gb/
├── platformio.ini
├── partitions.csv
├── include/
│   ├── hw_config.h
│   ├── display.h
│   ├── touch_input.h
│   ├── sd_manager.h
│   ├── ui_launcher.h
│   └── emulator_bridge.h
├── src/
│   ├── main.cpp
│   ├── display.cpp
│   ├── touch_input.cpp
│   ├── sd_manager.cpp
│   ├── ui_launcher.cpp
│   └── emulator_bridge.cpp
└── lib/            (empty folder)
```

> **Important:** The `.cpp` files MUST be inside the `src/` folder and `.h` files inside `include/`. PlatformIO will not compile the project if files are in the wrong location. If you downloaded the files flat, create these folders and move the files accordingly.

### 3. Download Peanut-GB

[Peanut-GB](https://github.com/deltabeard/Peanut-GB) is the emulator core (MIT license). It is **not included** in this repo — you must download it:

```bash
curl -L -o include/peanut_gb.h \
  "https://raw.githubusercontent.com/deltabeard/Peanut-GB/master/peanut_gb.h"
```

Or manually download `peanut_gb.h` from [here](https://github.com/deltabeard/Peanut-GB/blob/master/peanut_gb.h) and place it in the `include/` folder.

### 4. Prepare SD Card

Format as FAT32 and create this structure:

```
SD Card/
├── roms/
│   ├── gb/     <- put .gb ROM files here
│   └── gbc/    <- put .gbc ROM files here
└── saves/      <- created automatically
```

### 5. Build and Flash

```bash
# First time only: erase flash to initialize SPIFFS partition
pio run -t erase --upload-port /dev/ttyUSB0

# Build and upload
pio run -t upload --upload-port /dev/ttyUSB0

# Optional: serial monitor
pio device monitor -b 115200 --port /dev/ttyUSB0
```

> On Windows replace `/dev/ttyUSB0` with `COM3` (or your port).
> On macOS use `/dev/tty.usbserial-*`.

## Controls

| Button | Location |
|--------|----------|
| D-Pad | Bottom-left |
| A / B | Bottom-right |
| Start / Select | Bottom-center |
| Pause Menu | Top-right **II** icon |

**Pause menu:** Save, Load, Settings, Calibrate, Quit.

**Settings:** 20 color palettes, frame skip (0-4), brightness control.

## How It Works

CYD has no PSRAM (only 320KB RAM), so a 1MB ROM can't fit in memory. CYD-GB solves this with:

1. **SPIFFS cache** — ROM copied from SD to internal flash (10x faster reads)
2. **Page cache** — 16x4KB LRU cache with hash lookup
3. **Bank 0 pinning** — first 32KB always in RAM
4. **Bit-bang touch SPI** — custom driver avoids bus conflicts with display and SD card

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Black screen | Try `-DILI9341_DRIVER` instead of `-DILI9341_2_DRIVER` in platformio.ini |
| Touch not working | Use Pause menu -> Calibrate |
| SPIFFS mount failed | Run `pio run -t erase` then re-flash |
| Compile error about peanut_gb.h | Download it (see step 3 above) |
| Files won't compile | Make sure .cpp files are in `src/` and .h files in `include/` |

## Credits

- [Peanut-GB](https://github.com/deltabeard/Peanut-GB) — emulator core by Mahyar Koshkouei
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — display driver
- [CYD Community](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — hardware docs

## License

MIT. Peanut-GB is also MIT, copyright 2018-2023 Mahyar Koshkouei.
