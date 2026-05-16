# RFID Implementation — Status & Technical Notes

## Status: WORKING ✅

RFID is fully implemented and tested. The M5Dial's built-in WS1850S RFID chip
is operational. MIFARE Classic 1K fobs (13.56MHz) are the recommended credential.

---

## Hardware

- **Chip:** WS1850S (13.56MHz RFID/NFC reader)
- **Interface:** I2C — address `0x28` — via M5Unified's `M5.In_I2C` (internal bus)
- **NOT SPI** — earlier attempts with SPI init always returned `0x00` (wrong interface)
- **Location:** Built into the M5Dial body behind the touchscreen face
- **Reading distance:** ~20mm; bank-card sized credentials work best

## Library

Uses the **arozcan I2C fork** of the MFRC522 library, NOT the standard
`miguelbalboa/MFRC522` SPI library.

Files are stored locally in the project:
```
lib/MFRC522/MFRC522.h
lib/MFRC522/MFRC522.cpp
```

**Do NOT add `miguelbalboa/MFRC522` to `lib_deps`** — it is the SPI version and
will conflict with the I2C fork.

---

## Initialisation — Critical Notes

**Do NOT pass `true` for RFID in `M5Dial.begin()`** — this crashes the ESP32-S3
on boot before any code runs.

Correct sequence:
```cpp
// In Hardware::init() or setup():
auto cfg = M5.config();
M5.begin(cfg);               // M5Unified init — sets up In_I2C

// Then in Immobilizer::init():
rfid.begin();                // I2C MFRC522 init — uses M5.In_I2C internally
```

The `MFRC522` object is a private member of `Immobilizer`:
```cpp
MFRC522 rfid = MFRC522(RC522_I2C_ADDRESS);  // 0x28
```

---

## Immobilizer Integration

The immobilizer uses `ImmobMode` enum to track state:

| Mode | Description |
|------|-------------|
| `LOCKED` | PIN entry active, RFID checked against stored fobs |
| `UNLOCKED` | Normal operation, RFID tap locks vehicle |
| `PROGRAM_FOB` | Next card scanned is saved to NVS |
| `CHANGE_PIN_1` | Entering new PIN (first pass) |
| `CHANGE_PIN_2` | Confirming new PIN (second pass) |

### NVS Storage
- PIN stored as 4-char string under namespace `immobilizer`, key `pin`
- Fob UIDs stored as 4-byte blobs: `fob0`, `fob1`, ... up to 8 fobs
- Both persist across power cycles

### Unlock Callback
Unlock (via PIN or RFID) fires `onUnlockCb` lambda registered in `main.cpp`:
```cpp
immobilizer.setOnUnlock([]() {
    HealthChecker::getInstance().reset();
    HealthChecker::getInstance().startCheck();
    uiManager.setScreen(SCREEN_HEALTH_CHECK);
});
```

### UI Controls
| Action | Result |
|--------|--------|
| Rotate encoder (locked) | Select PIN digit 0-9 |
| Button click (locked) | Confirm digit |
| RFID tap (locked) | Unlock if authorised fob |
| Double-click (unlocked) | Enter fob programming mode |
| Long-press (unlocked) | Enter PIN change mode |
| Double-click (programming/change mode) | Cancel |

---

## Supported Credentials

| Type | Works? | Notes |
|------|--------|-------|
| MIFARE Classic 1K fob (13.56MHz) | ✅ | Recommended — fixed UID |
| MIFARE Classic card (13.56MHz) | ✅ | Fixed UID |
| NTAG213/215/216 | ✅ | Fixed UID |
| Android phone NFC | ⚠️ | Randomised UID — unreliable as key |
| 125kHz HID/EM4100 badges | ❌ | Wrong frequency |
| Corporate access badges | ❌ | Usually 125kHz HID |

---

## Testing Checklist

- [x] RFID chip detected on I2C (version register = `0x15`)
- [x] Unauthorised card correctly rejected (`[RFID] Unauthorized fob`)
- [x] PIN entry and unlock working
- [x] NVS PIN save/load working
- [ ] Fob programming mode (awaiting MIFARE fobs)
- [ ] Fob unlock (awaiting MIFARE fobs)
- [ ] Fob lock (tap while unlocked)
- [ ] Multiple fob support
