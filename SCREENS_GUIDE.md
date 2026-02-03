# M5Stack Dial ZombieVerter - Screen Guide

## 🎮 Navigation
- **Rotate Dial**: Switch between screens
- **Click Button**: Toggle WiFi AP mode
- **Long Press Button**: (reserved for future use)

---

## 📺 Available Screens

### 1️⃣ Dashboard (Main Screen)
**Purpose**: Quick overview of key driving parameters

**Displays**:
- **Motor RPM** (center, large)
- **DC Voltage** (top)
- **Power** (bottom)
- **Speed** (if available)

**Data Sources**:
- Motor RPM: ZombieVerter 0x257
- DC Voltage: IVT-S 0x522
- Power: IVT-S 0x527
- Speed: ZombieVerter 0x257

---

### 2️⃣ Power Screen
**Purpose**: Detailed power flow and battery metrics

**Displays**:
- **Power** (large, center) in kW
- **DC Current** (from IVT-S)
- **DC Voltage** (from IVT-S)
- **SOC** (State of Charge %)

**Data Sources**:
- Power: IVT-S 0x527
- Current: IVT-S 0x411 (when driving)
- Voltage: IVT-S 0x522
- SOC: ZombieVerter 0x355

---

### 3️⃣ Temperature Screen
**Purpose**: Monitor all temperature sensors

**Displays**:
- **Motor Temp** (from ZombieVerter)
- **Inverter Temp** (from ZombieVerter)
- **Coolant Temp** (from IVT-S, if available)

**Data Sources**:
- Motor Temp: ZombieVerter 0x356 (bytes 4-5)
- Inverter Temp: ZombieVerter 0x126 (bytes 4-5)
- Coolant Temp: IVT-S 0x526

**Color Coding**:
- Green: Normal temp
- Yellow: Warm
- Red: Hot (warning)

---

### 4️⃣ Battery Screen
**Purpose**: Battery pack health and status

**Displays**:
- **SOC** (State of Charge %) - large center gauge
- **Pack Voltage**
- **Pack Current**

**Data Sources**:
- SOC: ZombieVerter 0x355
- Voltage: IVT-S 0x522
- Current: IVT-S 0x411

---

### 5️⃣ BMS Screen
**Purpose**: Cell voltage monitoring and balance status

**Displays**:
- **Max Cell Voltage** (mV precision)
- **Min Cell Voltage** (mV precision)
- **Delta** (cell voltage spread)
- **Max Cell Temp** (optional, if sensor connected)

**Data Sources**:
- Victron/REC BMS 0x373 message
- Format: Min/Max cell voltages and temps

**Color Coding**:
- Delta in RED if >100mV (warning: cells unbalanced)
- Delta in WHITE if ≤100mV (normal)

**Font Sizes**: Extra large for easy reading

---

### 6️⃣ WiFi Screen
**Purpose**: Web configuration interface access

**Displays**:
- **SSID**: "ZombieVerter-Display"
- **Password**: "zombieverter"
- **IP Address**: 192.168.4.1
- **Instructions**: Rotate to exit

**How to Access**:
1. Click button to enter WiFi mode
2. Connect phone/laptop to "ZombieVerter-Display"
3. Open browser to 192.168.4.1
4. Configure parameters
5. Rotate dial to exit WiFi mode

---

### 7️⃣ Settings/Debug Screen
**Purpose**: CAN bus connection diagnostics

**Displays**:
- **Connection Status**: CONNECTED / DISCONNECTED
- **Parameter Count**: Number of params loaded
- **Update Status**: Which params are updating
- **Recent CAN IDs**: Last received messages

**Use Cases**:
- Troubleshooting CAN connection
- Verifying data is flowing
- Debugging new parameters

---

## 📡 CAN Data Sources Summary

### IVT-S Shunt (Isabellenhutte)
**CAN IDs**: 0x411, 0x521-0x528
- **0x411**: Current (I) in mA
- **0x522**: Voltage 2 (U2) in mV - **MAIN BATTERY VOLTAGE**
- **0x526**: Temperature in 0.1°C
- **0x527**: Power (P) in W
- **0x528**: Charge counter (As)

### ZombieVerter Inverter
**CAN IDs**: 0x126, 0x210, 0x257, 0x355, 0x356
- **0x126**: Inverter temperature (bytes 4-5)
- **0x210**: 12V supply voltage (bytes 4-5)
- **0x257**: Motor speed (bytes 0-1)
- **0x355**: State of Charge / SOC (bytes 0-1)
- **0x356**: Motor temperature (bytes 4-5)

