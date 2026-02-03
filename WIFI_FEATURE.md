# WiFi Parameter Upload Feature

## Overview

The M5Stack Dial now supports uploading ZombieVerter parameter JSON files via WiFi, just like the original Lilygo T-Embed version!

## How It Works

### 1. Enable WiFi Mode

**Press the encoder button once** - The device will:
- Start a WiFi Access Point
- Display connection instructions on screen
- Show the WiFi screen with:
  - SSID: `ZombieVerter-Display`
  - Password: `zombieverter`
  - IP Address: `192.168.4.1`

### 2. Connect to WiFi

On your phone, laptop, or tablet:
1. Open WiFi settings
2. Connect to network: **ZombieVerter-Display**
3. Enter password: **zombieverter**

### 3. Upload Parameters

1. Open a web browser
2. Go to: **http://192.168.4.1**
3. You'll see the configuration portal
4. Click **"Upload params.json"**
5. Select your ZombieVerter parameters JSON file
6. Click **Upload**
7. The display will reload with your new parameters!

### 4. Exit WiFi Mode

**Rotate the dial** or **long-press the button** to exit WiFi mode and return to normal operation.

## Web Interface Features

The configuration portal provides:

### Main Page (`http://192.168.4.1`)
- Device status and IP address
- Upload parameters button
- Download current parameters button
- Instructions

### Upload Page
- Simple file upload interface
- Accepts `.json` files
- Shows success/error messages

### Download Current Parameters
- Download the currently loaded parameters
- Useful for backup or editing

## Controls Summary

| Action | Function |
|--------|----------|
| **Click button** | Toggle WiFi mode on/off |
| **Rotate dial** | Exit WiFi mode (when enabled) |
| **Long-press** | Exit WiFi mode + return to Dashboard |

## File Storage

Parameters are stored in **SPIFFS** (flash file system):
- File: `/params.json`
- Max size: ~8KB
- Persists across reboots
- Auto-loads on startup

## Creating Your Parameters File

Create a JSON file like this:

```json
{
  "version": "1.0",
  "description": "My ZombieVerter Parameters",
  "parameters": [
    {
      "id": 1,
      "name": "Motor RPM",
      "type": "int16",
      "unit": "rpm",
      "min": 0,
      "max": 6000,
      "decimals": 0,
      "editable": false,
      "category": "Motor"
    },
    {
      "id": 2,
      "name": "Power",
      "type": "uint16",
      "unit": "kW",
      "min": 0,
      "max": 100,
      "decimals": 1,
      "editable": false,
      "category": "Power"
    }
  ]
}
```

### Parameter Fields

| Field | Description | Required |
|-------|-------------|----------|
| `id` | CAN parameter ID | Yes |
| `name` | Display name | Yes |
| `type` | Data type (int8, uint8, int16, uint16, int32, uint32, float) | Yes |
| `unit` | Unit string (rpm, kW, V, A, °C, %) | Yes |
| `min` | Minimum value | Yes |
| `max` | Maximum value | Yes |
| `decimals` | Decimal places for display | Yes |
| `editable` | Can user edit this parameter? | Yes |
| `category` | Grouping category (optional) | No |

## Example Workflow

1. **First Boot**
   - Device loads with default parameters
   - Click button to enter WiFi mode
   - Upload your custom params.json
   - Parameters are saved to SPIFFS

2. **Subsequent Boots**
   - Device automatically loads parameters from SPIFFS
   - No need to re-upload unless you want to change them

3. **Updating Parameters**
   - Click button to enter WiFi mode
   - Upload new params.json
   - Old parameters are replaced
   - Device reloads immediately

## Troubleshooting

### Can't Connect to WiFi

**Problem:** WiFi network not showing up
**Solution:**
- Make sure WiFi mode is enabled (click button)
- Check the WiFi screen is displayed
- Wait 5-10 seconds for AP to fully start
- Restart device and try again

### Can't Access Web Page

**Problem:** Browser can't reach 192.168.4.1
**Solution:**
- Verify you're connected to `ZombieVerter-Display`
- Some phones need "Stay connected" option
- Disable mobile data temporarily
- Try different browser
- Check IP on display matches what you're using

### Upload Fails

**Problem:** File upload doesn't work
**Solution:**
- Verify JSON file is valid (use jsonlint.com)
- Check file size is under 8KB
- Make sure file extension is `.json`
- Try uploading from different device
- Check serial monitor for errors

### Parameters Don't Load

**Problem:** Display shows old/default parameters
**Solution:**
- Verify upload succeeded (check for green success message)
- JSON might have syntax errors
- Check parameter IDs match your CAN configuration
- Enable debug mode and check serial output
- Try deleting SPIFFS and re-uploading

## Advanced: SPIFFS Management

### View SPIFFS Contents

Enable debug mode and add to `setup()`:
```cpp
File root = SPIFFS.open("/");
File file = root.openNextFile();
while(file){
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
}
```

### Clear SPIFFS

To reset parameters:
```cpp
SPIFFS.remove("/params.json");
```

### Format SPIFFS

To completely wipe flash storage:
```cpp
SPIFFS.format();
```

## Security Notes

⚠️ **Important Security Considerations:**

- WiFi password is hardcoded (change in Config.h if needed)
- Web interface has **no authentication**
- Only enable WiFi when needed
- Don't leave WiFi mode enabled while driving
- Anyone in range can connect to the AP

**To change WiFi credentials:**
Edit `include/Config.h`:
```cpp
#define WIFI_AP_SSID        "YourCustomSSID"
#define WIFI_AP_PASSWORD    "YourSecurePassword"
```

## Customization

### Change WiFi Settings

In `include/Config.h`:
```cpp
#define WIFI_AP_SSID        "ZombieVerter-Display"  // Your SSID
#define WIFI_AP_PASSWORD    "zombieverter"          // Your password
#define WIFI_AP_CHANNEL     1                       // WiFi channel
#define WEB_SERVER_PORT     80                      // HTTP port
#define MAX_JSON_SIZE       8192                    // Max file size
```

### Customize Web Interface

Edit `src/WiFiManager.cpp`:
- `generateIndexPage()` - Main page HTML
- `generateUploadPage()` - Upload page HTML
- Modify colors, layout, branding

## Comparison with Original

| Feature | Lilygo T-Embed | M5Stack Dial |
|---------|----------------|--------------|
| WiFi AP Mode | ✅ | ✅ |
| Web Upload | ✅ | ✅ |
| SPIFFS Storage | ✅ | ✅ |
| Download Parameters | ❌ | ✅ |
| Auto-load on boot | ✅ | ✅ |
| WiFi button toggle | Via menu | Single click |

## Tips

1. **Keep a backup** of your params.json on your computer
2. **Test parameters** before using in vehicle
3. **Use WiFi mode** for initial setup only
4. **Disable WiFi** during normal operation (saves power)
5. **Document your parameter IDs** for future reference

## Example Parameter Files

### Minimal Example
```json
{
  "parameters": [
    {"id": 1, "name": "Speed", "type": "int16", "unit": "rpm", "min": 0, "max": 6000, "decimals": 0, "editable": false}
  ]
}
```

### Complete Example
See `data/params.json` in the project for a full example with:
- Motor parameters
- Power parameters
- Battery parameters
- Temperature parameters
- BMS parameters (SIMPBMS)

## Next Steps

1. Create your params.json file
2. Click button to enable WiFi
3. Connect and upload
4. Verify parameters display correctly
5. Exit WiFi mode and enjoy!

Happy parameter uploading! 📡✨
