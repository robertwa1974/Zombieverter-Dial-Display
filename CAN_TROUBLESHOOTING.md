# CAN Bus Troubleshooting Guide

## Problem: All values show 0

You've successfully:
✅ Connected WiFi
✅ Uploaded params.json
✅ Display is working

But values show **0** (especially temperature readings).

This means: **CAN messages are not being received OR not being parsed correctly.**

---

## Step 1: Enable CAN Debugging

This version has **DEBUG_CAN** already enabled!

Upload and open serial monitor. You should see:

### ✅ Good Output (CAN working):
```
CAN TX: ID=0x601 Len=8 Data=[40 01 00 00 00 00 00 00]
CAN RX: ID=0x581 Len=8 Data=[43 01 00 00 1A 02 00 00]
  -> SDO Response detected
  -> CMD=0x43 ParamID=1
  -> Updated Motor RPM = 538
```

### ❌ Bad Output (No CAN):
```
CAN TX: ID=0x601 Len=8 Data=[40 01 00 00 00 00 00 00]
[nothing received - no CAN RX lines]
```

### ⚠️ Weird Output (Wrong messages):
```
CAN RX: ID=0x123 Len=8 Data=[...]
  -> Not an SDO response (expected 0x581)
```

---

## Step 2: Check Your Wiring

### M5Stack Dial CAN Connections

**M5Stack Dial Grove Port** →  **CAN Bus Unit**:
- Pin 1 (Yellow): Not used
- Pin 2 (White): Not used  
- Pin 3 (Red): 5V
- Pin 4 (Black): GND

**CAN Bus Unit** → **ZombieVerter VCU**:
- **CAN H** (High) → ZombieVerter CAN H
- **CAN L** (Low) → ZombieVerter CAN L
- **GND** → Common ground

**CRITICAL**: CAN H and CAN L must NOT be swapped!

### Verify Connections:
```
M5Stack Dial
     |
     | (Grove Cable)
     |
CAN Bus Unit
     |
     | (Twisted Pair recommended)
     |
ZombieVerter VCU (CAN H / CAN L)
```

---

## Step 3: Check CAN Bus Termination

CAN bus requires **120Ω termination resistors** at BOTH ends.

**ZombieVerter** usually has built-in termination.
**M5Stack CAN Unit** does NOT have termination.

### If You Have:
- **Just M5Stack + ZombieVerter**: Should work (ZombieVerter has termination)
- **Multiple devices**: May need external 120Ω resistor between CAN H and CAN L

### How to Check:
With VCU powered OFF, measure resistance between CAN H and CAN L:
- **~60Ω**: Good (2x 120Ω in parallel)
- **~120Ω**: Okay (1x termination)
- **Open circuit**: Bad (no termination)

---

## Step 4: Check CAN Baud Rate

**Default in code**: 500kbps

**ZombieVerter default**: Usually 500kbps

### If Your ZombieVerter Uses Different Speed:

Edit `include/Config.h`:
```cpp
#define CAN_BAUDRATE    250000  // Change to match VCU
```

Common rates:
- 250kbps = 250000
- 500kbps = 500000
- 1Mbps = 1000000

---

## Step 5: Check CAN Node ID

**Default in code**: Node ID = 1

**ZombieVerter default**: Usually Node ID = 1

The code expects:
- **TX to**: 0x601 (0x600 + node 1)
- **RX from**: 0x581 (0x580 + node 1)

### If Your VCU Uses Different Node ID:

Edit `include/Config.h`:
```cpp
#define CAN_NODE_ID     1  // Change to match your VCU
```

---

## Step 6: Check Parameter IDs

The params.json file must have the CORRECT parameter IDs that match your ZombieVerter configuration.

### Common Issue:
Your params.json has:
```json
{"id": 5, "name": "Motor Temp", ...}
```

But ZombieVerter Motor Temp is actually parameter ID **10**.

### How to Find Correct IDs:

1. **ZombieVerter Web Interface**:
   - Connect to VCU web UI
   - Go to Parameters page
   - Note the parameter numbers

2. **OpenInverter Wiki**:
   - Check your inverter model docs
   - Parameter IDs listed there

3. **CAN Monitor**:
   - Use CAN analyzer
   - See which parameter IDs are being broadcast

### Fix:
Update your params.json with correct IDs, re-upload via WiFi.

---

## Step 7: Interpret Serial Debug Output

### Scenario A: No CAN Messages At All
```
[No "CAN RX:" lines appear]
```

