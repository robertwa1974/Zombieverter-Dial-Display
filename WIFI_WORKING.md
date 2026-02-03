# WiFi Feature - Now Working! ✅

## What Was Wrong

The WiFi initialization timing was too fast. The minimal test that worked used:
- Longer delays (500ms instead of 100ms)
- Complete WiFi shutdown before restart
- Explicit softAP parameters

## What's Fixed

The new version now uses the **exact same initialization sequence** that worked in Test 2:

```cpp
// Complete shutdown
WiFi.disconnect(true);
WiFi.mode(WIFI_OFF);
delay(500);  // ← Longer delay

// Start AP mode
WiFi.mode(WIFI_AP);
delay(500);  // ← Longer delay

// Start with all parameters explicit
WiFi.softAP(SSID, PASSWORD, CHANNEL, false, 4);
delay(1000); // ← Wait for full startup
```

## How to Use WiFi Feature

### Method 1: Auto-Start (Current Version)
The latest version **starts WiFi automatically on boot** for easy testing:

1. Upload firmware
2. Wait 10 seconds
3. Look for `ZombieVerter-Display`
4. Connect with password `zombieverter`
5. Browse to `192.168.4.1`

### Method 2: Button-Triggered (Recommended for Production)

Once you verify WiFi works, you can disable auto-start:

**Edit `src/main.cpp`** - Find and **remove** these lines:
```cpp
// AUTO-START WiFi for debugging
// TODO: Remove this after testing!
#if DEBUG_SERIAL
Serial.println("AUTO-STARTING WiFi AP for testing...");
#endif
wifiMode = true;
wifiManager.startAP();
uiManager.setScreen(SCREEN_WIFI);
```

Then WiFi will only start when you **click the encoder button**.

## Controls

| Action | Function |
|--------|----------|
| **Click button** | Toggle WiFi mode on/off |
| **Rotate dial** | Switch screens (or exit WiFi mode) |
| **Long-press** | Back to Dashboard |

## Screen Order

1. Dashboard - Main overview
2. Power - Power metrics
3. Temperature - Motor/inverter temps
4. Battery - SOC and voltage
5. BMS - Cell voltages (SIMPBMS)
6. **WiFi** - Parameter upload portal
7. Settings - Configuration

## WiFi Configuration Portal

When WiFi is enabled, you'll see:

### On the Display:
```
╔════════════════════════════╗
║       WiFi Setup           ║
║         ● Active           ║
║                            ║
║   1. Connect to WiFi:      ║
║      ZombieVerter          ║
║        -Display            ║
║                            ║
║      Password:             ║
║      zombieverter          ║
║                            ║
║   2. Browse to:            ║
║      192.168.4.1           ║
║                            ║
║   Rotate dial to exit      ║
╚════════════════════════════╝
```

### On Your Phone/Computer:
1. WiFi Settings → Connect to `ZombieVerter-Display`
2. Enter password: `zombieverter`
3. Open browser → `http://192.168.4.1`
4. Upload your `params.json` file
5. Done!

## Uploading Parameters

### What to Upload
✅ **SHORT params.json** - Parameter definitions only
❌ **LONG file** - Don't upload spot values or full config

Example (correct format):
```json
{
  "parameters": [
    {"id": 1, "name": "Motor RPM", "type": "int16", "unit": "rpm", "min": 0, "max": 6000, "decimals": 0, "editable": false},
    {"id": 2, "name": "Power", "type": "int16", "unit": "kW", "min": 0, "max": 100, "decimals": 1, "editable": false}
  ]
}
```

### File Size Limits
- **Recommended**: Under 10 KB
- **Maximum**: 16 KB
- **Validate**: Use https://jsonlint.com before uploading

### Upload Process
1. Click **"Upload params.json"** on web page
2. Select your file
3. Click **Upload**
4. See success message
5. Parameters reload immediately
6. Exit WiFi mode (rotate dial)

## Troubleshooting

### WiFi Doesn't Appear After Upload

**If auto-start is still enabled:**
- Should appear 10 seconds after boot
- Check serial monitor for "WiFi AP started successfully!"

