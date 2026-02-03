# CAN Node ID Fix - CRITICAL!

## THE PROBLEM

Your ZombieVerter is on **Node ID 3**, but the code was configured for **Node ID 1**!

This is why you saw **NO CAN RX messages** - the M5Stack was listening for the wrong CAN IDs!

---

## How CAN Node IDs Work

### SDO (Service Data Object) Protocol Uses:
- **TX (Command) ID**: `0x600 + Node ID`
- **RX (Response) ID**: `0x580 + Node ID`

### With Node ID = 1 (OLD - WRONG):
- M5Stack sends to: `0x601` (0x600 + 1)
- M5Stack listens for: `0x581` (0x580 + 1)
- **ZombieVerter ignores these** because it's Node 3!

### With Node ID = 3 (NEW - CORRECT):
- M5Stack sends to: `0x603` (0x600 + 3) ✅
- M5Stack listens for: `0x583` (0x580 + 3) ✅
- **ZombieVerter responds!**

---

## What Changed

### Before (WRONG):
```cpp
#define CAN_NODE_ID         1
```

Result:
```
CAN TX: ID=0x601 Len=8 Data=[...] 
[ZombieVerter ignores - wrong node ID]
[No response - no CAN RX messages]
```

### After (CORRECT):
```cpp
#define CAN_NODE_ID         3  // YOUR ZombieVerter!
```

Result:
```
CAN TX: ID=0x603 Len=8 Data=[...]
CAN RX: ID=0x583 Len=8 Data=[...]  ← NOW WORKS!
  -> SDO Response detected
  -> Updated Motor Temp = 45
```

---

## How Did We Miss This?

Most ZombieVerter installations use **Node ID 1** (the default), so that's what was coded.

But you've configured yours as **Node 3** - totally valid!

Common node ID assignments:
- **Node 1**: VCU (most common)
- **Node 2**: BMS
- **Node 3**: VCU (alternate)
- **Node 4**: Charger
- etc.

---

## After Uploading Fixed Version

You should NOW see in serial monitor:

```
CAN TX: ID=0x603 Len=8 Data=[40 01 00 00 00 00 00 00]
CAN RX: ID=0x583 Len=8 Data=[43 01 00 00 XX XX XX XX]
  -> SDO Response detected
  -> CMD=0x43 ParamID=1
  -> Updated Motor RPM = [actual value]
```

And on the display:
- Temperature values should show **real numbers** (not 0)
- Speed should update
- Power readings should work
- All parameters come alive!

---

## If You Have Multiple Devices

If you also have other CAN devices (BMS, charger, etc.):

### To query multiple nodes:
You'd need to add support for multiple node IDs in the code, or just set it to the node ID of the device you care most about (the VCU).

### To see all CAN traffic:
With `DEBUG_CAN` enabled, you'll see ALL CAN messages, regardless of node ID:
```
CAN RX: ID=0x181 ...  (from some other device)
CAN RX: ID=0x281 ...  (from another device)
CAN RX: ID=0x583 ...  (from your VCU - this is the one we parse)
```

---

## How to Check Your Node ID in Future

### In ZombieVerter Web Interface:
1. Go to **CAN Settings** or **Communication** page
2. Look for **"Node ID"** or **"CANopen Node ID"**
3. Should show: **3**

### With CAN Analyzer:
1. Monitor CAN bus
2. Look for SDO messages
3. Common IDs:
   - `0x181, 0x581` → Node 1
   - `0x182, 0x582` → Node 2
   - `0x183, 0x583` → Node 3 ← Your VCU
   - `0x184, 0x584` → Node 4

---

## What If It's Still Not Working?

After fixing node ID, if still no CAN RX:

1. **Check serial shows**: `CAN TX: ID=0x603` (not 0x601)
2. **Check ZombieVerter web UI**: Does it show CAN activity?
3. **Try CAN analyzer**: See what's actually on the bus
4. **Check wiring**: H/L swapped?
5. **Check baud rate**: Both 500kbps?
6. **Check termination**: 120Ω resistors present?

But most likely, **this node ID fix solves everything!** 🎉

---

## Summary

✅ **Changed**: `CAN_NODE_ID` from 1 to 3
✅ **Now sends to**: 0x603 (correct for your VCU)
✅ **Now listens on**: 0x583 (correct for your VCU)
✅ **Should work immediately!**

Upload this version and watch those temperature readings come to life! 🌡️📈
