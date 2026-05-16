#include "UIManager.h"
#include "Immobilizer.h"
#include "HealthChecker.h"
#include "EfficiencyTracker.h"
#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include "FaultLogger.h"
#include <M5GFX.h>

// Static instance for callbacks
UIManager* UIManager::instance = nullptr;

UIManager::UIManager() 
    : canManager(nullptr), immobilizer(nullptr), currentScreen(SCREEN_SPLASH), 
      lastUpdateTime(0), buf1(nullptr), buf2(nullptr), editMode(false), lockPinPadVisible(false),
      settings_selected_item(0), settingsArrivalTime(0) {
    strncpy(dialFWVersion, "---", sizeof(dialFWVersion));
    strncpy(uiFWVersion,   "---", sizeof(uiFWVersion));
    instance = this;
    for (int i = 0; i < SCREEN_COUNT; i++) {
        screens[i] = nullptr;
    }
}

UIManager::~UIManager() {
    if (buf1) free(buf1);
    if (buf2) free(buf2);
}

bool UIManager::init(CANDataManager* canMgr, Immobilizer* immob) {
    canManager = canMgr;
    immobilizer = immob;
    
    lv_init();
    
    size_t buffer_size = SCREEN_WIDTH * 40;
    buf1 = (lv_color_t*)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t*)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    if (!buf1 || !buf2) {
        Serial.println("ERROR: Failed to allocate LVGL buffers!");
        return false;
    }
    
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buffer_size);
    
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    lv_disp_drv_register(&disp_drv);
    
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = lvgl_encoder_read_cb;
    indev_drv.user_data = this;
    lv_indev_drv_register(&indev_drv);
    
    lv_theme_t* theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_CYAN),
        true,
        &lv_font_montserrat_14
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);
    
    createSplashScreen();
    createLockScreen();
    createDashboardScreen();
    createPowerScreen();
    createTemperatureScreen();
    createBatteryScreen();
    createBMSScreen();
    createGearScreen();
    createMotorScreen();
    createRegenScreen();
    createWiFiScreen();
    createSettingsScreen();
    createChargingScreen();
    createHealthCheckScreen();
    
    setScreen(SCREEN_SPLASH);
    
    Serial.println("LVGL UI initialized successfully!");
    return true;
}

void UIManager::showFetchStatus(const char* msg) {
    if (splash_fetch_label) {
        lv_label_set_text(splash_fetch_label, msg);
        lv_timer_handler();
    }
}

void UIManager::updateWifiScreen(const String& ip) {
    if (wifi_ip_label) {
        lv_label_set_text_fmt(wifi_ip_label, "IP: %s", ip.c_str());
    }
    if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, "Status: Active");
        lv_obj_set_style_text_color(wifi_status_label,
            lv_palette_main(LV_PALETTE_GREEN), 0);
    }
}

void UIManager::resetWifiScreen() {
    if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, "Status: Inactive");
        lv_obj_set_style_text_color(wifi_status_label,
            lv_color_white(), 0);
    }
    if (wifi_ip_label) {
        lv_label_set_text(wifi_ip_label, "IP: 192.168.4.1");
    }
}

void UIManager::lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);
    M5.Display.pushPixels((uint16_t*)&color_p->full, w * h, true);
    M5.Display.endWrite();
    lv_disp_flush_ready(disp);
}

void UIManager::lvgl_encoder_read_cb(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    data->enc_diff = 0;
    data->state = LV_INDEV_STATE_RELEASED;
}

void UIManager::update() {
    lv_timer_handler();

    // Auto-hide warning overlay when it expires
    if (warningLabel && millis() > warningExpiry) {
        lv_obj_del(warningLabel);
        warningLabel = nullptr;
    }

    if (currentScreen == SCREEN_LOCK && immobilizer) {
        updateLockScreen();
    }
    
    if (millis() - lastUpdateTime >= 100) {
        lastUpdateTime = millis();
        
        switch (currentScreen) {
            case SCREEN_LOCK:        break;
            case SCREEN_DASHBOARD:   updateDashboard();   break;
            case SCREEN_POWER:       updatePower();       break;
            case SCREEN_TEMPERATURE: updateTemperature(); break;
            case SCREEN_BATTERY:     updateBattery();     break;
            case SCREEN_BMS:         updateBMS();         break;
            case SCREEN_GEAR:        updateGear();        break;
            case SCREEN_MOTOR:       updateMotor();       break;
            case SCREEN_REGEN:       updateRegen();       break;
            case SCREEN_SETTINGS:    updateSettings();    break;
            case SCREEN_CHARGING:    updateCharging();    break;
            case SCREEN_HEALTH_CHECK: updateHealthCheck(); break;
            default:                 break;
        }
    }
}