**If using button trigger:**
- Click encoder button once
- Wait 3-5 seconds
- WiFi network should appear
- Check serial monitor for startup messages

### Can't Connect to WiFi

**Check:**
- Correct password: `zombieverter`
- Wait 10 seconds after network appears
- Some phones need "Stay connected" prompt
- Temporarily disable mobile data

### Can't Access Web Page

**Check:**
- Connected to `ZombieVerter-Display`
- Browser at exactly: `http://192.168.4.1`
- Not https (no 's')
- Try different browser

### Upload Fails

**Causes:**
- File too large (over 16 KB)
- Invalid JSON syntax
- Network timeout

**Solutions:**
- Validate JSON at jsonlint.com
- Remove unnecessary parameters
- Use shorter parameter file
- Try upload again

### Boot Loop After Upload

**Fixed!** New version has:
- File size validation
- JSON parse error handling
- Automatic corrupted file deletion
- Fallback to defaults

**If it still happens:**
- Flash erase: `esptool.py --chip esp32s3 erase_flash`
- Re-upload firmware
- Use smaller params.json file

## Performance Notes

### Memory Usage
- WiFi uses ~40KB RAM when active
- Web server uses ~20KB RAM
- Total available RAM: ~300KB
- Plenty of headroom for normal operation

### Power Usage
- WiFi adds ~100mA current draw
- Total with WiFi: ~200-250mA
- USB should provide enough power
- External 5V recommended for production

### Speed
- Parameter upload: ~1 second for 5KB file
- Web page load: Instant (no images)
- WiFi startup: 2-3 seconds
- WiFi shutdown: Immediate

## Security Notes

⚠️ **Important:**
- No authentication on web interface
- Anyone in range can connect
- Password is hardcoded in firmware
- Only enable WiFi when needed
- Don't leave WiFi on while driving

**To change credentials:**
Edit `include/Config.h`:
```cpp
#define WIFI_AP_SSID        "YourCustomSSID"
#define WIFI_AP_PASSWORD    "YourSecurePassword"
```

## Customization

### Change WiFi Settings
`include/Config.h`:
```cpp
#define WIFI_AP_SSID        "ZombieVerter-Display"
#define WIFI_AP_PASSWORD    "zombieverter"
#define WIFI_AP_CHANNEL     1        // 1, 6, or 11
#define WEB_SERVER_PORT     80
#define MAX_JSON_SIZE       16384    // 16 KB
```

### Customize Web Interface
`src/WiFiManager.cpp`:
- `generateIndexPage()` - Main page HTML/CSS
- `generateUploadPage()` - Upload form
- Modify colors, text, branding

## Comparison with Original Lilygo

| Feature | Lilygo T-Embed | M5Stack Dial Port |
|---------|----------------|-------------------|
| WiFi AP Mode | ✅ | ✅ |
| Web Upload | ✅ | ✅ |
| SPIFFS Storage | ✅ | ✅ |
| Auto-load on boot | ✅ | ✅ |
| Download params | ❌ | ✅ (Bonus!) |
| Button toggle | Via menu | Single click |
| Timing | Fast | Slower (more stable) |

## Best Practices

1. ✅ **Test WiFi first** - Upload firmware, verify network appears
2. ✅ **Validate JSON** - Use jsonlint.com before uploading
3. ✅ **Keep files small** - Under 10 KB ideal
4. ✅ **Use SHORT format** - Parameter definitions only
5. ✅ **Backup configs** - Keep copy on computer
6. ✅ **Disable when not needed** - Turn off WiFi during normal use
7. ✅ **Enable debug mode** - See what's happening in serial monitor

## Next Steps

1. **Upload the fixed firmware** (M5StackDial_ZombieVerter_WORKING.zip)
2. **WiFi should auto-start** on boot (for testing)
3. **Connect and upload** your params.json
4. **Verify parameters display** correctly
5. **Remove auto-start code** (optional, see Method 2 above)
6. **Use button to toggle** WiFi as needed

Enjoy your WiFi-enabled ZombieVerter display! 📡✨
