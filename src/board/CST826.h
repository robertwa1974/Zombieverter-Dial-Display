#ifndef CST826_H
#define CST826_H

// CST826 capacitive touch controller (MaTouch 2.1). Polled over I2C — the
// board wires no INT/RST. Ported from Makerfabs' fw_test touch.cpp.

#include <Arduino.h>
#include <Wire.h>

class CST826 {
public:
    explicit CST826(uint8_t addr) : _addr(addr), _wire(nullptr) {}

    void begin(TwoWire& wire) { _wire = &wire; }

    // True while a finger is down; x/y are only written then.
    bool read(int16_t& x, int16_t& y);

private:
    bool readRegs(uint8_t reg, uint8_t* data, uint8_t len);

    uint8_t  _addr;
    TwoWire* _wire;
};

#endif // CST826_H