void UIManager::setScreen(ScreenID screen) {
    if (screen >= SCREEN_COUNT) return;
    // When leaving the splash screen, free the logo pixel buffer — it has already
    // been rendered and is no longer needed. Freeing here gives back ~115KB of
    // heap before WiFi starts, preventing the radio allocation from crashing.
    if (currentScreen == SCREEN_SPLASH && screen != SCREEN_SPLASH && splash_logo_buf) {
        free(splash_logo_buf);
        splash_logo_buf = nullptr;
        splash_logo_dsc.data = nullptr;   // prevent LVGL touching freed memory
        Serial.println("[SPLASH] Logo buffer freed");
    }
    // When leaving an editable screen, always exit edit mode
    if (isEditableScreen() && screen != currentScreen) {
        editMode = false;
    }
    // When entering the lock screen, always start in padlock view (not PIN pad)
    if (screen == SCREEN_LOCK) {
        lockPinPadVisible = false;
    }
    // When entering settings, reset selection to top item and record arrival time
    if (screen == SCREEN_SETTINGS) {
        // Un-highlight current before reset
        if (settings_menu_indicators[settings_selected_item]) {
            lv_obj_add_flag(settings_menu_indicators[settings_selected_item], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(settings_menu_labels[settings_selected_item],
                                         lv_color_white(), 0);
        }
        settings_selected_item = 0;
        settingsArrivalTime = millis();   // for tap debounce in main.cpp
        if (settings_menu_indicators[0]) {
            lv_obj_clear_flag(settings_menu_indicators[0], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(settings_menu_labels[0],
                                         lv_palette_main(LV_PALETTE_CYAN), 0);
        }
    }
    currentScreen = screen;
    if (screens[screen]) {
        lv_scr_load_anim(screens[screen], LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

// Screens excluded from normal dial rotation
static bool isNavigableScreen(ScreenID s) {
    if (s == SCREEN_SPLASH || s == SCREEN_LOCK ||
        s == SCREEN_HEALTH_CHECK || s == SCREEN_CHARGING) return false;
    // Check instance screen mask
    UIManager* ui = UIManager::getInstance();
    if (ui) {
        uint16_t mask = ui->getScreenMask();
        if (!(mask & (1u << (uint16_t)s))) return false;
    }
    return true;
}

ScreenID UIManager::getNextScreen() {
    int next = (int)currentScreen + 1;
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (next >= SCREEN_COUNT) next = SCREEN_DASHBOARD; // wrap to first real screen
        if (isNavigableScreen((ScreenID)next)) return (ScreenID)next;
        next++;
    }
    return currentScreen; // fallback
}

ScreenID UIManager::getPreviousScreen() {
    int prev = (int)currentScreen - 1;
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (prev < SCREEN_DASHBOARD) prev = SCREEN_COUNT - 1; // wrap to last
        if (isNavigableScreen((ScreenID)prev)) return (ScreenID)prev;
        prev--;
    }
    return currentScreen; // fallback
}

void UIManager::clearAllScreens() {
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (screens[i]) {
            lv_obj_del(screens[i]);
            screens[i] = nullptr;
        }
    }
}

lv_color_t UIManager::getColorForValue(int32_t value, int32_t min_val, int32_t max_val) {
    float normalized = (float)(value - min_val) / (max_val - min_val);
    if (normalized < 0.5f) {
        uint8_t r = (uint8_t)(normalized * 2.0f * 255);
        return lv_color_make(r, 255, 0);
    } else {
        uint8_t g = (uint8_t)((1.0f - (normalized - 0.5f) * 2.0f) * 255);
        return lv_color_make(255, g, 0);
    }
}

void UIManager::setMeterValue(lv_obj_t* meter, lv_meter_indicator_t* indic, int32_t value, int32_t min_val, int32_t max_val) {
    if (!meter || !indic) return;
    if (value < min_val) value = min_val;
    if (value > max_val) value = max_val;
    lv_meter_set_indicator_value(meter, indic, value);
}

// ============================================================================
// SCREEN CREATION
// ============================================================================

void UIManager::createSplashScreen() {
    screens[SCREEN_SPLASH] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SPLASH], lv_color_black(), 0);

    // ── Attempt to load logo from SPIFFS ─────────────────────────────────
    // /logo.bin: 4-byte header (uint16 w, uint16 h) + raw RGB565 pixels
    bool logoLoaded = false;

    if (SPIFFS.exists("/logo.bin")) {
        File f = SPIFFS.open("/logo.bin", "r");
        if (f) {
            uint16_t w = 0, h = 0;
            f.read((uint8_t*)&w, 2);
            f.read((uint8_t*)&h, 2);
            size_t pixBytes = (size_t)w * h * 2;

            if (w > 0 && h > 0 && w <= 240 && h <= 240 && pixBytes <= 120000) {
                // Allocate in PSRAM if available, else heap
                uint8_t* buf = (uint8_t*)heap_caps_malloc(pixBytes,
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                if (!buf) buf = (uint8_t*)malloc(pixBytes);

                if (buf && f.read(buf, pixBytes) == pixBytes) {
                    // Store buffer pointer so we can free on screen destroy (future)
                    splash_logo_buf = buf;

                    // Fill LVGL image descriptor
                    splash_logo_dsc.header.always_zero = 0;
                    splash_logo_dsc.header.w    = w;
                    splash_logo_dsc.header.h    = h;
                    splash_logo_dsc.header.cf   = LV_IMG_CF_TRUE_COLOR;
                    splash_logo_dsc.data_size   = pixBytes;
                    splash_logo_dsc.data        = buf;

                    // Full-screen image widget
                    splash_logo_img = lv_img_create(screens[SCREEN_SPLASH]);
                    lv_img_set_src(splash_logo_img, &splash_logo_dsc);
                    lv_obj_set_size(splash_logo_img, 240, 240);
                    lv_obj_align(splash_logo_img, LV_ALIGN_CENTER, 0, 0);

                    logoLoaded = true;
                    Serial.printf("[SPLASH] Logo loaded %dx%d\n", w, h);
                } else {
                    if (buf) free(buf);
                    splash_logo_buf = nullptr;
                    Serial.println("[SPLASH] Logo read failed");
                }
            }
            f.close();
        }
    }

    // ── Bottom overlay bar — shown whether or not logo is present ─────────
    // Dark semi-transparent rectangle across bottom ~40px
    lv_obj_t* overlay = lv_obj_create(screens[SCREEN_SPLASH]);
    lv_obj_set_size(overlay, 240, 44);
    lv_obj_align(overlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, logoLoaded ? LV_OPA_70 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    if (!logoLoaded) {
        // Text-only splash — show title and subtitle in centre
        lv_obj_t* title = lv_label_create(screens[SCREEN_SPLASH]);
        lv_label_set_text(title, "ZombieVerter");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

        lv_obj_t* subtitle = lv_label_create(screens[SCREEN_SPLASH]);
        lv_label_set_text(subtitle, "M5Dial Display");
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(subtitle, lv_palette_lighten(LV_PALETTE_CYAN, 2), 0);
        lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 4);
    }

    // Version label — inside overlay bar, left-aligned
    splash_version_label = lv_label_create(screens[SCREEN_SPLASH]);
    lv_label_set_text(splash_version_label, dialFWVersion);
    lv_obj_set_style_text_font(splash_version_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(splash_version_label,
        logoLoaded ? lv_color_white() : lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(splash_version_label, LV_ALIGN_BOTTOM_LEFT, 8, -26);

    // Fetch status label — inside overlay bar, centred
    splash_fetch_label = lv_label_create(screens[SCREEN_SPLASH]);
    lv_label_set_text(splash_fetch_label, "");
    lv_obj_set_style_text_font(splash_fetch_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(splash_fetch_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_set_style_text_align(splash_fetch_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(splash_fetch_label, LV_ALIGN_BOTTOM_MID, 0, -8);
    // Child index: logo(0 if present), overlay, [title, subtitle if no logo],
    // version, fetch — showFetchStatus uses last child by counting
}

// Called from main.cpp after a successful logo upload while WiFi is active.
// Loads /logo.bin from SPIFFS into the existing splash screen widget without
// requiring a reboot. Safe to call while WiFi is running — splash is not the
// active screen so LVGL won't be rendering it during the update.
void UIManager::reloadLogo() {
    if (!screens[SCREEN_SPLASH]) return;

    // Free any previously loaded buffer
    if (splash_logo_buf) {
        free(splash_logo_buf);
        splash_logo_buf = nullptr;
        splash_logo_dsc.data = nullptr;
    }
    // Remove old image widget if it exists
    if (splash_logo_img) {
        lv_obj_del(splash_logo_img);
        splash_logo_img = nullptr;
    }

    if (!SPIFFS.exists("/logo.bin")) {
        Serial.println("[SPLASH] reloadLogo: no logo.bin");
        return;
    }
    File f = SPIFFS.open("/logo.bin", "r");
    if (!f) return;

    uint16_t w = 0, h = 0;
    f.read((uint8_t*)&w, 2);
    f.read((uint8_t*)&h, 2);
    size_t pixBytes = (size_t)w * h * 2;

    if (w > 0 && h > 0 && w <= 240 && h <= 240 && pixBytes <= 115200) {
        uint8_t* buf = (uint8_t*)heap_caps_malloc(pixBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) buf = (uint8_t*)malloc(pixBytes);

        if (buf && f.read(buf, pixBytes) == pixBytes) {
            splash_logo_buf = buf;
            splash_logo_dsc.header.always_zero = 0;
            splash_logo_dsc.header.w    = w;
            splash_logo_dsc.header.h    = h;
            splash_logo_dsc.header.cf   = LV_IMG_CF_TRUE_COLOR;
            splash_logo_dsc.data_size   = pixBytes;
            splash_logo_dsc.data        = buf;

            splash_logo_img = lv_img_create(screens[SCREEN_SPLASH]);
            lv_obj_move_to_index(splash_logo_img, 0);
            lv_img_set_src(splash_logo_img, &splash_logo_dsc);
            lv_obj_set_size(splash_logo_img, 240, 240);
            lv_obj_align(splash_logo_img, LV_ALIGN_CENTER, 0, 0);
            Serial.printf("[SPLASH] reloadLogo: %dx%d loaded OK\n", w, h);
        } else {
            if (buf) free(buf);
            Serial.println("[SPLASH] reloadLogo: read failed");
        }
    } else {
        Serial.printf("[SPLASH] reloadLogo: bad dims %dx%d\n", w, h);
    }
    f.close();
}

void UIManager::createDashboardScreen() {
    screens[SCREEN_DASHBOARD] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DASHBOARD], lv_color_black(), 0);
    
    dash_rpm_meter = lv_meter_create(screens[SCREEN_DASHBOARD]);
    lv_obj_set_size(dash_rpm_meter, 200, 200);
    lv_obj_center(dash_rpm_meter);
    lv_obj_set_style_bg_opa(dash_rpm_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dash_rpm_meter, 0, 0);
    
    lv_meter_scale_t* scale_rpm = lv_meter_add_scale(dash_rpm_meter);
    lv_meter_set_scale_ticks(dash_rpm_meter, scale_rpm, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(dash_rpm_meter, scale_rpm, 8, 3, 15, lv_color_white(), 10);
    lv_meter_set_scale_range(dash_rpm_meter, scale_rpm, 0, 80, 270, 135);
    
    dash_rpm_arc = lv_meter_add_arc(dash_rpm_meter, scale_rpm, 8, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_meter_set_indicator_start_value(dash_rpm_meter, dash_rpm_arc, 0);
    lv_meter_set_indicator_end_value(dash_rpm_meter, dash_rpm_arc, 0);
    
    dash_rpm_needle = lv_meter_add_needle_line(dash_rpm_meter, scale_rpm, 4, lv_palette_main(LV_PALETTE_CYAN), -10);
    
    // RPM numeric label — shown inside meter area
    dash_rpm_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_label_set_text(dash_rpm_label, "0 RPM");
    lv_obj_set_style_text_font(dash_rpm_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dash_rpm_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(dash_rpm_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(dash_rpm_label, LV_ALIGN_CENTER, 0, 30);

    dash_soc_arc = lv_arc_create(screens[SCREEN_DASHBOARD]);
    lv_obj_set_size(dash_soc_arc, 220, 220);
    lv_obj_center(dash_soc_arc);
    lv_arc_set_rotation(dash_soc_arc, 135);
    lv_arc_set_bg_angles(dash_soc_arc, 0, 270);
    lv_arc_set_value(dash_soc_arc, 0);
    lv_obj_set_style_arc_width(dash_soc_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dash_soc_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_remove_style(dash_soc_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dash_soc_arc, LV_OBJ_FLAG_CLICKABLE);

    // Voltage moved to bottom position; kW label removed
    dash_voltage_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_label_set_text(dash_voltage_label, "---V");
    lv_obj_set_style_text_font(dash_voltage_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(dash_voltage_label, lv_color_white(), 0);
    lv_obj_align(dash_voltage_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    // dash_power_label kept as hidden member (updateDashboard still calculates kW)
    dash_power_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_obj_add_flag(dash_power_label, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::createPowerScreen() {
    screens[SCREEN_POWER] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_POWER], lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(title, "POWER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    power_meter = lv_meter_create(screens[SCREEN_POWER]);
    lv_obj_set_size(power_meter, 220, 220);
    lv_obj_center(power_meter);
    lv_obj_set_style_bg_opa(power_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(power_meter, 0, 0);

    lv_meter_scale_t* scale = lv_meter_add_scale(power_meter);
    // 41 ticks across 200 units = 5 units/tick. nth_tick=10 → major every 50 units.
    // Labels at: -50, 0, 50, 100, 150
    lv_meter_set_scale_ticks(power_meter, scale, 41, 2, 10, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_meter_set_scale_major_ticks(power_meter, scale, 10, 3, 15, lv_color_white(), 10);
    lv_meter_set_scale_range(power_meter, scale, -50, 150, 270, 135);
    
    lv_meter_indicator_t* arc_regen = lv_meter_add_arc(power_meter, scale, 10, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_meter_set_indicator_start_value(power_meter, arc_regen, -50);
    lv_meter_set_indicator_end_value(power_meter, arc_regen, 0);
    
    lv_meter_indicator_t* arc_low = lv_meter_add_arc(power_meter, scale, 10, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_meter_set_indicator_start_value(power_meter, arc_low, 0);
    lv_meter_set_indicator_end_value(power_meter, arc_low, 50);
    
    lv_meter_indicator_t* arc_med = lv_meter_add_arc(power_meter, scale, 10, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_meter_set_indicator_start_value(power_meter, arc_med, 50);
    lv_meter_set_indicator_end_value(power_meter, arc_med, 100);
    
    lv_meter_indicator_t* arc_high = lv_meter_add_arc(power_meter, scale, 10, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(power_meter, arc_high, 100);
    lv_meter_set_indicator_end_value(power_meter, arc_high, 150);
    
    power_arc = lv_meter_add_arc(power_meter, scale, 12, lv_color_white(), 0);
    lv_meter_set_indicator_start_value(power_meter, power_arc, 0);
    lv_meter_set_indicator_end_value(power_meter, power_arc, 0);
    
    power_needle = lv_meter_add_needle_line(power_meter, scale, 4, lv_color_white(), -10);
    
    power_label = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(power_label, "0");
    lv_obj_set_style_text_font(power_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(power_label, lv_color_white(), 0);
    lv_obj_align(power_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t* kw_unit = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(kw_unit, "kW");
    lv_obj_set_style_text_font(kw_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(kw_unit, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(kw_unit, LV_ALIGN_CENTER, 0, 22);

    power_voltage_label = lv_label_create(screens[SCREEN_POWER]);
    lv_obj_add_flag(power_voltage_label, LV_OBJ_FLAG_HIDDEN);

    power_current_label = lv_label_create(screens[SCREEN_POWER]);
    lv_obj_add_flag(power_current_label, LV_OBJ_FLAG_HIDDEN);

    // power_soc_label kept as hidden member to avoid dangling pointer in updatePower
    power_soc_label = lv_label_create(screens[SCREEN_POWER]);
    lv_obj_add_flag(power_soc_label, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::createTemperatureScreen() {
    screens[SCREEN_TEMPERATURE] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_TEMPERATURE], lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(title, "TEMPERATURE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // ── Three-column layout: MOTOR | INV | BAT ────────────────────────────
    // Vertically centred: header row at y=85, value row at y=101, legend at y=150

    // MOTOR column label
    lv_obj_t* motor_col_lbl = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(motor_col_lbl, "MOTOR");
    lv_obj_set_style_text_font(motor_col_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(motor_col_lbl, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(motor_col_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(motor_col_lbl, 76);
    lv_obj_align(motor_col_lbl, LV_ALIGN_TOP_LEFT, 4, 85);

    temp_motor_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_motor_label, "--\xC2\xB0");
    lv_obj_set_style_text_font(temp_motor_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(temp_motor_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_text_align(temp_motor_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temp_motor_label, 76);
    lv_obj_align(temp_motor_label, LV_ALIGN_TOP_LEFT, 4, 101);

    // INV column label
    lv_obj_t* inv_col_lbl = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(inv_col_lbl, "INV");
    lv_obj_set_style_text_font(inv_col_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inv_col_lbl, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(inv_col_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(inv_col_lbl, 76);
    lv_obj_align(inv_col_lbl, LV_ALIGN_TOP_MID, 0, 85);

    temp_inverter_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_inverter_label, "--\xC2\xB0");
    lv_obj_set_style_text_font(temp_inverter_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(temp_inverter_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_text_align(temp_inverter_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temp_inverter_label, 76);
    lv_obj_align(temp_inverter_label, LV_ALIGN_TOP_MID, 0, 101);

    // BAT column label
    lv_obj_t* bat_col_lbl = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(bat_col_lbl, "BAT");
    lv_obj_set_style_text_font(bat_col_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bat_col_lbl, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(bat_col_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(bat_col_lbl, 76);
    lv_obj_align(bat_col_lbl, LV_ALIGN_TOP_RIGHT, -4, 85);

    temp_battery_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_battery_label, "--\xC2\xB0");
    lv_obj_set_style_text_font(temp_battery_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(temp_battery_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_text_align(temp_battery_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(temp_battery_label, 76);
    lv_obj_align(temp_battery_label, LV_ALIGN_TOP_RIGHT, -4, 101);

    // Divider lines
    lv_obj_t* div1 = lv_obj_create(screens[SCREEN_TEMPERATURE]);
    lv_obj_set_size(div1, 1, 56);
    lv_obj_set_style_bg_color(div1, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(div1, 0, 0);
    lv_obj_align(div1, LV_ALIGN_TOP_MID, -38, 83);

    lv_obj_t* div2 = lv_obj_create(screens[SCREEN_TEMPERATURE]);
    lv_obj_set_size(div2, 1, 56);
    lv_obj_set_style_bg_color(div2, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(div2, 0, 0);
    lv_obj_align(div2, LV_ALIGN_TOP_MID, 38, 83);

    // Colour legend — raised well above bottom edge
    lv_obj_t* legend = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(legend, "Green <60  Yellow <80  Red 80+");
    lv_obj_set_style_text_font(legend, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(legend, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(legend, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(legend, LV_ALIGN_BOTTOM_MID, 0, -42);

    // Null out arc members — replaced by plain labels
    temp_motor_arc = nullptr;
    temp_motor_indicator = nullptr;
    temp_inverter_arc = nullptr;
    temp_inverter_indicator = nullptr;
}

void UIManager::createBatteryScreen() {
    screens[SCREEN_BATTERY] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_BATTERY], lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(title, "BATTERY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // SOC % — large, centred, colour-coded
    battery_soc_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_soc_label, "--");
    lv_obj_set_style_text_font(battery_soc_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(battery_soc_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(battery_soc_label, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t* pct = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(pct, "%");
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pct, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(pct, LV_ALIGN_CENTER, 0, 8);

    // Voltage — font 20
    battery_voltage_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_voltage_label, "---V");
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(battery_voltage_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_set_style_text_align(battery_voltage_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(battery_voltage_label, LV_ALIGN_BOTTOM_MID, 0, -58);

    // Current — font 20
    battery_current_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_current_label, "---A");
    lv_obj_set_style_text_font(battery_current_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(battery_current_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(battery_current_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(battery_current_label, LV_ALIGN_BOTTOM_MID, 0, -34);

    // Temperature — font 20
    battery_temp_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_temp_label, "--\xC2\xB0""C");
    lv_obj_set_style_text_font(battery_temp_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(battery_temp_label, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(battery_temp_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(battery_temp_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Null out meter members — no longer used
    battery_soc_meter = nullptr;
    battery_soc_needle = nullptr;
    battery_soc_arc = nullptr;
}

void UIManager::createBMSScreen() {
    screens[SCREEN_BMS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_BMS], lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(title, "BMS STATUS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    bms_cell_max_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_max_label, "Max Cell: -.---V");
    lv_obj_set_style_text_font(bms_cell_max_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(bms_cell_max_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(bms_cell_max_label, LV_ALIGN_TOP_MID, 0, 50);

    bms_cell_min_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_min_label, "Min Cell: -.---V");
    lv_obj_set_style_text_font(bms_cell_min_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(bms_cell_min_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(bms_cell_min_label, LV_ALIGN_TOP_MID, 0, 88);

    bms_cell_delta_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_delta_label, "Delta: ---mV");
    lv_obj_set_style_text_font(bms_cell_delta_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(bms_cell_delta_label, lv_color_white(), 0);
    lv_obj_align(bms_cell_delta_label, LV_ALIGN_TOP_MID, 0, 126);

    bms_soc_bar = lv_bar_create(screens[SCREEN_BMS]);
    lv_obj_add_flag(bms_soc_bar, LV_OBJ_FLAG_HIDDEN);

    bms_temp_max_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_temp_max_label, "Max Temp: --\xC2\xB0""C");
    lv_obj_set_style_text_font(bms_temp_max_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(bms_temp_max_label, lv_color_white(), 0);
    lv_obj_align(bms_temp_max_label, LV_ALIGN_TOP_MID, 0, 162);
}

void UIManager::createGearScreen() {
    screens[SCREEN_GEAR] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_GEAR], lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(title, "GEAR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    gear_current_label = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(gear_current_label, "AUTO");
    lv_obj_set_style_text_font(gear_current_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(gear_current_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(gear_current_label, LV_ALIGN_CENTER, 0, -16);

    // ── Horizontal pill row — 4 options evenly spaced ─────────────────────
    // Names match ZombieVerter gear enum: 0=LOW 1=HIGH 2=AUTO 3=HI/LO
    const char* gearNames[] = {"LOW", "HIGH", "AUTO", "HI/LO"};
    // Distribute across 200px centred: x offsets from centre
    const int pill_y = 40;        // y offset from centre (below value)
    const int pill_w = 42;
    const int pill_h = 22;
    const int pill_gap = 6;
    // Total width = 4*42 + 3*6 = 186; start x = (240-186)/2 = 27
    const int start_x = 27;

    for (int i = 0; i < 4; i++) {
        int px = start_x + i * (pill_w + pill_gap);

        // Pill background
        gear_indicators[i] = lv_obj_create(screens[SCREEN_GEAR]);
        lv_obj_set_size(gear_indicators[i], pill_w, pill_h);
        lv_obj_set_style_radius(gear_indicators[i], 11, 0);
        lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 4), 0);
        lv_obj_set_style_border_width(gear_indicators[i], 1, 0);
        lv_obj_set_style_border_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 2), 0);
        lv_obj_set_pos(gear_indicators[i], px, 120 + pill_y);

        // Label on the pill
        gear_option_labels[i] = lv_label_create(screens[SCREEN_GEAR]);
        lv_label_set_text(gear_option_labels[i], gearNames[i]);
        lv_obj_set_style_text_font(gear_option_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(gear_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_align(gear_option_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(gear_option_labels[i], pill_w);
        lv_obj_set_pos(gear_option_labels[i], px, 120 + pill_y + 5);
    }

    lv_obj_t* inst = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(inst, "Click to edit");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void UIManager::createMotorScreen() {
    screens[SCREEN_MOTOR] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_MOTOR], lv_color_black(), 0);

    lv_obj_t* title = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(title, "MOTOR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    motor_current_label = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(motor_current_label, "MG1+MG2");
    lv_obj_set_style_text_font(motor_current_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(motor_current_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(motor_current_label, LV_ALIGN_CENTER, 0, -16);

    // ── Horizontal pill row ───────────────────────────────────────────────
    const char* motorNames[] = {"MG1", "MG2", "Both", "Blend"};
    const int pill_w = 48;
    const int pill_h = 22;
    const int pill_gap = 4;
    const int start_x = 20;   // (240 - 4*48 - 3*4) / 2 = 20

    for (int i = 0; i < 4; i++) {
        int px = start_x + i * (pill_w + pill_gap);

        motor_indicators[i] = lv_obj_create(screens[SCREEN_MOTOR]);
        lv_obj_set_size(motor_indicators[i], pill_w, pill_h);
        lv_obj_set_style_radius(motor_indicators[i], 11, 0);
        lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 4), 0);
        lv_obj_set_style_border_width(motor_indicators[i], 1, 0);
        lv_obj_set_style_border_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 2), 0);
        lv_obj_set_pos(motor_indicators[i], px, 120 + 40);

        motor_option_labels[i] = lv_label_create(screens[SCREEN_MOTOR]);
        lv_label_set_text(motor_option_labels[i], motorNames[i]);
        lv_obj_set_style_text_font(motor_option_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(motor_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_style_text_align(motor_option_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(motor_option_labels[i], pill_w);
        lv_obj_set_pos(motor_option_labels[i], px, 120 + 40 + 5);
    }

    lv_obj_t* inst = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(inst, "Click to edit");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void UIManager::createRegenScreen() {
    screens[SCREEN_REGEN] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_REGEN], lv_color_black(), 0);
    
    regen_title_label = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(regen_title_label, "REGEN MAX");
    lv_obj_set_style_text_font(regen_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(regen_title_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(regen_title_label, LV_ALIGN_TOP_MID, 0, 5);
    
    regen_arc = lv_arc_create(screens[SCREEN_REGEN]);
    lv_obj_set_size(regen_arc, 180, 180);
    lv_obj_center(regen_arc);
    lv_arc_set_rotation(regen_arc, 135);
    lv_arc_set_bg_angles(regen_arc, 0, 270);
    lv_arc_set_value(regen_arc, 0);
    lv_arc_set_range(regen_arc, -35, 0);
    lv_obj_set_style_arc_width(regen_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(regen_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(regen_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(regen_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_remove_style(regen_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(regen_arc, LV_OBJ_FLAG_CLICKABLE);
    
    regen_value_label = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(regen_value_label, "0%");
    lv_obj_set_style_text_font(regen_value_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(regen_value_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(regen_value_label, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t* inst = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(inst, "Click to edit");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -15);
}

void UIManager::createWiFiScreen() {
    screens[SCREEN_WIFI] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_WIFI], lv_color_black(), 0);
    
    lv_obj_t* title = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(title, "WiFi CONFIG");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    
    wifi_status_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_status_label, "Status: Inactive");
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(wifi_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 32);
    
    wifi_ssid_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_ssid_label, "SSID:\nZombieVerter-Display");
    lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(wifi_ssid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_ssid_label, LV_ALIGN_CENTER, 0, -30);
    
    wifi_password_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_password_label, "PW: zombieverter");
    lv_obj_set_style_text_font(wifi_password_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_password_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(wifi_password_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_password_label, LV_ALIGN_CENTER, 0, 5);
    
    wifi_ip_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_ip_label, "IP: 192.168.4.1");
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_ip_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_align(wifi_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_CENTER, 0, 35);
    
    lv_obj_t* inst = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(inst, "Click button to deactivate");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(inst, LV_ALIGN_CENTER, 0, 65);
}

void UIManager::createSettingsScreen() {
    screens[SCREEN_SETTINGS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SETTINGS], lv_color_black(), 0);

    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    // Instruction hint — lighter colour, above bottom edge
    lv_obj_t* hint = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(hint, "Press screen to select");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);

    // ── 6 menu items: 26px each, start y=42, total=156px ─────────────────
    const char* menuLabels[SETTINGS_MENU_COUNT] = {
        LV_SYMBOL_WIFI   "  Pair BLE Beacon",
        LV_SYMBOL_PLUS   "  Program RFID Fob",
        LV_SYMBOL_TRASH  "  Clear BLE Beacons",
        LV_SYMBOL_TRASH  "  Clear RFID Fobs",
        LV_SYMBOL_LIST   "  System Info",
        LV_SYMBOL_EDIT   "  Change PIN",
    };

    const int MENU_START_Y = 42;
    const int MENU_ITEM_H  = 26;

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        // Highlight bar
        settings_menu_indicators[i] = lv_obj_create(screens[SCREEN_SETTINGS]);
        lv_obj_set_size(settings_menu_indicators[i], 210, MENU_ITEM_H - 2);
        lv_obj_set_style_bg_color(settings_menu_indicators[i],
                                   lv_palette_darken(LV_PALETTE_CYAN, 3), 0);
        lv_obj_set_style_border_width(settings_menu_indicators[i], 0, 0);
        lv_obj_set_style_radius(settings_menu_indicators[i], 4, 0);
        lv_obj_align(settings_menu_indicators[i], LV_ALIGN_TOP_MID, 0,
                     MENU_START_Y + i * MENU_ITEM_H);
        lv_obj_add_flag(settings_menu_indicators[i], LV_OBJ_FLAG_HIDDEN);

        // Label
        settings_menu_labels[i] = lv_label_create(screens[SCREEN_SETTINGS]);
        lv_label_set_text(settings_menu_labels[i], menuLabels[i]);
        lv_obj_set_style_text_font(settings_menu_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(settings_menu_labels[i], lv_color_white(), 0);
        lv_obj_align(settings_menu_labels[i], LV_ALIGN_TOP_MID, 0,
                     MENU_START_Y + i * MENU_ITEM_H + 8);
    }

    // Highlight first item
    lv_obj_clear_flag(settings_menu_indicators[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(settings_menu_labels[0], lv_palette_main(LV_PALETTE_CYAN), 0);

    // Status labels — hidden, used only by updateSettings() for System Info popup
    // We keep them as members so updateSettings() can read values, but don't show them
    settings_can_status_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_can_status_label, "");
    lv_obj_add_flag(settings_can_status_label, LV_OBJ_FLAG_HIDDEN);

    settings_version_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_version_label, "");
    lv_obj_add_flag(settings_version_label, LV_OBJ_FLAG_HIDDEN);

    settings_param_count_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_param_count_label, "");
    lv_obj_add_flag(settings_param_count_label, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::scrollSettingsMenu(int delta) {
    // Un-highlight current
    lv_obj_add_flag(settings_menu_indicators[settings_selected_item], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(settings_menu_labels[settings_selected_item],
                                 lv_color_white(), 0);

    settings_selected_item += delta;
    if (settings_selected_item < 0) settings_selected_item = SETTINGS_MENU_COUNT - 1;
    if (settings_selected_item >= SETTINGS_MENU_COUNT) settings_selected_item = 0;

    // Highlight new
    lv_obj_clear_flag(settings_menu_indicators[settings_selected_item], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(settings_menu_labels[settings_selected_item],
                                 lv_palette_main(LV_PALETTE_CYAN), 0);
}

// ============================================================================
// UPDATE FUNCTIONS
// ============================================================================

void UIManager::updateDashboard() {
    if (!canManager) return;

    CANParameter* rpm = canManager->getParameterByName("speed");
    if (rpm) {
        int32_t value = rpm->getValueAsInt() / 100;
        lv_meter_set_indicator_value(dash_rpm_meter, dash_rpm_needle, value);
        lv_meter_set_indicator_end_value(dash_rpm_meter, dash_rpm_arc, value);
        lv_label_set_text_fmt(dash_rpm_label, "%d RPM", rpm->getValueAsInt());
    }

    CANParameter* voltage = canManager->getParameterByName("udc");
    if (voltage) {
        lv_label_set_text_fmt(dash_voltage_label, "%dV", voltage->getValueAsInt());
    }

    CANParameter* soc = canManager->getParameterByName("SOC");
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_arc_set_value(dash_soc_arc, value);
        if (value > 80) {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        } else if (value > 20) {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
        }
    }

    CANParameter* current = canManager->getParameterByName("idc");
    if (voltage && current) {
        int32_t kw = (voltage->getValueAsInt() * current->getValueAsInt()) / 1000;
        lv_label_set_text_fmt(dash_power_label, "%dkW", kw);
    }
}

void UIManager::updatePower() {
    if (!canManager) return;

    CANParameter* voltage = canManager->getParameterByName("udc");
    CANParameter* current = canManager->getParameterByName("idc");

    if (voltage && current) {
        int32_t kw = (voltage->getValueAsInt() * current->getValueAsInt()) / 1000;
        lv_meter_set_indicator_value(power_meter, power_needle, kw);
        lv_meter_set_indicator_end_value(power_meter, power_arc, kw);
        lv_label_set_text_fmt(power_label, "%d", kw);
    }

    CANParameter* soc = canManager->getParameterByName("SOC");
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_label_set_text_fmt(power_soc_label, "SOC: %d%%", value);
        if (value > 80) {
            lv_obj_set_style_text_color(power_soc_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (value > 20) {
            lv_obj_set_style_text_color(power_soc_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_obj_set_style_text_color(power_soc_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
}

void UIManager::updateTemperature() {
    if (!canManager) return;

    auto applyTempColor = [](lv_obj_t* lbl, int32_t val) {
        lv_color_t c = val < 60 ? lv_palette_main(LV_PALETTE_GREEN) :
                       val < 80 ? lv_palette_main(LV_PALETTE_YELLOW) :
                                  lv_palette_main(LV_PALETTE_RED);
        lv_obj_set_style_text_color(lbl, c, 0);
    };

    CANParameter* motorTemp = canManager->getParameterByName("tmpm");
    if (motorTemp) {
        int32_t val = motorTemp->getValueAsInt();
        lv_label_set_text_fmt(temp_motor_label, "%d\xC2\xB0", val);
        applyTempColor(temp_motor_label, val);
    }

    CANParameter* invTemp = canManager->getParameterByName("tmphs");
    if (invTemp) {
        int32_t val = invTemp->getValueAsInt();
        lv_label_set_text_fmt(temp_inverter_label, "%d\xC2\xB0", val);
        applyTempColor(temp_inverter_label, val);
    }

    CANParameter* batTemp = canManager->getParameterByName("tmpaux");
    if (batTemp) {
        int32_t val = batTemp->getValueAsInt();
        lv_label_set_text_fmt(temp_battery_label, "%d\xC2\xB0", val);
        applyTempColor(temp_battery_label, val);
    }
}

void UIManager::updateBattery() {
    if (!canManager) return;

    CANParameter* soc = canManager->getParameterByName("SOC");
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_label_set_text_fmt(battery_soc_label, "%d", value);
        lv_color_t c = value > 80 ? lv_palette_main(LV_PALETTE_GREEN) :
                       value > 20 ? lv_palette_main(LV_PALETTE_YELLOW) :
                                    lv_palette_main(LV_PALETTE_RED);
        lv_obj_set_style_text_color(battery_soc_label, c, 0);
    }

    CANParameter* voltage = canManager->getParameterByName("udc");
    if (voltage) {
        lv_label_set_text_fmt(battery_voltage_label, "%dV", voltage->getValueAsInt());
    }

    CANParameter* current = canManager->getParameterByName("idc");
    if (current) {
        lv_label_set_text_fmt(battery_current_label, "%dA", current->getValueAsInt());
    }

    CANParameter* temp = canManager->getParameterByName("tmpm");
    if (temp) {
        lv_label_set_text_fmt(battery_temp_label, "%d\xC2\xB0""C", temp->getValueAsInt());
    }
}

void UIManager::updateBMS() {
    if (!canManager) return;

    CANParameter* vmax = canManager->getParameterByName("BMS_Vmax");
    CANParameter* vmin = canManager->getParameterByName("BMS_Vmin");
    CANParameter* tmax = canManager->getParameterByName("BMS_Tmax");

    // Use snprintf for voltage formatting — LVGL's lv_label_set_text_fmt
    // does not support %f (float printf disabled to save flash space).
    if (vmax && bms_cell_max_label) {
        int32_t mv = vmax->getValueAsInt();
        char buf[12];
        snprintf(buf, sizeof(buf), "%d.%03dV", mv / 1000, mv % 1000);
        lv_label_set_text_fmt(bms_cell_max_label, "Max Cell: %s", buf);
    }

    if (vmin && bms_cell_min_label) {
        int32_t mv = vmin->getValueAsInt();
        char buf[12];
        snprintf(buf, sizeof(buf), "%d.%03dV", mv / 1000, mv % 1000);
        lv_label_set_text_fmt(bms_cell_min_label, "Min Cell: %s", buf);
    }

    if (vmax && vmin && bms_cell_delta_label) {
        int32_t delta = vmax->getValueAsInt() - vmin->getValueAsInt();
        lv_label_set_text_fmt(bms_cell_delta_label, "Delta: %dmV", delta);
        lv_obj_set_style_text_color(bms_cell_delta_label,
            delta > 100 ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), 0);
    }

    if (tmax && bms_temp_max_label) {
        int32_t tempC = tmax->getValueAsInt();
        lv_label_set_text_fmt(bms_temp_max_label, "Max Temp: %d\xC2\xB0""C", tempC);
        lv_obj_set_style_text_color(bms_temp_max_label,
            tempC > 45 ? lv_palette_main(LV_PALETTE_RED) :
            tempC > 35 ? lv_palette_main(LV_PALETTE_YELLOW) :
                         lv_color_white(), 0);
    }
}

void UIManager::updateGear() {
    if (!canManager) return;

    // Show edit mode hint in title
    lv_obj_t* screen = screens[SCREEN_GEAR];
    if (screen) {
        lv_obj_t* title = lv_obj_get_child(screen, 0);
        if (title) {
            lv_label_set_text(title, editMode ? "GEAR  [EDITING]" : "GEAR SELECTION");
            lv_obj_set_style_text_color(title,
                editMode ? lv_palette_main(LV_PALETTE_ORANGE) : lv_color_white(), 0);
        }
    }

    CANParameter* gear = canManager->getParameterByName("Gear");
    if (gear) {
        int32_t value = gear->getValueAsInt();
        const char* gearNames[] = {"LOW", "HIGH", "AUTO", "HI/LO"};
        if (value >= 0 && value < 4) {
            lv_label_set_text(gear_current_label, gearNames[value]);
            for (int i = 0; i < 4; i++) {
                if (i == value) {
                    lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_main(LV_PALETTE_CYAN), 0);
                    lv_obj_set_style_border_color(gear_indicators[i], lv_palette_main(LV_PALETTE_CYAN), 0);
                    lv_obj_set_style_text_color(gear_option_labels[i], lv_color_black(), 0);
                } else {
                    lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 4), 0);
                    lv_obj_set_style_border_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 2), 0);
                    lv_obj_set_style_text_color(gear_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
                }
            }
        }
    }
}

void UIManager::updateMotor() {
    if (!canManager) return;

    lv_obj_t* screen = screens[SCREEN_MOTOR];
    if (screen) {
        lv_obj_t* title = lv_obj_get_child(screen, 0);
        if (title) {
            lv_label_set_text(title, editMode ? "MOTOR  [EDITING]" : "MOTOR MODE");
            lv_obj_set_style_text_color(title,
                editMode ? lv_palette_main(LV_PALETTE_ORANGE) : lv_color_white(), 0);
        }
    }

    CANParameter* motor = canManager->getParameterByName("MotActive");
    if (motor) {
        int32_t value = motor->getValueAsInt();
        const char* motorNames[] = {"MG1 only", "MG2 only", "MG1+MG2", "Blended"};
        if (value >= 0 && value < 4) {
            lv_label_set_text(motor_current_label, motorNames[value]);
            for (int i = 0; i < 4; i++) {
                if (i == value) {
                    lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_main(LV_PALETTE_ORANGE), 0);
                    lv_obj_set_style_border_color(motor_indicators[i], lv_palette_main(LV_PALETTE_ORANGE), 0);
                    lv_obj_set_style_text_color(motor_option_labels[i], lv_color_black(), 0);
                } else {
                    lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 4), 0);
                    lv_obj_set_style_border_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 2), 0);
                    lv_obj_set_style_text_color(motor_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
                }
            }
        }
    }
}

void UIManager::updateRegen() {
    if (!canManager) return;

    lv_obj_t* screen = screens[SCREEN_REGEN];
    if (screen) {
        lv_label_set_text(regen_title_label, editMode ? "REGEN  [EDITING]" : "REGEN MAX");
        lv_obj_set_style_text_color(regen_title_label,
            editMode ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    }

    CANParameter* regen = canManager->getParameterByName("regenmax");
    if (regen) {
        int32_t value = regen->getValueAsInt();
        lv_arc_set_value(regen_arc, value);
        lv_label_set_text_fmt(regen_value_label, "%d%%", value);
        int absValue = abs(value);
        if (absValue > 25) {
            lv_obj_set_style_arc_color(regen_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(regen_value_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (absValue > 10) {
            lv_obj_set_style_arc_color(regen_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(regen_value_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_obj_set_style_arc_color(regen_arc, lv_palette_darken(LV_PALETTE_GREY, 1), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(regen_value_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        }
    }
}

void UIManager::setVersionInfo(const char* dialFW, const char* uiFW) {
    strncpy(dialFWVersion, dialFW, sizeof(dialFWVersion) - 1);
    dialFWVersion[sizeof(dialFWVersion) - 1] = '\0';
    strncpy(uiFWVersion, uiFW, sizeof(uiFWVersion) - 1);
    uiFWVersion[sizeof(uiFWVersion) - 1] = '\0';
    // Update splash label if screen already created
    if (splash_version_label) {
        lv_label_set_text(splash_version_label, dialFWVersion);
    }
}

void UIManager::updateSettings() {
    // Settings screen has no live-updating labels — version info is shown
    // via showSuccess() popup when System Info menu item is activated.
    // Nothing to update here on each loop tick.
}

void UIManager::showSystemInfo() {
    if (!canManager) return;

    // Build version info string
    bool connected = canManager->isConnected();
    const char* canStr = connected ? "Connected" : "No signal";

    // VCU version
    char vcuStr[16] = "---";
    CANParameter* ver = canManager->getParameterByName("version");
    if (ver && ver->getValueAsInt() > 0) {
        int32_t v = ver->getValueAsInt();
        if (v == 4) strncpy(vcuStr, "v2.30.A", sizeof(vcuStr));
        else        snprintf(vcuStr, sizeof(vcuStr), "v%d", (int)v);
    }

    // Show as a 4-second info overlay (reuse warningLabel infrastructure)
    lv_obj_t* activeScreen = screens[currentScreen];
    if (!activeScreen) return;
    if (warningLabel) { lv_obj_del(warningLabel); warningLabel = nullptr; }

    warningLabel = lv_obj_create(activeScreen);
    lv_obj_set_size(warningLabel, 210, 130);
    lv_obj_center(warningLabel);
    lv_obj_set_style_bg_color(warningLabel, lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_bg_opa(warningLabel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(warningLabel, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_border_width(warningLabel, 2, 0);
    lv_obj_set_style_radius(warningLabel, 10, 0);

    lv_obj_t* text = lv_label_create(warningLabel);
    lv_label_set_text_fmt(text,
        "SYSTEM INFO\n\n"
        "CAN:  %s\n"
        "VCU:  %s\n"
        "Dial: %s\n"
        "UI:   %s",
        canStr, vcuStr, dialFWVersion, uiFWVersion);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(text, lv_color_white(), 0);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text, 190);
    lv_obj_center(text);

    warningExpiry = millis() + 4000;   // show for 4 seconds
}

// ============================================================================
// EDIT MODE
// ============================================================================

void UIManager::toggleEditMode() {
    if (isEditableScreen()) {
        editMode = !editMode;
    }
}

bool UIManager::isEditableScreen() {
    return (currentScreen == SCREEN_GEAR ||
            currentScreen == SCREEN_MOTOR ||
            currentScreen == SCREEN_REGEN);
}

void UIManager::showWarning(const char* msg) {
    // Get the currently active screen
    lv_obj_t* activeScreen = screens[currentScreen];
    if (!activeScreen) return;

    // Remove any existing warning overlay
    if (warningLabel) {
        lv_obj_del(warningLabel);
        warningLabel = nullptr;
    }

    // Create a container with semi-transparent red background
    warningLabel = lv_obj_create(activeScreen);
    lv_obj_set_size(warningLabel, 200, 80);
    lv_obj_center(warningLabel);
    lv_obj_set_style_bg_color(warningLabel, lv_color_make(180, 0, 0), 0);
    lv_obj_set_style_bg_opa(warningLabel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(warningLabel, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(warningLabel, 2, 0);
    lv_obj_set_style_radius(warningLabel, 10, 0);

    // Warning icon + text
    lv_obj_t* text = lv_label_create(warningLabel);
    lv_label_set_text_fmt(text, "⚠ %s", msg);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(text, lv_color_white(), 0);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text, 180);
    lv_obj_center(text);

    warningExpiry = millis() + 2000;  // auto-hide after 2 seconds

    Serial.printf("[UI] Warning: %s\n", msg);
}

void UIManager::showSuccess(const char* msg) {
    lv_obj_t* activeScreen = screens[currentScreen];
    if (!activeScreen) return;

    if (warningLabel) {
        lv_obj_del(warningLabel);
        warningLabel = nullptr;
    }

    warningLabel = lv_obj_create(activeScreen);
    lv_obj_set_size(warningLabel, 200, 80);
    lv_obj_center(warningLabel);
    lv_obj_set_style_bg_color(warningLabel, lv_color_make(0, 140, 0), 0);
    lv_obj_set_style_bg_opa(warningLabel, LV_OPA_90, 0);
    lv_obj_set_style_border_color(warningLabel, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(warningLabel, 2, 0);
    lv_obj_set_style_radius(warningLabel, 10, 0);

    lv_obj_t* text = lv_label_create(warningLabel);
    lv_label_set_text_fmt(text, LV_SYMBOL_OK "  %s", msg);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(text, lv_color_white(), 0);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text, 180);
    lv_obj_center(text);

    warningExpiry = millis() + 2000;

    Serial.printf("[UI] Success: %s\n", msg);
}

// ============================================================================
// LOCK SCREEN
// ============================================================================

void UIManager::createLockScreen() {
    screens[SCREEN_LOCK] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_LOCK], lv_color_black(), 0);

    // ── Padlock icon body ─────────────────────────────────────────────────
    lock_icon = lv_obj_create(screens[SCREEN_LOCK]);
    lv_obj_set_size(lock_icon, 36, 28);
    lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(lock_icon, 0, 0);
    lv_obj_set_style_radius(lock_icon, 4, 0);
    lv_obj_align(lock_icon, LV_ALIGN_CENTER, 0, -55);

    // Shackle arc
    lv_obj_t* shackle = lv_arc_create(screens[SCREEN_LOCK]);
    lv_obj_set_size(shackle, 30, 30);
    lv_arc_set_rotation(shackle, 0);
    lv_arc_set_bg_angles(shackle, 0, 360);
    lv_arc_set_angles(shackle, 180, 360);
    lv_obj_set_style_arc_color(shackle, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(shackle, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(shackle, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(shackle, 0, LV_PART_MAIN);
    lv_obj_remove_style(shackle, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(shackle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(shackle, LV_ALIGN_CENTER, 0, -72);

    // Title — shown in both padlock and PIN pad views
    lock_title_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
    lv_obj_set_style_text_font(lock_title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lock_title_label, LV_ALIGN_CENTER, 0, -10);

    // Status label — padlock view: "Tap to enter PIN" / "Long-press to lock"
    lock_status_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_status_label, "Tap screen to enter PIN");
    lv_obj_set_style_text_font(lock_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lock_status_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(lock_status_label, LV_ALIGN_CENTER, 0, 30);

    // PIN digit dots — hidden in padlock view, shown in PIN pad view
    lock_pin_display = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_pin_display, "_ _ _ _");
    lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lock_pin_display, lv_color_white(), 0);
    lv_obj_align(lock_pin_display, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_flag(lock_pin_display, LV_OBJ_FLAG_HIDDEN);

    lock_digit_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_digit_label, "0");
    lv_obj_set_style_text_font(lock_digit_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(lock_digit_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(lock_digit_label, LV_ALIGN_CENTER, 0, 52);
    lv_obj_add_flag(lock_digit_label, LV_OBJ_FLAG_HIDDEN);

    // Instruction bar — bottom of screen
    lock_instruction_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_instruction_label, "Long-press to lock");
    lv_obj_set_style_text_font(lock_instruction_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lock_instruction_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(lock_instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lock_instruction_label, LV_ALIGN_BOTTOM_MID, 0, -52);
}

// Show the PIN pad — called when user taps the lock screen
void UIManager::showLockPinPad() {
    lockPinPadVisible = true;
    if (!immobilizer) return;
    // Only show PIN pad when actually locked — not during program/change-PIN modes
    if (immobilizer->getMode() != ImmobMode::LOCKED) return;

    immobilizer->clearPIN();  // fresh start each time pad is revealed

    lv_obj_clear_flag(lock_pin_display,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lock_digit_label,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lock_status_label,     LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lock_instruction_label, "Rotate: Select\nClick: Enter");
    lv_label_set_text(lock_pin_display, "_ _ _ _");
    lv_label_set_text(lock_digit_label, "0");
}

void UIManager::updateLockScreen() {
    if (!immobilizer) return;

    ImmobMode mode = immobilizer->getMode();

    // ── PAIR BLE mode ─────────────────────────────────────────────────────
    if (mode == ImmobMode::PAIR_BLE) {
        lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_label_set_text(lock_title_label, "PAIR BLE");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_clear_flag(lock_pin_display, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_digit_label,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_status_label,  LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lock_pin_display, "BRING PHONE\nCLOSE");
        lv_obj_set_style_text_color(lock_pin_display, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_16, 0);
        lv_label_set_text(lock_instruction_label, "Dbl-click to cancel  (30s timeout)");
        return;
    }

    // ── PROGRAM FOB mode ─────────────────────────────────────────────────
    if (mode == ImmobMode::PROGRAM_FOB) {
        lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_label_set_text(lock_title_label, "PROGRAM FOB");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        // Reuse pin_display for the "TAP NEW FOB" message
        lv_obj_clear_flag(lock_pin_display, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_digit_label,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_status_label,  LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lock_pin_display, "TAP NEW FOB");
        lv_obj_set_style_text_color(lock_pin_display, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_16, 0);
        lv_label_set_text(lock_instruction_label, "Dbl-click to cancel");
        return;
    }

    // ── CHANGE PIN modes ─────────────────────────────────────────────────
    if (mode == ImmobMode::CHANGE_PIN_1 || mode == ImmobMode::CHANGE_PIN_2) {
        lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text(lock_title_label,
            mode == ImmobMode::CHANGE_PIN_1 ? "NEW PIN" : "CONFIRM PIN");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_clear_flag(lock_pin_display, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lock_digit_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_status_label,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(lock_pin_display, lv_color_white(), 0);
        uint8_t pinPos = immobilizer->getPINPosition();
        char pinDisplay[16];
        for (int i = 0; i < 4; i++) {
            pinDisplay[i * 2]     = (i < pinPos) ? '*' : '_';
            pinDisplay[i * 2 + 1] = ' ';
        }
        pinDisplay[7] = '\0';
        lv_label_set_text(lock_pin_display, pinDisplay);
        lv_obj_set_style_text_font(lock_digit_label, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(lock_digit_label, lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_label_set_text_fmt(lock_digit_label, "%d", immobilizer->getCurrentDigit());
        lv_label_set_text(lock_instruction_label, "Rotate: Select\nClick: Enter");
        return;
    }

    // ── LOCKED — padlock view vs PIN pad view ─────────────────────────────
    if (mode == ImmobMode::LOCKED) {
        lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);

        if (lockPinPadVisible) {
            // PIN entry view
            lv_obj_add_flag(lock_status_label,     LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lock_pin_display,    LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lock_digit_label,    LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_color(lock_pin_display, lv_color_white(), 0);
            lv_obj_set_style_text_font(lock_digit_label, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_color(lock_digit_label, lv_palette_main(LV_PALETTE_CYAN), 0);
            uint8_t pinPos = immobilizer->getPINPosition();
            char pinDisplay[16];
            for (int i = 0; i < 4; i++) {
                pinDisplay[i * 2]     = (i < pinPos) ? '*' : '_';
                pinDisplay[i * 2 + 1] = ' ';
            }
            pinDisplay[7] = '\0';
            lv_label_set_text(lock_pin_display, pinDisplay);
            lv_label_set_text_fmt(lock_digit_label, "%d", immobilizer->getCurrentDigit());
            lv_label_set_text(lock_instruction_label, "Rotate: Select\nClick: Enter");
        } else {
            // Padlock view — clean, no digit selector
            lv_obj_clear_flag(lock_status_label,   LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lock_pin_display,      LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lock_digit_label,      LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lock_status_label, "Tap screen to enter PIN");
            lv_label_set_text(lock_instruction_label, "Long-press to lock");
        }
        return;
    }

    // ── UNLOCKED (shown briefly during programming entry) ─────────────────
    if (mode == ImmobMode::UNLOCKED) {
        lv_obj_set_style_bg_color(lock_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(lock_title_label, "UNLOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_clear_flag(lock_status_label,   LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_pin_display,      LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lock_digit_label,      LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lock_status_label, "");
        lv_label_set_text(lock_instruction_label, "Tap: Lock\nRotate/Click: Cancel");
    }
}

// ============================================================================
// CHARGING SCREEN
// Auto-shown when opmode transitions to 3 (Charge).
// Shows: SOC arc, charge current, pack voltage, energy added, ETA to full.
// ============================================================================

void UIManager::createChargingScreen() {
    screens[SCREEN_CHARGING] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_CHARGING], lv_color_black(), 0);

    // Title
    chg_title_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_title_label, LV_SYMBOL_CHARGE " CHARGING");
    lv_obj_set_style_text_font(chg_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(chg_title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(chg_title_label, LV_ALIGN_TOP_MID, 0, 8);

    // SOC arc meter (270 degree sweep)
    chg_soc_arc = lv_meter_create(screens[SCREEN_CHARGING]);
    lv_obj_set_size(chg_soc_arc, 140, 140);
    lv_obj_align(chg_soc_arc, LV_ALIGN_CENTER, 0, 8);
    lv_obj_set_style_bg_color(chg_soc_arc, lv_color_black(), 0);
    lv_obj_set_style_border_width(chg_soc_arc, 0, 0);

    lv_meter_scale_t* scale = lv_meter_add_scale(chg_soc_arc);
    lv_meter_set_scale_range(chg_soc_arc, scale, 0, 100, 270, 135);
    lv_meter_set_scale_ticks(chg_soc_arc, scale, 0, 0, 0, lv_color_black());
    chg_soc_indicator = lv_meter_add_arc(chg_soc_arc, scale, 8,
                                          lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_meter_set_indicator_start_value(chg_soc_arc, chg_soc_indicator, 0);
    lv_meter_set_indicator_end_value(chg_soc_arc, chg_soc_indicator, 0);

    // SOC % label centred in arc
    chg_soc_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_soc_label, "-%");
    lv_obj_set_style_text_font(chg_soc_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(chg_soc_label, lv_color_white(), 0);
    lv_obj_align(chg_soc_label, LV_ALIGN_CENTER, 0, 0);

    // Charge current
    chg_current_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_current_label, "- A");
    lv_obj_set_style_text_font(chg_current_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(chg_current_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(chg_current_label, LV_ALIGN_CENTER, 0, 50);

    // Pack voltage
    chg_voltage_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_voltage_label, "- V");
    lv_obj_set_style_text_font(chg_voltage_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(chg_voltage_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_align(chg_voltage_label, LV_ALIGN_CENTER, 0, 68);

    // ETA
    chg_eta_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_eta_label, "ETA: --");
    lv_obj_set_style_text_font(chg_eta_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chg_eta_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(chg_eta_label, LV_ALIGN_BOTTOM_MID, 0, -24);

    // Energy added this session
    chg_energy_label = lv_label_create(screens[SCREEN_CHARGING]);
    lv_label_set_text(chg_energy_label, "+0.0 Wh");
    lv_obj_set_style_text_font(chg_energy_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(chg_energy_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(chg_energy_label, LV_ALIGN_BOTTOM_MID, 0, -8);

    // Init history state
    memset(chg_voltageHistory, 0, sizeof(chg_voltageHistory));
    chg_historyHead  = 0;
    chg_historyCount = 0;
    chg_sessionStart = 0;
    chg_startSoc     = 0;
}

void UIManager::updateCharging() {
    if (!canManager) return;

    CANParameter* pIdc = canManager->getParameterByName("idc");
    CANParameter* pUdc = canManager->getParameterByName("udc");
    CANParameter* pSoc = canManager->getParameterByName("SOC");

    float idc = pIdc ? (float)pIdc->getValueAsInt() : 0;
    float udc = pUdc ? (float)pUdc->getValueAsInt() : 0;
    int   soc = pSoc ? pSoc->getValueAsInt()          : 0;

    // Record session start SOC on first update
    if (chg_sessionStart == 0) {
        chg_sessionStart = millis();
        chg_startSoc = (float)soc;
    }

    // SOC arc colour: green → amber → cyan at top
    lv_meter_set_indicator_end_value(chg_soc_arc, chg_soc_indicator, soc);
    lv_color_t socColor = soc < 80 ? lv_palette_main(LV_PALETTE_GREEN) :
                          soc < 95 ? lv_palette_main(LV_PALETTE_AMBER) :
                                     lv_palette_main(LV_PALETTE_CYAN);
    lv_obj_set_style_arc_color(chg_soc_arc, socColor, LV_PART_INDICATOR);
    lv_label_set_text_fmt(chg_soc_label, "%d%%", soc);

    // Charging current (IVT-S reports as negative when charging)
    float chgCurrent = -idc;
    lv_label_set_text_fmt(chg_current_label, "%.1f A", chgCurrent > 0 ? chgCurrent : 0.0f);
    lv_label_set_text_fmt(chg_voltage_label, "%.1f V", udc);

    // Energy added
    float energyWh = EfficiencyTracker::getInstance().getChargeEnergyWh();
    lv_label_set_text_fmt(chg_energy_label, "+%.0f Wh", energyWh);

    // ETA estimate
    float socGained = chg_startSoc > 0 ? (float)soc - chg_startSoc : 0;
    if (chgCurrent > 1.0f && udc > 100.0f && soc < 100 && socGained > 2.0f && energyWh > 0) {
        float whPerSoc    = energyWh / socGained;
        float whRemaining = whPerSoc * (float)(100 - soc);
        float powerW      = chgCurrent * udc;
        float etaMin      = (whRemaining / powerW) * 60.0f;
        if (etaMin < 999.0f)
            lv_label_set_text_fmt(chg_eta_label, "ETA: %dm", (int)etaMin);
        else
            lv_label_set_text(chg_eta_label, "ETA: ...");
    } else if (soc >= 100) {
        lv_label_set_text(chg_eta_label, "Charge complete");
    } else {
        lv_label_set_text(chg_eta_label, "ETA: calculating");
    }
}

// ============================================================================
// HEALTH CHECK SCREEN
// Shown on unlock. Displays up to 8 parameter checks with pass/warn/fail.
// Auto-advances to dashboard after HEALTH_DISPLAY_MS (5 seconds).
// ============================================================================

void UIManager::createHealthCheckScreen() {
    screens[SCREEN_HEALTH_CHECK] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_HEALTH_CHECK], lv_color_black(), 0);

    // Title
    health_title_label = lv_label_create(screens[SCREEN_HEALTH_CHECK]);
    lv_label_set_text(health_title_label, "PRE-DRIVE CHECK");
    lv_obj_set_style_text_font(health_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(health_title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(health_title_label, LV_ALIGN_TOP_MID, 0, 28);

    // Overall status (CHECKING... / ALL OK / CHECK SYSTEM)
    health_status_label = lv_label_create(screens[SCREEN_HEALTH_CHECK]);
    lv_label_set_text(health_status_label, "Checking...");
    lv_obj_set_style_text_font(health_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(health_status_label, lv_color_white(), 0);
    lv_obj_align(health_status_label, LV_ALIGN_TOP_MID, 0, 50);

    // Individual item rows — centred, 18px spacing, starting below status label
    health_item_count = 0;
    for (int i = 0; i < 8; i++) {
        health_item_labels[i] = lv_label_create(screens[SCREEN_HEALTH_CHECK]);
        lv_label_set_text(health_item_labels[i], "");
        lv_obj_set_style_text_font(health_item_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(health_item_labels[i], lv_color_white(), 0);
        lv_obj_set_style_text_align(health_item_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(health_item_labels[i], 200);
        lv_obj_align(health_item_labels[i], LV_ALIGN_TOP_MID, 0, 72 + i * 18);
    }
}

void UIManager::updateHealthCheck() {
    HealthChecker& hc = HealthChecker::getInstance();
    int n = hc.getItemCount();

    // Update each item row
    for (int i = 0; i < 8; i++) {
        if (i >= n) { lv_label_set_text(health_item_labels[i], ""); continue; }
        const HealthItem& item = hc.getItem(i);

        if (!item.checked) {
            lv_label_set_text_fmt(health_item_labels[i], "... %s", item.name);
            lv_obj_set_style_text_color(health_item_labels[i],
                                         lv_palette_main(LV_PALETTE_GREY), 0);
        } else if (!item.passed) {
            lv_label_set_text_fmt(health_item_labels[i],
                                   LV_SYMBOL_CLOSE " %s: %s", item.name, item.detail);
            lv_obj_set_style_text_color(health_item_labels[i],
                                         lv_palette_main(LV_PALETTE_RED), 0);
        } else if (strstr(item.detail, "WARN")) {
            lv_label_set_text_fmt(health_item_labels[i],
                                   "! %s: %s", item.name, item.detail);
            lv_obj_set_style_text_color(health_item_labels[i],
                                         lv_palette_main(LV_PALETTE_AMBER), 0);
        } else {
            lv_label_set_text_fmt(health_item_labels[i],
                                   LV_SYMBOL_OK " %s: %s", item.name, item.detail);
            lv_obj_set_style_text_color(health_item_labels[i],
                                         lv_palette_main(LV_PALETTE_GREEN), 0);
        }
    }

    // Update status line and auto-advance when done
    if (hc.isComplete()) {
        if (hc.needsAck()) {
            // Waiting for button press — show prompt
            lv_label_set_text(health_status_label,
                hc.isPassed() ? LV_SYMBOL_WARNING " WARNINGS — Press button"
                              : LV_SYMBOL_CLOSE   " FAILED — Press button");
            lv_obj_set_style_text_color(health_status_label,
                hc.isPassed() ? lv_palette_main(LV_PALETTE_YELLOW)
                              : lv_palette_main(LV_PALETTE_RED), 0);
        } else if (hc.isPassed()) {
            lv_label_set_text(health_status_label, LV_SYMBOL_OK " ALL OK");
            lv_obj_set_style_text_color(health_status_label,
                                         lv_palette_main(LV_PALETTE_GREEN), 0);
        } else {
            lv_label_set_text(health_status_label, LV_SYMBOL_WARNING " CHECK SYSTEM");
            lv_obj_set_style_text_color(health_status_label,
                                         lv_palette_main(LV_PALETTE_RED), 0);
        }
        if (!hc.needsAck() && hc.displayTimeRemaining() == 0) {
            setScreen(SCREEN_DASHBOARD);
        }
    }
}