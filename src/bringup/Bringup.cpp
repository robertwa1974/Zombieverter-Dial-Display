// ============================================================================
// MaTouch 2.1 bring-up firmware — built only by [env:matouch21-bringup].
//
// Standalone test screens that confirm the hardware and the port assumptions
// before the full app runs: display colours/rotation, CST826 touch mapping,
// encoder + button, backlight, I2C bus, UART-connector pin identity, TWAI on
// 43/44 and single-framebuffer tearing under WiFi/PSRAM/flash load.
//
// Rotate = next/previous screen. Button actions are listed at the bottom of
// each screen. Everything shown is mirrored to USB serial (115200).
//
// The app envs compile this file to nothing (no BRINGUP_FW flag).
// ============================================================================
#if defined(BRINGUP_FW)

#if !defined(BOARD_MATOUCH21)
#error "Bring-up firmware targets BOARD_MATOUCH21"
#endif

#include <Arduino.h>
#include <lvgl.h>
#include <ESP32Encoder.h>
#include <OneButton.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <driver/twai.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_arduino_version.h>
#include "Hardware.h"

// ── Screens ─────────────────────────────────────────────────────────────────
enum Mode { M_INFO, M_COLOUR, M_TOUCH, M_INPUT, M_I2C, M_PINS, M_CAN, M_STRESS, M_COUNT };

struct ModeDef {
    const char* title;
    const char* hint;                 // button actions, shown at the bottom
    void (*build)(lv_obj_t* scr);
    void (*enter)();
    void (*exit)();
    void (*tick)();
    void (*click)();
    void (*doubleClick)();
    void (*longPress)();
    bool (*encoder)(int delta);       // return true if the screen consumed it
};

static lv_obj_t* screens[M_COUNT];
static int       currentMode = M_INFO;

// ── LVGL display ────────────────────────────────────────────────────────────
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t      dispDrv;
static volatile uint32_t  framesRendered = 0;

static void flushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* px) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    Hardware::flushDisplay(area->x1, area->y1, w, h, (uint16_t*)&px->full);
    lv_disp_flush_ready(disp);
}

static void monitorCb(lv_disp_drv_t* disp, uint32_t time, uint32_t px) {
    framesRendered++;
}

static bool initLvgl() {
    lv_init();
    size_t n = SCREEN_WIDTH * DISPLAY_DRAW_BUF_LINES;
    lv_color_t* b1 = (lv_color_t*)heap_caps_malloc(n * sizeof(lv_color_t), DISPLAY_DRAW_BUF_CAPS);
    lv_color_t* b2 = (lv_color_t*)heap_caps_malloc(n * sizeof(lv_color_t), DISPLAY_DRAW_BUF_CAPS);
    if (!b1 || !b2) return false;
    lv_disp_draw_buf_init(&drawBuf, b1, b2, n);
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res    = SCREEN_WIDTH;
    dispDrv.ver_res    = SCREEN_HEIGHT;
    dispDrv.flush_cb   = flushCb;
    dispDrv.monitor_cb = monitorCb;
    dispDrv.draw_buf   = &drawBuf;
    lv_disp_drv_register(&dispDrv);
    lv_theme_t* th = lv_theme_default_init(lv_disp_get_default(),
        lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_CYAN),
        true, &lv_font_montserrat_24);
    lv_disp_set_theme(lv_disp_get_default(), th);
    return true;
}

// ── Helpers ─────────────────────────────────────────────────────────────────
static lv_color_t GREY()   { return lv_palette_main(LV_PALETTE_GREY); }
static lv_color_t CYAN()   { return lv_palette_main(LV_PALETTE_CYAN); }
static lv_color_t GREEN()  { return lv_palette_main(LV_PALETTE_GREEN); }
static lv_color_t RED()    { return lv_palette_main(LV_PALETTE_RED); }
static lv_color_t YELLOW() { return lv_palette_main(LV_PALETTE_YELLOW); }

static lv_obj_t* mkLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color,
                         lv_align_t align, int x, int y, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, align, x, y);
    return l;
}

// Centred multi-line body text, kept inside the round panel
static lv_obj_t* mkBody(lv_obj_t* parent, int y) {
    lv_obj_t* l = mkLabel(parent, &lv_font_montserrat_24, lv_color_white(),
                          LV_ALIGN_CENTER, 0, y, "");
    lv_obj_set_width(l, 380);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    return l;
}

