# Web Interface Quick Start

## 🚀 5-Minute Setup

### Step 1: Upload Firmware
Flash the M5Dial with this firmware (same as before, now with web interface).

### Step 2: Power On
When M5Dial boots, you'll see:
```
========================================
Web Interface Started!
Access at: http://192.168.4.1
SSID: ESP-M5DIAL
Compatible with OpenInverter Web UIs
========================================
```

### Step 3: Connect
**On your phone or laptop:**
1. Go to WiFi settings
2. Connect to network: **ESP-M5DIAL**
3. No password required

### Step 4: Browse
Open your web browser and go to:
```
http://192.168.4.1
```

You should see the M5Dial web interface!

---

## 📱 Use With Existing Web UIs

### Option A: jamiejones85's Web UI (Recommended)

**This is the most popular OpenInverter web UI with gauges, logging, etc.**

1. **Download the web UI:**
   ```bash
   git clone https://github.com/jamiejones85/esp32-web-interface
   cd esp32-web-interface/data
   ```

2. **Open in browser:**
   - Simply open `index.html` in your browser
   - It will auto-detect the M5Dial at `http://192.168.4.1`

3. **Features you get:**
   - Real-time gauges
   - Parameter editing
   - Dark mode
   - Drive mode display
   - Data logging

### Option B: Use Phone as Dashboard

1. Connect phone to `ESP-M5DIAL` WiFi
2. Open browser to `http://192.168.4.1`
3. **On iPhone:** Tap Share → Add to Home Screen
4. **On Android:** Tap Menu → Install app

Now you have a dashboard app icon on your phone!

---

## 🔧 Quick API Test

Try these commands to test the API:

### View All Parameters
```bash
curl http://192.168.4.1/json
```

### Get Current RPM
```bash
curl http://192.168.4.1/get?param=1
```

### Set Max Current to 450A
```bash
curl "http://192.168.4.1/set?param=21&value=450"
```

### Save to Flash
```bash
curl http://192.168.4.1/save
```

---

## 📊 What Can You Do?

### Monitor While Driving
- View real-time gauges on your phone
- Check battery SOC, motor temp, power
- No need to look at M5Dial screen

### Configure Parameters
- Edit any ZombieVerter parameter from your laptop
- Easier than using the dial for precise values
- Save configurations

### Log Data
- Record parameters while driving
- Analyze performance later
- Debug issues

### Remote Control (Future)
- Unlock immobilizer from phone
- Change gear remotely
- Adjust regen on the fly

---

## ⚙️ Configuration Options

### Change WiFi Network Name
Edit `main.cpp`:
```cpp
webInterface.startAccessPoint("MyCarName", "optional_password");
```

### Connect to Home WiFi
Edit `main.cpp`:
```cpp
webInterface.connectToWiFi("YourWiFi", "YourPassword");
webInterface.init();
```
Now accessible at: `http://zombieverter.local`

---

## 🔍 Troubleshooting

**Can't see ESP-M5DIAL WiFi?**
- Check Serial Monitor for "Web Interface Started!"
- Try rebooting M5Dial
- Look for alternate name if you changed it

**Browser shows "can't connect"?**
- Make sure you're connected to ESP-M5DIAL WiFi
- Try `192.168.4.1` not `zombieverter.local`
- Disable cellular data on phone

**Parameters won't change?**
- Check CAN connection to ZombieVerter
- Verify parameter has CAN mapping (see immobilizer docs)
- Check Serial Monitor for errors

---

## 📚 Full Documentation

See **WEB_INTERFACE.md** for:
- Complete API reference
- All endpoints and parameters
- Integration examples
- Phone app development
- Security settings
- Advanced configuration

---

## 🎯 Next Steps

1. **Test basic connection** - Browse to `http://192.168.4.1`
2. **Try existing web UIs** - Download jamiejones85's UI
3. **Create phone shortcut** - Add to home screen
4. **Customize** - Change WiFi name, add password
5. **Develop!** - Build your own custom dashboard

---

**Happy monitoring! 🚗💨**
