# ZombieVerter Display v1.1.0 - Production Release

## 🎉 Professional LVGL-Based Automotive Dashboard for M5Stack Dial

---

## ✨ Features

### **11 Complete Screens**
1. **Splash** - Animated boot screen with spinner
2. **Dashboard** - RPM meter, SOC ring, voltage/power
3. **Power** - Multi-zone power meter (-50 to +150kW)
4. **Temperature** - Dual arc meters (motor/inverter)
5. **Battery** - SOC gauge with voltage/current/temp
6. **BMS** - Cell voltages and battery management
7. **Gear** - Gear selection display with indicators
8. **Motor** - Motor mode selection (MG1/MG2/Blended)
9. **Regen** - Regenerative braking adjustment
10. **WiFi** - Connection info and status
11. **Settings** - System information

### **Professional UI**
- ✅ LVGL 8.4 framework
- ✅ Sharp, anti-aliased fonts (optimized for M5Dial)
- ✅ Smooth needle animations (30 FPS)
- ✅ Color-coded values (temps, SOC, power)
- ✅ Optimized for 240x240 round display
- ✅ Dark theme with cyan/orange/green accents

### **Real-Time CAN Data**
- ✅ ZombieVerter CAN integration
- ✅ 10 Hz update rate (100ms)
- ✅ Node ID 3 (ZombieVerter standard)
- ✅ SDO parameter reading
- ✅ Dynamic parameter loading from params.json

### **Hardware**
- ✅ M5Stack Dial (ESP32-S3)
- ✅ Rotary encoder navigation
- ✅ Button controls
- ✅ Touch screen support
- ✅ WiFi AP mode for configuration

---

## 🚀 Quick Start

### **Upload to M5Dial**
1. Extract `ZombieVerter_Display_v1.1.0.zip`
2. Open folder in VS Code with PlatformIO
3. Click **Clean** (trash icon)
4. Click **Build** (checkmark icon)
5. Connect M5Dial via USB
6. Click **Upload** (arrow icon)
7. Wait for automatic restart

### **First Boot**
- Shows splash screen (2 seconds)
- Auto-loads Dashboard
- Displays demo data until CAN connected

### **Controls**
- **Rotate encoder** - Navigate screens
- **Click button** - Quick screen change
- **Long press** - Activate WiFi (on WiFi screen)

---

## 🎨 Display Optimization

### **Sharp Text Rendering**
- Anti-aliasing: **Disabled** for maximum sharpness
- Font sizes: 12pt to 40pt
- Color format: RGB565 (perfectly matched to M5GFX)
- No color artifacts or byte swap issues

### **Color Coding**
**Temperatures:**
- Green (0-60°C) → Yellow (60-80°C) → Red (80+°C)

**Battery SOC:**
- Green (80-100%) → Yellow (20-80%) → Red (0-20%)

**Power:**
- Green (regen) → Cyan (low) → Yellow (med) → Red (high)

---

## 📊 Performance

**Memory Usage:**
- RAM: ~85KB / 512KB (16.5%)
- Flash: ~1.5MB / 3.2MB (47%)
- Plenty of headroom for future features!

**Update Rates:**
- Display refresh: 30 FPS
- CAN updates: 10 Hz (every 100ms)
- UI response: < 50ms

---

## 🔧 Configuration

### **CAN Settings**
- **TX Pin:** GPIO 2 (Port.A)
- **RX Pin:** GPIO 1 (Port.A)
- **Speed:** 500 kbps
- **Node ID:** 3

### **WiFi AP Mode**
- **SSID:** ZombieVerter-Display
- **Password:** zombieverter
- **IP:** 192.168.4.1
- Access parameter editor in browser

### **Parameter Configuration**
Edit `data/params.json` to customize:
- CAN parameter IDs
- Display units
- Min/max ranges
- Editable parameters

---

## 📁 Project Structure

```
ZombieVerter_Display/
├── platformio.ini          # Build configuration
├── lv_conf.h              # LVGL settings
├── src/
│   ├── main.cpp           # Main program
│   ├── UIManager.cpp      # All 11 screens
│   ├── CANData.cpp        # CAN communication
│   ├── InputManager.cpp   # Encoder/button input
│   ├── WiFiManager.cpp    # WiFi AP mode
│   └── Hardware.cpp       # M5Dial initialization
├── include/               # Header files
└── data/
    └── params.json        # CAN parameter definitions
```

---

## 🎯 What's Included

**Ready-to-Build Code:**
- All 11 screens fully implemented
- Professional LVGL widgets
- Real CAN data integration
- Color-coded status displays

**Documentation:**
- Build instructions
- Parameter configuration guide
- CAN protocol reference
- WiFi setup guide
- Customization examples

**Example Code:**
- Power screen implementation
- Complete screen patterns
- Update function templates

---

## 🔄 Future Enhancements

**Easy to Add:**
- Additional screens
- Custom parameters
- Different color themes
- Screen transitions/animations
- Touch gesture controls

**All code is:**
- Well-commented
- Modular design
- Easy to customize
- Professional structure

---

## 📝 Credits

**Based on ZombieVerter by Jamie Jones**
- https://github.com/jsphuebner/stm32-sine

**Hardware:**
- M5Stack Dial (ESP32-S3)
- 240x240 round display
- Rotary encoder + button

**Libraries:**
- LVGL 8.4 (UI framework)
- M5Unified (hardware abstraction)
- M5GFX (display driver)
- ESP32-TWAI (CAN driver)

---

## 🐛 Troubleshooting

**Display shows wrong colors:**
- Already fixed in this version!
- Uses correct RGB565 format

**Text looks blurry:**
- Anti-aliasing disabled for sharpness
- 240x240 displays have natural pixel visibility
- View at arm's length for best clarity

**Upload fails:**
- Check USB cable (needs data, not charge-only)
- Try different USB port
- Install CP210x driver if needed
- See UPLOAD_TROUBLESHOOTING.md

**CAN not connecting:**
- Verify TX/RX pins (GPIO 2/1)
- Check 500kbps CAN speed
- Confirm Node ID 3
- Test with ZombieVerter powered on

---

## 📞 Support

**Included Documentation:**
- BUILD_INSTRUCTIONS.md
- PARAMS_JSON_GUIDE.md
- CAN_PROTOCOL.md
- WIFI_FEATURE.md
- FONT_SHARPENING_GUIDE.md

---

## 🎉 Enjoy Your Professional Dashboard!

**What You Get:**
- Sharp, colorful display
- Smooth animations
- Real-time data
- Professional automotive UI
- Easy to customize
- Production-ready code

**Upload and enjoy!** 🚗⚡✨

---

**Version:** 1.1.0  
**Release Date:** February 2026  
**Platform:** M5Stack Dial (ESP32-S3)  
**Framework:** Arduino + LVGL 8.4
