# M5Dial ZombieVerter Interface - FIXED VERSION

## 🎉 What's Fixed

This version contains the **CORRECT SDO format** that works with ZombieVerter:

### **Critical Fix: SDO Format Corrected**

**Previous (WRONG):**
```
SDO message: [0x40, 0x01, 0x20, param_id, ...]
Index: 0x2001 ❌
```

**Current (CORRECT):**
```
SDO message: [0x40, 0x00, 0x21, param_id, ...]
Index: 0x2100 ✅
```

**Changed in:** `include/SDOManager.h`
- `SDO_FIXED_BYTE1` changed from `0x01` to `0x00`
- `SDO_FIXED_BYTE2` changed from `0x20` to `0x21`

This matches the format used by the official `openinverter_can_tool` (oic).

---

## ⚠️ CRITICAL HARDWARE REQUIREMENT

### **GROUND CONNECTION IS MANDATORY**

**CAN bus requires common ground between all devices!**

**You MUST connect:**
```
M5Dial GND ──┬── ZombieVerter CAN GND (or battery negative)
             │
PCAN GND ────┘ (if using PCAN for testing)
```

**Without proper ground:**
- ❌ SDO will NOT work
- ❌ Communication will be unreliable
- ❌ You'll get no responses

**This was proven during testing:**
- Before ground: Manual SDO sends failed
- After ground: Manual SDO sends worked perfectly

**Where to connect ground:**
- **M5Dial:** GND pin on the device
- **ZombieVerter:** CAN connector GND pin, or battery negative, or chassis ground
- **Use a wire** to connect them together

---

## 📋 What This Version Includes

### **Working Features:**
- ✅ CAN message reception (speed, voltage, temperature, etc.)
- ✅ Web interface for monitoring
- ✅ CAN Traffic viewer
- ✅ Gear change via CAN mapping (parameter 27)
- ✅ Parameter monitoring (15 key parameters)
- ✅ File upload/download (params.json)

### **Fixed/Added:**
- ✅ Correct SDO format for reading parameters
- ✅ Correct SDO format for writing parameters
- ✅ SDO save to flash command
- ✅ Proper index calculation (0x2100)

### **Known Working:**
- Parameter read via SDO (tested with `oic` tool format)
- Parameter write via SDO
- Save to flash via SDO

---

## 🚀 Quick Start

### **1. Hardware Setup**

**Connect M5Dial to ZombieVerter:**
```
M5Dial Pin      →  ZombieVerter
GPIO 2 (CAN TX) →  CAN_H
GPIO 1 (CAN RX) →  CAN_L
GND             →  CAN GND / Battery Negative  ⚠️ CRITICAL!
```

**CAN Bus Requirements:**
- Baud rate: 500 kbps
- Termination: 120Ω at each end (60Ω total)
- **Common ground between all devices**

---

### **2. Software Setup**

**Open in VSCode with PlatformIO:**
1. Extract this zip file
2. Open VSCode
3. File → Open Folder → Select extracted folder
4. PlatformIO will auto-detect the project

**Upload to M5Dial:**
1. Connect M5Dial via USB
2. PlatformIO → Upload
3. Wait for "Success"

---

### **3. Usage**

**Access Web Interface:**
1. M5Dial will create WiFi AP: `M5Dial-XXXXXX`
2. Connect to this WiFi
3. Open browser: `http://192.168.4.1`

**Features:**
- **Monitor Tab:** View real-time parameters (speed, voltage, current, etc.)
- **Parameters Tab:** View and modify all parameters (via SDO)
- **CAN Traffic Tab:** Monitor raw CAN messages

---

## 🧪 Testing SDO

### **Test 1: Read Parameter via Web Interface**

1. Go to "Parameters" tab
2. Click "Refresh" (this uses SDO to query parameters)
3. Should see actual values (not zeros)
4. Takes ~30-60 seconds to query all parameters

### **Test 2: Write Parameter**

1. Find an editable parameter (has input field)
2. Change the value
3. Click "Set"
4. Should confirm successful write

### **Test 3: Save to Flash**

1. After making changes
2. Click "Save to Flash"
3. Parameters will persist after reboot

---

## 🔧 Troubleshooting

### **"No SDO responses / All parameters show zero"**

