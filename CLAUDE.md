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

## Version History Summary

| Version | Key changes |
|---|---|
| v2.5.2 | Web UI cleanup (removed STM32/ESP8266 legacy), WebSocket slot fix, vTaskDelay WiFi loop fix, encoder debug gated |
| v2.5.1 | DEBUG_CAN/SDO flags, PSRAM removed, GVRET/SavvyCAN bridge working |
| v2.5.0 | Initial GVRET bridge, health checker, trip logger |

## Related Project

**M5DialBMS** — separate device running partial SimpBMS port for Tesla cell
balancing over UART. Two-device architecture is correct and intentional.
