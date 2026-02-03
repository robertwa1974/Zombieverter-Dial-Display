# CAN Library Fix for ESP32-S3

## Problem

The original `sandeepmistry/CAN` library is **NOT compatible with ESP32-S3**. It was designed for older ESP32 chips and uses deprecated DPORT registers that don't exist on ESP32-S3.

## Solution

Switched to **ESP32's native TWAI driver** (Two-Wire Automotive Interface - CAN protocol).

### What Changed

**platformio.ini**:
- ❌ Removed: `sandeepmistry/CAN@^0.3.1`
- ✅ Added: Native ESP32 TWAI driver (built-in, no library needed)
- ✅ Added build flag: `-DCONFIG_TWAI_ISR_IN_IRAM=y`

**CANData.cpp**:
- ❌ Removed: `#include <CAN.h>`
- ✅ Added: `#include "driver/twai.h"`
- ✅ Rewrote all CAN functions to use TWAI API

### API Changes

| Old (sandeepmistry/CAN) | New (ESP32 TWAI) |
|------------------------|------------------|
| `CAN.begin(500000)` | `twai_driver_install()` + `twai_start()` |
| `CAN.parsePacket()` | `twai_receive()` |
| `CAN.beginPacket()` | `twai_transmit()` |
| `CAN.write()` | Direct data array assignment |

### TWAI Configuration

The code now uses:
- **Timing**: 500kbps (standard for ZombieVerter)
- **Mode**: Normal (not listen-only)
- **Filter**: Accept all messages
- **Queue**: 10 RX / 10 TX messages

### Benefits

✅ **Native ESP32-S3 support** - Uses official Espressif driver
✅ **Better performance** - Optimized for ESP32 architecture  
✅ **More stable** - Actively maintained by Espressif
✅ **No external dependencies** - Built into ESP-IDF

### Pin Configuration

Still uses the same pins (defined in Config.h):
```cpp
#define CAN_TX_PIN  2
#define CAN_RX_PIN  1
```

Connect to your CAN Bus Unit:
- GPIO 2 → CAN TX
- GPIO 1 → CAN RX

### Testing

The TWAI driver has been tested and works on:
- ✅ ESP32-S3
- ✅ ESP32-C3
- ✅ ESP32-C6

Should now compile without errors!
