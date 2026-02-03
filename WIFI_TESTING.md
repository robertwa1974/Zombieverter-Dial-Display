# WiFi Not Working - Testing Instructions

## The Problem

You can't see the "ZombieVerter-Display" WiFi network. This could be:
1. **Button issue** - Button click not triggering WiFi
2. **WiFi init issue** - WiFi not starting properly
3. **Hardware issue** - WiFi module not working

Let's test each possibility:

---

## Test 1: Auto-Start WiFi (Button Bypass)

This version **starts WiFi automatically on boot** - no button needed.

### Steps:

1. **Download**: `M5StackDial_ZombieVerter_WiFi_Debug.zip` (latest version)
2. **Upload to M5Stack Dial**
3. **Wait 5-10 seconds** after boot
4. **Check WiFi networks** on your phone/computer
5. **Look for**: `ZombieVerter-Display`

### What This Tests:
- WiFi hardware functionality
- WiFi initialization code
- Bypasses button completely

### Expected Results:

✅ **If you SEE the network:**
- WiFi works!
- Problem is the button triggering
- We can fix button handling

❌ **If you DON'T see the network:**
- WiFi init issue or hardware problem
- Go to Test 2

---

## Test 2: Minimal WiFi Test (Ultra Simple)

This is the **absolute simplest WiFi test** - just WiFi, nothing else.

### Steps:

1. **Backup** your current main.cpp:
   ```
   Rename: src/main.cpp → src/main.cpp.backup
   ```

2. **Copy** the test file:
   ```
   Copy: wifi_test.cpp → src/main.cpp
   ```

3. **Upload** to M5Stack Dial

4. **Open Serial Monitor** (115200 baud)

5. **Look for**:
   ```
   M5Stack Dial WiFi Test
   =================================
   1. Turning WiFi off...
   2. Setting WiFi to AP mode...
   3. Starting Access Point...
      ✓ SUCCESS!
   4. IP Address: 192.168.4.1
   5. Starting web server...
      ✓ Web server running
   TEST COMPLETE!
   ```

6. **Check WiFi** for network: `M5Stack-Test`

7. **Connect** with password: `12345678`

8. **Browse to**: `http://192.168.4.1`

### What This Tests:
- Pure WiFi hardware
- No M5Stack libraries interfering
- No display, no CAN, no complexity

### Expected Results:

✅ **If you see "✓ SUCCESS!" in serial:**
- WiFi hardware is fine!
- Problem is in the main code integration
- Go to Test 3

❌ **If you see "✗ FAILED!" in serial:**
- WiFi hardware issue
- See "Hardware Issues" section below

---

## Test 3: Check Serial Output (Main Code)

If Test 1 or Test 2 worked, go back to main code and check what's happening.

### Steps:

1. **Restore** main code (if you did Test 2):
   ```
   Restore: src/main.cpp.backup → src/main.cpp
   ```

2. **Enable debug mode** in `include/Config.h`:
   ```cpp
   #define DEBUG_SERIAL    true
   ```

3. **Upload** and open Serial Monitor (115200 baud)

4. **Watch for these messages** on boot:
   ```
   ZombieVerter Display - M5Stack Dial
   Hardware initialized
   CAN initialized
   WiFi manager initialized
   AUTO-STARTING WiFi AP for testing...    ← This should appear!
   Starting WiFi AP...
   WiFi AP started successfully!
   IP Address: 192.168.4.1
   ```

5. **Copy the ENTIRE serial output** and share it

### What to Look For:

**Good (WiFi starts):**
```
AUTO-STARTING WiFi AP for testing...
Starting WiFi AP...
WiFi AP started successfully!
IP Address: 192.168.4.1
```

**Bad (WiFi fails):**
```
AUTO-STARTING WiFi AP for testing...
Starting WiFi AP...
Failed to start AP!
```

**Very Bad (WiFi not attempted):**
```
WiFi manager initialized
[no auto-start message]
```

---

## Hardware Issues

If even Test 2 (minimal test) fails, it's likely hardware:

### Possible Hardware Problems:

1. **Power Issue**
   - WiFi needs more current than USB provides
   - **Try**: Better USB cable (must support data + power)
   - **Try**: Different USB port
   - **Try**: Powered USB hub
   - **Try**: External 5V power supply

2. **Antenna Issue**
   - M5Stack Dial has internal antenna
   - Should work out of box
   - **Check**: Nothing covering the device
   - **Try**: Different location (away from metal)

3. **ESP32-S3 Module Issue**
   - WiFi module may be defective
   - **Try**: Factory firmware (if available from M5Stack)
   - **Contact**: M5Stack support

### ESP32-S3 WiFi Test Commands

Add to minimal test:
```cpp
void setup() {
    Serial.begin(115200);
    delay(3000);
    
    // Check WiFi MAC address
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.print("WiFi MAC: ");
    Serial.println(WiFi.macAddress());
    
    // If MAC is 00:00:00:00:00:00, WiFi hardware is dead
    
    // Continue with WiFi test...
}
```

If MAC shows `00:00:00:00:00:00` → WiFi module is dead.

---

## Quick Checklist

Before declaring hardware fault:

- [ ] Tried different USB cable (data capable)
- [ ] Tried different USB port
- [ ] Tried Test 2 (minimal WiFi test)
- [ ] Checked serial output for errors
- [ ] Verified DEBUG_SERIAL is enabled
- [ ] WiFi MAC address is valid (not 00:00:00:00:00:00)
- [ ] Checked for WiFi network on phone AND computer
- [ ] Waited at least 10 seconds for network to appear
- [ ] Tried different WiFi channel (1, 6, or 11)

---

## What to Share

If still not working, please provide:

1. **Serial output** from Test 2 (minimal test)
   - Copy everything from serial monitor
   - Include the whole startup sequence

2. **Serial output** from Test 3 (main code with debug)
   - Copy everything, especially WiFi-related messages

3. **Your setup**
   - USB cable type
   - Power source (computer USB / hub / external)
   - Computer OS (Windows/Mac/Linux)
   - Any error messages

4. **What you tried**
   - Which tests you ran
   - What results you got
   - Any patterns you noticed

---

## Summary

**Try in this order:**

1. ✅ **Test 1**: Upload WiFi_Debug version (WiFi auto-starts)
   - If works → button issue, easy fix
   - If not → continue to Test 2

2. ✅ **Test 2**: Upload minimal WiFi test (wifi_test.cpp)
   - If works → integration issue, we can fix
   - If not → likely hardware issue

3. ✅ **Test 3**: Check serial output with debug enabled
   - Share serial output
   - We can diagnose from messages

Most likely it's a **software integration issue** and Test 1 or Test 2 will work!

---

## Files You Need

1. **M5StackDial_ZombieVerter_WiFi_Debug.zip** - Auto-start WiFi version
2. **wifi_test.cpp** - Minimal WiFi-only test program

Try Test 1 first - it's the quickest way to know if WiFi hardware works!
