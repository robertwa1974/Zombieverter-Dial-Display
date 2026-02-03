# M5Stack Dial - Port A vs Port B Configuration

## Your M5Stack Dial Has TWO Grove Ports:

### Port A (Red) - General Purpose GPIO
- Pin 1 (Yellow): **GPIO 2** (TX)
- Pin 2 (White): **GPIO 1** (RX)
- Pin 3 (Red): 5V
- Pin 4 (Black): GND

### Port B (Grey) - Usually I2C
- Pin 1 (Yellow): **GPIO 12** (SCL)
- Pin 2 (White): **GPIO 11** (SDA)
- Pin 3 (Red): 5V
- Pin 4 (Black): GND

---

## ✅ RECOMMENDED: Use Port A (Red)

The current code is configured for **Port A (GPIO 1/2)**.

**Just plug your CAN Bus Unit into the RED Port A connector.**

Configuration in code:
```cpp
#define CAN_TX_PIN          2   // Port A - Red connector
#define CAN_RX_PIN          1   // Port A - Red connector
```

This should work immediately with the Node 3 fixed version!

---

## Alternative: Use Port B (Grey)

If Port A doesn't work, or if you need Port A for something else, you can use Port B.

### To Switch to Port B:

Edit `include/Config.h`:

```cpp
// Change from Port A to Port B:
#define CAN_TX_PIN          12  // Port B - Grey connector  
#define CAN_RX_PIN          11  // Port B - Grey connector
```

Then:
1. Rebuild and upload
2. Plug CAN unit into **grey Port B**

---

## Which Port Should You Use?

### Use Port A (Red) if:
- ✅ It's available
- ✅ You don't need it for other GPIO devices
- ✅ Current code already configured for it

### Use Port B (Grey) if:
- You need Port A for something else
- Port A doesn't work for some reason
- You have a specific requirement

**Default recommendation: Start with Port A (Red)** since the code is already set up for it!

---

## Testing Steps:

1. **Try Port A first** (red connector) with current code
2. Upload the Node 3 fixed firmware
3. Check serial monitor for CAN messages
4. **If it doesn't work**, try Port B:
   - Change pins in Config.h to GPIO 11/12
   - Rebuild and upload
   - Move cable to grey Port B

---

## Port B Considerations:

Port B (GPIO 11/12) is typically labeled for I2C, but it CAN be used for general GPIO if needed. The TWAI (CAN) driver should work on these pins.

However, **Port A is the safer choice** for CAN since it's designated for general GPIO use.

---

## Summary:

✅ **Use Port A (Red)** - Already configured, should work!
⚠️ **Port B (Grey)** - Backup option if needed

The Node 3 fixed code is set up for **Port A (Red)** by default.
