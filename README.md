# ZombieVerter Display v1.3.1 - COMPLETE & READY

**Just upload - everything is already integrated!**

## Quick Start

1. Extract ZIP → Open in VS Code
2. Clean → Build → Upload  
3. Done!

## First Boot

- Splash (2 sec) → **LOCK SCREEN**
- Default PIN: **1234**
- Rotate to select digit, click to enter
- Unlocks → Dashboard

## Configure (Optional)

**Change PIN** (`include/Immobilizer.h` line 30):
```cpp
const uint8_t SECRET_PIN[4] = {1, 2, 3, 4};  // Your PIN
```

**Add RFID Card**:
1. Open Serial Monitor
2. Place card on BACK of M5Dial
3. Note UID (e.g., "DE AD BE EF")
4. Add to `include/Immobilizer.h` line 22

**Adjust Current Limit** (`src/main.cpp` line 45):
- Default: 200A
- Change bytes 4/5 for different limit

## Features

✅ 11 Screens (all functional)
✅ Immobilizer (PIN + RFID)
✅ Hardware timer (no BMS timeout!)
✅ Edit mode (gear/motor/regen)
✅ CAN integration
✅ Sharp display

## Critical: BMS Timeout Fix

This version uses hardware timer for CAN 0x351.
**Vehicle won't shut down during menu use.**

Test: Navigate menus for 60+ seconds - should stay running.

## Documentation

- **CAN_HEARTBEAT_FIX.md** - BMS timeout fix
- **IMMOBILIZER_FAQ.md** - 200A limit explained
- BUILD_INSTRUCTIONS.md
- CAN_PROTOCOL.md

**Everything else is in `archive/` and can be deleted.**

## Troubleshooting

**Won't compile:** Delete `.pio` folder, rebuild
**BMS timeout:** Check Serial for "CAN heartbeat timer started"
**RFID not working:** Card goes on BACK of device

---

**Upload and go! No manual changes needed.** 🚀

Version: 1.3.1 | Complete | Production Ready
