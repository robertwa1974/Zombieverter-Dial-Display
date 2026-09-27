// Makerfabs MaTouch 2.1 board layer — ST7701S RGB panel via Arduino_GFX,
// CST826 touch polled over I2C, backlight on ledc PWM.
#include "Hardware.h"

#if defined(BOARD_MATOUCH21)

#include <esp_sleep.h>
#include <Arduino_GFX_Library.h>
#include "CST826.h"

uint8_t Hardware::currentBrightness = DEFAULT_BRIGHTNESS;
bool Hardware::isSleeping = false;

static Arduino_DataBus*       lcdBus   = nullptr;
static Arduino_ESP32RGBPanel* rgbPanel = nullptr;
static Arduino_RGB_Display*   gfx      = nullptr;

static CST826      touchCtl(TOUCH_I2C_ADDR);
static TouchDetail touchDetail = {false, false, 0, 0};
static bool        touchDown   = false;
static uint32_t    lastTouchPollMs = 0;
static const uint32_t TOUCH_POLL_MS = 10;

bool Hardware::init() {
    // Backlight off until the panel has been initialised
    ledcSetup(BACKLIGHT_LEDC_CH, BACKLIGHT_LEDC_FREQ, 8);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_LEDC_CH);
    ledcWrite(BACKLIGHT_LEDC_CH, 0);

    // ST7701S: 3-wire SPI for the init sequence, 16-bit RGB for pixels
    lcdBus = new Arduino_SWSPI(GFX_NOT_DEFINED /* DC */, LCD_SPI_CS,
                               LCD_SPI_SCK, LCD_SPI_MOSI, GFX_NOT_DEFINED /* MISO */);
    rgbPanel = new Arduino_ESP32RGBPanel(
        LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
        LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
        LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
        LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
        LCD_HSYNC_POLARITY, LCD_HSYNC_FP, LCD_HSYNC_PW, LCD_HSYNC_BP,
        LCD_VSYNC_POLARITY, LCD_VSYNC_FP, LCD_VSYNC_PW, LCD_VSYNC_BP);
    gfx = new Arduino_RGB_Display(
        SCREEN_WIDTH, SCREEN_HEIGHT, rgbPanel, 0 /* rotation */, true /* auto_flush */,
        lcdBus, GFX_NOT_DEFINED /* RST */,
        st7701_type5_init_operations, sizeof(st7701_type5_init_operations));

    if (!gfx->begin()) {
        return false;
    }
    gfx->fillScreen(BLACK);

    // Touch
    i2cBus().begin(TOUCH_SDA, TOUCH_SCL);
    touchCtl.begin(i2cBus());

    setBacklight(currentBrightness);
    return true;
}

void Hardware::update() {
    // Sample touch once per loop, like M5.update(): edges last one call
    touchDetail.wasPressed  = false;
    touchDetail.wasReleased = false;

    uint32_t now = millis();
    if (now - lastTouchPollMs < TOUCH_POLL_MS) return;
    lastTouchPollMs = now;

    int16_t x, y;
    bool down = touchCtl.read(x, y);
    if (down) {
        touchDetail.x = x;
        touchDetail.y = y;
    }
    // On release x/y keep the last contact position
    touchDetail.wasPressed  = down && !touchDown;
    touchDetail.wasReleased = !down && touchDown;
    touchDown = down;
}

void Hardware::setBacklight(uint8_t brightness) {
    currentBrightness = brightness;
    ledcWrite(BACKLIGHT_LEDC_CH, brightness);
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

void Hardware::flushDisplay(int32_t x, int32_t y, uint32_t w, uint32_t h, uint16_t* pixels) {
    // Copies into the PSRAM framebuffer the RGB peripheral scans out;
    // auto_flush writes the cache back so the panel sees the new pixels.
    gfx->draw16bitRGBBitmap(x, y, pixels, w, h);
}

TouchDetail Hardware::getTouch() {
    return touchDetail;
}

TwoWire& Hardware::i2cBus() {
    return Wire;
}

// ── No power-hold, LED or battery on this board ─────────────────────────────

void Hardware::powerOn() {
}

void Hardware::powerOff() {
    setBacklight(0);
    esp_deep_sleep_start();
}

float Hardware::getBatteryVoltage() {
    return 0.0f;
}

bool Hardware::isCharging() {
    return false;
}

void Hardware::setLED(bool state) {
}

void Hardware::setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
}

#endif // BOARD_MATOUCH21
