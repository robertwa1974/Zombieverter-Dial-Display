# Quick Fixes for v1.3.1 Issues

## Issue 1: Hangs After Unlock (Can't Navigate)

**Problem:** After entering PIN 1234, goes to Dashboard but can't navigate.

**Debug Steps:**
1. Open Serial Monitor (115200 baud)
2. Enter PIN 1234
3. Look for message: ">>> UNLOCKED - Switching to Dashboard <<<"
4. Try rotating encoder
5. Look for messages: "Encoder: delta=X, screen=X, unlocked=1"

**If you see "System locked - navigation disabled":**
- The immobilizer thinks it's still locked
- Check Serial for unlock confirmation

**If you don't see any encoder messages:**
- Input events aren't being processed
- Check if `inputManager.update()` is being called

**Quick Test:**
Try this in Serial Monitor after unlocking:
```cpp
// Check immobilizer state
immobilizer.isUnlocked()  // Should return true
```

---

## Issue 2: RFID Not Working

**Problem:** No serial output when placing cards

**Likely Cause:** SPI bus conflict with display

**The M5Dial uses SPI for:**
- Display (M5GFX)
- RFID (RC522)

Both trying to use the same SPI bus causes conflicts.

**Quick Fix - Disable RFID temporarily:**

Edit `include/Immobilizer.h` line 10:
```cpp
#define RFID_ENABLED        false   // Disable for now
```

**Proper Fix Options:**

**Option 1: Use separate SPI bus**
M5Dial might have VSPI and HSPI. Need to research which bus RFID uses.

**Option 2: Initialize SPI properly**
M5GFX might need to share SPI. Check if M5.begin() already init'd SPI.

**Option 3: Use SPI transactions**
Wrap RFID calls in SPI.beginTransaction() / endTransaction()

**What to check:**
1. Serial output during boot
2. Look for: "MFRC522 Software Version: 0x..."
3. If you see "WARNING: RFID reader not detected!" → SPI issue

---

## Temporary Workaround

**To test everything else while debugging:**

1. **Disable RFID:**
```cpp
// In include/Immobilizer.h
#define RFID_ENABLED false
```

2. **Add manual unlock for testing:**
```cpp
// In src/main.cpp, in setup() after immobilizer.init():
immobilizer.unlock();  // Force unlock for testing
uiManager.setScreen(SCREEN_DASHBOARD);
```

3. Upload and test navigation

---

## Debug Output to Check

**On boot, you should see:**
```
ZombieVerter Display - M5Stack Dial
====================================
Hardware initialized
Immobilizer initialized - LOCKED
MFRC522 Software Version: 0xXX   ← Check this!
M5Dial built-in RFID initialized successfully
CAN heartbeat timer started (100ms)
*** 0x351 will be sent automatically every 100ms ***
System LOCKED - showing lock screen
Setup complete!
```

**After entering 1234:**
```
PIN entry: position 0 = 1
PIN entry: position 1 = 2
PIN entry: position 2 = 3
PIN entry: position 3 = 4
>>> IMMOBILIZER UNLOCKED <<<
>>> UNLOCKED - Switching to Dashboard <<<
```

**When rotating encoder (unlocked):**
```
Encoder: delta=1, screen=2, unlocked=1
```

---

## Next Steps

1. Upload the debug version
2. Check Serial Monitor output
3. Report what messages you see
4. We'll fix based on actual behavior

---

## If Navigation Works After Disabling RFID

Then we know it's an SPI conflict. Solutions:

1. Initialize RFID after M5.begin() completes
2. Use SPI transactions properly
3. Check if M5Dial RFID is on different pins than we think
4. May need to use M5Stack's built-in RFID library instead of MFRC522

---

**Let me know what you see in Serial Monitor!**
