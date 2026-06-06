# ZombieVerter Dial Display — v2.5.2

## Summary

Housekeeping release. Web UI cleaned of all ESP8266/STM32 legacy content,
WebSocket reload fix, WiFi loop blocking fix, and encoder debug gated.
No firmware behaviour changes.

## Fixed

### Web UI hangs on reload after closing browser (WebSocket slot exhaustion)
Stale WebSocket connections from a closed browser were not being reclaimed
because `cleanupClients()` was only called inside `pushFrame()` — gated on
CAN frames arriving. On the bench or with a quiet bus, slots never freed.

Fix: `CANMonitor::update()` now calls `ws->cleanupClients()` every 1 second
unconditionally, driven from `WiFiManager::update()` which runs every loop
iteration in WiFi mode.

### Web UI fails to load at all (ERR_EMPTY_RESPONSE on initial page load)
`delay(10)` in the WiFi mode branch of `loop()` was busy-waiting and starving
the `async_tcp` FreeRTOS task of CPU time. When a browser opens 6 parallel
connections on initial page load, the TCP stack couldn't service SYN packets
fast enough and connections timed out before any data was sent.

Fix: `delay(10)` replaced with `vTaskDelay(pdMS_TO_TICKS(5))` which yields
the loop task to the FreeRTOS scheduler properly.

### Encoder debug spam in Serial output
`[ENC] raw=N divided=N last=N` was printing every 2 seconds and
`ENCODER ROTATED` was printing on every rotation regardless of `DEBUG_SERIAL`
setting. Both are now gated behind `#if DEBUG_SERIAL`.

## Changed

### Web UI — legacy ESP8266/STM32 content removed
`index.html` and `ui.js` cleaned of all inherited OpenInverter/ESP8266/STM32
content that had no function on this hardware:

**Removed from `index.html`:**
- CAN Mapping tab and modal (sent `can tx/rx` commands not implemented in firmware)
- Support tab and link to `remote.html` (openinverter.org remote support proxy)
- "Start inverter in manual mode" / "Stop inverter" dashboard buttons (stm32-sine opcodes)
- Over-the-air firmware update button (fetched stm32-sine GitHub releases)
- UPDATE tab rewritten — now describes M5Dial/PlatformIO workflow correctly
- Parameter Database section removed (submit/subscribe/unsubscribe called openinverter.org API)
- syncofs tuner link removed (STM32 AC motor field alignment tool)
- Fixed title ("Huebner Inverter Management Console" → "ZombieVerter Dial Display")
- Fixed communication error bar text (removed ESP/STM reference)

**Removed from `ui.js`** (~21KB, 41% reduction):
- `uploadFirmwareFile`, `runUpdateStep`, `doOTAUpdate` — STM32 paged flash
- `swdUpdate`, `showEraseFlashConfirmationDialog`, `performHardReset` — SWD over UART
- `showUpdateFirmwareModal`, `showOTAUpdateFirmwareModal`, `populateReleasesDropdown`, `installOTAFirmwareUpdate`
- `parameterSubmit`, `showSubscribeModal`, `parameterDatabaseSubscribe/Unsubscribe/CheckForUpdates/ApplyUpdates`
- `canMapping`, `populateExistingCanMappingTable`, `populateSpotValueDropDown`

**Deleted from `data/`:**
- `syncofs.html`, `rtc.html`, `sdcard.html`, `remote.html`, `enhanced_dashboard.html`, `log.html`

### `Connection: close` headers added to large responses
`handleTripLog`, `handleFaultLog`, and `handleSpot` now send `Connection: close`
to prevent browsers from parking these connections in their keep-alive pool.

## Files changed

| File | Change |
|---|---|
| `src/main.cpp` | `delay(10)` → `vTaskDelay(pdMS_TO_TICKS(5))` in WiFi mode loop |
| `src/CANMonitor.cpp` | Added `update()` with 1s `cleanupClients()`; removed from `pushFrame()` |
| `src/CANMonitor.h` | Added `void update()` declaration |
| `src/WiFiManager.cpp` | Calls `CANMonitor::instance().update()` in `update()`; `Connection: close` headers |
| `src/InputManager.cpp` | ENC debug prints gated behind `#if DEBUG_SERIAL` |
| `data/index.html` | Legacy content removed, title fixed, UPDATE tab rewritten |
| `data/ui.js` | ~21KB of dead functions removed |
| `data/` | 6 legacy HTML files deleted |

## Known limitations

- **Logo upload** not functional without PSRAM — deferred to future release
- **`[IMMOBILIZER] DriveInhibited read failed`** on param 2124 — non-fatal, VCU config dependent
