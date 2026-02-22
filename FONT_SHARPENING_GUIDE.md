# Font Sharpening Guide - Make Text Crisp!

The fonts look blurry? Let's fix that! Try these solutions in order.

---

## Solution 1: Disable Anti-Aliasing (Sharpest, Try This First!)

Anti-aliasing smooths edges but can make small text look blurry.

**Edit `lv_conf.h` (project root):**

Find line ~92:
```cpp
/* Enable anti-aliasing (lines, and radiuses will be smoothed) */
#define LV_ANTIALIAS 1
```

**Change to:**
```cpp
/* Enable anti-aliasing (lines, and radiuses will be smoothed) */
#define LV_ANTIALIAS 0  // Disable for sharper text
```

**Then:**
1. Clean
2. Build
3. Upload

**Result:** Text will be much sharper, but circles/arcs might have slight jagged edges.

---

## Solution 2: Use Bolder Fonts (If Solution 1 Isn't Sharp Enough)

The Montserrat font family has different weights. Current code uses regular weight.

**Edit `src/UIManager.cpp`:**

**Option A: Use Larger Font Sizes**

Find places using small fonts and bump them up:

```cpp
// BEFORE (line ~213):
lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);

// AFTER:
lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, 0);
```

**Option B: Add Bold Font Support**

LVGL doesn't have built-in bold Montserrat, but we can use larger sizes which appear bolder.

For critical displays (like RPM), use bigger fonts:

```cpp
// BEFORE (line ~266):
lv_obj_set_style_text_font(dash_rpm_label, &lv_font_montserrat_32, 0);

// AFTER - Use even larger for more impact:
lv_obj_set_style_text_font(dash_rpm_label, &lv_font_montserrat_48, 0);
```

Enable font 48 in `lv_conf.h` line ~118:
```cpp
#define LV_FONT_MONTSERRAT_48    1  // Enable 48pt font
```

---

## Solution 3: Enable Subpixel Rendering (Advanced)

This uses the RGB subpixels for sharper rendering.

**Edit `lv_conf.h`:**

Find line ~104 and change to:
```cpp
#define LV_FONT_MONTSERRAT_12    0  // Disable normal
#define LV_FONT_MONTSERRAT_12_SUBPX  1  // Enable subpixel version
```

**Then in code**, change references from:
```cpp
&lv_font_montserrat_12
```
to:
```cpp
&lv_font_montserrat_12_subpx
```

**Note:** Only size 12 has a subpixel version. This is complex, so try Solution 1 first!

---

## Solution 4: Adjust Font Rendering Style

**Edit `src/UIManager.cpp`:**

Add this to make text rendering sharper:

After creating labels, add:
```cpp
lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
lv_obj_set_style_text_letter_space(label, 0, 0);  // Remove letter spacing
lv_obj_set_style_text_line_space(label, 0, 0);    // Remove line spacing
```

---

## Solution 5: Increase Display Brightness (Quick Test)

Sometimes perceived blurriness is just low brightness making pixels blend.

**Edit `src/Hardware.cpp`:**

Find the display brightness setting and increase it:
```cpp
M5.Display.setBrightness(255);  // Max brightness (0-255)
```

---

## Recommended Approach:

**Start with Solution 1 (Disable Anti-Aliasing):**

This single change will make the biggest difference for text sharpness!

1. Edit `lv_conf.h`
2. Change `LV_ANTIALIAS` from `1` to `0`
3. Clean + Build + Upload
4. Check if text is sharper

**If you want even sharper:**

Combine Solution 1 + Solution 4 (disable anti-aliasing + adjust spacing)

---

## Before/After Example:

**With Anti-Aliasing (Current - Blurry):**
```
Pixels are smoothed at edges
Text has soft, gray borders
Can look fuzzy on small displays
```

**Without Anti-Aliasing (Sharp):**
```
Pixels are crisp black/white
Text has hard edges
Looks sharper but circles less smooth
```

---

## Trade-offs:

| Setting | Text Sharpness | Circle/Arc Smoothness | Recommendation |
|---------|---------------|----------------------|----------------|
| Anti-alias ON | ★★☆☆☆ Blurry | ★★★★★ Smooth | Use if circles are priority |
| Anti-alias OFF | ★★★★★ Sharp | ★★★☆☆ Slight jagged | **Use for text priority** |

For an **automotive dashboard**, sharp text is MORE important than smooth circles, so **disable anti-aliasing!**

---

## Quick Test Script:

Want to test quickly? I can create a version with anti-aliasing disabled. Let me know!

---

**TL;DR: Edit `lv_conf.h`, change line 92 from `#define LV_ANTIALIAS 1` to `#define LV_ANTIALIAS 0`, rebuild!**
