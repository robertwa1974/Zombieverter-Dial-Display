# Changelog

## v2.3.0 — CAN Monitor, OTA Updates & Safety Improvements

- CAN Monitor web page (`/can.html`) — live traffic, filtering, transmit, CSV logging, inline decoding for all known IDs including full IVT-S shunt support
- SDO-only writes for Gear, Motor, Regen — VCU CAN map no longer required for any dial function
- Speed-locked gear and motor changes — blocked above 50 RPM with red warning overlay
- OTA firmware update from web UI (`/ota.html`) — drag and drop firmware.bin, progress bar, automatic reboot
- Refetch VCU parameters button in web UI — downloads fresh params over CAN without rebooting
- Dial screen improvements: SOC arc thicker, temperature arc range 0–80°C, power meter labels simplified, lock screen padlock fixed, BMS screen evenly spaced with live cell voltages
- Fixed IVT-S voltage and current decode in CAN Monitor
- Fixed WebSocket stability in CAN Monitor
- Repository documentation rewritten and cleaned up

## v2.2.1 — Auto Parameter Fetch & One-Click Web Installer

- Automatic parameter schema download from VCU at boot via SDO segmented transfer (index 0x5001)
- Three-tier fallback: VCU fetch → cached SPIFFS → built-in defaults
- One-click web installer at https://robertwa1974.github.io/Zombieverter-Dial-Display
- GitHub Actions CI/CD — factory.bin built and deployed to GitHub Pages on every push
- SDOManager starts after VCU fetch to avoid frame routing conflicts

## v2.2.0 — SDO-Only Writes & Parameter Fetch

- All parameter writes now use SDO — VCU CAN map no longer required
- Auto VCU parameter fetch at boot with 2s delay, queue drain, and 3 retry attempts
- TWAI RX queue depth increased from 10 to 32 frames

## v2.1.0 — Live Values

- Live telemetry on all dial screens, updating at 100ms via CAN broadcast + SDO polling
- `/spot` endpoint serves live values to web interface
- SDOManager extended to support 16-bit VCU param IDs
- All parameter lookups by name — compatible with full VCU params.json
- Broadcast params protected from SDO overwrite
- Edit mode for Gear, Motor, Regen screens
- WiFi screen shows Active/IP when enabled
- VCU firmware version displayed correctly on Settings screen

## v1.1.0 — LVGL UI

- Full LVGL 8.4 UI with 10 screens optimised for 240×240 round display
- Dashboard, Power, Temperature, Battery, BMS, Gear, Motor, Regen, WiFi, Settings
- Rotary encoder navigation with WiFi AP mode
- Dynamic parameter loading from params.json
- Real-time CAN broadcast parsing and SDO polling
- Colour-coded values (temperatures, SOC, power zones)
