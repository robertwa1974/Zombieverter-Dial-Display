# Which params.json Should You Upload?

## Quick Answer

**Upload the SHORT version** - the one that just contains the parameter definitions without all the spot values.

## Explanation

The ZombieVerter VCU typically has TWO different JSON files:

### 1. **params.json** (SHORT - Use This One!) ✅

This contains just the **parameter definitions**:
```json
{
  "parameters": [
    {
      "id": 1,
      "name": "Motor RPM",
      "type": "int16",
      "unit": "rpm",
      "min": 0,
      "max": 6000,
      "decimals": 0,
      "editable": false
    },
    {
      "id": 2,
      "name": "Power",
      ...
    }
  ]
}
```

**This is what you upload** because it tells the display:
- Which CAN parameter IDs to read
- What to call each parameter
- What units to display
- Min/max ranges
- Whether it's editable

### 2. **spot_values.json** or **full_config.json** (LONG - Don't Use) ❌

This contains **actual current values** from the VCU:
```json
{
  "parameters": [
    {
      "id": 1,
      "name": "Motor RPM", 
      "value": 3456,
      "timestamp": "2024-01-29T12:34:56"
    },
    ...
  ],
  "spot_values": {
    "rpm": 3456,
    "power": 45.2,
    ...
  }
}
```

**Don't upload this** because:
- It contains snapshot data, not definitions
- The display reads live values from CAN bus anyway
- It's much larger and can cause memory issues
- Values would be stale/outdated immediately

## How to Get the Right File

### From ZombieVerter Web Interface

1. Log into your ZombieVerter web interface
2. Go to **Configuration** or **Parameters** section
3. Look for **"Download Parameter Definitions"** or **"Export Parameters"**
4. Download that file (should be small, ~2-10 KB)

### From OpenInverter Wiki

1. Visit https://openinverter.org/wiki/
2. Navigate to your specific inverter model
3. Download the example params.json
4. Customize the IDs to match your setup

### Create Your Own

You can create a minimal version:

```json
{
  "version": "1.0",
  "parameters": [
    {"id": 1, "name": "Motor RPM", "type": "int16", "unit": "rpm", "min": 0, "max": 6000, "decimals": 0, "editable": false},
    {"id": 2, "name": "DC Voltage", "type": "int16", "unit": "V", "min": 0, "max": 450, "decimals": 0, "editable": false},
    {"id": 3, "name": "DC Current", "type": "int16", "unit": "A", "min": -300, "max": 300, "decimals": 0, "editable": false}
  ]
}
```

Add only the parameters you want to display.

## File Size Guidelines

| Type | Size | Status |
|------|------|--------|
| Parameter definitions | 1-10 KB | ✅ Perfect |
| Parameter definitions | 10-16 KB | ⚠️ OK but large |
| With spot values | 20+ KB | ❌ Too large, will crash |

The display can handle up to **16 KB**, but smaller is better.

## What the Display Does

1. **You upload**: Parameter definitions (which params to show)
2. **Display reads**: Live values from CAN bus
3. **Display shows**: Live data with your parameter names/units

The display is **NOT** storing historical data - it shows real-time CAN values.

## Common Mistakes

### ❌ Wrong: Uploading Spot Values
```json
{
  "rpm": 3456,
  "voltage": 350,
  "current": 125
}
```
This is just data, not definitions!

### ❌ Wrong: Uploading Full VCU Config
```json
{
  "version": "5.24.R",
  "vcu_config": {...},
  "parameters": [...],
  "spot_values": {...},
  "can_map": {...}
}
```
This is the entire VCU configuration - way too big!

### ✅ Correct: Parameter Definitions Only
```json
{
  "parameters": [
    {"id": 1, "name": "Motor RPM", "type": "int16", ...},
    {"id": 2, "name": "Power", "type": "int16", ...}
  ]
}
```
Clean, simple, just the definitions!

## Troubleshooting

### "File too large" Error

**Problem**: Upload fails with size error
**Solution**:
- Remove unnecessary parameters
- Remove any "spot_values" section
- Remove comments (JSON doesn't officially support them anyway)
- Keep only parameters you actually want to display

### Boot Loop After Upload

**Problem**: Device crashes and reboots continuously
**Solution**:
- The file was too large or contained invalid JSON
- Flash erase required (you already did this ✅)
- Use the fixed version with better error handling
- Upload a smaller file next time

### Parameters Don't Update

**Problem**: Display shows 0 or old values
**Solution**:
- Verify CAN bus is connected
- Check parameter IDs match your VCU
- IDs in params.json must match what VCU transmits
- Enable debug mode to see CAN messages

## Testing Your JSON

Before uploading, validate your JSON:

1. **Online Validator**: https://jsonlint.com/
2. **Copy/paste your JSON**
3. **Click "Validate JSON"**
4. **Fix any errors** before uploading

Common JSON errors:
- Missing comma between items
- Extra comma after last item
- Mismatched brackets `[]` or braces `{}`
- Unquoted property names
- Single quotes instead of double quotes

## Recommended Workflow

1. ✅ Start with minimal params.json (3-5 parameters)
2. ✅ Upload and verify it works
3. ✅ Add more parameters gradually
4. ✅ Test after each upload
5. ✅ Keep a backup copy on your computer

## Example: Minimal Working File

Save this as `params.json`:

```json
{
  "parameters": [
    {"id": 1, "name": "RPM", "type": "int16", "unit": "rpm", "min": 0, "max": 6000, "decimals": 0, "editable": false},
    {"id": 2, "name": "Power", "type": "int16", "unit": "kW", "min": 0, "max": 100, "decimals": 1, "editable": false},
    {"id": 3, "name": "Voltage", "type": "int16", "unit": "V", "min": 0, "max": 400, "decimals": 0, "editable": false}
  ]
}
```

Upload this first to test - it should work perfectly!

## Summary

✅ **DO**: Upload parameter definitions (SHORT file)
❌ **DON'T**: Upload spot values or full config (LONG file)
💾 **SIZE**: Keep under 16 KB (ideally under 10 KB)
🧪 **TEST**: Validate JSON before uploading
💡 **TIP**: Start small, add more parameters later

Need help? Check your VCU documentation for the correct params.json format!
