# ZombieVerter Dial Display — Claude Session Reference

Last updated: v2.5.2

## Project Overview

Custom EV inverter dashboard running on M5Stack Dial (ESP32-S3, 240×240 round
display, rotary encoder). Interfaces with ZombieVerter VCU via CAN bus using
SDO protocol. Web UI served from SPIFFS over WiFi AP.

## Hardware

- **Device:** M5Stack Dial — ESP32-S3, 8MB flash, no PSRAM
- **CAN transceiver:** SN65HVD230 — GPIO2=TX, GPIO1=RX, 500kbps, node ID 3
- **RFID:** WS1850S chip via I2C at address 0x28 using `M5.In_I2C` — NOT SPI
- **Encoder:** 4x quadrature mode — divide raw count by 4 for detent clicks
- **IVT-S shunt** on CAN bus

## Build & Flash

```bash
# Firmware
pio run -e m5stack-dial --target upload

# Filesystem (web UI)
pio run -e m5stack-dial --target uploadfs
```

**platformio.ini critical flags:** No `-DBOARD_HAS_PSRAM`, no `qio_opi` — device has no PSRAM.

### Second target: MaTouch 2.1 (`matouch21-port` branch) — HARDWARE-UNTESTED

Makerfabs MaTouch ESP32-S3 Rotary 2.1" (ST7701S 480×480, 16MB flash, 8MB OPI PSRAM).
Builds clean but has **not yet been run on hardware** (display, CST826 touch, CAN on 43/44 all unverified).

```bash
pio run -e matouch21                   # build
pio run -e matouch21 --target upload   # flash
pio run -e matouch21 --target uploadfs # web UI (partitions_16MB.csv)
```

- Board selected by `-DBOARD_MATOUCH21`; no flag = `BOARD_M5DIAL` (see `include/BoardConfig.h`).
  The Dial-only PSRAM rule above does not apply to `matouch21`, which needs `qio_opi` + `-DBOARD_HAS_PSRAM`.
- Board code lives in `src/board/` (`Hardware_M5Dial.cpp`, `Hardware_MaTouch21.cpp`, `CST826.*`).
- UI is authored at 240×240: wrap pixel values in `S()` and use `UI_FONT_nn` (`include/UIScale.h`).
- `HAS_RFID=0` on MaTouch: RFID compiled out, fob menu items hidden, PIN + BLE unlock only.
- CAN is TWAI TX 43 / RX 44 (UART connector); GPIO 1/2/21/46 are LCD pins on this board.

### Bring-up checklist (MaTouch 2.1, first power-on)

Flash the standalone test firmware before the app, then open the serial monitor (115200, USB) and keep the log:

```bash
pio run -e matouch21-bringup --target upload && pio device monitor -b 115200
```

Rotate to change screens; button actions are shown at the bottom of each one. Pass criteria:

| # | Screen | Pass |
|---|---|---|
| 1 | INFO | 16 MB flash, ~8192 KB PSRAM, Arduino 2.0.17, reset POWERON |
| 2 | COLOUR | Bars read RED/GREEN/BLUE/WHITE in those colours (R↔B swap = RGB pin order); TOP/RIGHT/BOTTOM/LEFT match the board; click-through fills are uniform |
| 3 | TOUCH | Red dot sits under the finger; after tapping TOP→RIGHT→BOTTOM→LEFT it shows *PASS*, otherwise set the reported `TOUCH_SWAP_XY/MIRROR_X/MIRROR_Y` in `BoardConfig.h` |
| 4 | INPUT | 4 raw counts per detent, CW = positive; click/double/long all logged; click then rotate ramps backlight 0→100% smoothly |
| 5 | I2C | `0x15 CST826 touch` FOUND |
| 6 | PINS | **Transceiver disconnected.** Meter/LED on the UART connector: 1 Hz pin = GPIO43 = CAN TX, 4 Hz = GPIO44 = CAN RX. Also watch GPIO43 through a reset — the ESP32-S3 ROM prints its boot banner on UART0 TX (GPIO43); if it does, that traffic reaches the CAN bus on every boot |
| 7 | CAN | Wired to the bus at 500k: RX count rises, state RUNNING, TEC/REC stay 0. Click sends ID 0x7A0. With no other node: double-click → SELF-TEST, click → RX increments |
| 8 | STRESS | ~30 FPS (LVGL refresh cap); bar/arc edges clean with load OFF and ON. Tearing, or a panel shift/jitter at each 2 s NVS write, is the trigger to discuss core 3.x (RGB bounce buffers) |

Paste the serial log back for review; the app (`-e matouch21`) goes on only after all eight pass.

## Release Process

```bash
git tag v2.x.x
git push origin main --tags
```

GitHub Actions builds `factory.bin` → deploys to GitHub Pages web installer at
`https://robertwa1974.github.io/Zombieverter-Dial-Display`

