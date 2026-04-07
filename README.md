This project began with this Reddit post from StefanPetrick:
(https://www.reddit.com/r/FastLED/comments/1rny5j3/i_used_codex_for_the_first_time/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button)

Further info to be added.


Quoting/paraphrasing Stefan:

EMITTERS
The emitter (aka injector / color source) is anything that is drawn directly.
Think of it as a pencil or painbrush or paint spray gun. But it can be anything, for example:
- bouncing balls
- an audio-reactive pulsating ring
- Animartrix output that still contains some black areas
The emitter geometry can be static or dynamic (e.g., fixed rectangular border vs. orbiting dots)
The emitter may use one or more noise functions in its internal pipeline
The emitter output could be displayed and would be a normal animation


COLOR FLOW FIELDS
You can think of a ColorFlowField as an invisible wind that moves the previous pixels and blends them together.
Each ColorFlowField follows its own different rules and can produce characteristic outputs:
- spirals
- vortices / flows towards or away from an origin (which could be staionary or dynamic)
- polar warp flows
- directional / geometric flows?
- smoke/vapor?

---------------------------------------------------------------------------------

## Dual-Target Architecture

FlowFields runs on two ESP32 platforms from a single codebase:

| Target | Board | LED Driver | BLE | Build |
|--------|-------|-----------|-----|-------|
| **ESP32-S3** | Seeed XIAO ESP32S3 | RMT | On-chip | VSCode PlatformIO button |
| **ESP32-P4** | Waveshare ESP32-P4-WIFI6 | PARLIO | Companion ESP32-C6 via ESP-Hosted VHCI over SDIO | CLI (see below) |

### How It Works

Both targets share all source files. Compile-time guards handle platform differences:
- `#ifdef AUDIO_ENABLED` — audio capture and processing (S3 only)
- `#if __has_include("hosted_ble_bridge.h")` — ESP-Hosted BLE init (P4 only)
- `#if defined(CONFIG_IDF_TARGET_ESP32S3)` — S3-specific serial config
- `src/board_config.h` — pin assignments, matrix dimensions, LED driver selection (`BIG_BOARD` toggle)

### Key Files

| File | Purpose |
|------|---------|
| `platformio.ini` | S3 config (loaded by VSCode) |
| `platformio_p4.ini` | P4 config (CLI only) |
| `sdkconfig.defaults` | ESP-IDF settings for P4 (ESP-Hosted, NimBLE, PSRAM, PARLIO) |
| `src/board_config.h` | Hardware abstraction: pins, matrix size, LED driver |
| `src/parameterSchema.h` | cVar declarations, PARAMETER_TABLE X-macro, component registries |
| `src/bleControl.h` | NimBLE BLE transport, callbacks, state sync |
| `src/hosted_ble_bridge.cpp/.h` | P4-specific ESP-Hosted BLE controller init |
| `src/flowFieldsEngine.hpp` | Visualization pipeline: emitter/flow dispatch, rendering |

### NimBLE Libraries

The S3 and P4 use different NimBLE builds:
- **S3**: `libNimBLE/NimBLE-Arduino-2.5.0` — Arduino-compatible wrapper
- **P4**: `h2zero/esp-nimble-cpp @ 2.5.0` — ESP-IDF component (downloaded via lib_deps)

### Building

**S3** — Use the PlatformIO build/upload buttons in VSCode as normal.

**P4** — Build from the terminal (PowerShell):
```powershell
$env:PLATFORMIO_PROJECT_CONF="platformio_p4.ini"
C:/Users/Jeff/.platformio/penv/Scripts/pio.exe run -c platformio_p4.ini -t upload




Copyright and licensing info to be added.
