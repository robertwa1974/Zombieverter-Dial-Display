#include "Hardware.h"
#include <esp_sleep.h>

uint8_t Hardware::currentBrightness = DEFAULT_BRIGHTNESS;
bool Hardware::isSleeping = false;

bool Hardware::init() {
    auto cfg = M5.config();
    
    M5.begin(cfg);
    
    // Hold power on
    pinMode(POWER_HOLD_PIN, OUTPUT);
    digitalWrite(POWER_HOLD_PIN, HIGH);
    
    // Initialize display
    M5.Display.setRotation(0);
    M5.Display.setBrightness(currentBrightness);
    M5.Display.setColorDepth(16);
    M5.Display.setSwapBytes(false);
    M5.Display.fillScreen(TFT_BLACK);
    
    return true;
}

void Hardware::update() {
    M5.update();
}

void Hardware::setBacklight(uint8_t brightness) {
    currentBrightness = brightness;
    M5.Display.setBrightness(brightness);
}

uint8_t Hardware::getBacklight() {
    return currentBrightness;
}

void Hardware::sleep() {
    if (!isSleeping) {
        setBacklight(0);
        isSleeping = true;
    }
}

void Hardware::wake() {
    if (isSleeping) {
        setBacklight(currentBrightness);
        isSleeping = false;
    }
}

void Hardware::powerOn() {
    digitalWrite(POWER_HOLD_PIN, HIGH);
}

void Hardware::powerOff() {
    digitalWrite(POWER_HOLD_PIN, LOW);
    // Deep sleep
    esp_deep_sleep_start();
}

float Hardware::getBatteryVoltage() {
    // M5Stack Dial battery reading
    return 0.0f; // Placeholder - implement based on your hardware
}

bool Hardware::isCharging() {
    return false; // Placeholder - implement based on your hardware
}

void Hardware::setLED(bool state) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, state ? HIGH : LOW);
}

void Hardware::setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
    // M5Stack Dial has RGB LED, implement if available
    // For now, just use simple on/off
    setLED(r > 0 || g > 0 || b > 0);
}