## Key Architecture

- **SDO only** — all VCU writes (gear/motor/regen/DriveInhibit) via SDO, no direct CAN frames
- **VCU CAN1** — SDO reads/writes; CAN2 — broadcast only
- **DriveInhibit:** param ID 156 (write), spot ID 2124 (read) — Rob's custom ZombieVerter branch
- **`MAX_PARAMETERS` must be ≥250** to capture all VCU params including BMS entries
- **LVGL `%f` not supported** — use `snprintf` + `%s` for floats
- **`SPIFFS.begin()` before `uiManager.init()`**
- **`M5.In_I2C` needs `delay(100)` before `rfid.begin()`**
- **Loop blocking = most common WiFi failure** — use `vTaskDelay()` not `delay()` in WiFi mode
- **SDO retry storm:** never call SDO writes directly from `onSDOResult()` on failure — use state machine

## RFID Critical Facts

- Use **arozcan I2C MFRC522 fork** in `lib/MFRC522/` — remove `miguelbalboa/MFRC522` from lib_deps
- Do NOT pass `true` for RFID in `M5Dial.begin()` — crashes device
- Call `rfid.begin()` + `rfid.PCD_Init()` manually after `M5.begin()`
- **MIFARE Classic 1K fobs (13.56MHz) only** — 125kHz badges and Android UIDs won't work

## Immobilizer

- `ImmobMode` enum: `LOCKED / UNLOCKED / PROGRAM_FOB / CHANGE_PIN_1 / CHANGE_PIN_2`
- NVS-backed PIN and fob UIDs (max 8 fobs)
- Double-click = program fob; long-press = change PIN
- Unlock → `onUnlockCb` → starts HealthChecker → `SCREEN_HEALTH_CHECK`

## Debug Flags (Config.h)

| Flag | Default | Purpose |
|---|---|---|
| `DEBUG_SERIAL` | false | General serial debug including ENC prints |
| `DEBUG_CAN` | false | CAN frame hex dump (saturates serial at bus speed) |
| `DEBUG_SDO` | false | SDO TX/RX per-transaction logging |
| `DEBUG_TOUCH` | false | Touch event coordinates |

**Never enable DEBUG_CAN in production** — saturates 115200 baud UART and stalls WiFi/TCP stack.

## Web UI Files (data/)

Active pages: `index.html`, `ui.js`, `inverter.js`, `plot.js`, `log.js`,
`wifi.js`, `wifi.html`, `modal.js`, `style.css`, `docstrings.js`, `can.html`,
`settings.html`, `triplog.html`, `faults.html`, `ota.html`, `logo.html`,
`params.json`, `manifest.json`, `sw.js`, `offline.html`, `subscription.js`

JS libraries (gz compressed): `chart.min.js.gz`, `gauge.min.js.gz`,
`chartjs-annotation.min.js.gz`, `jquery.core.min.js.gz`, `jquery.knob.min.js.gz`

**Do not add:** `syncofs.html`, `rtc.html`, `sdcard.html`, `remote.html`,
`enhanced_dashboard.html`, `log.html` — these are ESP8266/STM32 legacy files
that were removed in v2.5.2.

## Known Deferred Issues

- **Logo upload** — requires ~115KB decoded buffer, won't fit in internal heap without PSRAM. Feature is latent until redesigned or hardware with PSRAM targeted.
- **`[IMMOBILIZER] DriveInhibited read failed`** — SDO abort `0x06020000` on param 2124. Normal on VCU configs without that spot value. Non-fatal.
- **Coordinated CAN ID shift** — SimpBMS 0x355/0x356/0x373 → 0x455/0x456/0x473 (two lines in CANData.cpp + simultaneous M5DialBMS update)
- **Thunderstruck BMS support** — 29-bit extended CAN IDs for a friend's Ford Capri conversion; awaiting CAN monitor snapshot
- **`Hardware::sleep()`/`wake()` brightness bug (fix on main)** — `sleep()` calls `setBacklight(0)`, which overwrites `currentBrightness`, so `wake()` restores 0 and the screen stays dark. Latent: nothing calls sleep/wake today. Fix: write the backlight directly in `sleep()` without touching `currentBrightness`. Present in both board files.

## Version History Summary

| Version | Key changes |
|---|---|
| v2.5.2 | Web UI cleanup (removed STM32/ESP8266 legacy), WebSocket slot fix, vTaskDelay WiFi loop fix, encoder debug gated |
| v2.5.1 | DEBUG_CAN/SDO flags, PSRAM removed, GVRET/SavvyCAN bridge working |
| v2.5.0 | Initial GVRET bridge, health checker, trip logger |

## Related Project

**M5DialBMS** — separate device running partial SimpBMS port for Tesla cell
balancing over UART. Two-device architecture is correct and intentional.
