# CAN Pin Configuration Fix

## CRITICAL FIX: Wrong CAN Pins!

The previous versions had the **WRONG GPIO pins** configured for CAN!

### ❌ Previous (WRONG):
```cpp
#define CAN_TX_PIN          2
#define CAN_RX_PIN          1
```

### ✅ Corrected (RIGHT):
```cpp
#define CAN_TX_PIN          13  // Grove Port A
#define CAN_RX_PIN          15  // Grove Port A
```

## Why This Matters

The **M5Stack Dial Grove Port A** uses:
- **GPIO 13** - TX (transmit to CAN unit)
- **GPIO 15** - RX (receive from CAN unit)
- **5V** - Power
- **GND** - Ground

GPIO 1 and 2 are **NOT connected to the Grove port!**

## M5Stack Dial Pin Map

```
Grove Port A (the only external connector):
┌─────────────┐
│ 1  2  3  4  │
└─────────────┘
  │  │  │  │
  │  │  │  └─ GND (Black)
  │  │  └──── 5V  (Red)
  │  └─────── G15 RX (White) ← CAN RX
  └────────── G13 TX (Yellow) ← CAN TX
```

## Connection Diagram

```
M5Stack Dial
   Grove Port A
   ┌─────┐
   │G13─┼─→ TX ──┐
   │G15─┼─→ RX ──┤
   │ 5V─┼─→ VCC ─┤  M5Stack CAN Bus Unit
   │GND─┼─→ GND ─┘  (CA-IS3050G)
   └─────┘
       │
       │ (CAN H / CAN L wires)
       ↓
  ZombieVerter VCU
```

## If You Were Using GPIO 1/2

**Those pins are NOT on the Grove connector!**

They are:
- GPIO 1: Internal (RTC clock)
- GPIO 2: Internal (not broken out)

So CAN was **never going to work** with the old pin configuration.

## M5Stack CAN Bus Unit

**Model**: CA-IS3050G or similar

**Pinout** (looking at connector):
```
Grove Connector:
Pin 1: G13 (TX) - Yellow wire
Pin 2: G15 (RX) - White wire  
Pin 3: 5V       - Red wire
Pin 4: GND      - Black wire
```

**CAN Transceiver**: ISO1050 or SN65HVD230

## How to Verify Pins

### Method 1: Multimeter Test
1. Power off everything
2. Connect Grove cable
3. Check continuity:
   - Pin 1 (Yellow) → Should connect to CAN TX chip pin
   - Pin 2 (White) → Should connect to CAN RX chip pin

### Method 2: M5Stack Documentation
Check: https://docs.m5stack.com/en/unit/can

### Method 3: Visual Inspection
- Look at CAN unit PCB
- Trace which Grove pins connect to CAN chip

## After Fixing Pins

Upload the new version and you should NOW see:

```
TWAI (CAN) initialized
CAN TX: ID=0x601 Len=8 Data=[...]
CAN RX: ID=0x581 Len=8 Data=[...]  ← Should appear now!
```

## If Still No CAN RX

After fixing pins, if still no RX messages:

1. **Check wiring**:
   - CAN H → CAN H (not swapped!)
   - CAN L → CAN L
   - Common ground

2. **Check baud rate**:
   - Default: 500kbps
   - Match ZombieVerter setting

3. **Check ZombieVerter**:
   - Is CAN enabled?
   - Is it transmitting?
   - Check with CAN analyzer if available

4. **Check termination**:
   - CAN bus needs 120Ω resistors
   - ZombieVerter usually has built-in
   - May need to add if long cable

## Alternative: Port B

Some M5Stack Dial variants might have a second Grove port (Port B):
- **GPIO 1** - TX
- **GPIO 2** - RX

If your unit has Port B and you want to use it instead:

```cpp
#define CAN_TX_PIN          1   // Grove Port B
#define CAN_RX_PIN          2   // Grove Port B
```

But **Port A (GPIO 13/15) is standard** on M5Stack Dial.

## Summary

✅ **Upload the fixed version with GPIO 13/15**
✅ **CAN should now work!**
✅ **Look for "CAN RX:" messages in serial monitor**

This was likely THE problem - wrong pins meant CAN couldn't receive anything!
