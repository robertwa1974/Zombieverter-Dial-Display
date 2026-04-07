# Release Notes — v2.3.0

## ZombieVerter Dial Display — CAN Monitor, OTA Updates & Safety Improvements

---

## What's New

### CAN Monitor

A full real-time CAN bus analyser is now available in the web interface at `/can.html`, accessible via the CAN MONITOR link in the navigation sidebar.

**Traffic view**
- Live scrolling table of all CAN frames with timestamp, ID, name, data bytes, and decoded value
- Rows update in place per ID rather than flooding the table — fast-updating IDs increment a counter rather than adding new rows
- Pause/resume without losing capture
- WebSocket streaming with 50ms throttle per ID to prevent browser overload

**Filter tab**
- Show all / show only / hide modes with comma-separated ID list
- Quick filter buttons: Hide SDO, Hide broadcasts, SDO only
- Click any row in the traffic view to cycle through highlight colours for that ID

**Transmit tab**
- Raw frame builder: enter CAN ID and 8 data bytes in hex, send with confirmation
- SDO builder: select node ID, command (Read/Write/Save/Reset/Segment), index, subindex and value — previews the 8-byte frame before sending, applies ×32 fixed-point encoding automatically

**Log & Stats tab**
- Triggered CSV logging to device storage — press Start, press Stop, download the file
- CSV format: timestamp_ms, id_hex, id_name, len, b0–b7, decoded
- Session statistics: total frames, frames/sec, top 10 IDs by count with bar chart

**Inline decoding** for all known IDs:
- SDO requests and responses (including parameter name lookup)
- ZombieVerter broadcasts: SOC, motor temp, inverter temp, speed
- IVT-S shunt: voltage (U1–U4), current (I/I2), temperature, power, charge counter
- SimpBMS: cell min/max/delta

---

### SDO-Only Parameter Writes

Gear, Motor mode, and Regen are now written via SDO for all changes from the dial. Previously these used direct CAN frame broadcasts which required CAN RX map entries to be configured on the VCU.

**This means the display works out of the box with no VCU CAN map configuration required.**

The response is imperceptibly slower for Gear and Motor (10–50ms vs 1ms). Regen has a small debounce to prevent SDO queue flooding during fast encoder spinning.

---

### Speed-Locked Gear and Motor Changes

Gear selection and Motor mode changes are now blocked while the vehicle is moving. Attempting to enter edit mode or change values while speed > 50 RPM shows a red warning overlay on the screen for 2 seconds.

Regen adjustment is not locked — adjusting regen while driving is intentional behaviour.

---

### OTA Firmware Update from Web Interface

A new update page at `/ota.html` (accessible via the UPDATE link in the sidebar) allows firmware to be flashed wirelessly without a USB cable or development tools.

- Drag and drop a `firmware.bin` file onto the page
- Progress bar shows upload status
- Device reboots automatically on completion
- The `firmware.bin` is produced by a PlatformIO build in `.pio/build/m5stack-dial/`

---

### Refetch VCU Parameters from Web Interface

The same update page includes a **Fetch from VCU** button that triggers a fresh parameter download from the VCU over CAN without rebooting the display.

Useful after updating VCU firmware — the display will download the new parameter list and the dial splash screen shows progress.

---

### Dial Screen Improvements

- **SOC ring on dashboard** — thickness increased from 6px to 10px, much more visible
- **Temperature arcs** — range changed from 0–120°C to 0–80°C so normal operating temperatures (20–60°C) show meaningful arc fill
- **Power meter** — scale labels reduced to three values (−50, 0, 150) for cleaner readability
- **Lock screen padlock** — replaced broken Unicode glyph with LVGL-drawn shape (rectangle body + arc shackle), correctly changes colour on unlock
- **Regen screen** — "Click to edit" instruction text now clearly visible
- **BMS screen** — all four fields (Max Cell, Min Cell, Delta, Max Temp) evenly spaced across the display; Delta turns red if >100mV
- **BMS live values** — screen now populates from `BMS_Vmax` / `BMS_Vmin` parameters rather than showing dashes

---

## Bug Fixes

- Fixed IVT-S voltage decode in CAN Monitor — bytes 2–4 little-endian ÷1000, confirmed against known pack voltage
- Fixed IVT-S current decode with same correction
- Fixed WebSocket connect/disconnect cycling — added browser-side 5s ping and server-side `cleanupClients()` every 2 seconds
- Fixed CAN Monitor status bar flashing "Disconnected" briefly during reconnect

---

## Documentation

Removed outdated development and debug documents from the repository:

- `FEATURE_CHANGES.md` — internal dev notes
- `FEEDBACK_CHANGES_v1.2.md` — internal dev notes
- `FETCH_FROM_ZOMBIEVERTER.md` — superseded by automatic VCU fetch
- `IMMOBILIZER_FAQ.md` — superseded
- `IMMOBILIZER_INTEGRATION.md` — superseded
- `RFID_STATUS.md` — debug file
- `RELEASE_NOTES_v1.1.0.md` — historical
- `M5DIAL_PINOUT_CONFIRMED.md` — replaced by `HARDWARE_REFERENCE.md`
- `PROJECT_SUMMARY.md`, `LOCK_SCREEN_IMPLEMENTATION.cpp`, `integrate.bat/.sh` — removed

Added or rewrote:
- `README.md` — complete rewrite as proper project front page with web installer link
- `HARDWARE_REFERENCE.md` — clean pinout and wiring reference
- `SCREENS_GUIDE.md` — accurate to current code
- `BUILD_INSTRUCTIONS.md` — web installer as primary option
- `QUICK_START.md` — simplified, web installer first
- `CREDITS.md` — updated with all libraries and contributions
- `CHANGELOG.md` — clean version history

---

## Upgrade Notes

- Flash firmware and filesystem image
- The UPDATE nav link now opens `/ota.html` rather than the original OpenInverter update tab
- No changes to `params.json` format
- `data/can.html` and `data/ota.html` must be uploaded to SPIFFS
