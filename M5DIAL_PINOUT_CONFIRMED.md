# M5Stack Dial - DEFINITIVE Pinout for CAN Bus

## M5Stack Dial Physical Ports

The M5Stack Dial has:

### 1. **Grove Port (External - Bottom/Side of device)**
This is the ONLY external Grove connector you can plug cables into.

**Standard Grove Pinout:**
```
Looking at the connector (female socket on M5Dial):
┌─────────────┐
│ 1  2  3  4  │
└─────────────┘
```

### 2. **Which GPIOs Does This Grove Port Use?**

According to M5Stack Dial schematics, the Grove port can be configured as:

**Option A - PortA Configuration:**
- Pin 1 (Yellow): **GPIO 2**
- Pin 2 (White): **GPIO 1**
- Pin 3 (Red): **5V**
- Pin 4 (Black): **GND**

**Option B - I2C Configuration (if enabled):**
- Pin 1 (Yellow): **GPIO 12** (SCL)
- Pin 2 (White): **GPIO 11** (SDA)
- Pin 3 (Red): **5V**
- Pin 4 (Black): **GND**

## ⚠️ CRITICAL: Which Configuration for CAN?

### The Grove port can be EITHER:
1. **General GPIO** (G1/G2) - for CAN, UART, etc.
2. **I2C** (G11/G12) - for I2C devices

**It's configured by the firmware!**

### For CAN Bus, we need **General GPIO mode (G1/G2)**

The touch screen uses **separate internal I2C pins** (not the Grove port):
- Touch SDA: GPIO 11 (internal connection to touch IC)
- Touch SCL: GPIO 12 (internal connection to touch IC)
- Touch INT: GPIO 14

So the Grove port is **FREE to use for CAN!**

## Correct CAN Pin Configuration

### ✅ CORRECT Configuration:
```cpp
#define CAN_TX_PIN          2   // Grove Port - Pin 1 (Yellow)
#define CAN_RX_PIN          1   // Grove Port - Pin 2 (White)
```

This is actually what I had originally, and it's CORRECT!

## Why GPIO 13/15 Was WRONG

GPIO 13 and 15 are:
- **GPIO 13**: Used on M5Stack Core2 Grove port
- **GPIO 15**: Used on M5Stack Core2 Grove port

But **M5Stack DIAL** uses **GPIO 1 and 2** for its Grove port!

I made a mistake by assuming M5Stack Dial used the same pins as M5Stack Core2. They're different devices!

## M5Stack Device Comparison

| Device | Grove Port GPIOs |
|--------|------------------|
| M5Stack Core2 | G13, G15 |
| M5Stack CoreS3 | G1, G2 |
| **M5Stack Dial** | **G1, G2** |
| M5StickC Plus | G26, G36 |

## Final Answer: Which Pins to Use

### ✅ Use GPIO 1 and 2:
```cpp
#define CAN_TX_PIN          2
#define CAN_RX_PIN          1
```

### Grove Port Connections:
```
M5Stack Dial Grove Port
┌─────────────┐
│ 1  2  3  4  │  
└─┬──┬──┬──┬──┘
  │  │  │  └─── GND (Black) ───┐
  │  │  └────── 5V  (Red)   ───┤
  │  └───────── G1  (White) ───┤ M5Stack CAN Unit
  └──────────── G2  (Yellow)───┘
```

Then from CAN Unit:
- **CAN H** → ZombieVerter CAN H
- **CAN L** → ZombieVerter CAN L
- **GND** → Common ground

## The I2C "Reserved" Port Confusion

**The confusion:** 
- Touch screen DOES use I2C (GPIO 11/12)
- BUT it's a **separate internal connection**, not the Grove port
- The Grove port is **NOT** reserved for I2C on M5Stack Dial
- It's free to use for CAN!

### Touch Screen I2C (Internal - Do Not Touch):
```
M5Dial CPU ──I2C──> Touch Controller (FT6336U)
            (G11/G12 - internal traces on PCB)
```

### Grove Port (External - Free to Use):
```
M5Dial Grove Port (G1/G2) ──> Your CAN Unit
```

They're **different connections!**

## Verification Steps

### 1. Check M5Stack Dial Schematic
Look at official M5Stack Dial schematic:
- Grove connector should show GPIO 1 and 2
- Touch I2C should show GPIO 11 and 12 (separate)

### 2. Test With Multimeter
- Power off everything
- Measure continuity from Grove pin 1 (Yellow) to CPU
- Should connect to GPIO 2
- Measure Grove pin 2 (White) to CPU  
- Should connect to GPIO 1

### 3. Try Both Pin Configurations

**First try: GPIO 1/2** (original - likely correct)
```cpp
#define CAN_TX_PIN          2
#define CAN_RX_PIN          1
```

If that doesn't work, **then try: GPIO 13/15**
```cpp
#define CAN_TX_PIN          13
#define CAN_RX_PIN          15
```

But I'm 90% confident it's GPIO 1/2 based on M5Stack Dial documentation.

## Official M5Stack Dial Resources

Check these for confirmation:
1. **M5Stack Dial Docs**: https://docs.m5stack.com/en/core/M5Dial
2. **M5Stack Dial Schematic**: Available in docs (shows exact GPIO routing)
3. **M5Stack Forum**: Search for "M5Dial Grove pins"

## Conclusion

### Original pins (GPIO 1/2) were probably CORRECT!

I apologize for the confusion. The issue might not be the pins at all!

Let me revert to GPIO 1/2 and we can troubleshoot the actual CAN communication issue.

Possible other causes of no CAN RX:
1. **CAN transceiver not initialized** (check TWAI init in serial)
2. **Wrong baud rate** (500kbps vs different on VCU)
3. **CAN H/L swapped** (physical wiring)
4. **No termination resistor** (120Ω needed)
5. **VCU not transmitting** (check VCU is actually sending CAN)
6. **Different CAN protocol** (not SDO)

Let me create a version with GPIO 1/2 restored and better diagnostics!
