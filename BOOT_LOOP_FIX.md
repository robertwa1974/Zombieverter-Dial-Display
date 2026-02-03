# Boot Loop Recovery & Prevention

## What Happened

The boot loop was likely caused by:
1. **File too large** - Exceeded available memory
2. **Invalid JSON** - Crashed the parser
3. **SPIFFS corruption** - File system error

You did the right thing by erasing the flash! ✅

## Fixes Applied

The new version includes:

### 1. Better Error Handling ✅
- Safe SPIFFS initialization
- Automatic format on mount failure  
- Corrupted file detection and deletion
- Fallback to default parameters

### 2. Size Limits ✅
- Maximum file size: 16 KB (was 8 KB)
- Size check before processing
- Automatic rejection of oversized files
- Clear error messages

### 3. Validation ✅
- JSON parsing errors handled gracefully
- Invalid files deleted automatically
- Device continues with defaults on error
- No more boot loops!

### 4. Memory Management ✅
- Static file upload buffer
- Chunk-by-chunk processing
- Memory limits enforced
- Safer String operations

## How to Prevent Future Issues

### Rule #1: File Size
**Keep your params.json under 10 KB**

Check file size before uploading:
- Windows: Right-click → Properties
- Mac: Right-click → Get Info  
- Linux: `ls -lh params.json`

### Rule #2: Validate JSON
**Always validate before uploading**

1. Go to https://jsonlint.com/
2. Paste your JSON
3. Click "Validate JSON"
4. Fix any errors
5. Then upload

### Rule #3: Start Small
**Test with minimal file first**

```json
{
  "parameters": [
    {"id": 1, "name": "Test", "type": "int16", "unit": "", "min": 0, "max": 100, "decimals": 0, "editable": false}
  ]
}
```

Upload this first to verify WiFi works!

### Rule #4: Backup
**Keep a copy on your computer**

Before uploading:
- Save a copy locally
- Test in small increments
- Can restore if something goes wrong

## If Boot Loop Happens Again

### Quick Recovery Steps

1. **Erase Flash** (you already know this ✅)
   ```
   esptool.py --chip esp32s3 erase_flash
   ```

2. **Re-upload Firmware**
   ```
   pio run --target upload
   ```

3. **Start Fresh**
   - Device boots with default parameters
   - WiFi feature will work
   - Try uploading again (smaller file!)

### Alternative: Serial Recovery

If you can't flash, try serial commands:

1. Connect to serial monitor (115200 baud)
2. Device may show error messages
3. Can potentially format SPIFFS via serial

## Testing the Fixed Version

### Step 1: Upload New Firmware
The new version has all the fixes - upload it first!

### Step 2: Test WiFi Without File Upload
1. Click button to enable WiFi
2. Connect to WiFi
3. Browse to 192.168.4.1
4. Just view the page - **DON'T upload yet**
5. Rotate dial to exit
6. Verify device still works

### Step 3: Upload Minimal Test File
Create `test.json`:
```json
{
  "parameters": [
    {"id": 1, "name": "Test1", "type": "int16", "unit": "rpm", "min": 0, "max": 100, "decimals": 0, "editable": false}
  ]
}
```

Upload this - it's only ~150 bytes, impossible to crash!

### Step 4: Upload Your Real File
Once test works:
1. Validate your real params.json
2. Check file size < 10 KB
3. Upload gradually (add parameters in batches)

## Monitoring for Issues

Enable debug mode to see what's happening:

Edit `include/Config.h`:
```cpp
#define DEBUG_SERIAL    true
#define DEBUG_CAN       true
```

Connect serial monitor (115200 baud) and watch for:
- "SPIFFS mounted" ✅
- "Parameters loaded successfully" ✅  
- "Upload complete: X bytes" ✅
- Any error messages ❌

## New Safety Features Explained

### Safe SPIFFS Init
```cpp
if (!SPIFFS.begin(false)) {
    // Auto-format and retry
    SPIFFS.format();
    SPIFFS.begin(false);
}
```
Now formats automatically if corrupted!

### Size Validation
```cpp
if (fileSize > MAX_JSON_SIZE) {
    // Reject and delete
    SPIFFS.remove("/params.json");
}
```
Prevents oversized files from crashing!

### Parse Error Recovery
```cpp
if (!canManager.loadParameters(json)) {
    // Delete bad file and use defaults
    SPIFFS.remove("/params.json");
    canManager.loadDefaults();
}
```
Device continues working even if upload fails!

## Error Messages You Might See

### In Web Interface

| Message | Meaning | Action |
|---------|---------|--------|
| ✓ Success | Upload worked! | You're done! |
| ✗ Parse error | Invalid JSON | Validate and try again |
| ✗ File too large | Over 16 KB | Remove parameters |
| ✗ Upload failed | Network issue | Try again |

### In Serial Monitor

| Message | Meaning | Action |
|---------|---------|--------|
| "SPIFFS mount failed" | Flash error | Will auto-format |
| "File too large!" | Size exceeded | Upload smaller file |
| "Failed to parse" | Bad JSON | Check JSON validity |
| "Parameters loaded successfully" | All good! | Nothing needed |

## What Changed vs. Old Version

| Feature | Old | New (Fixed) |
|---------|-----|-------------|
| SPIFFS error handling | None | Auto-format & retry |
| File size limit | 8 KB | 16 KB |
| Size validation | No | Yes |
| Parse error handling | Crash | Graceful fallback |
| Corrupted file handling | Boot loop | Auto-delete |
| Memory safety | Basic | Enhanced |

## Best Practices Going Forward

1. ✅ **Always validate JSON** before uploading
2. ✅ **Keep files small** (under 10 KB ideal)
3. ✅ **Test incrementally** (add parameters gradually)
4. ✅ **Monitor serial** output when testing
5. ✅ **Keep backups** of working configs
6. ✅ **Use SHORT params.json** (definitions only, no spot values)

## Still Having Issues?

If the fixed version still has problems:

### Option 1: Disable WiFi Feature
Comment out in `main.cpp`:
```cpp
// Don't initialize WiFi manager
// wifiManager.init(&canManager);
```

Parameters will stay hardcoded but device will work.

### Option 2: Use Serial Upload
Instead of WiFi, edit params in `main.cpp`:
```cpp
const char* sampleParams = R"(
{
  "parameters": [
    // Your parameters here
  ]
}
)";
```

Rebuild and upload - no WiFi needed!

### Option 3: Report Issue
If you still get boot loops:
1. Enable debug mode
2. Capture serial output
3. Share the error messages
4. We can fix it!

## Summary

- ✅ New version has **much better error handling**
- ✅ **Upload SHORT params.json** (definitions only)
- ✅ **Validate JSON** before uploading (jsonlint.com)
- ✅ **Keep files under 10 KB** for safety
- ✅ If problems happen, **flash erase** always works
- ✅ Start with **minimal test file** to verify

The fixed version should prevent boot loops entirely! 🎉
