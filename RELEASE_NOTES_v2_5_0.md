# ZombieVerter Dial Display — v2.5.0 Release Notes

**Release date:** 2026-05-16
**Platform:** M5Stack Dial (ESP32-S3)
**Previous version:** v2.4.0

---

## Headline Features

### Automotive-Grade Immobilizer System

The M5Dial now includes a full multi-factor vehicle immobilizer. The system communicates directly with the ZombieVerter VCU via SDO — when locked, it writes `DriveInhibit=2` to the VCU, physically preventing the motor from running regardless of throttle input. Unlocking writes `DriveInhibit=0` and confirms via a read-back of the spot value.

**Unlock methods:**

- **4-digit PIN** — entered via the rotary encoder on the lock screen. Rotate to select digit 0–9, press to confirm, repeat four times. PIN is stored in NVS and changeable without reflashing.
- **RFID fob** (verified working) — tap any MIFARE Classic 1K card (13.56MHz) to the back of the M5Dial. The built-in WS1850S I2C RFID reader (address 0x28, using the arozcan fork) reads the card UID and unlocks instantly if it matches a stored fob. Up to 8 fobs can be registered.
- **BLE proximity** (installed, pending verification) — UUID-based proximity unlock using advertised service UUIDs rather than MAC addresses, avoiding Android MAC randomisation. Phone runs a BLE beacon app (nRF Connect or Beacon Simulator). Unlock-only by design — locking is always deliberate to prevent accidental immobilisation while driving.

**Security behaviour:**

- Boots to lock screen with padlock icon — all other screens are blocked until unlocked
- On unlock, runs the pre-drive health check before showing the dashboard
- Auto-locks after 5 seconds without a VCU CAN heartbeat (0x500) — handles ignition-off events
- SDO retry state machine with 3 second backoff and 5 attempt limit — prevents TWAI TX queue flooding on bus errors
- DriveInhibit write confirmed by reading back `DriveInhibited` spot value (2124) before declaring unlock complete

**Fob management:**

- Double-click encoder while unlocked → fob programming mode (next card scanned is saved)
- Long-press encoder while unlocked → PIN change flow
- All fob UIDs and PIN stored in NVS, survive power cycles

**New in v2.5.0 — web UI configuration:**

- Enable/disable the immobilizer entirely from the settings page — when disabled, dial boots directly to Dashboard and DriveInhibit writes are suppressed. Useful for bench testing without a VCU connected.
- VCU CAN parameter IDs configurable without reflashing: DriveInhibit write param (default 156) and DriveInhibited read spot (default 2124). Change if your VCU firmware uses different IDs.
- All settings persisted to NVS and applied live — no reboot required.

---

### Splash Screen Logo Upload

Upload a custom PNG logo from the web UI settings page — no reflashing required. The image is converted to RGB565 on the M5Dial and stored in SPIFFS. On the next boot it renders full-screen behind the version and status labels.

- Upload uses XHR with a real progress bar — shows percentage during transfer, then "Decoding PNG on dial…" while the M5Dial processes the image
- Browser polls `/logo-status` every 500ms for the actual decode result — no more false failure messages
- Existing logo is preserved atomically — a failed upload never overwrites a working logo
- Logo buffer (~115KB) is freed immediately when leaving the splash screen, so WiFi startup is unaffected
- Remove logo button clears SPIFFS and live widget with full feedback
- Dial briefly shows the new logo and "Logo updated!" before returning to the WiFi screen

---

### Dial Screen Visibility Control

Choose which screens appear when rotating the dial from the settings web page. Changes take effect immediately — no reboot needed.

Dashboard, WiFi and Settings are always shown and cannot be disabled. All other screens (Power, Temperature, Battery, BMS, Gear, Motor Select, Regen) can be toggled individually. Selection is persisted to NVS.

---

## Bug Fixes

