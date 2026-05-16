# ZombieVerter Dial Display — Changelog

---

## v2.5.0 — 2026-05-16

### New Features

**Immobilizer system (web UI configuration)**
- Enable/disable toggle from settings page — disabled = boots to Dashboard, DriveInhibit writes suppressed
- Configurable VCU CAN param IDs: DriveInhibit write (default 156) and DriveInhibited read spot (default 2124)
- All immobilizer settings persisted to NVS `dialsettings`, applied live without reboot

**Immobilizer system (core — fully operational)**
- 4-digit PIN entry via rotary encoder; PIN stored in NVS, changeable without reflashing
- RFID fob unlock via built-in WS1850S I2C reader (0x28, arozcan fork); MIFARE Classic 1K verified working; up to 8 fobs stored in NVS
- BLE proximity unlock installed (UUID-based, unlock-only, Android MAC-randomisation resistant) — pending field verification
- VCU DriveInhibit SDO write/readback state machine: 3s backoff, 5 retry limit, prevents TWAI TX queue flooding
- Auto-lock on VCU heartbeat timeout (5s without 0x500)
- Pre-drive health check on every unlock
- Fob programming via double-click; PIN change via long-press

**Splash screen logo upload**
- Upload PNG from settings web page; decoded on M5Dial, stored in SPIFFS
- XHR upload with live progress bar; `/logo-status` polling for actual decode result
- Atomic write via `/logo.tmp` — failed upload never overwrites existing logo
- Logo buffer freed on splash screen leave; WiFi startup unaffected
- Dial shows "Logo updated!" confirmation after successful upload

**Dial screen visibility control**
- Per-screen enable/disable checkboxes in settings web page
- Changes applied live via `UIManager::setScreenMask()`; persisted to NVS
- Dashboard, WiFi and Settings always forced on

### Bug Fixes
- WiFi enable crash (LoadProhibited) — logo buffer freed in `setScreen()` before WiFi heap allocation
- Logo upload watchdog crash — PNG decode deferred to loopTask via `WiFiManager::update()`
- Logo upload always reporting failure — `/logo-status` polling replaces immediate POST response
- Delete logo not clearing live LVGL widget — delete triggers `reloadLogo()`
- Failed upload destroying previous logo — atomic temp-file rename
- `UIManager::getInstance()` private access compile error — moved to public section

### Files Changed
`src/main.cpp`, `src/UIManager.cpp`, `include/UIManager.h`, `src/WiFiManager.cpp`, `include/WiFiManager.h`, `src/Immobilizer.cpp`, `include/Immobilizer.h`, `data/settings.html`

---

## v2.4.0 — 2026-05-02

### New Features
- **SavvyCAN GVRET bridge** — live CAN frame streaming over WiFi (TCP/23, GVRET protocol); frame transmit supported
- **ZombieVerter ecosystem DBC file** — covers VCU broadcasts, IVT-S shunt (24-bit signed), SimpBMS Victron/REC, CANopen SDO
- **Trip Logger** — NVS ring buffer (200 entries, ~16 min); web UI with summary stats, three time-series charts, CSV download
- **Plot, Data Logger and Launch Gauges** — previously broken web UI features now working

### Bug Fixes
- SavvyCAN showing 77 buses — wrong `GET_NUM_BUSES` command byte (`0x0C` not `0x03`)
- Frames stop after ~15 captures — client counter decrement bug; fixed by recounting live connections
- SavvyCAN "Not Connected" — device info was plain text; fixed with correct binary struct

### Files Changed
`src/main.cpp`, `src/WiFiManager.cpp`, `include/WiFiManager.h`, `src/CANData.cpp`, `include/CANData.h`, `data/index.html`, `include/GVRETServer.h` (new), `src/GVRETServer.cpp` (new), `include/TripLogger.h` (new), `src/TripLogger.cpp` (new), `data/triplog.html` (new)

---

## v2.3.0

- Charging screen with SOC arc and charge ETA
- Efficiency tracker (Wh/km, persisted drive params)
- Fault logger with NVS ring buffer and web UI
- Pre-drive health checker on unlock
- Settings web page (four tabs: Safety limits, Throttle, Charge, Display)
- Parameters page with staged apply workflow (sequential SDO write, orange→green status)
- CAN Monitor web page (live frame table, per-ID statistics, CSV log)

---

## v2.2.0

- RFID immobilizer (WS1850S I2C, arozcan fork, NVS-backed PIN and fob UIDs)
- BLE proximity unlock (UUID-based, unlock-only)
- Health check screen on unlock
- Auto-lock on VCU heartbeat timeout

---

## v2.1.0

- Live parameter values on dial display and web UI (100ms update)
- SDO polling continues in WiFi mode
- `/spot` endpoint for compact live JSON
- SDOManager extended to full 16-bit param IDs (VCU spot values 2000+)
- Settings screen shows VCU firmware version, CAN status, parameter count
- Edit mode for Gear/Motor/Regen screens
- WiFi screen status reflects actual AP state

---

## v2.0.0

- LVGL 8.4 UI replacing M5GFX direct draw
- 14 screens: Splash, Lock, Dashboard, Power, Temperature, Battery, BMS, Gear, Motor, Regen, WiFi, Settings, Charging, Health Check
- Round 240×240 display optimised layouts
- VCU parameter auto-fetch on boot via SDO
- WiFi AP mode with OpenInverter-style web interface

---

## v1.9.0

- All 119 ZombieVerter parameters hardcoded (no SPIFFS dependency)
- Fixed-point ×32 encoding/decoding for correct value display
- `usesRawValue()` for configuration enums, GPIO assignments, time values

---

## v1.x.x

- v1.5.1 — Basic CAN broadcast reception
- v1.6.0 — SDO protocol support
- v1.7.0 — CAN transceiver voltage fix (3.3V required)
- v1.8.0 — Web interface with parameter display
- v1.8.1 — Dial encoder fix, immobilizer prototype
- v1.9.0 — Full 119 parameters, correct value encoding
