# How to Update ZombieVerter Parameters

If the ZombieVerter firmware changes parameter definitions in future releases, you'll need to update the hardcoded parameter list in this firmware.

## When to Update

You should update the parameter list if:
- ZombieVerter adds new parameters
- Parameter IDs change
- Parameter names or units change
- Enum values (dropdown options) change

## How to Get Current Parameters

### Method 1: Export from ZombieVerter Web Interface

1. Connect to your ZombieVerter web interface (usually via WiFi or USB)
2. Go to the Parameters tab
3. Click "Save Parameters" or export function
4. This creates a `params.json` file with all current parameter definitions

### Method 2: Get from OpenInverter GitHub

1. Visit https://github.com/jsphuebner/stm32-sine
2. Navigate to the specific inverter variant (e.g., `stm32-sine/src/`)
3. Look for parameter definition files (usually `.cpp` or `.h` files)
4. The parameter list is typically in a file like `param_prj.cpp`

## Updating the Firmware

### Step 1: Locate the Parameter Array

Open `src/WebInterface.cpp` and find the `ParamDef params[]` array (around line 302).

### Step 2: Update Parameter Definitions

Each parameter entry looks like this:
```cpp
{"ParamName", ID, "unit", "Category", isEditable},
```

**Example:**
```cpp
{"idcmax", 37, "A", "Throttle", true},
```

**Fields:**
- **ParamName**: Exact name from ZombieVerter (case-sensitive)
- **ID**: Parameter SDO subindex (the "i" field in params.json)
- **unit**: Display unit (V, A, %, rpm, °C, etc.)
- **Category**: Grouping for web interface
- **isEditable**: `true` if user can change it, `false` for read-only

### Step 3: Fixed-Point Encoding

**IMPORTANT:** Most ZombieVerter parameters use fixed-point encoding (multiply by 32).

The firmware handles this automatically in `WebInterface.cpp` using the `usesRawValue()` function.

**Parameters that use RAW values (no ×32 encoding):**
- Configuration enums (Inverter, Vehicle, Gear, etc.)
- CAN bus assignments (InverterCan, VehicleCan, etc.)
- GPIO function assignments (Out1Func, PWM1Func, etc.)
- Digital ADC values (potmin, potmax, BrkVacThresh, etc.)
- Time values (Set_Hour, Chg_Hrs, etc.)
- PWM timer registers (Tim3_Presc, Tim3_Period, etc.)

**Parameters that use ×32 encoding:**
- Voltages (udcmin, udclim, Voltspnt, etc.)
- Currents (idcmax, idcmin, CCS_ILim, etc.)
- Percentages (throtmax, regenmax, SOC, etc.)
- Temperatures (tmphsmax, tmpmmax, FanTemp, etc.)
- RPM values (regenrpm, revlim, cruisestep, etc.)
- Power (Pwrspnt, HeatPwr)
- Rates (throtramp, regenramp)

If you add new parameters, check the ZombieVerter source code to see if they use fixed-point encoding.

### Step 4: Compile and Flash

1. Open project in PlatformIO (VS Code)
2. Build firmware: `pio run`
3. Upload to M5Dial: `pio run -t upload`

Or use the Arduino IDE:
1. Open `src/main.cpp`
2. Click "Verify" to compile
3. Click "Upload" to flash

## Example Update

**Old parameter (changed name):**
```cpp
{"throtmax", 41, "%", "Throttle", true},
```

**New parameter (if ZombieVerter renamed it):**
```cpp
{"throttlemax", 41, "%", "Throttle", true},
```

**New parameter added:**
```cpp
{"newparam", 119, "V", "Power", true},  // Add to end of array
```

## Testing After Update

1. Upload firmware to M5Dial
2. Connect to web interface (button press to enable WiFi)
3. Go to Parameters tab
4. Click "Refresh (slow!)" to query all parameters
5. Verify values display correctly
6. Try changing a parameter and clicking "Save to Flash"
7. Power cycle ZombieVerter and verify changes persisted

## Common Issues

### Parameter Shows Wrong Value
- Check if it needs fixed-point encoding (most do)
- Verify the parameter ID matches ZombieVerter's current firmware
- Some parameters are read-only on ZombieVerter's side

### Parameter Won't Change
- Verify `isEditable` is set to `true`
- Check if parameter has safety limits in ZombieVerter firmware
- Use Serial Monitor to see SDO abort codes (0x06090030 = value range exceeded)

### Compile Errors
- Too many parameters: Reduce PARAM_COUNT or remove unused parameters
- Syntax errors: Check commas, quotes, and brackets
- Memory issues: The ESP32-S3 has limited flash; 119 parameters is near the limit

## Getting Help

If you need assistance:
1. Check Serial Monitor output for error messages
2. Post on OpenInverter forum with:
   - Your ZombieVerter firmware version
   - The parameter you're trying to update
   - Any error messages from Serial Monitor
3. Include the params.json from your ZombieVerter for comparison

## Notes

- The M5Dial firmware stores NO parameters locally - all changes are sent directly to ZombieVerter via SDO
- The "Save to Flash" button tells ZombieVerter to save parameters from RAM to EEPROM
- Parameter changes are lost on ZombieVerter power cycle unless you click "Save to Flash"
- The hardcoded list is ONLY for the web interface display - you can modify any parameter 0-255 via SDO regardless
