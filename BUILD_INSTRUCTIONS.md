# How to Build and Upload

## Quick Start (5 minutes)

### Option 1: Using VS Code + PlatformIO (Recommended)

1. **Install VS Code**
   - Download from https://code.visualstudio.com/

2. **Install PlatformIO Extension**
   - Open VS Code
   - Click Extensions icon (or Ctrl+Shift+X)
   - Search "PlatformIO IDE"
   - Click Install
   - Restart VS Code

3. **Open Project**
   - File → Open Folder
   - Select the `M5StackDial_ZombieVerter` folder

4. **Build**
   - Click the checkmark (✓) icon in the bottom toolbar
   - Wait for build to complete (first time downloads libraries)

5. **Upload**
   - Connect M5Stack Dial via USB-C
   - Click the arrow (→) icon in the bottom toolbar
   - Firmware will upload automatically

**Done!** Your M5Stack Dial should now be running the ZombieVerter Display.

---

### Option 2: Using Arduino IDE

1. **Install Arduino IDE 2.x**
   - Download from https://www.arduino.cc/en/software

2. **Install ESP32 Board Support**
   - File → Preferences
   - Additional Board Manager URLs: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Tools → Board → Boards Manager
   - Search "esp32" and install

3. **Install Libraries**
   - Tools → Manage Libraries
   - Install each:
     - M5Unified
     - M5GFX
     - lvgl (version 8.3.x)
     - ESP32Encoder
     - OneButton
     - CAN by Sandeep Mistry
     - ArduinoJson

4. **Configure**
   - Copy all files from `src/` and `include/` to sketch folder
   - Rename `main.cpp` to `M5StackDial_ZombieVerter.ino`

5. **Select Board**
   - Tools → Board → ESP32 Arduino → ESP32S3 Dev Module

6. **Upload**
   - Sketch → Upload

---

### Option 3: Command Line (Linux/Mac)

```bash
# Install PlatformIO Core
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py

# Navigate to project
cd M5StackDial_ZombieVerter

# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

---

## Manual Binary Upload

If you just want to upload a pre-compiled binary:

### Using esptool.py (All Platforms)

1. **Install esptool**
   ```bash
   pip install esptool
   ```

2. **Connect M5Stack Dial** via USB

3. **Find USB Port**
   - Windows: Check Device Manager (e.g., COM3)
   - Linux/Mac: `ls /dev/tty*` (e.g., /dev/ttyUSB0)

4. **Upload**
   ```bash
   esptool.py --chip esp32s3 --port COM3 --baud 921600 \
     write_flash -z 0x0 firmware.bin
   ```
   
   Replace `COM3` with your actual port.

### Using M5Burner (Windows/Mac)

1. Download M5Burner from https://m5stack.com/pages/download
2. Open M5Burner
3. Connect M5Stack Dial
4. Click "Custom" → "Add"
5. Select your `firmware.bin` file
6. Click "Burn"

### Using ESP Flash Download Tool (Windows)

1. Download from https://www.espressif.com/en/support/download/other-tools
2. Run the tool
3. Select "ESP32-S3"
4. Add firmware.bin at address 0x0
5. Click "START"

---

## Partition Scheme (Advanced)

The project uses a custom partition scheme for more program space:

```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, 0x300000
app1,     app,  ota_1,   0x310000,0x300000
spiffs,   data, spiffs,  0x610000,0x9F0000
```

This allows for:
- 3MB application space (plenty for this project)
- 10MB SPIFFS for future data logging

---

## Troubleshooting Build Issues

### "Library not found"
**Solution**: Wait for PlatformIO to download libraries automatically on first build.

### "Espressif 32 is not installed"
**Solution**: 
```bash
pio pkg install --platform "espressif32"
```

### "Permission denied" (Linux/Mac)
**Solution**: 
```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

### "Upload failed"
**Solutions**:
1. Hold BOOT button while plugging in USB
2. Try different USB cable (must support data)
3. Check USB port permissions
4. Reduce upload speed in platformio.ini:
   ```
   upload_speed = 115200
   ```

### "Out of memory" during compile
**Solution**: 
- Close other applications
- Or use pre-compiled binary

### M5Stack Dial not detected
**Solutions**:
1. Install CH343 USB driver (Windows)
2. Check cable supports data (not charge-only)
3. Try different USB port
4. Press reset button on M5Stack Dial

---

## Verifying Upload

After upload, you should see:

1. **Immediately**: "ZombieVerter Display" splash screen
2. **After 2 seconds**: Dashboard screen
3. **Bottom of screen**: "CAN Disconnected" (until VCU connected)

If you see a blank screen:
- Press the encoder button (should switch screens)
- Check serial monitor for error messages
- Verify power LED is on

---

## Next Steps

Once uploaded successfully:

1. Connect CAN Bus Unit to M5Stack Dial
2. Connect to ZombieVerter VCU
3. Power on and verify "CAN Connected" status
4. Customize parameters in `src/main.cpp`
5. Rebuild and upload

For detailed usage, see main README.md