**Check:**
1. ✅ Is M5Dial GND connected to ZombieVerter GND? (CRITICAL!)
2. ✅ Is ZombieVerter powered on?
3. ✅ Is CAN_H and CAN_L connected correctly?
4. ✅ Is 120Ω termination present?
5. ✅ Is M5Dial on same CAN bus as working devices?

**Test:**
- Try `oic read gear` from computer
- If that works but M5Dial doesn't → Check M5Dial ground connection!

---

### **"CAN Traffic shows messages but no SDO"**

**This means:**
- Broadcast CAN works ✅
- SDO doesn't work ❌

**Likely cause:** No ground connection between M5Dial and ZombieVerter

**Fix:** Connect M5Dial GND to ZombieVerter GND with a wire

---

### **"Gear change doesn't work"**

**Check:**
1. Is parameter 27 (gear) in the params.json?
2. Is CAN mapping configured in ZombieVerter?
3. Current mapping: ID 768 (0x300), position 0, length 8

**Note:** Gear change uses CAN mapping, not SDO (different mechanism)

---

## 📊 Technical Details

### **SDO Protocol Format**

**Read Parameter:**
```
TX (0x603): [0x40, 0x00, 0x21, param_id, 0x00, 0x00, 0x00, 0x00]
RX (0x583): [0x43, 0x00, 0x21, param_id, val0, val1, val2, val3]
```

**Write Parameter:**
```
TX (0x603): [0x23, 0x00, 0x21, param_id, val0, val1, val2, val3]
RX (0x583): [0x60, 0x00, 0x21, param_id, 0x00, 0x00, 0x00, 0x00]
```

**Save to Flash:**
```
TX (0x603): [0x23, 0x02, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00]
RX (0x583): [0x60, 0x02, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00]
```

**Index Calculation:**
- All parameters use index **0x2100**
- Subindex = parameter ID
- Values are 32-bit fixed-point (multiply by 32)

---

## 🔍 What Changed From Previous Version

**File:** `include/SDOManager.h`

**Line 24-25 changed from:**
```cpp
#define SDO_FIXED_BYTE1 0x01  // ❌ WRONG - was index 0x2001
#define SDO_FIXED_BYTE2 0x20
```

**To:**
```cpp
#define SDO_FIXED_BYTE1 0x00  // ✅ CORRECT - now index 0x2100
#define SDO_FIXED_BYTE2 0x21
```

**This matches the format used by:**
- Official `openinverter_can_tool` (verified in source code)
- jamiejones85's ESP32 web interface
- Standard CANopen with index 0x2100

---

## 📝 Version History

**v1.8.1 (Current - FIXED):**
- ✅ Corrected SDO format (0x00, 0x21 instead of 0x01, 0x20)
- ✅ Added ground connection documentation
- ✅ Verified against working `oic` tool

**v1.8.0 (Previous - BROKEN):**
- ❌ Wrong SDO format (0x01, 0x20)
- ❌ No SDO responses from ZombieVerter

**v1.5.1 (Working baseline):**
- ✅ CAN mapping works (gear change, etc.)
- ✅ Broadcast parameter monitoring
- ❌ No SDO implementation

---

## 🙏 Acknowledgments

**SDO format discovered through analysis of:**
- Official `openinverter_can_tool` Python source code
- jamiejones85's ESP32 web interface implementation
- Extensive testing with PCAN-View and SavvyCAN

**Critical ground connection issue identified through:**
- Testing manual SDO sends with PCAN
- Comparing working vs non-working configurations

---

## 🆘 Support

**If SDO still doesn't work after:**
1. ✅ Uploading this fixed version
2. ✅ Connecting M5Dial GND to ZombieVerter GND
3. ✅ Verifying CAN wiring and termination

**Then check:**
- Is `oic read gear` working from computer?
- Is M5Dial seeing broadcast CAN messages?
- Is there voltage between M5Dial GND and ZombieVerter GND? (should be 0V)

**For further help, provide:**
- Screenshot of web interface Parameters tab
- Output of `oic read gear` command
- Voltage measurement between M5Dial GND and ZombieVerter GND

---

## 🎯 Summary

**Two things needed for SDO to work:**
1. ✅ **Correct format** - This version has it! (0x00, 0x21)
2. ✅ **Common ground** - YOU must wire it! (M5Dial GND to ZombieVerter GND)

**With both of these, SDO will work!** 🚀

Good luck!