**Causes**:
- CAN bus not connected
- Wrong pins (should be GPIO 1/2)
- CAN unit not powered
- ZombieVerter not transmitting

**Solutions**:
- Check wiring
- Verify 5V power to CAN unit
- Check ZombieVerter is running
- Try different CAN unit

### Scenario B: CAN Messages But Wrong IDs
```
CAN RX: ID=0x123 Len=8 Data=[...]
  -> Not an SDO response (expected 0x581)
```

**Causes**:
- Wrong CAN node ID
- Different CAN protocol
- Broadcast messages (not SDO)

**Solutions**:
- Check CAN_NODE_ID in Config.h
- Verify VCU is using SDO protocol
- Check what ID 0x123 actually is

### Scenario C: SDO Responses But Wrong Parameter IDs
```
CAN RX: ID=0x581 Len=8 Data=[43 0A 00 00 1E 00 00 00]
  -> SDO Response detected
  -> CMD=0x43 ParamID=10
  -> Parameter ID 10 not found in params list!
```

**Cause**: params.json doesn't have ID 10

**Solution**: Add parameter ID 10 to your params.json

### Scenario D: Parameters Update But Still Show 0
```
CAN RX: ID=0x581 Len=8 Data=[43 05 00 00 00 00 00 00]
  -> SDO Response detected
  -> CMD=0x43 ParamID=5
  -> Updated Motor Temp = 0
```

**Cause**: ZombieVerter is sending 0 as the actual value

**Solutions**:
- Motor might actually be 0°C (not running?)
- Check VCU web interface - does it show real values?
- Temperature sensor not connected to VCU?

---

## Step 8: Check ZombieVerter Configuration

### In ZombieVerter Web UI:

1. **CAN Settings**:
   - Verify CAN is enabled
   - Check baud rate
   - Check node ID

2. **SDO Configuration**:
   - SDO should be enabled
   - Check which parameters are available via SDO

3. **Parameter Values**:
   - Do values show correctly in web UI?
   - If web UI shows 0, problem is with VCU sensors

---

## Common ZombieVerter Parameter IDs

These are **typical** but may vary:

| ID | Parameter | Unit |
|----|-----------|------|
| 1 | Motor RPM | rpm |
| 2 | Motor Voltage | V |
| 3 | Motor Current | A |
| 4 | Motor Power | kW |
| 5 | Motor Temp | °C |
| 6 | Inverter Temp | °C |
| 7 | Battery Voltage | V |
| 8 | Battery Current | A |
| 10 | Throttle | % |
| 13 | Vehicle Speed | km/h |

**Check your specific inverter documentation!**

---

## Testing Without ZombieVerter

### Use CAN Test Device

If you have a CAN analyzer or another CAN device:

1. Send test message:
   ```
   ID: 0x581
   Data: 43 05 00 00 1E 00 00 00
   ```
   This simulates: Motor Temp (ID 5) = 30°C

2. M5Stack should show "Motor Temp = 30"

3. Confirms M5Stack CAN receiving works

---

## Quick Checklist

- [ ] CAN wiring correct (H/L not swapped)
- [ ] CAN unit powered (5V from Grove)
- [ ] CAN baud rate matches (500kbps default)
- [ ] CAN node ID matches (1 default)
- [ ] params.json has correct parameter IDs
- [ ] ZombieVerter CAN enabled in settings
- [ ] ZombieVerter SDO enabled
- [ ] Serial monitor shows "CAN RX:" messages
- [ ] Serial monitor shows "Updated [param]" messages
- [ ] ZombieVerter web UI shows non-zero values

---

## What To Share

Please provide:

1. **Serial output** when connected to ZombieVerter (copy/paste)
2. **Does it show "CAN RX:" messages?** (yes/no)
3. **What CAN IDs do you see?** (e.g., 0x581, 0x123)
4. **Your params.json** (first few parameters)
5. **ZombieVerter CAN settings**:
   - Baud rate?
   - Node ID?
   - SDO enabled?
6. **Do values show correctly in ZombieVerter web UI?**

---

## Most Likely Causes

1. **Wrong parameter IDs in params.json** (90% of cases)
   - Fix: Get correct IDs from VCU web interface

2. **CAN H/L swapped** (5% of cases)
   - Fix: Swap the wires

3. **Wrong baud rate** (3% of cases)
   - Fix: Match VCU baud rate in Config.h

4. **VCU not sending SDO responses** (2% of cases)
   - Fix: Enable SDO in VCU settings

With DEBUG_CAN enabled, the serial output will tell us exactly what's happening!
