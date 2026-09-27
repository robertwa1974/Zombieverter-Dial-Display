#include "BoardConfig.h"

#if defined(BOARD_MATOUCH21)

#include "CST826.h"

bool CST826::read(int16_t& x, int16_t& y) {
    // 0x02: finger count, 0x03: event<<6 | X[11:8], 0x04: X[7:0],
    // 0x05: Y[11:8], 0x06: Y[7:0]
    uint8_t raw[7];
    if (!readRegs(0x02, raw, sizeof(raw))) return false;

    uint8_t event = raw[1] >> 6;
    if (event != 2) return false;   // 2 = contact

    x = (int16_t)(raw[2] + (raw[1] & 0x0F) * 256);
    y = (int16_t)(raw[4] + (raw[3] & 0x0F) * 256);
    return true;
}

bool CST826::readRegs(uint8_t reg, uint8_t* data, uint8_t len) {
    if (!_wire) return false;
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(true) != 0) return false;
    if (_wire->requestFrom(_addr, len, (uint8_t)true) != len) return false;
    for (uint8_t i = 0; i < len; i++) {
        data[i] = _wire->read();
    }
    return true;
}

#endif // BOARD_MATOUCH21
