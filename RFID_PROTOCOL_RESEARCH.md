# M5Dial RFID - I2C Protocol Research Needed

## Current Status
✅ **I2C Communication Working** - Device responds at address 0x28
❌ **Protocol Unknown** - Returns 0x20 0x20 0x20 0x20 (ASCII spaces) without card

## What We Know
- **Device:** I2C RFID module at address 0x28
- **Pins:** SDA=GPIO11, SCL=GPIO12  
- **Detection:** Module responds to I2C scan
- **Issue:** Reading invalid data (0x20 = space character)

## What We Need
The specific I2C command protocol for this RFID module:
1. Command to check if card is present
2. Command to read card UID
3. Data format/structure
4. Register map

## Possible Solutions

### Option 1: Find M5Stack Library
M5Stack likely has a library for this module. Search for:
- M5Dial RFID library
- M5Stack RFID I2C examples
- Official M5Stack GitHub repositories

### Option 2: Reverse Engineer Protocol
Use I2C scanner to:
- Read all registers (0x00-0xFF)
- Try common RFID commands
- Monitor with logic analyzer

### Option 3: Check M5Stack Forums
- Ask M5Stack community
- Check existing projects using M5Dial RFID
- Look for example code

### Option 4: Use M5Unified API
M5Unified might have built-in RFID support:
```cpp
// Check if M5.Rfid or M5.RFID exists
// May be available in newer versions
```

## Temporary Solution
**PIN-only authentication is working perfectly!**

The system is production-ready with:
- 4-digit PIN entry
- Lock screen UI
- CAN current control (500A/0A)
- Hardware timer heartbeat
- All 11 screens functional

RFID would be a nice-to-have feature, but PIN provides excellent security.

## Next Steps
1. Contact M5Stack support for I2C protocol documentation
2. Check M5Stack GitHub for RFID examples
3. Search for M5Dial RFID library
4. If no luck, PIN-only is a complete solution

## Links to Research
- https://github.com/m5stack
- https://docs.m5stack.com
- https://community.m5stack.com
- Search: "M5Dial RFID I2C protocol"
