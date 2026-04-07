# Hardware Reference

## M5Stack Dial Grove Port B

The Grove port B on the M5Stack Dial exposes **GPIO 1 and GPIO 2** — these are used for CAN.

```
Grove Port (looking at connector):
┌─────────────────┐
│ Pin1  Pin2  Pin3  Pin4 │
│  G2    G1   5V   GND  │
└─────────────────┘
Yellow  White  Red  Black
```

**Note:** The Grove port provides 5V on Pin 3. The SN65HVD230 CAN transceiver requires **3.3V** — use the M5Dial's 3.3V output or a regulator. Do not power the transceiver directly from 5V.

## CAN Transceiver Wiring

| SN65HVD230 Pin | M5Dial Connection |
|----------------|------------------|
| VCC | 3.3V |
| GND | GND |
| TX | GPIO 2 (Grove Pin 1, Yellow) |
| RX | GPIO 1 (Grove Pin 2, White) |
| CANH | ZombieVerter CANH |
| CANL | ZombieVerter CANL |

## Firmware Pin Definitions

```cpp
#define CAN_TX_PIN  2   // GPIO 2 — Grove Pin 1 (Yellow)
#define CAN_RX_PIN  1   // GPIO 1 — Grove Pin 2 (White)
```

## CAN Bus Termination

The CAN bus requires 120Ω termination at each end. Measure between CANH and CANL with power off:

- ~60Ω = correctly terminated at both ends
- ~120Ω = only one terminator present (add one at the other end)
- Open circuit = no termination (add 120Ω resistors at both ends)

## Internal GPIO Usage (Do Not Use)

| GPIO | Function |
|------|---------|
| 11 | Touch screen SDA (internal) |
| 12 | Touch screen SCL (internal) |
| 14 | Touch screen INT (internal) |
| 40 | Display SDA (internal) |
| 41 | Display SCL (internal) |
| 42 | Encoder A |
| 43 | Button / Encoder button |
