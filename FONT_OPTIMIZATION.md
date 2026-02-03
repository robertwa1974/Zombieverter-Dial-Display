# Font and Display Optimization Guide

## Overview

The M5Stack Dial has a unique 240x240 circular display. This build has been optimized for maximum readability on this screen.

## Font Size Guidelines

### What's Different From Standard Builds

**Original sizes** → **Optimized sizes**

| Element | Original | Optimized | Reason |
|---------|----------|-----------|--------|
| Main Value (Dashboard) | Size 4 | Size 5-6 | Center focus, easier to read |
| Unit Labels | Size 1 | Size 2-3 | Better visibility |
| Screen Titles | Size 2 | Size 1 (subtle) | Less clutter |
| WiFi SSID | Size 2 | Size 2 | Clear presentation |
| WiFi IP | Size 2 | Size 3 | Most important info |
| BMS Values | Size 2 | Size 2-4 | Hierarchy by importance |
| Status Text | Size 1 | Size 1 | Compact |

## Screen-by-Screen Optimizations

### Dashboard Screen
- **Main Speed**: Size 6 (very large, impossible to miss)
- **Unit**: Size 2 (clear but not overpowering)
- **Power Arc**: Thicker lines, positioned at edge
- **Voltage**: Size 2 at bottom (glanceable)
- **CAN Status**: Small corner indicator

**Why**: Dashboard is the most-viewed screen. Main value should dominate.

### Power Screen
- **Power Value**: Size 5 (large number)
- **Unit**: Size 3 (clear unit label)
- **Ring Gauge**: Outer edge (115-119px radius)
- **Current**: Size 2 at bottom

**Why**: Focus on single metric with peripheral ring indicator.

### Temperature Screen
- **Motor Temp**: Size 5 (main focus)
- **Motor Label**: Size 1 (compact)
- **Unit**: Size 2 (visible)
- **Inverter Temp**: Size 2 at bottom (secondary)

**Why**: Motor temp is primary concern, inverter is reference.

### Battery Screen
- **SOC Number**: Size 6 (huge!)
- **% Symbol**: Size 3 (clear)
- **Ring Gauge**: Edge indicator
- **Voltage**: Size 2 at bottom

**Why**: SOC is the most critical battery metric.

### BMS Screen
- **Max/Min Cells**: Size 2 (compact, left-aligned)
- **Average Cell**: Size 4 (center, main value)
- **Unit**: Size 1 (under average)
- **Delta/Temp**: Size 1 at bottom edges

**Why**: Average is most important, max/min are references.

### WiFi Screen
- **Title**: Size 3 (clear)
- **SSID**: Size 2 (readable)
- **Password**: Size 2 (important)
- **IP Address**: Size 3 (MOST important - you type this!)
- **Exit**: Size 1 (footer)

**Why**: IP is what you need to remember/type.

## Typography Hierarchy

### Size Scale
```
Size 1: Labels, status, footer text
Size 2: Secondary values, units, identifiers
Size 3: Important values, titles
Size 4: Key metrics
Size 5: Primary focus values
Size 6: Absolute center of attention
```

### Color Hierarchy
```
TFT_WHITE:     Primary values
TFT_YELLOW:    Labels/warnings
TFT_CYAN:      Units/titles
TFT_GREEN:     Good status/high values
TFT_ORANGE:    Medium/power
TFT_RED:       Errors/low values
TFT_DARKGREY:  Subtle text
TFT_LIGHTGREY: De-emphasized text
```

## Positioning Strategy

### Circular Display Layout
```
        [Title]
          Top
     _____________
    /             \
   |   [Status]    |
   |               |
   |  [MAIN VALUE] | ← Size 5-6
   |     [unit]    | ← Size 2-3
   |               |
    \   [footer]  /
     ‾‾‾‾‾‾‾‾‾‾‾‾‾
       Bottom
```

### Text Anchoring
- **Center**: Main values, titles
- **Top-left**: Status indicators
- **Top-center**: Screen labels
- **Bottom-center**: Secondary info
- **Bottom-edges**: Tertiary data

## Reading Distance Optimization

