# QUICK START GUIDE

## What You Need

1. **M5Stack Dial** device
2. **USB-C cable** (data capable)
3. **Computer** with USB port
4. **PlatformIO** (we'll install this)
5. **CAN Bus Unit** for M5Stack (for actual use with VCU)

## Step-by-Step Setup (15 minutes)

### STEP 1: Install Software (5 min)

**Windows/Mac/Linux:**

1. Download VS Code: https://code.visualstudio.com/
2. Install and open VS Code
3. Click the Extensions icon (square blocks on left sidebar)
4. Search for "PlatformIO IDE"
5. Click Install on "PlatformIO IDE" by PlatformIO
6. Wait for installation (may take a few minutes)
7. Restart VS Code when prompted

### STEP 2: Open Project (1 min)

1. In VS Code: File → Open Folder
2. Navigate to and select the `M5StackDial_ZombieVerter` folder
3. Click "Select Folder" / "Open"
4. Wait for PlatformIO to initialize (bottom toolbar will show icons)

### STEP 3: Build Firmware (5 min first time)

1. Look at the bottom toolbar in VS Code
2. Click the **✓ checkmark** icon (Build)
3. Wait for build to complete
   - First time: Downloads libraries (~2-3 min)
   - Subsequent builds: Much faster (~30 sec)
4. Look for "SUCCESS" in the terminal

**Troubleshooting:**
- If build fails, try: PlatformIO menu → Clean
- Then build again

### STEP 4: Upload to M5Stack Dial (2 min)

1. Connect M5Stack Dial to computer via USB-C cable
2. Wait for computer to recognize it
3. Click the **→ arrow** icon (Upload) in bottom toolbar
4. Wait for upload to complete
5. Look for "SUCCESS" message

**Troubleshooting:**
- **Upload fails?** Hold the encoder button while plugging in USB
- **Port not found?** Try a different USB cable (must be data cable)
- **Windows driver issue?** Install CH343 driver from M5Stack website

### STEP 5: Verify It's Working

The M5Stack Dial should now show:

1. **Splash screen**: "ZombieVerter Display" (2 seconds)
2. **Dashboard screen**: Shows "CAN Disconnected" at bottom
3. **Click encoder button**: Switches to next screen
4. **Rotate encoder**: Future feature for value adjustment

**Success!** The firmware is now running.

---

## Using with ZombieVerter VCU

### Hardware Connection

1. Get M5Stack CAN Bus Unit (M5Stack official accessory)
2. Connect to M5Stack Dial Grove port
3. Wire CAN Bus Unit to ZombieVerter:
   - CAN H → VCU CAN H
   - CAN L → VCU CAN L
   - GND → GND (important!)

### Power On

1. Power on M5Stack Dial
2. Power on ZombieVerter VCU
3. Wait for "CAN Connected" to appear
4. Data should start updating

---

## Customization

### Change Parameters

1. Open `src/main.cpp` in VS Code
2. Find the `sampleParams` section (~line 15)
3. Modify the JSON to match your VCU setup
4. Change parameter IDs, names, units, etc.
5. Save file
6. Click Build (✓) then Upload (→)

### Change CAN Settings

1. Open `include/Config.h`
2. Modify:
   ```cpp
   #define CAN_BAUDRATE    500000  // Your VCU baud rate
   #define CAN_NODE_ID     1       // Your VCU node ID
   ```
3. Save, Build, Upload

---

## Controls

| Action | Function |
|--------|----------|
| **Rotate encoder** | Adjust values (when editing) |
| **Single click** | Next screen |
| **Double click** | Select / Save value |
| **Long press** | Previous screen |

## Screens

1. **Dashboard** - Main view with speed, power, voltage
2. **Power** - Detailed power metrics
3. **Temperature** - Motor and inverter temps
4. **Battery** - SOC and voltage
5. **Settings** - Coming soon

---

## Troubleshooting

### Display Issues

**Blank screen:**
- Press encoder button
- Check power LED
- Re-upload firmware

**Wrong data displayed:**
- Check CAN wiring
- Verify parameter IDs in main.cpp
- Check serial monitor for errors

### CAN Bus Issues

**"CAN Disconnected" won't change:**
- Verify CAN H/L wiring (not swapped)
- Check baud rate matches VCU (500kbps default)
- Ensure VCU is powered and transmitting
- Check CAN Bus Unit is properly connected

**Data not updating:**
- Verify parameter IDs are correct
- Check node ID matches VCU
- Monitor serial output for messages

### Build/Upload Issues

**"Library not found":**
- Just wait - PlatformIO downloads automatically

**"Upload failed":**
- Hold encoder button while connecting USB
- Try different USB cable
- Check Device Manager (Windows) for port

**"Permission denied" (Linux):**
```bash
sudo usermod -a -G dialout $USER
# Log out and log back in
```

---

## Getting Help

### Debug Mode

Enable detailed logging:

1. Open `include/Config.h`
2. Change to:
   ```cpp
   #define DEBUG_SERIAL    true
   #define DEBUG_CAN       true
   ```
3. Rebuild and upload
4. View output: PlatformIO → Serial Monitor

### Serial Monitor

Click the **plug** icon in bottom toolbar to see debug messages.

---

## Next Steps

- Join OpenInverter forum: https://openinverter.org/forum/
- Customize your display
- Add new screens
- Share your setup!

---

## Safety Note

This display is for monitoring only. Always ensure your vehicle has proper safety systems and never rely solely on this display for critical vehicle information.
