# WiFi Not Appearing - Troubleshooting Guide

## Quick Diagnostics

### Step 1: Enable Debug Mode

Edit `include/Config.h`:
```cpp
#define DEBUG_SERIAL    true
```

### Step 2: Connect Serial Monitor

1. Connect M5Stack Dial via USB
2. Open PlatformIO Serial Monitor (115200 baud)
3. Click the encoder button to enable WiFi
4. Watch the serial output

### What You Should See:

```
WiFi Manager initialized (WiFi off)
WiFi mode enabled
Starting WiFi AP...
SSID: ZombieVerter-Display
Password: zombieverter
WiFi AP started successfully!
IP Address: 192.168.4.1
Channel: 1
Web server started on port 80
```

### What Indicates Problems:

```
Failed to start AP!
```
OR no WiFi messages at all.

## Common Issues & Fixes

### Issue 1: WiFi Not Starting

**Symptoms:**
- No WiFi network visible
- Serial says "Failed to start AP!"

**Cause:** WiFi peripheral conflict or initialization order

**Fix:** Add delay before WiFi init

Edit `src/main.cpp`, in `setup()`:
```cpp
// Before WiFi manager init
delay(1000);  // Wait for system to stabilize

// Initialize WiFi manager
if (!wifiManager.init(&canManager)) {
    ...
}
```

### Issue 2: Button Not Triggering WiFi

**Symptoms:**
- Button click does nothing
- Serial shows no "WiFi mode enabled" message

**Check:** Encoder button pin

Edit `include/Config.h`:
```cpp
#define ENCODER_BUTTON      42  // Verify this is correct
```

**Test:** Add this to button click callback:
```cpp
void onButtonClick() {
    Serial.println("Button clicked!"); // Debug
    
    if (!wifiMode) {
        wifiMode = true;
        ...
    }
}
```

### Issue 3: M5Stack Dial WiFi Hardware Issue

**Symptoms:**
- WiFi never appears
- Other M5Stack features work fine

**Cause:** ESP32-S3 WiFi requires proper power and antenna

**Check:** 
1. M5Stack Dial has built-in antenna
2. WiFi should work out of the box
3. Try a simple WiFi test first

**Simple WiFi Test:**

Create a minimal test program:
```cpp
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting WiFi AP test...");
    
    WiFi.mode(WIFI_AP);
    bool success = WiFi.softAP("TestAP", "12345678");
    
    if (success) {
        Serial.println("SUCCESS! WiFi AP started");
        Serial.print("IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("FAILED to start AP");
    }
}

void loop() {
    delay(1000);
}
```

Upload this and check serial output. If this fails, it's a hardware issue.

### Issue 4: SSID Not Broadcasting

**Symptoms:**
- Serial says WiFi started
- Still can't see network

**Cause:** Hidden SSID or wrong WiFi mode

**Fix:** Force visible AP

Edit `src/WiFiManager.cpp`:
```cpp
// In startAP() function
WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, false, 4);
//                                                           ^^^^^ hidden=false
//                                                                 ^^^^^ max connections
```

### Issue 5: Interference from Other WiFi

**Symptoms:**
- WiFi works sometimes, not others
- Disappears randomly

**Fix:** Change WiFi channel

Edit `include/Config.h`:
```cpp
#define WIFI_AP_CHANNEL     6  // Try different channels: 1, 6, or 11
```

Channels 1, 6, 11 have least interference.

### Issue 6: Power Supply Issue

**Symptoms:**
- WiFi starts then immediately stops
- Device reboots when WiFi enables

**Cause:** WiFi uses more power than USB can supply

**Fix:** 
1. Use good quality USB cable (data + power)
2. Try different USB port
3. Use powered USB hub
4. Try external power supply

## Step-by-Step Debug Process

### 1. Verify Button Works
```cpp
void onButtonClick() {
    Serial.println("=== BUTTON CLICKED ===");
    Serial.print("WiFi mode before: ");
    Serial.println(wifiMode);
    
    if (!wifiMode) {
        Serial.println("Attempting to start WiFi...");
        wifiMode = true;
        wifiManager.startAP();
        uiManager.setScreen(SCREEN_WIFI);
    }
}
```

