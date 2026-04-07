# Screen Guide

## Navigation

| Action | Result |
|--------|--------|
| Rotate encoder | Move to next/previous screen |
| Button click | WiFi mode (on telemetry screens) / Edit mode (on editable screens) |
| Long press | Return to Dashboard |

---

## Screens

### Dashboard

The main driving screen. Shows at a glance everything you need while moving.

- **RPM** — large center meter with arc indicator
- **Voltage** — pack voltage at top
- **Power** — kW at bottom
- **SOC ring** — outer arc coloured green/yellow/red by state of charge

---

### Power

Detailed power flow with a full 270° meter spanning −50kW (regen) to +150kW (discharge).

- **Colour zones** — green (regen), cyan (light load), yellow (medium), red (high)
- **SOC** — shown at bottom

---

### Temperature

Dual arc gauges with colour coding: green below 60°C, yellow 60–80°C, red above 80°C.

- **Motor temp** (tmpm) — left arc
- **Inverter temp** (tmphs) — right arc
- **Battery temp** (tmpaux) — bottom label

---

### Battery (SOC)

SOC gauge with supporting data below.

- **SOC %** — large center number, coloured by charge level
- **Voltage** — pack voltage
- **Current** — pack current (appears when driving)
- **Temperature** — motor temperature

---

### BMS Status

Cell voltage monitoring from SimpBMS (Victron/REC protocol, CAN 0x373).

- **Max cell voltage** — green
- **Min cell voltage** — orange
- **Delta** — difference between max and min; turns red if >100mV
- **Max cell temp** — bottom label

---

### Gear Selection *(editable)*

Shows current gear. Press button to enter edit mode, rotate to change, press again to confirm.

Options: LOW / HIGH / AUTO / HI/LO

When in edit mode the title turns **orange** and shows `[EDITING]`.

---

### Motor Mode *(editable)*

Shows active motor configuration. Press button to enter edit mode, rotate to change, press again to confirm.

Options: MG1 only / MG2 only / MG1+MG2 / Blended

---

### Regen Max *(editable)*

Shows current regenerative braking limit. Press button to enter edit mode, rotate to adjust −35% to 0%, press again to confirm.

Arc colour: grey (low regen), yellow (moderate), green (high regen).

---

### WiFi Config

Shows connection details for the web interface access point.

- **Status** — Active (green) or Inactive
- **SSID** — ZombieVerter-Display
- **Password** — zombieverter
- **IP** — 192.168.4.1

Press button to exit WiFi mode. Rotating the encoder also exits.

---

### Settings

System status and diagnostics.

- **CAN status** — Connected (green) or No signal (red)
- **VCU firmware version** — read directly from VCU via SDO
- **Parameter count** — number of parameters loaded

---

## CAN Data Sources

| Parameter | CAN ID | Source |
|-----------|--------|--------|
| SOC | 0x355 | ZombieVerter broadcast |
| Motor temp (tmpm) | 0x356 | ZombieVerter broadcast |
| Inverter temp (tmphs) | 0x126 | ZombieVerter broadcast |
| Speed/RPM | 0x257 | ZombieVerter broadcast |
| Pack voltage (udc) | 0x522 | IVT-S shunt |
| Pack current (idc) | 0x411 | IVT-S shunt |
| BMS cell voltages | 0x373 | SimpBMS (Victron/REC) |
| All other params | SDO | Polled via SDO round-robin |

---

## Edit Mode — How It Works

Gear, Motor, and Regen screens support in-place editing without leaving the screen.

1. Navigate to the screen using the encoder
2. Press the button — title turns **orange**, shows `[EDITING]`
3. Rotate encoder to change the value — sends SDO write to VCU immediately
4. Press button again — exits edit mode, encoder returns to screen navigation

No CAN map configuration is required on the VCU — all writes use SDO.

---

## Troubleshooting

**Screen shows dashes**
CAN not connected or VCU not transmitting. Check wiring and check the Settings screen for CAN status.

**BMS screen shows no data**
SimpBMS must be configured to transmit on CAN ID 0x373. Verify BMS CAN output is enabled.

**Gear/Motor/Regen changes not taking effect**
Confirm the VCU is receiving SDO writes — check the Serial Monitor for SDO response messages.