- **WiFi enable crash (LoadProhibited / watchdog abort)** — logo pixel buffer (~115KB) now freed in `UIManager::setScreen()` the moment the splash screen is left, before WiFi radio allocates its heap.
- **Logo upload watchdog crash (async_tcp)** — PNG decode moved off async_tcp task to loopTask via deferred buffer pattern in `WiFiManager::update()`.
- **Logo upload always reporting failure** — fixed with `/logo-status` polling; browser waits for actual decode result.
- **Delete logo not clearing live widget** — delete handler now triggers `reloadLogo()` to update the LVGL splash screen.
- **Failed upload destroying previous logo** — atomic temp-file write: `/logo.tmp` only replaces `/logo.bin` on confirmed success.
- **`getInstance()` private access compile error** — `UIManager::getInstance()` moved to public section.
- **Immobilizer SDO retry storm** — `onSDOResult()` never calls `sendDriveInhibit()` directly on failure; sets `writePending=true` and lets `update()` fire the retry after a 3 second backoff, preventing TWAI TX queue flooding.

---

## Upgrade Instructions

### Firmware files changed

Replace in `src/` and `include/`:

| File | Changes |
|------|---------|
| `src/main.cpp` | Screen mask + immob settings from NVS at boot; conditional lock/dashboard start; logo reload dial feedback; wires UIManager and Immobilizer into WiFiManager |
| `src/UIManager.cpp` | Logo buffer freed on splash screen leave; `reloadLogo()` implementation; screen mask check in `isNavigableScreen()` |
| `include/UIManager.h` | `setScreenMask()`, `getScreenMask()`, `getInstance()`, `SCREEN_MASK_FORCED`, `screenMask` member |
| `src/WiFiManager.cpp` | Deferred PNG decode in `update()`; `/logo-status` endpoint; buffer-only upload handler; `immobWriteId`/`immobReadId` + `immobEnabled` + `screenMask` in dial-settings GET/POST |
| `include/WiFiManager.h` | `setUIManager()`, `setImmobilizer()`, PNG buffer public members |
| `src/Immobilizer.cpp` | `update()` early return when disabled; `sendDriveInhibit`/`pollDriveInhibited`/`onSDOResult` use configurable member IDs |
| `include/Immobilizer.h` | `setEnabled()`, `isEnabled()`, `setInhibitParams()`, `inhibitWriteId`, `inhibitReadId`; `isUnlocked()`/`getMode()` bypass when disabled |

### Web assets changed

Replace in `data/` and run `uploadfs`:

| File | Changes |
|------|---------|
| `data/settings.html` | Logo progress bar + XHR upload with polling; screen visibility checkbox grid; immobilizer enable toggle + CAN param ID inputs; delete logo feedback |

### Flash commands

```bash
# Firmware
pio run -e m5stack-dial --target upload

# Filesystem (required — settings.html changed)
pio run -e m5stack-dial --target uploadfs
```

### First boot after upgrade

NVS keys `screenMask`, `immobEnabled`, `immobWriteId`, `immobReadId` are new. They default to safe values — all screens on, immobilizer enabled, standard param IDs (156/2124). Existing PIN, fob UIDs and BLE UUIDs are untouched.

---

## Compatibility

- ZombieVerter VCU firmware: v2.30.A (tested)
- SavvyCAN: V208 (tested)
- M5Stack Dial hardware: ESP32-S3, 8MB flash
- RFID: MIFARE Classic 1K, 13.56MHz (verified)
- IVT-S shunt: all firmware versions
- SimpBMS: Victron/REC CAN protocol

---

## Known Issues

- SimpBMS 0x355/0x356 CAN ID conflict with ZombieVerter unchanged from v2.4.0 — pending coordinated release with M5DialBMS.
- BLE proximity unlock is installed and compilable but not yet field-verified — treat as beta.
- Logo upload requires a PNG ≤ 600KB.

---

## Credits

Original ZombieVerter Display: Jamie Jones (jamiejones85)
M5Stack Dial port and extensions: Robert Wahler (robertwa1974)
GVRET protocol: Collin Kidder (collin80/SavvyCAN)
ZombieVerter VCU: Damien Maguire / openinverter.org community
