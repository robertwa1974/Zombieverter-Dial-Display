#ifndef HARDWARE_H
#define HARDWARE_H

#include "Config.h"
#if defined(BOARD_M5DIAL)
#include <M5Unified.h>
#endif
#include <Arduino.h>
#if defined(BOARD_MATOUCH21)
#include <Wire.h>
#endif

// Touch state sampled by Hardware::update(), mirroring M5Unified's touch detail
struct TouchDetail {
    bool wasPressed;
    bool wasReleased;
    int16_t x;
    int16_t y;
};

class Hardware {
public:
    static bool init();
    static void update();
    
    // Display control
    static void setBacklight(uint8_t brightness);
    static uint8_t getBacklight();
    static void sleep();
    static void wake();

    // LVGL flush target: push a w x h block of RGB565 pixels at (x, y)
    static void flushDisplay(int32_t x, int32_t y, uint32_t w, uint32_t h, uint16_t* pixels);

    // Touch
    static TouchDetail getTouch();

#if defined(BOARD_MATOUCH21)
    // Touch I2C bus (SDA 17 / SCL 18) — shareable with a future RFID reader
    static TwoWire& i2cBus();
#endif
    
    // Power management
    static void powerOn();
    static void powerOff();
    static float getBatteryVoltage();
    static bool isCharging();
    
    // LED control
    static void setLED(bool state);
    static void setLEDColor(uint8_t r, uint8_t g, uint8_t b);
    
private:
    static uint8_t currentBrightness;
    static bool isSleeping;
};

#endif // HARDWARE_H