### At Arm's Length (60cm)
- Size 6 text: 12mm height → readable
- Size 3 text: 6mm height → readable
- Size 1 text: 2mm height → requires focus

### At Dashboard Distance (100cm)
- Size 6: Still readable
- Size 3: Glanceable
- Size 1: Too small for driving

**Design Goal**: Main value readable at 1 meter!

## Contrast and Visibility

### Background
- Always TFT_BLACK for OLED burn-in prevention
- Maximum contrast with white/colored text

### Text Colors
- **Critical info**: TFT_WHITE (maximum contrast)
- **Status good**: TFT_GREEN (familiar, positive)
- **Status bad**: TFT_RED (alert, negative)
- **Labels**: TFT_CYAN/TFT_YELLOW (distinct from values)

## Ring Gauge Optimization

### Original vs Optimized

**Before**:
- Radius: 95-100px
- Thickness: 5px
- Segments: 50

**After**:
- Radius: 115-119px (at screen edge!)
- Thickness: 4px
- Segments: 100 (smoother)

**Why**: Pushes gauge to edge, maximizes center space for text.

## Font Rendering Notes

### M5Stack Dial Uses
- Built-in fonts (sizes 1-6)
- No anti-aliasing (crisp but pixelated)
- Monospace for numbers (better alignment)

### Best Practices
1. **Whole number sizes** - Fractional sizes not supported
2. **High contrast** - Black background essential
3. **Centered text** - Uses setTextDatum(middle_center)
4. **Short strings** - Screen width limits ~20 chars at size 2

## Accessibility Considerations

### For Users With:

**Presbyopia** (age-related farsightedness):
- ✅ Size 5-6 main values work great
- ✅ High contrast helps
- ⚠️  Size 1 text may be challenging

**Astigmatism**:
- ✅ Black background reduces glare
- ✅ Simple fonts help
- ⚠️  Small text harder to focus

**Color Blindness**:
- ✅ Not relying on color alone
- ✅ Green/Red used with position context
- ✅ Text labels accompany all values

## Customization

### To Make Text Larger
Edit `src/UIManager.cpp`:
```cpp
// Dashboard main value
M5.Display.setTextSize(6);  // Change to 7 or 8 (max useful is ~8)
```

### To Make Text Smaller
```cpp
M5.Display.setTextSize(5);  // Change to 4
```

### To Change Colors
```cpp
M5.Display.setTextColor(TFT_WHITE);  // Change to any color
```

Available colors:
- TFT_BLACK, TFT_WHITE
- TFT_RED, TFT_GREEN, TFT_BLUE
- TFT_CYAN, TFT_MAGENTA, TFT_YELLOW
- TFT_ORANGE, TFT_PINK, TFT_PURPLE
- TFT_DARKGREY, TFT_LIGHTGREY

## Testing Your Changes

### Visual Test Checklist
- [ ] Main value visible from 1 meter
- [ ] All text readable at arm's length
- [ ] No text cut off at screen edges
- [ ] Ring gauges don't overlap text
- [ ] Color-blind friendly (test grayscale)
- [ ] Bright sunlight readable (high contrast)

### Field Testing
1. Mount in vehicle
2. Check readability while sitting in driver seat
3. Verify glanceable at road angles
4. Test in bright/dim conditions
5. Verify no critical text too small

## Recommended Settings

For optimal viewing in vehicle:

**Brightness**: 200-255 (adjust in Config.h)
```cpp
#define DEFAULT_BRIGHTNESS  220
```

**Update Rate**: 100ms (current setting)
```cpp
#define UI_UPDATE_INTERVAL_MS  100
```

**Screen Timeout**: 5 minutes (power saving)
```cpp
#define SLEEP_TIMEOUT_MS  300000
```

## Summary

✅ **Main values**: Size 5-6 (huge, center)
✅ **Secondary values**: Size 2-3 (clear)
✅ **Labels**: Size 1-2 (compact)
✅ **Ring gauges**: Edge-mounted (maximizes space)
✅ **Colors**: High contrast (readable in all light)
✅ **Layout**: Circular-optimized (no corners wasted)

The result: **Glanceable, safe, professional display** optimized for the M5Stack Dial!
