# ZombieVerter Dial Display

A real-time dashboard for the ZombieVerter VCU, running on the M5Stack Dial (ESP32-S3).

Displays live telemetry from your ZombieVerter over CAN, serves the full OpenInverter web interface over WiFi, and allows editing of Gear, Motor mode, and Regen directly from the dial — no laptop required.

---

## One-Click Install

Flash your M5Stack Dial directly from your browser — no development tools needed:

**https://robertwa1974.github.io/Zombieverter-Dial-Display**

Requires Chrome or Edge. Takes about 60 seconds.

---

## Features

- **Live telemetry** — speed, voltage, current, power, SOC, motor and inverter temperatures, BMS cell voltages, all updating at 100ms
- **Full OpenInverter web UI** — served from the Dial over WiFi, same interface as the VCU itself
- **CAN Monitor** — real-time CAN bus analyser with live traffic, filtering, transmit, CSV logging, and inline decoding for all known IDs
- **OTA firmware update** — flash new firmware wirelessly from the browser, no USB or development tools needed
- **Live web values** — `/spot` endpoint merges real-time CAN data into the web parameter list
- **Automatic parameter fetch** — downloads parameter schema directly from VCU at boot via SDO, no manual file upload needed
- **Three-tier fallback** — VCU fetch → cached SPIFFS file → built-in defaults
- **Editable parameters** — Gear, Motor mode, and Regen adjustable from the dial in edit mode
- **Speed-locked edits** — Gear and Motor changes blocked while vehicle is moving, with on-screen warning
- **All writes via SDO** — no CAN map configuration required on the VCU
- **10 screens** — Dashboard, Power, Temperature, Battery, BMS, Gear, Motor, Regen, WiFi, Settings

---

## Hardware

| Item | Details |
|------|---------|
| Display | M5Stack Dial (ESP32-S3, 240×240 round display) |
| CAN transceiver | SN65HVD230 (3.3V — critical, do not use 5V) |
| CAN TX | GPIO 2 (Grove port, yellow wire) |
| CAN RX | GPIO 1 (Grove port, white wire) |
| CAN speed | 500 kbps |
| VCU node ID | 3 (ZombieVerter default) |

**Wiring:**

```
M5Stack Dial Grove Port
Pin 1 (Yellow) → SN65HVD230 TX
Pin 2 (White)  → SN65HVD230 RX
Pin 3 (Red)    → 3.3V (use regulator — Grove provides 5V)
Pin 4 (Black)  → GND
```

---

## Controls

| Action | Result |
|--------|--------|
| Rotate encoder | Scroll through screens |
| Button click (telemetry screen) | Enable WiFi mode |
| Button click (Gear/Motor/Regen) | Enter edit mode |
| Rotate encoder in edit mode | Change value |
| Button click in edit mode | Exit edit mode, resume scrolling |
| Rotate encoder in WiFi mode | Exit WiFi mode |
| Long press button | Return to Dashboard |

---

## Screens

| Screen | What it shows |
|--------|--------------|
| Dashboard | Speed (RPM), voltage, SOC ring, power (kW) |
| Power | Live power meter with colour zones |
| Temperature | Motor temp, inverter temp, battery temp |
| Battery (SOC) | SOC gauge, voltage, current |
| BMS Status | Max/min/delta cell voltages |
| Gear Selection | Current gear — click to edit |
| Motor Mode | MG1/MG2 mode — click to edit |
| Regen Max | Regen level — click to edit |
| WiFi Config | SSID, password, IP address |
| Settings | VCU firmware version, CAN status, parameter count |

---

## WiFi Interface

Press the button on any telemetry screen to enable WiFi mode.

- **SSID:** `ZombieVerter-Display`
- **Password:** `zombieverter`
- **URL:** `http://192.168.4.1`

The web interface is identical to the OpenInverter interface — view and edit all VCU parameters, save to flash. Live values update every 3 seconds via the `/spot` endpoint.

**Additional pages:**
- `/can.html` — CAN Monitor (traffic, filter, transmit, log)
- `/ota.html` — OTA firmware update and VCU parameter refetch

---

## First Boot

On first power-up with the VCU connected and CAN active:

1. Splash screen appears
2. Display shows **"Fetching params from VCU..."**
3. Parameter schema is downloaded from VCU via SDO (~5 seconds)
4. Display shows **"VCU params loaded!"**
5. Dashboard appears with live data

The downloaded `params.json` is cached to internal storage. On subsequent boots without the VCU, the cached file is used automatically.

---

## Building from Source

Requires VS Code with the PlatformIO extension.

```bash
# Clone
git clone https://github.com/robertwa1974/Zombieverter-Dial-Display.git
cd Zombieverter-Dial-Display

# Build and upload firmware
pio run -e m5stack-dial --target upload

# Upload web interface files to SPIFFS
pio run -e m5stack-dial --target uploadfs
```

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for full details.

---

## No CAN Map Required

All parameter writes (Gear, Motor, Regen) use SDO — no CAN RX map entries needed on the VCU. Connect the display, power on, and it works.

---

## Credits

Based on the original [ZombieVerterDisplay](https://github.com/jamiejones85/ZombieVerterDisplay) by Jamie Jones (jamiejones85), which provided the CAN/SDO protocol foundation. Ported to M5Stack Dial and significantly extended by Robert W (robertwa1974).

See [CREDITS.md](CREDITS.md) for full acknowledgements.

---

## Links

- [OpenInverter Forum](https://openinverter.org/forum/)
- [ZombieVerter VCU](https://openinverter.org/wiki/Zombieverter)
- [M5Stack Dial](https://docs.m5stack.com/en/core/M5Dial)
