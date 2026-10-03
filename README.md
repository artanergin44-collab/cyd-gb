# CYD-GB 🎮

[Türkçe README](README.tr.md)

![CYD-GB pixel-art banner and hub illustration](assets/hero.svg)

*Illustration of the interface; hardware photos and verified device screenshots will be added separately.*

Game Boy emulator for the **ESP32-2432S028R Cheap Yellow Display**. Play ROMs from a microSD card using the built-in touchscreen, or connect an optional PCF8574 button board. No PSRAM is required.

> **Türkçe:** CYD-GB, ESP32 Cheap Yellow Display üzerinde çalışan bir Game Boy emülatörüdür. FAT32 microSD karta ROM dosyalarınızı koyup dokunmatik ekranla oynayabilirsiniz. Fiziksel tuşlar isteğe bağlıdır.

## Features

- Touch D-pad, A, B, Start, Select, and pause menu; optional PCF8574 I²C buttons
- ROM browser with touch and button navigation (up to 64 listed ROMs)
- Cartridge RAM saves on microSD; save and load from the pause menu
- One quick save-state slot per ROM for resuming the complete emulator session
- 20 monochrome palettes, frame skip, and brightness settings stored in NVS
- Five-point touch calibration stored in NVS
- BLE beacon scanner in the launcher
- SPIFFS ROM cache with SD fallback

**Current limits:** Audio is disabled. Game Boy Color only games are not guaranteed to run: the emulator targets the original Game Boy. CYD display and touch pinouts vary by board revision; this configuration targets the ESP32-2432S028R.

## Hardware

| Part | Required | Notes |
| --- | --- | --- |
| ESP32-2432S028R CYD | Yes | ILI9341 display and XPT2046 touch |
| FAT32 microSD card | Yes | ROM and save storage |
| PCF8574 button board | No | I²C address `0x20`; SDA GPIO 16, SCL GPIO 17 |

The pin assignments are in [`include/hw_config.h`](include/hw_config.h). Check your board's schematic before adapting the firmware to a different CYD revision.

## Install

1. Install [PlatformIO](https://platformio.org/install) and clone this repository:

   ```sh
   git clone https://github.com/artanergin44-collab/cyd-gb.git
   cd cyd-gb
   ```

2. Format the microSD card as FAT32 and copy your legally obtained ROMs:

   ```text
   microSD/
   ├── roms/
   │   ├── gb/       game.gb
   │   └── gbc/      game.gbc (compatibility varies)
   └── saves/         created by the firmware
   ```

3. Connect the CYD, then build and upload:

   ```sh
   pio run
   pio run -t upload --upload-port PORT
   pio device monitor -b 115200 --port PORT
   ```

   Replace `PORT` with your serial port, such as `/dev/ttyUSB0`, `/dev/cu.usbserial-...`, or `COM3`. `include/peanut_gb.h` is already included; no separate core download is needed.

The build uses `partitions.csv` for a 4 MB flash device. A partition change may require erasing flash. ROMs and `.sav` files live on the microSD card.

## Controls

| Action | Touchscreen | Optional buttons |
| --- | --- | --- |
| Choose a ROM or scanner | Tap an entry | D-pad, then A |
| Change launcher page | Tap the footer | Left / Right |
| Move in game | Lower-left D-pad | D-pad |
| A / B, Start / Select | Lower control area | Matching buttons |
| Pause | Top-right `II` | Start + Select |
| Save RAM / Load RAM / Quick Save / Quick Load / Settings / Quit | Tap a pause-menu item | D-pad and A; B resumes |
| Calibrate touch | `CAL` in the ROM browser | Tap the same control |

Settings are saved when you select **Done**. If touch coordinates are inaccurate, use **CAL** in the ROM browser and follow the five points.

## Storage and performance

The ROM stays on microSD or is copied to SPIFFS; it is not loaded entirely into RAM. The emulator keeps the first 32 KB and a 16-page, 4 KB page cache in RAM.

There are two different save types:

| Pause-menu action | File | Use |
| --- | --- | --- |
| Save RAM / Load RAM | `/saves/<game>.sav` | Battery-backed cartridge memory; suitable for normal game progress |
| Quick Save / Quick Load | `/saves/<game>.state` | One snapshot of the current emulator session |

A previous save is retained as `.bak` when overwritten. Quick states are tied to the same ROM and emulator state format. After a firmware update, a quick state may be incompatible; keep ordinary in-game `.sav` progress as your long-term save. ROMs requiring more than 32 KB of cartridge RAM exceed the current limit.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Touch does not respond | Flash a current build, then use `CAL`. Check the touch pins in `hw_config.h` against your CYD revision. |
| I²C read failures | The PCF8574 board is optional. If fitted, verify address `0x20` and the wiring. |
| Black screen or wrong colors | Confirm the display controller and pinout for your board. The driver is set in `platformio.ini`. |
| ROM is absent | Use FAT32 and `/roms/gb`; keep filenames within the 48-character browser field. |
| SPIFFS fails to mount | The firmware falls back to SD reads. Check the serial log and flash/partition compatibility. |
| Save does not load | The ROM must support cartridge RAM. Check `/saves` and the serial log for a size mismatch. |

## Roadmap

These are planned improvements, not features in the current firmware:

1. **Generic BLE gamepad support:** HID controller discovery, pairing, button mapping, reconnection, and testing alongside the existing BLE beacon scanner. Compatibility will be checked against actual controllers before claiming support.
2. **Save-state expansion:** Multiple named slots, a preview/metadata panel, and compatibility handling across firmware versions.
3. **Richer game hub:** Recent games, favorites, ROM search, and optional cover art stored on microSD. Animations will stay lightweight for the ESP32 display and memory budget.
4. **Hardware polish:** Verified photos and screenshots for supported CYD revisions, plus board-specific display/touch presets.

The project already includes a short animated boot screen and a pixel-style ROM hub. The reusable vector [logo](assets/logo.svg) and [banner](assets/hero.svg) are in `assets/`.

## Project layout

- `src/` — launcher, display, input, storage, emulator bridge, and BLE scanner
- `include/` — headers and the bundled Peanut-GB core
- `platformio.ini` — board, libraries, and display configuration
- `partitions.csv` — flash layout
- `assets/` — pixel logo and README banner

## Credits and license

CYD-GB is MIT licensed; see [LICENSE](LICENSE). The emulator core is [Peanut-GB](https://github.com/deltabeard/Peanut-GB) by Mahyar Koshkouei. Display support uses [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI). The [CYD community repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) has hardware references.
