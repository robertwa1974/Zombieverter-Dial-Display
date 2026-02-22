# M5Dial Web Interface Documentation

## Overview

The M5Dial now includes a **full OpenInverter-compatible web API** that allows you to control and monitor your ZombieVerter from any device with a web browser.

This makes your M5Dial compatible with:
- jamiejones85/esp32-web-interface
- dimecho/Open-Inverter-WebInterface  
- outlandnish/openinverter-web-interface
- Any custom web app that uses the OpenInverter protocol

---

## Quick Start

### 1. Power On M5Dial
The web interface starts automatically in **Access Point mode**:
- **SSID:** `ESP-M5DIAL`
- **IP Address:** `192.168.4.1`
- **No password** (configurable)

### 2. Connect Your Device
On your phone/laptop:
1. Connect to WiFi network `ESP-M5DIAL`
2. Open browser to `http://192.168.4.1`
3. You'll see the M5Dial web interface!

### 3. Use Existing Web UIs
Point any OpenInverter web UI to `http://192.168.4.1` and it will work immediately!

---

## Modes of Operation

### Access Point Mode (Default)
- M5Dial creates its own WiFi network
- Perfect for in-car use
- Connect directly from your phone
- No internet required
- **Default SSID:** `ESP-M5DIAL`
- **Default IP:** `192.168.4.1`

### Station Mode (Optional)
- M5Dial connects to your existing WiFi
- Access from anywhere on your network
- Better for home/workshop use
- Uses mDNS: `http://zombieverter.local`

To switch modes, modify `main.cpp`:
```cpp
// Instead of:
webInterface.init();  // Starts in AP mode

// Use:
webInterface.connectToWiFi("YourSSID", "YourPassword");
webInterface.init();
```

---

## API Endpoints

All endpoints return JSON or plain text. Compatible with OpenInverter protocol.

### GET /
Returns HTML status page showing:
- Device info
- Current IP address
- Available API endpoints
- Links to compatible UIs

### GET /json
Returns all parameters in OpenInverter JSON format.

**Optional parameter:** `?hidden=1` to include hidden parameters

**Response format:**
```json
{
  "speed": {
    "unit": "rpm",
    "value": 3000,
    "isparam": false
  },
  "idcmax": {
    "unit": "A",
    "value": 500,
    "isparam": true,
    "minimum": 0,
    "maximum": 1000,
    "default": 500,
    "i": 21
  }
}
```

**Example:**
```bash
curl http://192.168.4.1/json
```

### GET /spot
Returns real-time "spot" values (non-parameters only).

**Response:**
```json
{
  "speed": 3000,
  "udc": 350,
  "idc": 120,
  "power": 42.0,
  "tmpm": 65,
  "tmphs": 55,
  "soc": 87
}
```

**Example:**
```bash
curl http://192.168.4.1/spot
```

### GET /get?param=ID
Get a specific parameter value.

**Parameters:**
- `param` - Parameter ID (e.g., 1 for RPM, 3 for voltage)

**Response:** Plain text value
```
3000
```

**Example:**
```bash
curl "http://192.168.4.1/get?param=1"
```

### GET /set?param=ID&value=VALUE
Set a parameter value (sends SDO command to ZombieVerter).

**Parameters:**
- `param` - Parameter ID
- `value` - New value

**Response:** `OK` or error message

**Example:**
```bash
curl "http://192.168.4.1/set?param=21&value=450"
```

This sets `idcmax` (parameter 21) to 450A.

### GET /save
Save all parameters to ZombieVerter flash memory.

**Response:** `Parameters saved to flash`

**Example:**
```bash
curl http://192.168.4.1/save
```

### GET /load
Reload parameters from flash (on next ZombieVerter reboot).

**Response:** `Reload on next boot`

---

## Parameter IDs

Common parameters from your ZombieVerter:

| ID | Name | Unit | Description |
|----|------|------|-------------|
| 1 | Speed | rpm | Motor RPM |
| 2 | Power | kW | Current power |
| 3 | Voltage | V | DC bus voltage |
| 4 | Current | A | DC current |
| 5 | Motor Temp | °C | Motor temperature |
| 6 | Inverter Temp | °C | Inverter temperature |
| 7 | Battery SOC | % | State of charge |
| 21 | idcmax | A | Max discharge current |
| 22 | idcmin | A | Max regen current |
| 27 | Gear | - | Gear selection |

See `/json` endpoint for complete list.

---

## Integration with Existing Web UIs

### Option 1: jamiejones85/esp32-web-interface

This is the most popular OpenInverter web UI.

**Setup:**
1. Clone the repo: `git clone https://github.com/jamiejones85/esp32-web-interface`
2. Open `data/index.html` in a text editor
3. Find the API URL configuration
4. Change to: `http://192.168.4.1`
5. Open `index.html` in your browser

**Features:**
- Real-time gauges
- Parameter editing
- Dark mode
- Drive mode page
- Logging to SD card

### Option 2: dimecho/Open-Inverter-WebInterface

Alternative web UI with different styling.

**Setup:**
1. Clone: `git clone https://github.com/dimecho/Open-Inverter-WebInterface`
2. Open in browser
3. Point to: `http://192.168.4.1`

### Option 3: outlandnish/openinverter-web-interface

React-based modern UI.

**Setup:**
1. Clone: `git clone https://github.com/outlandnish/openinverter-web-interface`
2. `npm install`
3. Edit `src/config.js` to point to `http://192.168.4.1`
4. `npm start`

