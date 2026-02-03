# Fetching params.json from ZombieVerter

## 🎯 Overview

The M5Stack Dial can now automatically fetch the `params.json` file directly from your ZombieVerter's built-in web interface! This eliminates the need to manually download/upload the file.

---

## ⚙️ Configuration

### Step 1: Update Config.h

Before compiling, update these settings in `include/Config.h`:

```cpp
// WiFi Client Mode (for fetching from ZombieVerter)
#define ZOMBIEVERTER_WIFI_SSID      "Zombieverter"     // Your ZombieVerter's WiFi SSID
#define ZOMBIEVERTER_WIFI_PASSWORD  "openinverter"     // Your ZombieVerter's WiFi password
#define ZOMBIEVERTER_IP             "192.168.4.1"      // ZombieVerter's IP address
#define ZOMBIEVERTER_PARAMS_URL     "/params.json"     // URL path to params.json
```

**Important**: Replace these values with YOUR ZombieVerter's actual WiFi settings!

---

## 🚀 How to Use

### Method 1: Via Web Interface (Recommended)

1. **Enter WiFi Mode**
   - Click the M5Dial button
   - Display shows "ZombieVerter-Display" WiFi info

2. **Connect to Display**
   - On your phone/laptop, connect to WiFi: `ZombieVerter-Display`
   - Password: `zombieverter`

3. **Open Web Interface**
   - Browse to: `http://192.168.4.1`

4. **Click "Fetch from ZombieVerter"**
   - Click the 🔄 button
   - Wait 10-15 seconds
   - Page will auto-refresh

5. **Done!**
   - params.json is now loaded
   - Exit WiFi mode by rotating the dial

---

### Method 2: Automatic on Boot (Optional)

You can add auto-fetch on boot by modifying `main.cpp`:

```cpp
// In setup(), after SPIFFS init:
if (!SPIFFS.exists("/params.json")) {
    Serial.println("No params.json found, fetching from ZombieVerter...");
    wifiManager.fetchParamsFromZombieVerter();
}
```

---

## 🔍 How It Works

1. **M5Dial stops its AP** (temporarily)
2. **Connects to ZombieVerter WiFi** as a client
3. **HTTP GET request** to `http://192.168.4.1/params.json`
4. **Downloads params.json** to SPIFFS
5. **Loads parameters** into CAN manager
6. **Restores AP mode** (if it was running)

---

## 📋 Technical Details

### WiFi Modes

The M5Dial switches between two WiFi modes:

**Access Point (AP) Mode**:
- SSID: `ZombieVerter-Display`
- Used for web configuration
- Default mode when button clicked

**Station (STA) Mode**:
- Connects to ZombieVerter WiFi
- Used to fetch params.json
- Temporary mode during fetch

### HTTP Request

```
GET /params.json HTTP/1.1
Host: 192.168.4.1
Connection: close
```

### Error Handling

The fetch function handles:
- ✅ WiFi connection timeout (10 seconds)
- ✅ HTTP connection failure
- ✅ Invalid JSON size
- ✅ JSON parse errors
- ✅ SPIFFS write errors
- ✅ Auto-restore AP mode on failure

---

## 🛠️ Troubleshooting

### "Failed to connect to ZombieVerter WiFi"

**Problem**: Can't connect to ZombieVerter's WiFi

**Solutions**:
- ✅ Check `ZOMBIEVERTER_WIFI_SSID` in Config.h
- ✅ Check `ZOMBIEVERTER_WIFI_PASSWORD` in Config.h
- ✅ Make sure ZombieVerter WiFi is enabled
- ✅ Move M5Dial closer to ZombieVerter
- ✅ Check ZombieVerter WiFi settings in its web interface

---

### "Connection to ZombieVerter web server failed"

**Problem**: Connected to WiFi but can't reach web server

**Solutions**:
- ✅ Check `ZOMBIEVERTER_IP` in Config.h (usually 192.168.4.1)
- ✅ Verify ZombieVerter web interface is accessible
- ✅ Try browsing to `http://192.168.4.1` from phone
- ✅ Check ZombieVerter's web server is running

---

### "Failed to parse parameters from ZombieVerter"

**Problem**: Downloaded but JSON is invalid

**Solutions**:
- ✅ Verify `/params.json` exists on ZombieVerter
- ✅ Check params.json is valid JSON format
- ✅ Try downloading manually to verify format
- ✅ Check `MAX_JSON_SIZE` in Config.h (default 16KB)

---

### "Timeout waiting for response"

**Problem**: Request sent but no response

**Solutions**:
- ✅ ZombieVerter may be slow to respond
- ✅ Increase timeout in WiFiManager.cpp (currently 5 seconds)
- ✅ Check ZombieVerter isn't busy/overloaded
- ✅ Try manual download to test ZombieVerter response time

---

## 📊 Serial Monitor Output

**Successful Fetch**:
```
=== Fetch from ZombieVerter triggered ===
Attempting to fetch params.json from ZombieVerter...
Connecting to Zombieverter...
..
Connected! IP: 192.168.4.2
Fetching http://192.168.4.1/params.json...
Received 8472 bytes of JSON
params.json saved to SPIFFS
Successfully loaded 26 parameters from ZombieVerter
=== Fetch complete: SUCCESS ===
```

**Failed Fetch**:
```
=== Fetch from ZombieVerter triggered ===
Attempting to fetch params.json from ZombieVerter...
Connecting to Zombieverter...
....................
Failed to connect to ZombieVerter WiFi
=== Fetch complete: FAILED ===
```

---

## 🔄 Alternative Methods

If auto-fetch doesn't work, you can still use the traditional methods:

### Manual Upload via Web Interface
1. Download params.json from ZombieVerter to your computer
2. Upload it via M5Dial web interface

### Pre-installed in Firmware
1. Place params.json in `data/` folder
2. Run `pio run --target uploadfs`
3. Params.json is permanently in SPIFFS

---

## 💡 Tips

**First Time Setup**:
- Use manual upload for initial setup
- Verify parameters are correct
- Then use auto-fetch for updates

**Regular Updates**:
- Click "Fetch from ZombieVerter" whenever you change params
- No need to manually download/upload
- Takes 10-15 seconds total

**Offline Operation**:
- Once fetched, params.json is stored in SPIFFS
- M5Dial works without WiFi connection
- Parameters persist across reboots

---

## 🎯 Best Practices

1. **Configure WiFi settings before compiling**
   - Update Config.h with YOUR ZombieVerter WiFi
   - Recompile and upload firmware

2. **Test connection first**
   - Connect to ZombieVerter WiFi from phone
   - Verify you can access http://192.168.4.1
   - Verify /params.json exists

3. **Use Serial Monitor**
   - Watch for connection status
   - Debug any issues
   - Confirm successful fetch

4. **Keep backups**
   - Download params.json via web interface
   - Save to computer
   - Easy to restore if needed

---

## 🚀 Future Enhancements

Possible improvements:
- Auto-fetch on parameter change detection
- Scheduled auto-fetch (e.g., daily)
- mDNS discovery of ZombieVerter
- Multiple ZombieVerter support
- Retry logic with exponential backoff

---

**Enjoy automatic parameter syncing!** 🎉