static lv_obj_t* mkCircle(lv_obj_t* parent, int d, lv_color_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// ============================================================================
// 1. INFO
// ============================================================================
static lv_obj_t* infoLbl;
static uint32_t  infoLastMs;

static const char* resetReasonName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

static void infoRender(bool toSerial) {
    char buf[480];
    snprintf(buf, sizeof(buf),
        "%s rev %d, %d cores @ %lu MHz\n"
        "Flash %lu MB @ %lu MHz\n"
        "PSRAM %lu KB, free %lu KB\n"
        "Heap free %lu KB (min %lu)\n"
        "Internal free %lu KB\n"
        "Arduino %d.%d.%d  IDF %s\n"
        "LVGL %d.%d.%d\n"
        "Reset: %s   Up: %lus",
        ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(),
        (unsigned long)ESP.getCpuFreqMHz(),
        (unsigned long)(ESP.getFlashChipSize() >> 20),
        (unsigned long)(ESP.getFlashChipSpeed() / 1000000),
        (unsigned long)(ESP.getPsramSize() >> 10), (unsigned long)(ESP.getFreePsram() >> 10),
        (unsigned long)(ESP.getFreeHeap() >> 10), (unsigned long)(ESP.getMinFreeHeap() >> 10),
        (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
        ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH,
        esp_get_idf_version(),
        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
        resetReasonName(esp_reset_reason()), (unsigned long)(millis() / 1000));
    lv_label_set_text(infoLbl, buf);
    if (toSerial) Serial.printf("[INFO]\n%s\n", buf);
}

static void infoBuild(lv_obj_t* s) { infoLbl = mkBody(s, 10); }
static void infoEnter()            { infoRender(true); infoLastMs = millis(); }
static void infoClick()            { infoRender(true); }
static void infoTick() {
    if (millis() - infoLastMs >= 1000) { infoLastMs = millis(); infoRender(false); }
}

// ============================================================================
// 2. COLOUR / ORIENTATION
// ============================================================================
static lv_obj_t* colourFill;
static int       colourFillIdx = 0;   // 0 = off

static void colourBuild(lv_obj_t* s) {
    struct Bar { const char* name; lv_color_t c; lv_color_t text; };
    const Bar bars[4] = {
        {"RED",   lv_color_make(255, 0, 0),     lv_color_white()},
        {"GREEN", lv_color_make(0, 255, 0),     lv_color_black()},
        {"BLUE",  lv_color_make(0, 0, 255),     lv_color_white()},
        {"WHITE", lv_color_make(255, 255, 255), lv_color_black()},
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = lv_obj_create(s);
        lv_obj_set_size(b, 300, 48);
        lv_obj_set_style_bg_color(b, bars[i].c, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(b, LV_ALIGN_CENTER, 0, -84 + i * 56);
        mkLabel(b, &lv_font_montserrat_24, bars[i].text, LV_ALIGN_CENTER, 0, 0, bars[i].name);
    }
    // Edge markers — read them against the physical board to confirm rotation
    mkLabel(s, &lv_font_montserrat_28, YELLOW(), LV_ALIGN_TOP_MID,    0,   8, "TOP");
    mkLabel(s, &lv_font_montserrat_28, YELLOW(), LV_ALIGN_BOTTOM_MID, 0,  -8, "BOTTOM");
    mkLabel(s, &lv_font_montserrat_28, YELLOW(), LV_ALIGN_LEFT_MID,   8,   0, "LEFT");
    mkLabel(s, &lv_font_montserrat_28, YELLOW(), LV_ALIGN_RIGHT_MID, -8,   0, "RIGHT");

    // Full-screen solid fill (uniformity / dead pixels), cycled by click
    colourFill = lv_obj_create(s);
    lv_obj_set_size(colourFill, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_border_width(colourFill, 0, 0);
    lv_obj_set_style_radius(colourFill, 0, 0);
    lv_obj_center(colourFill);
    lv_obj_add_flag(colourFill, LV_OBJ_FLAG_HIDDEN);
}

static void colourClick() {
    static const char*    names[] = {"off", "RED", "GREEN", "BLUE", "WHITE", "BLACK"};
    static const uint32_t rgb[]   = {0, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000};
    colourFillIdx = (colourFillIdx + 1) % 6;
    if (colourFillIdx == 0) {
        lv_obj_add_flag(colourFill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_bg_color(colourFill, lv_color_hex(rgb[colourFillIdx]), 0);
        lv_obj_clear_flag(colourFill, LV_OBJ_FLAG_HIDDEN);
    }
    Serial.printf("[COLOUR] full-screen fill: %s\n", names[colourFillIdx]);
}

static void colourEnter() {
    Serial.println("[COLOUR] Bars top->bottom must read RED, GREEN, BLUE, WHITE in those colours;"
                   " TOP/RIGHT/BOTTOM/LEFT must match the board's orientation");
}

static void colourExit() {
    colourFillIdx = 0;
    lv_obj_add_flag(colourFill, LV_OBJ_FLAG_HIDDEN);
}

// ============================================================================
// 3. TOUCH — guided taps on four targets reveal swap/mirror of CST826 vs panel
// ============================================================================
enum { T_TOP, T_RIGHT, T_BOTTOM, T_LEFT, T_COUNT };
static const char* const targetNames[T_COUNT] = {"TOP", "RIGHT", "BOTTOM", "LEFT"};
static const int16_t targetPos[T_COUNT][2] = {
    {240, 100}, {380, 240}, {240, 380}, {100, 240}
};
static lv_obj_t* touchTargets[T_COUNT];
static lv_obj_t* touchDot;
static lv_obj_t* touchLbl;
static lv_obj_t* touchResultLbl;
static int16_t   touchSamples[T_COUNT][2];
static int       touchStep;
static bool      touchWasDown;
static uint32_t  touchLastLogMs;

static void touchResetSequence() {
    touchStep = 0;
    for (int i = 0; i < T_COUNT; i++) {
        lv_obj_set_style_bg_color(touchTargets[i], lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    }
    lv_obj_set_style_bg_color(touchTargets[0], YELLOW(), 0);
    lv_label_set_text(touchResultLbl, "Tap the yellow target: TOP");
}

static void touchBuild(lv_obj_t* s) {
    for (int i = 0; i < T_COUNT; i++) {
        touchTargets[i] = mkCircle(s, 64, lv_palette_darken(LV_PALETTE_GREY, 2));
        lv_obj_set_pos(touchTargets[i], targetPos[i][0] - 32, targetPos[i][1] - 32);
        mkLabel(touchTargets[i], &lv_font_montserrat_16, lv_color_white(),
                LV_ALIGN_CENTER, 0, 0, targetNames[i]);
    }
    touchLbl = mkLabel(s, &lv_font_montserrat_20, lv_color_white(), LV_ALIGN_CENTER, 0, -20, "");
    touchResultLbl = mkLabel(s, &lv_font_montserrat_20, CYAN(), LV_ALIGN_CENTER, 0, 40, "");
    lv_obj_set_width(touchResultLbl, 230);
    lv_label_set_long_mode(touchResultLbl, LV_LABEL_LONG_WRAP);
    touchDot = mkCircle(s, 36, RED());
    lv_obj_add_flag(touchDot, LV_OBJ_FLAG_HIDDEN);
}

// Work out TOUCH_SWAP_XY / MIRROR_X / MIRROR_Y from the raw samples.
// Order matches Hardware::update(): swap first, then mirror.
static void touchInfer() {
    int16_t p[T_COUNT][2];
    int dxTB = abs(touchSamples[T_BOTTOM][0] - touchSamples[T_TOP][0]);
    int dyTB = abs(touchSamples[T_BOTTOM][1] - touchSamples[T_TOP][1]);
    bool swap = dxTB > dyTB;   // TOP->BOTTOM should move panel Y
    for (int i = 0; i < T_COUNT; i++) {
        p[i][0] = swap ? touchSamples[i][1] : touchSamples[i][0];
        p[i][1] = swap ? touchSamples[i][0] : touchSamples[i][1];
    }
    int spanY = p[T_BOTTOM][1] - p[T_TOP][1];
    int spanX = p[T_RIGHT][0]  - p[T_LEFT][0];
    char buf[200];
    if (abs(spanX) < 100 || abs(spanY) < 100) {
        snprintf(buf, sizeof(buf), "Inconclusive (spanX=%d spanY=%d).\nClick to retry.", spanX, spanY);
    } else {
        bool mirX = spanX < 0;
        bool mirY = spanY < 0;
        bool same = (swap == TOUCH_SWAP_XY) && (mirX == TOUCH_MIRROR_X) && (mirY == TOUCH_MIRROR_Y);
        snprintf(buf, sizeof(buf),
                 "SWAP_XY=%d MIRROR_X=%d MIRROR_Y=%d\n%s",
                 swap, mirX, mirY, same ? "PASS: matches BoardConfig.h" : "Set these in BoardConfig.h");
    }
    lv_label_set_text(touchResultLbl, buf);
    Serial.printf("[TOUCH] result: %s\n", buf);
    for (int i = 0; i < T_COUNT; i++) {
        Serial.printf("[TOUCH]   %-6s target=(%d,%d) raw=(%d,%d)\n", targetNames[i],
                      targetPos[i][0], targetPos[i][1], touchSamples[i][0], touchSamples[i][1]);
    }
}

static void touchEnter() {
    touchResetSequence();
    lv_label_set_text(touchLbl, "raw  --\nmapped  --");
    Serial.printf("[TOUCH] Tap TOP, RIGHT, BOTTOM, LEFT in turn. Current config: "
                  "SWAP_XY=%d MIRROR_X=%d MIRROR_Y=%d\n",
                  TOUCH_SWAP_XY, TOUCH_MIRROR_X, TOUCH_MIRROR_Y);
}

static void touchTick() {
    TouchDetail t = Hardware::getTouch();
    int16_t rx, ry;
    bool down = Hardware::getTouchRaw(rx, ry);

    if (down) {
        lv_obj_clear_flag(touchDot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(touchDot, t.x - 18, t.y - 18);
        lv_label_set_text_fmt(touchLbl, "raw  %d, %d\nmapped  %d, %d", rx, ry, t.x, t.y);
        if (millis() - touchLastLogMs >= 100) {
            touchLastLogMs = millis();
            Serial.printf("[TOUCH] %s raw=(%d,%d) mapped=(%d,%d)\n",
                          touchWasDown ? "move" : "down", rx, ry, t.x, t.y);
        }
    } else if (touchWasDown) {
        lv_obj_add_flag(touchDot, LV_OBJ_FLAG_HIDDEN);
        Serial.printf("[TOUCH] up   raw=(%d,%d) mapped=(%d,%d)\n", rx, ry, t.x, t.y);
    }

    if (t.wasPressed && touchStep < T_COUNT) {
        touchSamples[touchStep][0] = rx;
        touchSamples[touchStep][1] = ry;
        Serial.printf("[TOUCH] sample %s: raw=(%d,%d)\n", targetNames[touchStep], rx, ry);
        lv_obj_set_style_bg_color(touchTargets[touchStep], GREEN(), 0);
        touchStep++;
        if (touchStep < T_COUNT) {
            lv_obj_set_style_bg_color(touchTargets[touchStep], YELLOW(), 0);
            lv_label_set_text_fmt(touchResultLbl, "Tap the yellow target: %s", targetNames[touchStep]);
        } else {
            touchInfer();
        }
    }
    touchWasDown = down;
}

static void touchClick() {
    touchResetSequence();
    Serial.println("[TOUCH] sequence restarted");
}

// ============================================================================
// 4. INPUT — encoder, button events, backlight ramp
// ============================================================================
static lv_obj_t* inputEncLbl;
static lv_obj_t* inputBtnLbl;
static lv_obj_t* inputBlLbl;
static lv_obj_t* inputBlBar;
static bool      inputCapture = false;   // encoder drives backlight
static int       inputBlPct   = 50;
static uint8_t   inputBlSaved;
static char      inputEvents[5][28];
static int       inputLastDir = 0;

static ESP32Encoder encoder;

static int64_t detentOf(int64_t raw) {
    // Floor division: 4 raw counts per detent in both directions
    return raw >= 0 ? raw / 4 : -((-raw + 3) / 4);
}

static void inputRenderBacklight() {
    lv_label_set_text_fmt(inputBlLbl, "Backlight %d%%%s", inputBlPct,
                          inputCapture ? "  (rotate)" : "");
    lv_bar_set_value(inputBlBar, inputBlPct, LV_ANIM_OFF);
}

static void inputRenderEvents() {
    char buf[5 * 30] = "";
    for (int i = 0; i < 5; i++) {
        if (inputEvents[i][0]) { strcat(buf, inputEvents[i]); strcat(buf, "\n"); }
    }
    lv_label_set_text(inputBtnLbl, buf[0] ? buf : "No button events yet");
}

// Called for every button event while this screen is active
static void inputLogEvent(const char* name) {
    for (int i = 4; i > 0; i--) strcpy(inputEvents[i], inputEvents[i - 1]);
    snprintf(inputEvents[0], sizeof(inputEvents[0]), "%lu.%02lus  %s",
             (unsigned long)(millis() / 1000), (unsigned long)((millis() / 10) % 100), name);
    inputRenderEvents();
}

static void inputBuild(lv_obj_t* s) {
    inputEncLbl = mkLabel(s, &lv_font_montserrat_24, lv_color_white(), LV_ALIGN_CENTER, 0, -110, "");
    inputBtnLbl = mkLabel(s, &lv_font_montserrat_20, CYAN(), LV_ALIGN_CENTER, 0, -10, "");
    inputBlLbl  = mkLabel(s, &lv_font_montserrat_20, YELLOW(), LV_ALIGN_CENTER, 0, 88, "");
    inputBlBar  = lv_bar_create(s);
    lv_obj_set_size(inputBlBar, 260, 20);
    lv_obj_align(inputBlBar, LV_ALIGN_CENTER, 0, 120);
    lv_bar_set_range(inputBlBar, 0, 100);
}

static void inputEnter() {
    inputBlSaved = Hardware::getBacklight();
    inputBlPct   = (inputBlSaved * 100 + 127) / 255;
    inputCapture = false;
    inputRenderBacklight();
    inputRenderEvents();
}

static void inputExit() {
    inputCapture = false;
    Hardware::setBacklight(inputBlSaved);
    Serial.printf("[BL] restored to %d/255\n", inputBlSaved);
}

static void inputTick() {
    int64_t raw = encoder.getCount();
    lv_label_set_text_fmt(inputEncLbl, "Encoder raw %ld  detents %ld\nlast dir: %s",
                          (long)raw, (long)detentOf(raw),
                          inputLastDir > 0 ? "CW" : inputLastDir < 0 ? "CCW" : "-");
}

static void inputClick() {
    inputCapture = !inputCapture;
    inputRenderBacklight();
    Serial.printf("[BL] encoder %s backlight\n", inputCapture ? "now drives" : "released");
}

static bool inputEncoder(int delta) {
    inputLastDir = delta > 0 ? 1 : -1;
    if (!inputCapture) return false;
    inputBlPct += delta * 5;
    if (inputBlPct < 0)   inputBlPct = 0;
    if (inputBlPct > 100) inputBlPct = 100;
    Hardware::setBacklight((uint8_t)((inputBlPct * 255) / 100));
    inputRenderBacklight();
    Serial.printf("[BL] %d%% (%d/255)\n", inputBlPct, (inputBlPct * 255) / 100);
    return true;
}

// ============================================================================
// 5. I2C SCAN — touch bus SDA 17 / SCL 18
// ============================================================================
static lv_obj_t* i2cLbl;

static void i2cScan() {
    TwoWire& bus = Hardware::i2cBus();
    char buf[320];
    int  len = snprintf(buf, sizeof(buf), "SDA %d / SCL %d\n\n", TOUCH_SDA, TOUCH_SCL);
    int  found = 0;
    bool touchFound = false;
    Serial.printf("[I2C] scanning SDA %d / SCL %d\n", TOUCH_SDA, TOUCH_SCL);
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        bus.beginTransmission(addr);
        if (bus.endTransmission() == 0) {
            found++;
            if (addr == TOUCH_I2C_ADDR) touchFound = true;
            Serial.printf("[I2C]   0x%02X%s\n", addr, addr == TOUCH_I2C_ADDR ? "  CST826 touch" : "");
            if (len < (int)sizeof(buf) - 32) {
                len += snprintf(buf + len, sizeof(buf) - len, "0x%02X%s\n", addr,
                                addr == TOUCH_I2C_ADDR ? "  CST826 touch" : "");
            }
        }
    }
    snprintf(buf + len, sizeof(buf) - len, "\n%d device(s)\nCST826 @0x%02X: %s",
             found, TOUCH_I2C_ADDR, touchFound ? "FOUND" : "MISSING");
    lv_label_set_text(i2cLbl, buf);
    lv_obj_set_style_text_color(i2cLbl, touchFound ? lv_color_white() : RED(), 0);
    Serial.printf("[I2C] %d device(s), CST826 %s\n", found, touchFound ? "FOUND" : "MISSING");
}

static void i2cBuild(lv_obj_t* s) { i2cLbl = mkBody(s, 0); }
static void i2cEnter()            { i2cScan(); }
static void i2cClick()            { i2cScan(); }

// ============================================================================
// 6. PIN FINDER — GPIO 43 at 1 Hz, GPIO 44 at 4 Hz on the UART connector
// ============================================================================
static lv_obj_t* pinsLed43;
static lv_obj_t* pinsLed44;
static lv_obj_t* pinsStateLbl;
static bool      pinsPaused;

static void pinsBuild(lv_obj_t* s) {
    lv_obj_t* warn = mkLabel(s, &lv_font_montserrat_20, lv_color_white(), LV_ALIGN_CENTER, 0, -95,
        "WARNING: GPIO 43/44 are being\ndriven as outputs. Disconnect\nthe CAN transceiver first.");
    lv_obj_set_style_bg_color(warn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
    lv_obj_set_style_bg_opa(warn, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(warn, 8, 0);

    pinsLed43 = lv_led_create(s);
    lv_obj_set_size(pinsLed43, 44, 44);
    lv_obj_align(pinsLed43, LV_ALIGN_CENTER, -80, 10);
    lv_led_set_color(pinsLed43, lv_palette_main(LV_PALETTE_ORANGE));
    mkLabel(s, &lv_font_montserrat_20, lv_color_white(), LV_ALIGN_CENTER, -80, 60, "GPIO43\n1 Hz");

    pinsLed44 = lv_led_create(s);
    lv_obj_set_size(pinsLed44, 44, 44);
    lv_obj_align(pinsLed44, LV_ALIGN_CENTER, 80, 10);
    lv_led_set_color(pinsLed44, lv_palette_main(LV_PALETTE_ORANGE));
    mkLabel(s, &lv_font_montserrat_20, lv_color_white(), LV_ALIGN_CENTER, 80, 60, "GPIO44\n4 Hz");

    pinsStateLbl = mkLabel(s, &lv_font_montserrat_20, YELLOW(), LV_ALIGN_CENTER, 0, 118, "");
}

static void pinsEnter() {
    pinsPaused = false;
    pinMode(CAN_TX_PIN, OUTPUT);
    pinMode(CAN_RX_PIN, OUTPUT);
    lv_label_set_text(pinsStateLbl, "TOGGLING");
    Serial.printf("[PINS] GPIO%d toggling at 1 Hz, GPIO%d at 4 Hz\n", CAN_TX_PIN, CAN_RX_PIN);
}

static void pinsExit() {
    digitalWrite(CAN_TX_PIN, LOW);
    digitalWrite(CAN_RX_PIN, LOW);
    pinMode(CAN_TX_PIN, INPUT);
    pinMode(CAN_RX_PIN, INPUT);
    Serial.println("[PINS] stopped, GPIO 43/44 released to inputs");
}

static void pinsTick() {
    bool s43 = !pinsPaused && ((millis() / 500) & 1);   // 1 Hz square wave
    bool s44 = !pinsPaused && ((millis() / 125) & 1);   // 4 Hz square wave
    digitalWrite(CAN_TX_PIN, s43);
    digitalWrite(CAN_RX_PIN, s44);
    if (s43) lv_led_on(pinsLed43); else lv_led_off(pinsLed43);
    if (s44) lv_led_on(pinsLed44); else lv_led_off(pinsLed44);
}

static void pinsClick() {
    pinsPaused = !pinsPaused;
    lv_label_set_text(pinsStateLbl, pinsPaused ? "PAUSED (both LOW)" : "TOGGLING");
    Serial.printf("[PINS] %s\n", pinsPaused ? "paused, both LOW" : "toggling");
}

// ============================================================================
// 7. CAN — TWAI TX 43 / RX 44 at 500 kbps
// ============================================================================
#define BRINGUP_CAN_TEST_ID  0x7A0   // clear of ZombieVerter/SDO/BMS IDs

static lv_obj_t* canLbl;
static bool      canInstalled;
static bool      canSelfTest;
static esp_err_t canInstallErr;
static uint32_t  canRx, canTx, canTxFail, canRxLogged, canRxLastSec, canRxAtLastSec;
static uint32_t  canLastStatusMs;
static twai_message_t     canLast;
static bool               canHaveLast;
static twai_status_info_t canStatus;

static const char* canStateName(twai_state_t s) {
    switch (s) {
        case TWAI_STATE_STOPPED:    return "STOPPED";
        case TWAI_STATE_RUNNING:    return "RUNNING";
        case TWAI_STATE_BUS_OFF:    return "BUS-OFF";
        case TWAI_STATE_RECOVERING: return "RECOVERING";
        default:                    return "?";
    }
}

static void canStart() {
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN,
        canSelfTest ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    canInstallErr = twai_driver_install(&g, &t, &f);
    if (canInstallErr == ESP_OK) canInstallErr = twai_start();
    canInstalled = (canInstallErr == ESP_OK);
    canRx = canTx = canTxFail = canRxLogged = 0;
    canRxAtLastSec = 0;
    canHaveLast = false;
    Serial.printf("[CAN] %s mode, TX %d RX %d 500k: %s\n",
                  canSelfTest ? "SELF-TEST (NO_ACK)" : "NORMAL",
                  CAN_TX_PIN, CAN_RX_PIN, esp_err_to_name(canInstallErr));
}

static void canStop() {
    twai_stop();
    twai_driver_uninstall();
    canInstalled = false;
    pinMode(CAN_TX_PIN, INPUT);
    pinMode(CAN_RX_PIN, INPUT);
}

static void canRender() {
    char data[3 * 8 + 1] = "";
    if (canHaveLast) {
        for (int i = 0; i < canLast.data_length_code && i < 8; i++) {
            snprintf(data + i * 3, 4, "%02X ", canLast.data[i]);
        }
    }
    char buf[400];
    if (!canInstalled) {
        snprintf(buf, sizeof(buf), "%s\nDriver: %s",
                 canSelfTest ? "SELF-TEST" : "NORMAL", esp_err_to_name(canInstallErr));
    } else {
        snprintf(buf, sizeof(buf),
            "%s  %s\n"
            "RX %lu (%lu/s)  TX %lu  fail %lu\n"
            "Last: %s%03lX [%d]\n%s\n"
            "TEC %lu  REC %lu\n"
            "bus err %lu  arb lost %lu\n"
            "tx queued %lu  rx missed %lu",
            canSelfTest ? "SELF-TEST" : "NORMAL", canStateName(canStatus.state),
            (unsigned long)canRx, (unsigned long)canRxLastSec,
            (unsigned long)canTx, (unsigned long)canTxFail,
            canHaveLast ? (canLast.extd ? "x" : "") : "", canHaveLast ? (unsigned long)canLast.identifier : 0UL,
            canHaveLast ? canLast.data_length_code : 0, canHaveLast ? data : "--",
            (unsigned long)canStatus.tx_error_counter, (unsigned long)canStatus.rx_error_counter,
            (unsigned long)canStatus.bus_error_count, (unsigned long)canStatus.arb_lost_count,
            (unsigned long)canStatus.msgs_to_tx, (unsigned long)canStatus.rx_missed_count);
    }
    lv_label_set_text(canLbl, buf);
}

static void canBuild(lv_obj_t* s) { canLbl = mkBody(s, 0); lv_obj_set_style_text_font(canLbl, &lv_font_montserrat_20, 0); }

static void canEnter() {
    memset(&canStatus, 0, sizeof(canStatus));
    canStart();
    canLastStatusMs = millis();
    canRender();
}

static void canExit() {
    if (canInstalled) canStop();
    Serial.println("[CAN] stopped, driver uninstalled");
}

static void canTick() {
    if (!canInstalled) return;
    twai_message_t m;
    while (twai_receive(&m, 0) == ESP_OK) {
        canRx++;
        canLast = m;
        canHaveLast = true;
        // Log the first frames individually, then only per-second totals
        // (logging every frame on a busy bus saturates the serial port)
        if (canRxLogged < 20) {
            canRxLogged++;
            Serial.printf("[CAN] RX %s0x%03lX [%d]", m.extd ? "x" : "", (unsigned long)m.identifier,
                          m.data_length_code);
            for (int i = 0; i < m.data_length_code && i < 8; i++) Serial.printf(" %02X", m.data[i]);
            Serial.println(canRxLogged == 20 ? "  (further frames summarised per second)" : "");
        }
    }
    uint32_t now = millis();
    if (now - canLastStatusMs >= 1000) {
        canLastStatusMs = now;
        twai_state_t prev = canStatus.state;
        twai_get_status_info(&canStatus);
        canRxLastSec   = canRx - canRxAtLastSec;
        canRxAtLastSec = canRx;
        Serial.printf("[CAN] %s rx=%lu (%lu/s) tx=%lu fail=%lu TEC=%lu REC=%lu buserr=%lu\n",
                      canStateName(canStatus.state), (unsigned long)canRx, (unsigned long)canRxLastSec,
                      (unsigned long)canTx, (unsigned long)canTxFail,
                      (unsigned long)canStatus.tx_error_counter, (unsigned long)canStatus.rx_error_counter,
                      (unsigned long)canStatus.bus_error_count);
        if (prev != canStatus.state) {
            Serial.printf("[CAN] state -> %s\n", canStateName(canStatus.state));
        }
        canRender();
    }
}

static void canClick() {
    if (!canInstalled) return;
    twai_message_t m = {};
    m.identifier       = BRINGUP_CAN_TEST_ID;
    m.data_length_code = 8;
    m.self             = canSelfTest ? 1 : 0;   // self-reception in self-test
    m.data[0] = 0xB0; m.data[1] = 0x07;         // fixed marker, [4..7] = counter
    m.data[4] = (canTx >> 24) & 0xFF; m.data[5] = (canTx >> 16) & 0xFF;
    m.data[6] = (canTx >> 8) & 0xFF;  m.data[7] = canTx & 0xFF;
    esp_err_t e = twai_transmit(&m, pdMS_TO_TICKS(50));
    if (e == ESP_OK) canTx++; else canTxFail++;
    Serial.printf("[CAN] TX 0x%03X #%lu: %s\n", BRINGUP_CAN_TEST_ID, (unsigned long)canTx,
                  esp_err_to_name(e));
    canRender();
}

static void canDoubleClick() {
    if (canInstalled) canStop();
    canSelfTest = !canSelfTest;
    canStart();
    canRender();
}

static void canLongPress() {
    if (!canInstalled) return;
    twai_get_status_info(&canStatus);
    if (canStatus.state == TWAI_STATE_BUS_OFF) {
        Serial.printf("[CAN] bus-off recovery: %s\n", esp_err_to_name(twai_initiate_recovery()));
    } else if (canStatus.state == TWAI_STATE_STOPPED) {
        Serial.printf("[CAN] restart: %s\n", esp_err_to_name(twai_start()));
    } else {
        Serial.println("[CAN] long-press: not bus-off, nothing to recover");
    }
}

// ============================================================================
// 8. TEAR / STRESS — moving bar + spinning arc; click toggles WiFi AP with
//    UDP flood, PSRAM copies and periodic NVS writes on core 0
// ============================================================================
#define STRESS_AP_SSID  "MaTouch-Bringup"
#define STRESS_AP_PASS  "bringup123"

static lv_obj_t* stressBar;
static lv_obj_t* stressArc;
static lv_obj_t* stressLbl;
static uint32_t  stressLastSecMs, stressFramesAtSec, stressFps;
static volatile bool     stressRun, stressTaskAlive;
static volatile uint32_t stressUdpPkts, stressNvsWrites, stressCopyKB;

static void stressTask(void*) {
    WiFiUDP udp;
    Preferences prefs;
    static uint8_t pkt[1024];
    const size_t   copyLen = 64 * 1024;
    uint8_t* a = (uint8_t*)heap_caps_malloc(copyLen, MALLOC_CAP_SPIRAM);
    uint8_t* b = (uint8_t*)heap_caps_malloc(copyLen, MALLOC_CAP_SPIRAM);
    uint32_t lastNvs = millis();
    uint32_t nvsCount = 0;
    IPAddress bcast(192, 168, 4, 255);
    while (stressRun) {
        // Network load: broadcast UDP from the AP
        memset(pkt, (uint8_t)stressUdpPkts, sizeof(pkt));
        if (udp.beginPacket(bcast, 4210)) {
            udp.write(pkt, sizeof(pkt));
            if (udp.endPacket()) stressUdpPkts++;
        }
        // PSRAM bus contention with the RGB panel's framebuffer reads
        if (a && b) { memcpy(b, a, copyLen); stressCopyKB += copyLen / 1024; }
        // Flash writes suspend the cache — the classic RGB-panel glitch trigger
        if (millis() - lastNvs >= 2000) {
            lastNvs = millis();
            prefs.begin("bringup", false);
            prefs.putUInt("n", ++nvsCount);
            prefs.end();
            stressNvsWrites++;
        }
        vTaskDelay(1);
    }
    if (a) free(a);
    if (b) free(b);
    stressTaskAlive = false;
    vTaskDelete(nullptr);
}

static void stressLoadOn() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(STRESS_AP_SSID, STRESS_AP_PASS);
    stressUdpPkts = stressNvsWrites = stressCopyKB = 0;
    stressRun = true;
    stressTaskAlive = true;
    xTaskCreatePinnedToCore(stressTask, "stress", 6144, nullptr, 1, nullptr, 0);
    Serial.printf("[STRESS] load ON: AP '%s' + UDP flood + PSRAM copy + NVS write every 2 s\n",
                  STRESS_AP_SSID);
}

static void stressLoadOff() {
    if (!stressRun) return;
    stressRun = false;
    for (int i = 0; i < 100 && stressTaskAlive; i++) vTaskDelay(pdMS_TO_TICKS(10));
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[STRESS] load OFF");
}

static void stressBuild(lv_obj_t* s) {
    stressArc = lv_arc_create(s);
    lv_obj_set_size(stressArc, 300, 300);
    lv_obj_center(stressArc);
    lv_arc_set_bg_angles(stressArc, 0, 360);
    lv_arc_set_angles(stressArc, 0, 90);
    lv_obj_set_style_arc_width(stressArc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(stressArc, 16, LV_PART_INDICATOR);
    lv_obj_remove_style(stressArc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(stressArc, LV_OBJ_FLAG_CLICKABLE);

    stressBar = lv_obj_create(s);
    lv_obj_set_size(stressBar, 24, 360);
    lv_obj_set_style_bg_color(stressBar, lv_color_white(), 0);
    lv_obj_set_style_border_width(stressBar, 0, 0);
    lv_obj_set_style_radius(stressBar, 0, 0);
    lv_obj_align(stressBar, LV_ALIGN_LEFT_MID, 60, 0);

    stressLbl = mkLabel(s, &lv_font_montserrat_20, YELLOW(), LV_ALIGN_CENTER, 0, 0, "");
}

static void stressRender() {
    lv_label_set_text_fmt(stressLbl, "%lu FPS\nload %s\nudp %lu  nvs %lu\npsram %lu MB",
                          (unsigned long)stressFps, stressRun ? "ON" : "OFF",
                          (unsigned long)stressUdpPkts, (unsigned long)stressNvsWrites,
                          (unsigned long)(stressCopyKB / 1024));
}

static void stressEnter() {
    stressLastSecMs = millis();
    stressFramesAtSec = framesRendered;
    stressFps = 0;
    stressRender();
    Serial.println("[STRESS] watch the bar/arc edges for tearing, shifts or flicker");
}

static void stressExit() { stressLoadOff(); }

static void stressTick() {
    uint32_t now = millis();
    // Bar sweeps 60..396 px and back every 2 s; arc turns once per second
    uint32_t ph = now % 2000;
    int x = 60 + (int)((ph < 1000 ? ph : 2000 - ph) * 336 / 1000);
    lv_obj_set_x(stressBar, x);
    lv_arc_set_rotation(stressArc, (uint16_t)((now % 1000) * 360 / 1000));

    if (now - stressLastSecMs >= 1000) {
        stressFps = framesRendered - stressFramesAtSec;
        stressFramesAtSec = framesRendered;
        stressLastSecMs = now;
        stressRender();
        Serial.printf("[STRESS] fps=%lu load=%s udp=%lu nvs=%lu psramMB=%lu heap=%lu\n",
                      (unsigned long)stressFps, stressRun ? "ON" : "OFF",
                      (unsigned long)stressUdpPkts, (unsigned long)stressNvsWrites,
                      (unsigned long)(stressCopyKB / 1024), (unsigned long)ESP.getFreeHeap());
    }
}

static void stressClick() {
    if (stressRun) stressLoadOff(); else stressLoadOn();
    stressRender();
}

// ============================================================================
// Mode table
// ============================================================================
static const ModeDef modes[M_COUNT] = {
    {"INFO",   "Click: log to serial",
     infoBuild,   infoEnter,   nullptr,     infoTick,   infoClick,   nullptr,        nullptr,      nullptr},
    {"COLOUR", "Click: full-screen fills",
     colourBuild, colourEnter, colourExit,  nullptr,    colourClick, nullptr,        nullptr,      nullptr},
    {"TOUCH",  "Click: restart taps",
     touchBuild,  touchEnter,  nullptr,     touchTick,  touchClick,  nullptr,        nullptr,      nullptr},
    {"INPUT",  "Click: encoder <-> backlight",
     inputBuild,  inputEnter,  inputExit,   inputTick,  inputClick,  nullptr,        nullptr,      inputEncoder},
    {"I2C",    "Click: rescan",
     i2cBuild,    i2cEnter,    nullptr,     nullptr,    i2cClick,    nullptr,        nullptr,      nullptr},
    {"PINS",   "Click: pause/resume",
     pinsBuild,   pinsEnter,   pinsExit,    pinsTick,   pinsClick,   nullptr,        nullptr,      nullptr},
    {"CAN",    "Click: send  Dbl: self-test\nLong: bus-off recovery",
     canBuild,    canEnter,    canExit,     canTick,    canClick,    canDoubleClick, canLongPress, nullptr},
    {"STRESS", "Click: WiFi/NVS load on/off",
     stressBuild, stressEnter, stressExit,  stressTick, stressClick, nullptr,        nullptr,      nullptr},
};

static void buildScreens() {
    for (int i = 0; i < M_COUNT; i++) {
        lv_obj_t* s = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(s, lv_color_black(), 0);
        lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
        modes[i].build(s);
        char title[32];
        snprintf(title, sizeof(title), "%d/%d  %s", i + 1, M_COUNT, modes[i].title);
        // COLOUR keeps the top edge for its TOP/BOTTOM markers
        int ty = (i == M_COLOUR) ? 48 : 40;
        mkLabel(s, &lv_font_montserrat_28, CYAN(), LV_ALIGN_TOP_MID, 0, ty, title);
        mkLabel(s, &lv_font_montserrat_16, GREY(), LV_ALIGN_BOTTOM_MID, 0,
                (i == M_COLOUR) ? -44 : -36, modes[i].hint);
        screens[i] = s;
    }
}

static void enterMode(int m) {
    if (modes[currentMode].exit) modes[currentMode].exit();
    currentMode = m;
    lv_scr_load(screens[m]);
    Serial.printf("\n[MODE] %d/%d %s\n", m + 1, M_COUNT, modes[m].title);
    if (modes[m].enter) modes[m].enter();
}

// ============================================================================
// Input dispatch
// ============================================================================
static OneButton button(ENCODER_BUTTON, true, true);
static volatile uint8_t pendingBtn;   // 1 click, 2 double, 3 long
static int64_t lastDetent;

static void handleButton(uint8_t ev) {
    static const char* names[] = {"", "click", "double-click", "long-press"};
    Serial.printf("[BTN] %s\n", names[ev]);
    if (currentMode == M_INPUT) inputLogEvent(names[ev]);
    const ModeDef& md = modes[currentMode];
    if (ev == 1 && md.click)       md.click();
    if (ev == 2 && md.doubleClick) md.doubleClick();
    if (ev == 3 && md.longPress)   md.longPress();
}

static void handleEncoder() {
    int64_t raw = encoder.getCount();
    int64_t det = detentOf(raw);
    if (det == lastDetent) return;
    int delta = (int)(det - lastDetent);
    lastDetent = det;
    Serial.printf("[ENC] raw=%ld detent=%ld delta=%+d\n", (long)raw, (long)det, delta);
    if (modes[currentMode].encoder && modes[currentMode].encoder(delta)) return;
    int next = currentMode + (delta > 0 ? 1 : -1);
    if (next < 0)        next = M_COUNT - 1;
    if (next >= M_COUNT) next = 0;
    enterMode(next);
}

// ============================================================================
// setup / loop
// ============================================================================
void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) vTaskDelay(pdMS_TO_TICKS(10));
    Serial.println("\n=== MaTouch 2.1 bring-up (" BOARD_NAME ") ===");

    if (!Hardware::init()) {
        Serial.println("[BOOT] Hardware::init FAILED (panel begin)");
    } else {
        Serial.println("[BOOT] panel + touch + backlight initialised");
    }
    if (!initLvgl()) {
        Serial.println("[BOOT] LVGL draw buffer allocation FAILED");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);
    encoder.clearCount();
    lastDetent = 0;

    button.attachClick([]()       { pendingBtn = 1; });
    button.attachDoubleClick([]() { pendingBtn = 2; });
    button.attachLongPressStart([]() { pendingBtn = 3; });
    button.setClickMs(250);
    button.setPressMs(800);

    buildScreens();
    currentMode = M_INFO;
    lv_scr_load(screens[M_INFO]);
    Serial.printf("[MODE] 1/%d %s\n", M_COUNT, modes[M_INFO].title);
    modes[M_INFO].enter();
}

void loop() {
    Hardware::update();
    button.tick();
    if (pendingBtn) {
        uint8_t ev = pendingBtn;
        pendingBtn = 0;
        handleButton(ev);
    }
    handleEncoder();
    if (modes[currentMode].tick) modes[currentMode].tick();
    lv_timer_handler();
    vTaskDelay(1);
}

#endif // BRINGUP_FW
