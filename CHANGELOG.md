# ZombieVerter Dial Display — Changelog

---

## v2.4.0 — 2026-05-02

### New Features

**SavvyCAN GVRET Bridge**
The M5Dial now doubles as a SavvyCAN-compatible CAN analyser with zero additional hardware. When the WiFi AP is active, a raw TCP server on port 23 implements the GVRET binary protocol that SavvyCAN uses natively.

- Every CAN frame received by the M5Dial is forwarded to all connected SavvyCAN clients in real time via a `CANDataManager` frame observer — zero overhead when no client is connected
- Up to 4 simultaneous SavvyCAN connections
- Full GVRET command set implemented: binary mode negotiation, device info, bus count, extended bus info, comm validation, bus enable/disable, frame transmit (SavvyCAN can send frames onto the live bus)
- Frame buffering: outgoing frames are batched every 10ms rather than one TCP packet per frame
- GVRET server starts and stops automatically with the WiFi AP

To connect: enable WiFi on the dial → open SavvyCAN → Connection → Add New Device Connection → Network Connection (GVRET) → IP 192.168.4.1 → Connect → Enable Bus

**ZombieVerter Ecosystem DBC File**
First published DBC file for the ZombieVerter CAN bus ecosystem (`zombieverter_ecosystem.dbc`). Load into SavvyCAN (File → DBC File Manager) and tick Interpret Frames for live decoded signal display.

Covers 18 messages / 55 signals across all devices on the bus:
- ZombieVerter VCU broadcasts: `speed`, `tmphs`, `SOC`, `tmpm`, `U12V`
- ZombieVerter CANopen SDO: request (0x603) and response (0x583)
- IVT-S shunt: all 7 channels with correct 24-bit signed factor/offset
- SimpBMS Victron/REC protocol: limits, cell data, alarms with enum labels, name
- 0x355/0x356 ID conflicts between ZombieVerter and SimpBMS documented in comments

**Trip Logger**
Automatic telemetry logging to ESP32 NVS at 5-second intervals when speed exceeds 10 RPM. No user action required — starts and stops with vehicle movement. Persists across power cycles.

- Ring buffer stores 200 entries (~16 minutes). Once full, oldest entry is silently replaced — logging never stops
- Records: timestamp, speed (RPM), pack voltage, current, power, SOC, heatsink temp, motor temp, throttle position (`potnorm`)
- Web UI at `/triplog.html`: summary stats, three time-series charts (throttle vs speed, power vs speed, current vs temperature), data table with colour-coded warnings, CSV download, clear button

**Plot & Gauge Fix**
The Plot page, Data Logger, and Launch Gauges features in the existing web UI were broken — the firmware was missing the HTTP endpoints they depend on. All three now work:
- `/cmd?cmd=get names&repeat=N` — implemented with correct `repeat` handling
- `/cmd?cmd=stream N names` — implemented for newer SavvyCAN/plot.js builds
- `/value?id=N` — implemented for `gauges.html` per-parameter polling

**Trip Log Navigation**
TRIP LOG nav item added to the web UI sidebar (replaces INSTALL APP which has been removed). Opens `/triplog.html` in a new tab.

### Bug Fixes

- GVRET: fixed 77-bus count — `GET_NUM_BUSES` command is `0x0C` not `0x03`; our `0x03` response was being misread as a data byte producing 77 as the bus count
- GVRET: fixed frames stopping after ~15 captures — `_clientCount` was being decremented for every empty client slot on every 10ms flush cycle, racing to zero within 1-2 flushes and causing `pushFrame()` to silently discard all subsequent frames. Fixed by recounting live clients on each flush rather than maintaining a counter
- GVRET: device info response was a text string; SavvyCAN expects a binary struct (4-byte build number + 4-byte CAN0 baud + 4-byte CAN1 baud + 1-byte single-wire flag). Now correctly formatted
- `cmdGetRepeated`: values now always emitted with decimal point so `plot.js` regex matches correctly. Integer-valued parameters (e.g. `speed = 1200`) were previously emitted without a decimal and silently dropped by the regex
- WiFiManager: `/value?id=N` was calling `getParameterById()` which doesn't exist; fixed to `getParameter()`
- `TripLogger::update()` argument count mismatch (7 vs 8) resolved; `potnorm` now fully wired through struct, CSV and main loop call

### Files Added

| File | Location | Purpose |
|------|----------|---------|
| `GVRETServer.h` | `include/` | GVRET TCP server class declaration |
| `GVRETServer.cpp` | `src/` | GVRET TCP server implementation |
| `TripLogger.h` | `include/` | Trip logger class — ring buffer, NVS storage |
| `TripLogger.cpp` | `src/` | Trip logger implementation |
| `triplog.html` | `data/` | Trip log web UI — upload to SPIFFS |
| `zombieverter_ecosystem.dbc` | repo root | SavvyCAN DBC file for full bus decoding |

### Files Changed

| File | Change |
|------|--------|
| `src/main.cpp` | TripLogger init + loop update, GVRETServer frame observer registration |
| `src/WiFiManager.cpp` | GVRET start/stop with AP, `/value` + `/log` routes, `cmdGetRepeated()`, `handleValue()`, `handleTripLog()`, `handleTripLogDelete()`, `stream` command support |
| `include/WiFiManager.h` | New method declarations |
| `src/CANData.cpp` | `FrameObserver` callback added — called for every received frame |
| `include/CANData.h` | `FrameObserver` typedef, `setFrameObserver()`, `_frameObserver` member, `#include <functional>` |
| `data/index.html` | INSTALL APP removed, TRIP LOG nav item added |

### Upgrade Notes

Flash firmware and filesystem:
```
pio run -e m5stack-dial --target upload
pio run -e m5stack-dial --target uploadfs
```

`uploadfs` is required to deploy `triplog.html`. No NVS migration needed — trip log initialises cleanly from zero on first boot.

---

## v2.3.0 — 2026-04-28

PWA implementation, service worker fixes, SDO-only writes for all VCU parameter changes, speed-locked gear/motor mode edits (blocked above 50 RPM with visual warning), CAN Monitor web page with WebSocket streaming, filtering, transmit, CSV logging and inline decoding. Auto VCU parameter fetch on boot with 3 retries. OTA firmware update and VCU parameter refetch via web UI.

---

## v2.1.0 — 2026-04

Live parameter values on dial display and web UI. `/spot` endpoint serving live values from RAM. SDOManager extended to 16-bit param IDs for ZombieVerter spot values. Edit mode for Gear/Motor/Regen screens. Settings screen showing live VCU firmware version and CAN connection status.

---

## v2.0.0 — 2026-04

LVGL 8.4 UI framework, 10 complete screens, WiFi AP mode, web interface with parameter editor, SDO parameter polling round-robin, IVT-S shunt integration, SimpBMS cell voltage display.

---

## v1.9.0 — 2026-03

Full 119 ZombieVerter parameter definitions hardcoded, fixed-point encoding/decoding, complete web interface with dashboard and parameters tabs.

---

**Full version history:** v1.5.1 → v1.6.0 → v1.7.0 → v1.8.0 → v1.8.1 → v1.8.2 → v1.8.3 → v1.9.0 → v2.0.0 → v2.1.0 → v2.2.0 → v2.3.0 → **v2.4.0**
