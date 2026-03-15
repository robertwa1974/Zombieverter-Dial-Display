# Release Notes — v2.1.0

## ZombieVerter Dial Display — Live Values

This release adds live parameter values to both the M5Stack Dial display and the WiFi web UI.

---

## What's New

### Live Values on Dial Display

All telemetry screens now show live data updating at 100ms intervals:

- **Dashboard** — speed, battery voltage, SOC ring, power (kW)
- **Power screen** — live power meter (kW), SOC
- **Temperature** — motor temp (tmpm), inverter temp (tmphs), aux temp
- **Battery** — SOC gauge, voltage, current, temperature
- **BMS** — SOC bar
- **Settings** — CAN connection status, VCU firmware version (v2.30.A), parameter count

### Live Values in Web UI

The web interface now shows live values updating every 3 seconds. A new `/spot` endpoint serves live parameter values as a compact JSON object built entirely from RAM — no SPIFFS access, no blocking.

### Edit Mode for Gear / Motor / Regen Screens

Editable screens now require explicit activation before values can be changed:

1. Rotate encoder to navigate to Gear, Motor, or Regen screen
2. Press button to **enter edit mode** — title turns orange and shows `[EDITING]`
3. Rotate encoder to change value
4. Press button again to **exit edit mode** — rotation advances to next screen

### Settings Screen

The Settings screen now displays live information:
- VCU firmware version read directly from CAN (shows v2.30.A)
- Parameter count loaded from params.json
- CAN connection status (green = connected, red = no signal)

### WiFi Screen Improvements

- Status updates to **Active** (green) when WiFi AP is running, showing actual IP address
- Status resets to **Inactive** when WiFi is disabled from any exit method (button, encoder, touch, long press)
- Layout adjusted so all labels fit within the 240×240 display

---

## Bug Fixes

- Fixed SDO polling stopping when WiFi mode was active — spot values now update continuously in both modes
- Fixed dial display showing wrong values after params.json load — all lookups now use parameter names rather than legacy short IDs
- Fixed SDO overwriting CAN broadcast values with incorrectly-scaled data — broadcast params (udc, idc, SOC, speed, tmphs, tmpm) are protected from SDO polling
- Fixed firmware version showing raw enum index (4) instead of version string (v2.30.A)
- Fixed WiFi screen "Active" status persisting after WiFi disabled

---

## Technical Notes

- SDOManager extended to support full 16-bit VCU param IDs (previously limited to uint8_t / IDs 1–255; ZombieVerter spot values use IDs 2000+)
- All SDO values correctly divided by 32 (ZombieVerter fixed-point encoding)
- `/spot` endpoint serves ~2KB JSON from RAM, avoiding all SPIFFS operations during WiFi serving
- `inverter.js` updated to call `/spot` after `cmd=json` and merge live values before rendering

---

## Upgrade Notes

- Flash firmware and filesystem image
- No changes to `params.json` format required
- `data/inverter.js` must be re-uploaded to SPIFFS (included in filesystem image)
