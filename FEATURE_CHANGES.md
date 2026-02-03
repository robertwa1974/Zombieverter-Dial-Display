# Feature Updates

## Changes Made

### 1. ✅ Encoder Controls Changed

**Before:**
- Rotate encoder: Adjust values
- Click: Next screen
- Double-click: Save value
- Long-press: Previous screen

**After:**
- **Rotate encoder: Switch between screens** ⬅️➡️
- Click: Reserved for future use
- Double-click: Reserved for future use  
- Long-press: Jump back to Dashboard

**Why:** More intuitive - just turn the dial to browse through screens!

### 2. ✅ New BMS Screen Added

Added a dedicated screen showing SIMPBMS data:

**BMS Screen displays:**
- 🟢 **Max Cell Voltage** (highest cell in pack)
- 🟡 **Min Cell Voltage** (lowest cell in pack)
- 🔵 **Avg Cell Voltage** (pack average)
- 🟠 **Delta Cell Voltage** (difference between max and min)
- 🟣 **Avg Cell Temperature** (pack temperature)

**Layout:**
```
╔════════════════════╗
║    BMS Status      ║
║                    ║
║  Max Cell:         ║
║     3.65V          ║
║                    ║
║  Min Cell:         ║
║     3.62V          ║
║                    ║
║  Avg Cell:         ║
║     3.63V          ║
║                    ║
║ Delta: 30mV  Temp: 25°C ║
╚════════════════════╝
```

## Updated Screen Order

1. **Splash** - Boot screen
2. **Dashboard** - Main overview (speed, power, voltage)
3. **Power** - Detailed power metrics
4. **Temperature** - Motor and inverter temps
5. **Battery** - SOC and pack voltage
6. **🆕 BMS** - Cell voltages and temperatures
7. **Settings** - Configuration

## CAN Parameter IDs for SIMPBMS

The following CAN parameter IDs were added:

| ID | Parameter | Unit | Type | Range |
|----|-----------|------|------|-------|
| 20 | Max Cell Voltage | mV | uint16 | 2500-4200 |
| 21 | Min Cell Voltage | mV | uint16 | 2500-4200 |
| 22 | Avg Cell Voltage | mV | uint16 | 2500-4200 |
| 23 | Delta Cell Voltage | mV | uint16 | 0-500 |
| 24 | Avg Cell Temp | °C | int16 | -20 to 80 |

**Note:** These are example IDs. You'll need to update them to match your actual SIMPBMS CAN output configuration.

## Configuring for Your SIMPBMS

### Step 1: Find Your SIMPBMS CAN IDs

Check your SIMPBMS configuration to find the actual CAN message IDs for:
- Highest cell voltage
- Lowest cell voltage  
- Average cell voltage
- Cell voltage delta
- Average cell temperature

### Step 2: Update the Parameter IDs

Edit `src/main.cpp` and change the parameter IDs in the `sampleParams` JSON:

```cpp
const char* sampleParams = R"(
{
  "parameters": [
    ...
    {"id": YOUR_MAX_CELL_ID, "name": "Max Cell Voltage", ...},
    {"id": YOUR_MIN_CELL_ID, "name": "Min Cell Voltage", ...},
    {"id": YOUR_AVG_CELL_ID, "name": "Avg Cell Voltage", ...},
    {"id": YOUR_DELTA_ID, "name": "Delta Cell Voltage", ...},
    {"id": YOUR_TEMP_ID, "name": "Avg Cell Temp", ...}
  ]
}
)";
```

### Step 3: Update Units if Needed

SIMPBMS might output voltage in different units:
- **Volts (V)**: Change `"unit": "V"` and adjust min/max accordingly
- **Millivolts (mV)**: Keep as is (current configuration)

### Step 4: Update the BMS Screen Code

If your SIMPBMS uses different parameter IDs, update in `src/UIManager.cpp`:

```cpp
void UIManager::drawBMS() {
    // Change these IDs to match your SIMPBMS
    CANParameter* cellVoltMax = canManager->getParameter(YOUR_MAX_CELL_ID);
    CANParameter* cellVoltMin = canManager->getParameter(YOUR_MIN_CELL_ID);
    CANParameter* cellVoltAvg = canManager->getParameter(YOUR_AVG_CELL_ID);
    CANParameter* cellVoltDelta = canManager->getParameter(YOUR_DELTA_ID);
    CANParameter* cellTempAvg = canManager->getParameter(YOUR_TEMP_ID);
    ...
}
```

## Testing

1. **Build and upload** the firmware
2. **Rotate the dial** - screens should switch
3. **Long-press** the button - should jump back to Dashboard
4. **Connect to CAN bus** with SIMPBMS
5. **Navigate to BMS screen** - should show cell data

## Troubleshooting

**BMS screen shows no data:**
- Verify SIMPBMS is transmitting on CAN bus
- Check parameter IDs match your SIMPBMS configuration
- Enable debug mode to see CAN messages:
  ```cpp
  #define DEBUG_CAN  true
  ```
- Monitor serial output to verify messages are being received

**Screen doesn't switch when rotating:**
- Check encoder is properly connected (GPIO 40/41)
- Verify encoder direction (may need to swap A/B pins)
- Check serial monitor for input events

## Future Enhancements

Potential features you could add:
- **Click button** to highlight/edit individual cells
- **Cell imbalance warning** when delta > threshold
- **Temperature warning** when cells too hot/cold
- **Historical min/max** tracking
- **Per-cell voltage display** (if SIMPBMS provides it)
- **Color coding** (green=good, yellow=warning, red=critical)

## Notes

- All SIMPBMS parameters are marked as **read-only** (not editable)
- Values update every 100ms from CAN bus
- Screen updates 10 times per second for smooth display
- BMS data is stored alongside ZombieVerter parameters

Enjoy your new BMS monitoring screen! 🔋📊