### 2. Check WiFi Manager Initialization
```cpp
// In setup()
Serial.println("About to init WiFi manager...");
bool wifiOk = wifiManager.init(&canManager);
Serial.print("WiFi manager init result: ");
Serial.println(wifiOk ? "SUCCESS" : "FAILED");
```

### 3. Verify AP Starts
Add to `WiFiManager::startAP()`:
```cpp
Serial.println("=== START AP FUNCTION CALLED ===");
Serial.print("Current apMode state: ");
Serial.println(apMode);

// ... existing code ...

Serial.print("WiFi.softAP returned: ");
Serial.println(apStarted);

Serial.print("WiFi mode: ");
Serial.println(WiFi.getMode());

Serial.print("AP IP: ");
Serial.println(WiFi.softAPIP());
```

### 4. Check If Anyone Connected
Add to `WiFiManager::update()`:
```cpp
// Show connected clients count
static unsigned long lastCheck = 0;
if (millis() - lastCheck > 5000) {
    Serial.print("Connected clients: ");
    Serial.println(WiFi.softAPgetStationNum());
    lastCheck = millis();
}
```

## Alternative: WiFi Always On Mode

If WiFi mode toggle isn't working, try always-on mode:

Edit `src/main.cpp`:
```cpp
void setup() {
    // ... existing setup code ...
    
    // Start WiFi immediately (for testing)
    wifiMode = true;
    wifiManager.startAP();
    uiManager.setScreen(SCREEN_WIFI);
    
    Serial.println("WiFi started automatically for testing");
    
    // ... rest of setup ...
}
```

Then WiFi should be on as soon as device boots.

## Minimal WiFi-Only Build

Create a test version that ONLY does WiFi:

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

void handleRoot() {
    server.send(200, "text/html", "<h1>It works!</h1>");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting minimal WiFi test...");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("M5-Test", "12345678");
    
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    server.on("/", handleRoot);
    server.begin();
    
    Serial.println("Web server started");
}

void loop() {
    server.handleClient();
}
```

If THIS works, the problem is in integration.

## Checking Your Phone/Computer

### Android
1. Settings → WiFi
2. Refresh network list
3. Look for "ZombieVerter-Display"
4. Some phones hide 2.4GHz networks - check settings

### iPhone/iPad
1. Settings → WiFi
2. Wait 10 seconds for refresh
3. Look for "ZombieVerter-Display"
4. Try turning WiFi off/on

### Windows
1. Click WiFi icon in taskbar
2. Refresh available networks
3. "ZombieVerter-Display" should appear
4. Try "Forget network" if you see it but can't connect

### Mac
1. WiFi menu → Open Network Preferences
2. Click WiFi in sidebar
3. "ZombieVerter-Display" should be in list
4. Try creating new location if issues

## Expected Behavior

✅ **Correct Sequence:**
1. Device boots → No WiFi (normal operation)
2. Click button → WiFi AP starts (takes 2-3 seconds)
3. WiFi screen shows → "ZombieVerter-Display" appears in WiFi list
4. Connect with password "zombieverter"
5. Browse to 192.168.4.1
6. Rotate dial → WiFi stops

## Serial Output Reference

**Good:**
```
WiFi Manager initialized (WiFi off)
Button clicked!
WiFi mode enabled
Starting WiFi AP...
WiFi AP started successfully!
Connected clients: 1
```

**Bad:**
```
WiFi Manager initialized (WiFi off)
Button clicked!
WiFi mode enabled
Failed to start AP!
```

**Very Bad:**
```
WiFi Manager initialized (WiFi off)
[nothing happens when button clicked]
```

## If Nothing Works

### Nuclear Option: Hardcode WiFi On

Edit `src/WiFiManager.cpp`:
```cpp
bool WiFiManager::init(CANDataManager* canMgr) {
    canManager = canMgr;
    
    // FORCE WiFi on for testing
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("M5-FORCED", "12345678");
    
    Serial.println("FORCED WiFi AP started");
    Serial.println(WiFi.softAPIP());
    
    // ... rest of init ...
}
```

WiFi will start immediately on boot. If THIS doesn't work, it's definitely hardware.

## Report Back

When debugging, provide:
1. **Serial output** (copy/paste everything)
2. **Which test** you tried
3. **What phone/computer** you're using
4. **WiFi settings** in your area (crowded? quiet?)

I can help further diagnose from there!