### SimpBMS (Victron/REC Protocol)
**CAN IDs**: 0x351-0x37X
- **0x351**: Battery limits/alarms
- **0x355**: SOC % (conflicts with ZombieVerter)
- **0x356**: Pack voltage (conflicts with ZombieVerter)
- **0x373**: Min/Max cell voltages and temps
- **0x35A**: Battery status flags
- **0x35E**: BMS name ("SIMP BMS")

---

## ⚙️ Scaling & Units

### Voltage
- **IVT-S**: Raw in mV, displayed in V (÷1000)
- **ZombieVerter**: Varies by parameter
- **Display**: Always shows with 1 decimal place (e.g., 311.0V)

### Current
- **IVT-S**: Raw in mA, displayed in A (÷1000)
- **Display**: Shows with 1 decimal place (e.g., 1.3A)

### Power
- **IVT-S**: Raw in W, displayed in kW (÷1000)
- **Display**: Shows with 1 decimal place (e.g., 1.8kW)

### Temperature
- **ZombieVerter**: Direct in °C
- **IVT-S**: Raw in 0.1°C, converted to °C (÷10)
- **BMS**: Raw in 0.1°C, converted to °C (÷10)
- **Display**: Integer °C

### SOC
- **Direct**: Percentage (0-100%)
- **Display**: Integer %

---

## 🔧 Configuration

### Hardware
- **CAN Port**: Port B (GPIO 2/1)
- **CAN Speed**: 500 kbps
- **Node ID**: 3 (for ZombieVerter SDO)

### WiFi AP
- **SSID**: ZombieVerter-Display
- **Password**: zombieverter
- **IP**: 192.168.4.1
- **Web Interface**: Parameter configuration

---

## 🎨 Screen Layouts

All screens use:
- **240x240 circular display**
- **High contrast** (white text on black background)
- **Color coding** for warnings/status
- **Large fonts** for readability while driving
- **Minimal animation** for stability

---

## ✅ Working Status

### Fully Working ✅
- Dashboard screen
- Power screen
- Temperature screen (motor + inverter)
- Battery screen (SOC + voltage)
- BMS screen (min/max/delta)
- WiFi configuration
- Settings/debug screen
- IVT-S voltage parsing
- IVT-S power parsing
- ZombieVerter SOC parsing
- ZombieVerter temp parsing
- SimpBMS cell voltage parsing

### Needs Testing 🧪
- Current reading (requires driving/load)
- BMS temperature sensor (no sensor connected)
- 12V supply reading (shows 0V)

### Known Issues 🐛
- 0x356 conflicts between ZombieVerter and SimpBMS (ZombieVerter takes priority)
- IVT-S temp shows 0°C (may not be configured)
- Current only appears when driving (normal behavior)

---

## 📝 Update History

**v1.0** - Initial release
- Basic screen layouts
- CAN data parsing
- WiFi configuration

**v2.0** - IVT-S Integration
- Fixed voltage scaling (÷1000 not ÷100)
- Fixed power scaling (÷1000 not ÷100)
- Fixed current scaling (÷1000 not ÷100)
- Added IVT-S shunt support
- Corrected voltage source (0x522 not 0x521)

**v3.0** - BMS Integration
- Added SimpBMS Victron/REC protocol support
- Large font BMS screen
- Min/Max/Delta cell voltage display
- Color-coded delta warning (>100mV)

**v3.1** - Final Polish
- All screens tested and working
- Motor temp from 0x356 working
- SOC display working
- Clean documentation

---

## 🚀 Quick Start

1. **Upload firmware** via PlatformIO
2. **Power on** M5Stack Dial
3. **Wait for CAN** connection (green "CAN" indicator)
4. **Rotate dial** to switch between screens
5. **Click button** for WiFi configuration if needed
6. **Drive!** Current readings will appear when driving

---

## 📞 Troubleshooting

**No CAN data?**
- Check Port B wiring (GPIO 2 = CAN_TX, GPIO 1 = CAN_RX)
- Verify 500 kbps CAN speed
- Check CAN termination resistors
- View Settings screen for diagnostics

**Wrong values?**
- Check scaling in this guide
- Verify CAN message formats match
- Check parameter definitions in params.json

**Screen not updating?**
- Check Settings screen for update status
- Verify CAN IDs are being received
- Check lastUpdateTime values

**WiFi not working?**
- Click button to enable AP mode
- Look for "ZombieVerter-Display" SSID
- Password: zombieverter
- Connect to 192.168.4.1

---

**Enjoy your M5Stack Dial ZombieVerter Display!** 🎉