---

## Phone App Possibilities

### Progressive Web App (PWA)
Any of the above web UIs can be "installed" as an app:

**On iPhone:**
1. Open `http://192.168.4.1` in Safari
2. Tap Share button
3. Tap "Add to Home Screen"
4. App appears on home screen!

**On Android:**
1. Open `http://192.168.4.1` in Chrome
2. Tap menu (⋮)
3. Tap "Install app" or "Add to Home screen"

### Native App Development
The API can be used by native apps:

**iOS (Swift):**
```swift
let url = URL(string: "http://192.168.4.1/json")!
URLSession.shared.dataTask(with: url) { data, response, error in
    // Parse JSON and display
}.resume()
```

**Android (Kotlin):**
```kotlin
val client = OkHttpClient()
val request = Request.Builder()
    .url("http://192.168.4.1/json")
    .build()
client.newCall(request).execute()
```

**React Native:**
```javascript
fetch('http://192.168.4.1/json')
  .then(response => response.json())
  .then(data => console.log(data));
```

---

## Advanced Configuration

### Change WiFi Credentials

Edit `main.cpp`:
```cpp
// Change AP credentials
webInterface.startAccessPoint("MyCarWiFi", "MyPassword");

// Or connect to existing WiFi
webInterface.connectToWiFi("HomeWiFi", "HomePassword");
```

### Change Hostname

```cpp
webInterface.setHostname("mycar");
// Now accessible at: http://mycar.local
```

### Enable/Disable CORS

```cpp
webInterface.enableCORS(true);  // Allow cross-origin requests
```

### Custom Parameter Mapping

Add your own parameters in `sampleParams` JSON in `main.cpp`:
```json
{
  "id": 100,
  "name": "My Custom Param",
  "type": "int16",
  "unit": "unit",
  "min": 0,
  "max": 1000,
  "decimals": 0,
  "editable": true
}
```

---

## Troubleshooting

### Can't connect to ESP-M5DIAL
- Make sure M5Dial is powered on
- Check Serial Monitor for "Web Interface Started!" message
- Try forgetting the network and reconnecting
- Default password is blank (no password)

### Web UI shows "Connection Error"
- Verify IP address in browser matches M5Dial's IP
- Check Serial Monitor for actual IP
- Try `http://192.168.4.1` (default AP mode)
- Try `http://zombieverter.local` (if using mDNS)

### Parameters won't change
- Check CAN bus connection to ZombieVerter
- Verify parameter is editable (`isparam: true`)
- Check Serial Monitor for SDO transmission errors
- Make sure ZombieVerter has CAN mapping configured (for real-time changes)

### Can't access from phone
- Make sure phone is connected to `ESP-M5DIAL` WiFi
- Some phones require you to disable cellular data while connected
- iOS: Settings → WiFi → ESP-M5DIAL → Disable "Auto-Join Hotspot"

---

## Example Use Cases

### 1. Monitor While Driving
- Connect phone to `ESP-M5DIAL`
- Open web UI in browser
- Add to home screen for quick access
- View real-time gauges while parked

### 2. Configure Parameters
- Use laptop for easier typing
- Connect to `ESP-M5DIAL`
- Browse to `http://192.168.4.1`
- Edit parameters, click save

### 3. Data Logging
- Use jamiejones85's UI with SD logging
- Records all parameters while driving
- Analyze later for performance tuning

### 4. Remote Monitoring (Workshop)
- Connect M5Dial to workshop WiFi
- Access from any device on network
- Monitor from across the garage

---

## Security Notes

⚠️ **The web interface has NO authentication by default!**

This is intentional for ease of use, but means:
- Anyone connected to `ESP-M5DIAL` WiFi can access the interface
- Anyone on your home network (if using Station mode) can access it

**To add security:**
1. Set a WiFi password (AP mode)
2. Use MAC filtering
3. Only enable web interface when needed
4. Future: Add HTTP Basic Auth

For immobilizer security, the PIN protection is separate from web access.

---

## Performance Notes

- Web interface uses ~40KB RAM
- Minimal CPU usage (only during HTTP requests)
- Does not interfere with M5Dial display or immobilizer
- CAN performance unchanged
- Supports ~10 concurrent connections

---

## Future Enhancements

Planned features:
- [ ] WebSocket support for live updates
- [ ] Built-in web UI (no external files needed)
- [ ] HTTP Basic Authentication
- [ ] OTA firmware updates via web
- [ ] CAN message logging
- [ ] Export parameters to JSON file
- [ ] Immobilizer control via web
- [ ] Real-time graphs/charts

---

## API Compatibility

The M5Dial web API implements the **OpenInverter Serial Protocol** over HTTP.

Compatible with:
✅ esp32-web-interface (jamiejones85)
✅ Open-Inverter-WebInterface (dimecho)
✅ openinverter-web-interface (outlandnish)
✅ Custom apps using OpenInverter protocol
✅ Any HTTP client (curl, Postman, etc.)

Differences from STM32-based systems:
- Uses CAN (SDO) instead of UART
- Parameter changes require CAN mapping for real-time effect
- Some commands (flash update, etc.) not applicable

---

## Support

**Questions?** Open an issue on GitHub
**Web UI not working?** Check compatibility with OpenInverter protocol
**Want to contribute?** PRs welcome!

---

## License

Same as M5Dial ZombieVerter Display project.

---

**Enjoy your web-enabled M5Dial! 🚗💻📱**
