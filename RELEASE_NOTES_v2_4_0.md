# ZombieVerter Dial Display — v2.4.0 Release Notes

**Release date:** 2026-05-02
**Platform:** M5Stack Dial (ESP32-S3)
**Previous version:** v2.3.0

---

## Headline Features

### SavvyCAN GVRET Bridge

The M5Dial now doubles as a full SavvyCAN CAN analyser — no additional hardware needed. Enable WiFi on the dial, connect your laptop to the `ZombieVerter-Display` AP, open SavvyCAN and add a Network Connection (GVRET) to `192.168.4.1`. The dial streams every frame from the ZombieVerter bus to SavvyCAN in real time.

Combined with the new DBC file (below), every frame decodes instantly with signal names, units and scaled values — voltage in volts, current in amps, temperatures in °C, all labelled.

Frame transmit also works — you can send frames onto the live bus from SavvyCAN's Send Frames window for testing and diagnostics.

### ZombieVerter Ecosystem DBC File

`zombieverter_ecosystem.dbc` is included in the repo root — the first published DBC file covering the full ZombieVerter installation CAN bus. Load it into SavvyCAN via File → DBC File Manager.

Covers ZombieVerter VCU broadcasts, CANopen SDO protocol, IVT-S shunt (all 7 channels with correct 24-bit signed scaling), and SimpBMS Victron/REC protocol. Signal enum labels decode alarm states to text (OK / Warning / Alarm). ID conflicts between ZombieVerter and SimpBMS on 0x355 and 0x356 are documented in the file comments.

### Trip Logger

The M5Dial now silently logs driving telemetry to non-volatile storage (NVS) whenever the motor is turning. No setup needed.

**What's logged every 5 seconds:** speed (RPM), pack voltage, pack current, power (kW), SOC, heatsink temp, motor temp, throttle position (`potnorm`).

**Capacity:** 200 entries — approximately 16 minutes of continuous driving. After that it loops automatically, replacing the oldest entry with the newest. Data survives power cycles.

**Access:** enable WiFi → open `http://192.168.4.1/triplog.html` in any browser. Shows:
- Summary stats: max speed, min voltage, peak power, peak throttle, SOC consumed, max motor temp
- Three time-series charts:
  - Throttle % vs Speed — shows driver input vs vehicle response, reveals current limiting
  - Power kW vs Speed — shows drivetrain efficiency shape
  - Current vs Motor/Heatsink temperature — shows thermal lag after load events
- Scrollable data table with colour-coded temperature warnings
- CSV download for further analysis in Excel or SavvyCAN
- Clear button with confirmation

### Plot & Gauge now working

The Plot page, Data Logger and Launch Gauges features in the existing web UI were previously broken due to missing firmware endpoints. All three now work correctly. Select signals from the parameter list, click Start Plot for a live scrolling chart, or Launch Gauges for analogue needle gauges.

---

## Bug Fixes

Several GVRET bugs were worked through during development and are resolved in this release:

- **77 buses shown in SavvyCAN** — wrong command byte mapping for `GET_NUM_BUSES` (`0x0C` not `0x03`)
- **Frames stop after ~15 captures** — client counter was racing to zero on every flush cycle due to a decrement-per-empty-slot bug; fixed by recounting live connections on each flush
- **SavvyCAN showing Not Connected despite frames flowing** — device info response was a text string; SavvyCAN requires a specific binary struct with build number and baud rates

---

## Upgrade Instructions

1. Copy new source files to your project:
   - `include/GVRETServer.h`
   - `src/GVRETServer.cpp`
   - `include/TripLogger.h`
   - `src/TripLogger.cpp`
   - `data/triplog.html`

2. Replace modified files:
   - `src/main.cpp`
   - `src/WiFiManager.cpp`
   - `include/WiFiManager.h`
   - `src/CANData.cpp`
   - `include/CANData.h`
   - `data/index.html`

3. Flash firmware:
   ```
   pio run -e m5stack-dial --target upload
   ```

4. Flash filesystem (required for triplog.html):
   ```
   pio run -e m5stack-dial --target uploadfs
   ```

5. First boot after upgrade: trip log initialises cleanly from zero — no NVS migration needed.

---

## Compatibility

- ZombieVerter VCU firmware: v2.30.A (tested)
- SavvyCAN: V208 (tested, Build Nov 30 2022)
- M5Stack Dial hardware: ESP32-S3, 8MB flash
- IVT-S shunt: all firmware versions
- SimpBMS: Victron/REC CAN protocol

---

## Known Issues

- SimpBMS broadcasts on 0x355 and 0x356 conflict with ZombieVerter broadcasts on the same IDs. This is a protocol-level collision between the Victron/REC BMS CAN standard and ZombieVerter's custom broadcast map. In practice the M5Dial handles this by giving ZombieVerter priority on both IDs. A future release will coordinate with M5DialBMS to shift SimpBMS onto non-conflicting IDs (0x455/0x456 etc.).
- GVRET frame transmit from SavvyCAN to the live bus works but is unfiltered — any frame SavvyCAN sends will be transmitted. Use with care on a live vehicle.

---

## Credits

Original ZombieVerter Display: Jamie Jones (jamiejones85)
M5Stack Dial port and extensions: Robert Wahler (robertwa1974)
GVRET protocol: Collin Kidder (collin80/SavvyCAN, collin80/ESP32RET)
ZombieVerter VCU: Damien Maguire / openinverter.org community
