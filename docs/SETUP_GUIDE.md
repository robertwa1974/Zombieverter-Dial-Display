# M5Dial ZombieVerter Display - Setup Guide

## Table of Contents
1. [What You Need](#what-you-need)
2. [Hardware Setup](#hardware-setup)
3. [Firmware Installation](#firmware-installation)
4. [First Boot & Configuration](#first-boot--configuration)
5. [Daily Operation](#daily-operation)
6. [Theory of Operation](#theory-of-operation)
7. [Troubleshooting](#troubleshooting)

---

## What You Need

### Hardware
- **M5Stack Dial** (ESP32-S3 based rotary display)
- **SN65HVD230 CAN transceiver module** (3.3V version - CRITICAL!)
- **CAN bus connection** to your ZombieVerter (CANH, CANL, GND)
- **USB-C cable** for programming and power
- **Jumper wires** for CAN connections

### Software
- **PlatformIO** (recommended) or **Arduino IDE**
- **USB serial driver** for ESP32-S3
- This firmware package

### ZombieVerter Requirements
- Firmware version supporting SDO (Service Data Object) protocol
- CAN bus configured and operational
- Node ID set to 3 (default for ZombieVerter)
- CAN bitrate: 500kbps (standard)

---

## Hardware Setup

### Step 1: CAN Transceiver Wiring

**CRITICAL: Use a 3.3V CAN transceiver module (SN65HVD230), NOT 5V!**

Connect the CAN transceiver to M5Dial:

| CAN Transceiver Pin | M5Dial Pin | Notes |
|---------------------|------------|-------|
| VCC | 3.3V | From M5Dial Grove connector |
| GND | GND | Common ground |
| TX | GPIO 2 | CAN TX |
| RX | GPIO 1 | CAN RX |
| CANH | ZombieVerter CANH | Yellow wire typical |
| CANL | ZombieVerter CANL | Green wire typical |

**Wiring Notes:**
- The M5Dial Grove connector provides 3.3V and GND
- GPIO 1 and 2 are exposed on the Grove connector
- CANH and CANL must connect to your vehicle's CAN bus where ZombieVerter is connected
- **Ground connection is critical** - ensure solid connection between M5Dial and ZombieVerter ground

### Step 2: CAN Bus Termination

**Check if termination is needed:**
- CAN bus should have 120Ω resistor at each end
- If ZombieVerter is at one end, it likely has termination
- If M5Dial is at the other end, add a 120Ω resistor between CANH and CANL
- Use a multimeter: measure resistance between CANH and CANL with power OFF
  - Should read ~60Ω if properly terminated at both ends
  - Reads ~120Ω if only one terminator present

### Step 3: Physical Mounting

The M5Dial is designed for dashboard mounting:
- Clean, flat surface required
- Avoid direct sunlight (screen washout)
- Within reach for dial interaction
- USB-C cable should be secured to prevent disconnection

---

## Firmware Installation

### Method 1: PlatformIO (Recommended)

1. **Install VS Code** from https://code.visualstudio.com/
2. **Install PlatformIO extension** in VS Code
3. **Extract firmware package** to a project folder
4. **Open project** in VS Code (File → Open Folder)
5. **Connect M5Dial** via USB-C
6. **Build and upload:**
   - Click the PlatformIO icon (alien head) in left sidebar
   - Click "Upload" under "Project Tasks"
   - Wait for "SUCCESS" message

### Method 2: Arduino IDE

1. **Install Arduino IDE 2.x** from https://www.arduino.cc/
2. **Add ESP32 board support:**
   - File → Preferences → Additional Board Manager URLs
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. **Install ESP32 boards:**
   - Tools → Board → Boards Manager
   - Search "ESP32" and install "esp32 by Espressif Systems"
4. **Select board:**
   - Tools → Board → ESP32 Arduino → ESP32S3 Dev Module
5. **Configure board settings:**
   - USB CDC On Boot: Enabled
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs
6. **Open main.cpp** from src/ folder
7. **Upload** via Tools → Upload

### Verify Installation

After upload completes:
1. M5Dial should display the PIN entry screen (lock icon)
2. Serial Monitor (115200 baud) should show:
   ```
   [BOOT] M5Dial Initializing...
   [CAN] Initializing CAN bus...
   [CAN] CAN bus initialized successfully
   [IMMOBILIZER] System LOCKED - Waiting for PIN
   ```

---

## First Boot & Configuration

### Default PIN Code

**Default PIN: 1234**

Enter using the rotary dial:
1. Rotate dial to select first digit (1)
2. Press button to confirm
3. Repeat for remaining digits (2, 3, 4)
4. After 4th digit, display automatically unlocks

### Change PIN (Recommended!)

Currently, PIN is hardcoded in `src/Immobilizer.cpp`:
```cpp
const char DEFAULT_PIN[5] = "1234";
```

To change:
1. Edit `src/Immobilizer.cpp` line 19
2. Change `"1234"` to your 4-digit PIN (keep quotes)
3. Recompile and upload firmware

**Future versions will support PIN change via web interface.**

### WiFi Access Point

To access web interface:
1. **While unlocked**, press and hold the M5Dial button for 2 seconds
2. Display shows "WiFi Mode" with connection info
3. Connect to WiFi network:
   - **SSID:** `ZombieVerter-Display`
   - **Password:** `zombieverter`
4. Open browser to: `http://192.168.4.1`

### Web Interface Overview

The web interface has two tabs:

**Dashboard Tab:**
- Real-time parameter display
- Voltage, current, temperature, SOC, speed
- Updates every 500ms from ZombieVerter

**Parameters Tab:**
- View/edit all 119 ZombieVerter parameters
- Buttons:
  - **Refresh (slow!):** Query all parameters via SDO (~30-60 seconds)
  - **Download JSON:** Backup current values to file
  - **Upload JSON:** Restore parameters from backup file
  - **Save to Flash:** Tell ZombieVerter to save RAM parameters to EEPROM

---

## Daily Operation

### Normal Operation

1. **Power On:** M5Dial boots to PIN entry screen
2. **Unlock:** Enter PIN (1234 default) or tap RFID card (if configured)
3. **Main Screens:** Rotate dial to cycle through:
   - Dashboard (voltage, current, SOC, speed)
   - Battery details (cell voltages, temperature)
   - Motor status (temperature, RPM)
   - Inverter status
   - Gear selection
   - Regen control
   - Motor selection (if dual-motor)

4. **Adjust Parameters:** Rotate dial when on adjustable screens (gear, regen, motor)
5. **Press Button:** Quick press to toggle WiFi, double-click to lock (for testing)

### Screen Navigation

| Screen ID | Name | Editable | Function |
|-----------|------|----------|----------|
| 0 | Dashboard | No | Main status display |
| 1 | Battery | No | SOC, voltages, temps |
| 2 | Motor | No | Motor temp, speed |
| 3 | Inverter | No | Inverter temp, power |
| 4 | Gear | Yes | Change transmission gear |
| 5 | Regen | Yes | Adjust regenerative braking |
| 6 | Motor Select | Yes | Choose active motor (dual-motor only) |

**Rotate dial** to change screens
**Rotate while on editable screen** to change parameter value

### Immobilizer Feature

**When LOCKED:**
- Display shows PIN entry screen
- SDO commands send `idcmax=0` to ZombieVerter every 100ms
- Vehicle cannot draw current (immobilized)
- All other screens blocked

**When UNLOCKED:**
- Full access to all screens
- SDO commands send `idcmax=500A` to ZombieVerter
- Normal vehicle operation

**Note:** Current firmware has immobilizer in testing phase. The lock/unlock SDO commands are being sent but may not take effect depending on ZombieVerter configuration.

---

## Theory of Operation

### System Architecture

```
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│   M5Dial    │◄──CAN──►│ CAN Trans-   │◄──CAN──►│ ZombieVerter │
│  ESP32-S3   │         │  ceiver      │         │  Controller  │
│             │         │ SN65HVD230   │         │              │
└─────────────┘         └──────────────┘         └──────────────┘
      ↓                                                  ↑
  USB Power                                        CAN Protocol
  Web Interface                                    SDO Commands
```

### Communication Protocol

The M5Dial communicates with ZombieVerter using two CAN protocols:

1. **Broadcast Messages (Receive Only)**
   - ZombieVerter continuously broadcasts status on CAN IDs like:
     - 0x257: Motor speed (RPM)
     - 0x126: Heatsink temperature
     - 0x355: Battery SOC
     - 0x356: Motor temperature
   - M5Dial listens and updates display in real-time
   - No request needed - passive reception

2. **SDO (Service Data Object) Protocol**
   - Used for reading/writing parameters
   - Request-response model
   - M5Dial sends command on 0x603 → ZombieVerter responds on 0x583

### SDO Command Format

**Read Parameter (0x40 command):**
```
TX 0x603: [40 00 21 PP 00 00 00 00]
          │  │  │  │
          │  │  │  └─ Parameter ID (subindex)
          │  │  └──── Index high byte (0x2100 = ZombieVerter params)
          │  └─────── Index low byte
          └────────── SDO read command (0x40)

RX 0x583: [43 00 21 PP VV VV VV VV]
          │  │  │  │  │
          │  │  │  │  └─ Value (4 bytes, little-endian)
          │  │  │  └──── Parameter ID (echo)
          │  │  └─────── Index (echo)
          │  └────────── Index (echo)
          └───────────── SDO read response (0x43)
```

**Write Parameter (0x23 command):**
```
TX 0x603: [23 00 21 PP VV VV VV VV]
          │  │  │  │  │
          │  │  │  │  └─ Value to write (4 bytes)
          │  │  │  └──── Parameter ID
          │  │  └─────── Index high byte
          │  └────────── Index low byte
          └───────────── SDO write command (0x23)

RX 0x583: [60 00 21 PP 00 00 00 00]
          │  │  │  │
          │  │  │  └──── Parameter ID (confirmation)
          │  │  └─────── Index (echo)
          │  └────────── Index (echo)
          └───────────── SDO write ACK (0x60)

OR (error):
RX 0x583: [80 00 21 PP AA AA AA AA]
          │  │  │  │  │
          │  │  │  │  └─ Abort code (e.g., 0x06090030 = value range exceeded)
          │  │  │  └──── Parameter ID
          │  │  └─────── Index (echo)
          │  └────────── Index (echo)
          └───────────── SDO abort (0x80)
```

### Fixed-Point Encoding

ZombieVerter uses fixed-point arithmetic (multiply by 32) for most parameters:

**When READING:**
```cpp
displayValue = sdoValue / 32.0f;
```
Example: SDO returns 3200 → Display shows 100.0 (3200 ÷ 32 = 100)

**When WRITING:**
```cpp
sdoValue = displayValue * 32;
```
Example: User sets 100 → SDO sends 3200 (100 × 32 = 3200)

**Exceptions (RAW values, no encoding):**
- Configuration enums (Inverter type, vehicle type, etc.)
- GPIO assignments (Out1Func, PWM functions, etc.)
- Digital ADC thresholds (potmin, potmax, etc.)
- Time values (Set_Hour, Chg_Hrs, etc.)
- PWM registers (Tim3_Presc, etc.)

### Data Flow Example: Changing Gear

1. **User rotates dial** on Gear screen
2. **Encoder interrupt** fires (GPIO interrupt on dial rotation)
3. **CANDataManager::setParameter(27, newGear)** called
4. **Direct CAN mapping** used (not SDO):
   ```cpp
   msg.id = 0x300;
   msg.data[0] = newGear;  // 0-3
   // Fill rest with zeros
   ```
5. **ZombieVerter receives** on CAN ID 0x300
6. **Gear changes** immediately (no SDO handshake needed)
7. **Display updates** optimistically

### Data Flow Example: Web Interface Parameter Change

1. **User edits** parameter in web interface
2. **HTTP POST** to `/set?id=37&value=450` (set idcmax to 450A)
3. **WebInterface::setParameterValue()** called
4. **Fixed-point encoding** applied: 450 × 32 = 14400
5. **SDO write** command sent:
   ```
   0x603: [23 00 21 25 90 38 00 00]
          │  │  │  │  └──────────── 14400 = 0x3890 (little-endian)
          │  │  │  └───────────────  Param 37 (0x25 hex)
          │  │  └────────────────── Index 0x2100
          │  └───────────────────── Index 0x2100
          └──────────────────────── Write command
   ```
6. **ZombieVerter responds** on 0x583:
   - Success: `[60 00 21 25 ...]` → Display "✓ Parameter updated"
   - Error: `[80 00 21 25 ...]` → Display error code
7. **Parameter stored in RAM** on ZombieVerter
8. **User clicks "Save to Flash"** → ZombieVerter saves RAM to EEPROM

### Memory Architecture

**M5Dial (ESP32-S3):**
- Program Flash: ~1.5MB used (includes all 119 parameter definitions)
- RAM: ~50KB used (dynamic CAN parameter storage, web server)
- SPIFFS: Not currently used (reserved for future features)
- Parameters: NOT stored locally - all queries go to ZombieVerter via SDO

**ZombieVerter:**
- Parameter storage: Internal EEPROM/flash
- RAM: Active parameter values (updated via SDO or CAN mapping)
- Flash: Persistent storage (saved when "Save to Flash" clicked)

**Key Concept:** M5Dial is a "view/controller" only. It stores NO vehicle parameters locally. Every value displayed is queried from ZombieVerter in real-time or received via broadcast CAN messages.

### Component Responsibilities

| Component | Responsibilities |
|-----------|------------------|
| **M5Dial ESP32** | CAN communication, display rendering, user input, web server |
| **CAN Transceiver** | Physical layer conversion (3.3V logic ↔ CAN differential) |
| **ZombieVerter** | Parameter storage, motor control, actual vehicle logic |
| **CAN Bus** | Transport layer for all communication |

---

## Troubleshooting

### M5Dial Won't Boot

**Symptoms:** Black screen, no response
**Solutions:**
1. Check USB-C cable - try different cable
2. Hold button while plugging in USB (enters bootloader mode)
3. Check Serial Monitor at 115200 baud for error messages
4. Reflash firmware using esptool.py if corrupted

### No CAN Communication

**Symptoms:** "No CAN response" errors in Serial Monitor
**Solutions:**
1. **Check wiring:**
   - CANH and CANL not swapped
   - Ground connected between M5Dial and ZombieVerter
   - 3.3V power to CAN transceiver
2. **Check termination:**
   - Should be ~60Ω between CANH and CANL
3. **Verify ZombieVerter CAN:**
   - Use CAN analyzer to confirm ZombieVerter is transmitting
   - Check ZombieVerter CAN bitrate (should be 500kbps)
4. **Check transceiver voltage:**
   - **CRITICAL:** Must be 3.3V, not 5V!
   - Measure VCC pin - should be 3.3V

### Parameters Show Garbage Values

**Symptoms:** Crazy numbers like "3200" instead of "100"
**Solution:** This is expected! Web interface should auto-convert (divide by 32). If you see raw values:
1. Check that `usesRawValue()` function is working
2. Verify parameter name matches exactly (case-sensitive)
3. Some parameters (like Gear) actually ARE raw values

### SDO Timeouts

**Symptoms:** "SDO timeout" in Serial Monitor
**Solutions:**
1. **ZombieVerter too busy:**
   - Reduce query frequency
   - Don't click "Refresh" while driving
2. **Wrong node ID:**
   - Verify ZombieVerter is node 3
   - Check ZombieVerter CAN settings
3. **CAN bus noise:**
   - Check for loose connections
   - Add 120Ω termination if missing
   - Keep CAN wires twisted, away from high-current wires

### Web Interface Won't Load

**Symptoms:** Can't connect to 192.168.4.1
**Solutions:**
1. **Verify WiFi mode enabled:**
   - Press button to toggle WiFi
   - Display should show "WiFi Mode" with IP
2. **Check connection:**
   - SSID: `ZombieVerter-Display`
   - Password: `zombieverter`
   - Sometimes takes 10-15 seconds to appear
3. **Try different device:**
   - Some phones/laptops have issues with 192.168.4.x
   - Use phone browser if laptop fails
4. **Restart M5Dial:**
   - Power cycle
   - Try enabling WiFi again

### Immobilizer Doesn't Work

**Known Issue:** The immobilizer SDO commands are sent correctly but may not take effect on the ZombieVerter. This is being investigated.

**Current Status:**
- LOCKED: Sends `idcmax=0` via SDO → Gets ACK from ZombieVerter ✓
- UNLOCKED: Sends `idcmax=500` via SDO → Gets "value range exceeded" error ✗

**Possible Causes:**
- ZombieVerter firmware may restrict SDO writes to idcmax
- Parameter may have safety limits preventing external control
- Alternative implementation may be needed (CAN mapping vs SDO)

**Workaround:** Immobilizer feature is currently for testing only. Do not rely on it for vehicle security.

### Serial Monitor Shows Errors

**Common Error Codes:**

| Error Code | Meaning | Solution |
|------------|---------|----------|
| 0x06090030 | Value range exceeded | Value too high/low, check ZombieVerter limits |
| 0x06090011 | Subindex does not exist | Wrong parameter ID |
| 0x06010000 | Unsupported access | Parameter is read-only |
| 0x08000020 | Data cannot be transferred | CAN bus issue |
| 0x08000021 | Local control | ZombieVerter has local control priority |
| 0x08000022 | Device state | Not allowed in current state (e.g., while running) |

### Getting More Help

**Enable Debug Output:**
Serial Monitor (115200 baud) shows detailed logs:
```
[CAN] CAN TX: ID=0x603 Len=8 Data=[40 00 21 25 00 00 00 00]
[CAN] CAN RX: ID=0x583 Len=8 Data=[43 00 21 25 80 3E 00 00]
[SDO] Parameter 37 read: value=16000 (decoded: 500)
```

**Forum Support:**
Post on OpenInverter forum (openinverter.org/forum) with:
1. M5Dial firmware version
2. ZombieVerter firmware version
3. Serial Monitor output (copy/paste error messages)
4. What you were trying to do when error occurred

**GitHub Issues:**
If you find a bug, open an issue with full details and Serial Monitor logs.

---

## Appendix: Pin Reference

### M5Dial GPIO Usage

| GPIO | Function | Notes |
|------|----------|-------|
| 1 | CAN RX | Grove connector |
| 2 | CAN TX | Grove connector |
| 40 | Display SDA | Internal |
| 41 | Display SCL | Internal |
| 42 | Encoder A | Rotary dial |
| 43 | Button | Center button + dial button |

### CAN Transceiver Connections

```
M5Dial Grove Connector Pinout (Top View):
┌────────────────┐
│ 1   2   3   4  │
│GND  5V  G2  G1 │
└────────────────┘
     │       │  │
     │       │  └─ GPIO 1 (CAN RX)
     │       └──── GPIO 2 (CAN TX)
     └──────────── Use for GND/VCC (but use 3.3V regulator!)
```

**CRITICAL:** M5Dial Grove provides 5V, but CAN transceiver needs 3.3V. Use 3.3V regulator or power transceiver from M5Dial's 3.3V output (check schematic).

---

## Safety Notice

**This device is for monitoring and configuration only. It does NOT control vehicle safety systems.**

- Do not rely on immobilizer for security (under development)
- Always ensure ZombieVerter has proper safety limits configured
- Parameter changes can affect vehicle behavior - understand what you're changing
- Test parameter changes at low speed in safe area
- Always click "Save to Flash" to persist important changes
- Keep backup of parameter settings (Download JSON button)

**When in doubt, consult the OpenInverter community before changing parameters!**
