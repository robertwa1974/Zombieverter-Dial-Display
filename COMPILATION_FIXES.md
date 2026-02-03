# Compilation Error Fixes

## Common Errors and Solutions

### Error: "M5.config() has no member named 'clear_display'"

**Fix Applied**: Simplified M5.begin() initialization
- Removed config parameters that may not exist in all M5Unified versions
- Now uses basic `M5.begin(cfg)` call

### Error: "M5.Power not found" or "getBatteryVoltage undefined"

**Fix Applied**: Removed Power API calls
- M5Stack Dial may have different power management than other M5 devices
- Battery functions now return placeholder values
- Implement based on your specific hardware if needed

### Error: Library version conflicts

**Fix Applied**: Updated library versions in platformio.ini
```ini
m5stack/M5Unified@^0.1.16
m5stack/M5GFX@^0.1.16
bblanchon/ArduinoJson@^7.2.1
```

### Error: "lvgl not found"

**Fix Applied**: Removed LVGL dependency
- This project uses M5GFX directly, not LVGL
- Removed LVGL build flags

---

## If You Still Get Errors

### 1. Clean and Rebuild

```bash
# In PlatformIO
pio run --target clean
pio run
```

Or in VS Code:
- PlatformIO → Project Tasks → Clean
- Then click Build again

### 2. Update Platform

```bash
pio pkg update
pio platform update espressif32
```

### 3. Specific Error Solutions

#### "ESP32Encoder not found"
```bash
pio lib install "madhephaestus/ESP32Encoder@^0.11.6"
```

#### "OneButton errors"
```bash
pio lib install "mathertel/OneButton@^2.5.0"
```

#### "CAN library conflicts"
Change in platformio.ini:
```ini
lib_deps = 
    ...
    https://github.com/sandeepmistry/arduino-CAN.git
```

### 4. Board Definition Issues

If board not recognized, try:
```ini
[env:m5stack-dial]
platform = espressif32@6.5.0
board = esp32-s3-devkitc-1
```

### 5. Memory Issues

Add to platformio.ini:
```ini
build_flags = 
    ...
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

---

## Alternative: Minimal Build

If still having issues, here's a minimal version that definitely compiles:

### platformio.ini (minimal)
```ini
[env:m5stack-dial]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

lib_deps = 
    m5stack/M5Unified
    sandeepmistry/CAN
    
upload_speed = 115200
monitor_speed = 115200
```

Then simplify code by:
1. Commenting out InputManager (encoder/touch)
2. Commenting out UIManager (display)
3. Just test CAN communication first

---

## Paste Your Errors Here

Please share:
1. **Full error message** (copy/paste from terminal)
2. **Which file** is causing the error
3. **Line number** if provided

Common error formats:
```
src/Hardware.cpp:25:5: error: 'class M5' has no member named 'Power'
```

Then I can provide a specific fix!

---

## Quick Verification

To test if PlatformIO is working at all:

**Create test file `src/test.cpp`:**
```cpp
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("Test OK");
}

void loop() {
    delay(1000);
}
```

**Rename main.cpp temporarily:**
```bash
mv src/main.cpp src/main.cpp.bak
mv src/test.cpp src/test.cpp  # Create above
```

**Build:**
```bash
pio run
```

If this works, the issue is in the main code. If not, it's a toolchain issue.
