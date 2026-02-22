# M5Dial ZombieVerter Display - v1.9.0 Changelog

## 🎉 Major Release - Full Parameter Support

### ✨ New Features
- **All 119 ZombieVerter parameters hardcoded** - No more SPIFFS dependency
- **Complete web interface** with parameters tab for viewing/editing all parameters
- **Comprehensive documentation:**
  - `SETUP_GUIDE.md` - Complete setup instructions with theory of operation
  - `FUTURE_UPDATES.md` - How to update parameters when ZombieVerter firmware changes
- **Fixed-point encoding/decoding** - Automatic conversion for proper value display
- **Double-click lock/unlock** - Testing feature for immobilizer

### 🐛 Bug Fixes
- **Fixed enum/config parameter display** - No longer shows "100" for BMSCan
- **Proper raw value handling** - Configuration enums, CAN assignments, GPIO functions, time values, PWM registers, and digital ADC values now display correctly
- **usesRawValue() function** - Comprehensive logic to determine which parameters need ×32 encoding

### 📊 Parameter Handling

**Parameters Using Fixed-Point Encoding (×32):**
- Voltages (udcmin, udclim, Voltspnt, etc.)
- Currents (idcmax, idcmin, CCS_ILim, etc.)
- Percentages (throtmax, regenmax, SOC, etc.)
- Temperatures (tmphsmax, tmpmmax, FanTemp, etc.)
- RPM values (regenrpm, revlim, cruisestep, etc.)
- Power/rates (Pwrspnt, HeatPwr, throtramp, etc.)

**Parameters Using Raw Values (No Encoding):**
- Configuration enums (Inverter, Vehicle, Gear, chargemodes, etc.)
- CAN bus assignments (InverterCan, VehicleCan, BMSCan, etc.)
- GPIO functions (Out1Func, PWM1Func, SL1Func, etc.)
- Digital ADC values (potmin, potmax, BrkVacThresh, etc.)
- Time values (Set_Hour, Chg_Hrs, Pre_Dur, etc.)
- PWM timer registers (Tim3_Presc, Tim3_Period, etc.)

### 🔧 Technical Changes
- Removed SPIFFS dependency (prevents boot loops)
- Parameter array in WebInterface.cpp: 119 params, ~15KB flash
- Improved SDO error handling with detailed abort code messages
- Better Serial Monitor debugging output

### ⚠️ Known Issues

**Immobilizer (Testing Phase):**
- LOCKED (idcmax=0): ✓ SDO ACK received, works correctly
- UNLOCKED (idcmax=500): ✗ SDO ABORT "value range exceeded"
- Root cause under investigation (see FORUM_POST.md for details)
- Feature is functional for lock but not yet reliable for unlock

**SPIFFS Boot Loop:**
- Loading parameters from JSON causes boot loop on second boot
- Temporarily resolved by hardcoding all parameters
- Long-term solution being investigated

### 📦 What's Included
- Complete firmware source code
- All 119 ZombieVerter parameter definitions
- Web interface with dashboard + parameters tabs
- M5Dial rotary screens (dashboard, battery, motor, gear, regen, motor select)
- Comprehensive documentation in `docs/` folder
- Forum post template for community help

### 🚀 How to Update from Previous Version
1. Extract M5Dial_v1.9.0_FINAL.zip
2. Flash firmware using PlatformIO or Arduino IDE
3. Read docs/SETUP_GUIDE.md for complete instructions
4. All parameters will now display with correct values
5. Test web interface - BMSCan should show "0" or "1" not "100"

### 📝 Notes
- PIN code: 1234 (hardcoded, see SETUP_GUIDE.md to change)
- WiFi SSID: ZombieVerter-Display
- WiFi Password: zombieverter
- Web interface: http://192.168.4.1
- ZombieVerter CAN node: 3
- CAN bitrate: 500kbps

### 🙏 Community Help Needed
See `FORUM_POST.md` for:
1. Immobilizer SDO write issue (value range exceeded on unlock)
2. SPIFFS boot loop investigation

Your insights and suggestions are welcome!

---

**Full Changelog:** v1.5.1 → v1.9.0
- v1.5.1: Basic CAN broadcast reception working
- v1.6.0: Added SDO protocol support
- v1.7.0: Fixed CAN transceiver voltage issue (3.3V required)
- v1.8.0: Added web interface with parameter display
- v1.8.1: Fixed dial encoder, added immobilizer
- v1.8.2: Added parameter list expansion
- v1.8.3: Tested various immobilizer approaches
- v1.9.0: **Full 119 parameters, fixed value encoding, comprehensive docs**
