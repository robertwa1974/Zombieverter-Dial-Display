#include "UIManager.h"
#include "Immobilizer.h"  // Need full definition, not just forward declaration
#include <M5GFX.h>

// Static instance for callbacks
UIManager* UIManager::instance = nullptr;

UIManager::UIManager() 
    : canManager(nullptr), immobilizer(nullptr), currentScreen(SCREEN_SPLASH), 
      lastUpdateTime(0), buf1(nullptr), buf2(nullptr), editMode(false) {
    instance = this;
    
    // Initialize screen array
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
    
    // Initialize LVGL
    lv_init();
    
    // Allocate LVGL draw buffers (using PSRAM for better performance)
    size_t buffer_size = SCREEN_WIDTH * 40; // 40 lines buffer
    buf1 = (lv_color_t*)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t*)heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    if (!buf1 || !buf2) {
        Serial.println("ERROR: Failed to allocate LVGL buffers!");
        return false;
    }
    
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buffer_size);
    
    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;
    lv_disp_drv_register(&disp_drv);
    
    // Initialize input device driver (for encoder)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = lvgl_encoder_read_cb;
    indev_drv.user_data = this;
    lv_indev_drv_register(&indev_drv);
    
    // Apply default theme
    lv_theme_t* theme = lv_theme_default_init(
        lv_disp_get_default(),
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_CYAN),
        true,  // Dark mode
        &lv_font_montserrat_14
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);
    
    // Create all screens
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
    
    // Load splash screen
    setScreen(SCREEN_SPLASH);
    
    Serial.println("LVGL UI initialized successfully!");
    return true;
}

void UIManager::lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // Push pixels to M5GFX display
    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);
    M5.Display.pushPixels((uint16_t*)&color_p->full, w * h, true);
    M5.Display.endWrite();
    
    lv_disp_flush_ready(disp);
}

void UIManager::lvgl_encoder_read_cb(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    // This will be handled by InputManager externally
    // For now, just report no change
    data->enc_diff = 0;
    data->state = LV_INDEV_STATE_RELEASED;
}

void UIManager::update() {
    // Handle LVGL tasks
    lv_timer_handler();
    
    // Update lock screen if active
    if (currentScreen == SCREEN_LOCK && immobilizer) {
        updateLockScreen();
    }
    
    // Update current screen data every 100ms
    if (millis() - lastUpdateTime >= 100) {
        lastUpdateTime = millis();
        
        switch (currentScreen) {
            case SCREEN_LOCK:
                // Lock screen updates handled above
                break;
            case SCREEN_DASHBOARD:
                updateDashboard();
                break;
            case SCREEN_POWER:
                updatePower();
                break;
            case SCREEN_TEMPERATURE:
                updateTemperature();
                break;
            case SCREEN_BATTERY:
                updateBattery();
                break;
            case SCREEN_BMS:
                updateBMS();
                break;
            case SCREEN_GEAR:
                updateGear();
                break;
            case SCREEN_MOTOR:
                updateMotor();
                break;
            case SCREEN_REGEN:
                updateRegen();
                break;
            default:
                break;
        }
    }
}

void UIManager::setScreen(ScreenID screen) {
    if (screen >= SCREEN_COUNT) return;
    
    currentScreen = screen;
    
    if (screens[screen]) {
        lv_scr_load_anim(screens[screen], LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }
}

ScreenID UIManager::getNextScreen() {
    int next = (int)currentScreen + 1;
    if (next >= SCREEN_COUNT) next = 0;
    return (ScreenID)next;
}

ScreenID UIManager::getPreviousScreen() {
    int prev = (int)currentScreen - 1;
    if (prev < 0) prev = SCREEN_COUNT - 1;
    return (ScreenID)prev;
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
    // Green for low values, yellow for medium, red for high
    float normalized = (float)(value - min_val) / (max_val - min_val);
    
    if (normalized < 0.5f) {
        // Green to Yellow
        uint8_t r = (uint8_t)(normalized * 2.0f * 255);
        return lv_color_make(r, 255, 0);
    } else {
        // Yellow to Red
        uint8_t g = (uint8_t)((1.0f - (normalized - 0.5f) * 2.0f) * 255);
        return lv_color_make(255, g, 0);
    }
}

void UIManager::setMeterValue(lv_obj_t* meter, lv_meter_indicator_t* indic, int32_t value, int32_t min_val, int32_t max_val) {
    if (!meter || !indic) return;
    
    // Clamp value
    if (value < min_val) value = min_val;
    if (value > max_val) value = max_val;
    
    lv_meter_set_indicator_value(meter, indic, value);
}

// ============================================================================
// SCREEN CREATION FUNCTIONS
// ============================================================================

void UIManager::createSplashScreen() {
    screens[SCREEN_SPLASH] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SPLASH], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_SPLASH]);
    lv_label_set_text(title, "ZombieVerter");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);
    
    // Subtitle
    lv_obj_t* subtitle = lv_label_create(screens[SCREEN_SPLASH]);
    lv_label_set_text(subtitle, "M5Dial Display");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_palette_lighten(LV_PALETTE_CYAN, 2), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, -10);
    
    // Version
    lv_obj_t* version = lv_label_create(screens[SCREEN_SPLASH]);
    lv_label_set_text(version, "v1.1.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(version, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 20);
    
    // Spinner
    lv_obj_t* spinner = lv_spinner_create(screens[SCREEN_SPLASH], 1000, 60);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
}

void UIManager::createDashboardScreen() {
    screens[SCREEN_DASHBOARD] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_DASHBOARD], lv_color_black(), 0);
    
    // Main RPM meter (large, centered)
    dash_rpm_meter = lv_meter_create(screens[SCREEN_DASHBOARD]);
    lv_obj_set_size(dash_rpm_meter, 200, 200);
    lv_obj_center(dash_rpm_meter);
    lv_obj_set_style_bg_opa(dash_rpm_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dash_rpm_meter, 0, 0);
    
    // Create RPM scale (x100 - shows 0 to 80 for 0-8000 RPM)
    lv_meter_scale_t* scale_rpm = lv_meter_add_scale(dash_rpm_meter);
    lv_meter_set_scale_ticks(dash_rpm_meter, scale_rpm, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(dash_rpm_meter, scale_rpm, 8, 3, 15, lv_color_white(), 10);
    lv_meter_set_scale_range(dash_rpm_meter, scale_rpm, 0, 80, 270, 135);  // 0-80 (x100)
    
    // Add colored arc indicators
    dash_rpm_arc = lv_meter_add_arc(dash_rpm_meter, scale_rpm, 8, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_meter_set_indicator_start_value(dash_rpm_meter, dash_rpm_arc, 0);
    lv_meter_set_indicator_end_value(dash_rpm_meter, dash_rpm_arc, 0);
    
    // Add needle
    dash_rpm_needle = lv_meter_add_needle_line(dash_rpm_meter, scale_rpm, 4, lv_palette_main(LV_PALETTE_CYAN), -10);
    
    // Create hidden label for updateDashboard (don't display it)
    dash_rpm_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_obj_add_flag(dash_rpm_label, LV_OBJ_FLAG_HIDDEN);  // Hide the label
    
    // SOC arc (outer ring)
    dash_soc_arc = lv_arc_create(screens[SCREEN_DASHBOARD]);
    lv_obj_set_size(dash_soc_arc, 220, 220);
    lv_obj_center(dash_soc_arc);
    lv_arc_set_rotation(dash_soc_arc, 135);
    lv_arc_set_bg_angles(dash_soc_arc, 0, 270);
    lv_arc_set_value(dash_soc_arc, 0);
    lv_obj_set_style_arc_width(dash_soc_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dash_soc_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_remove_style(dash_soc_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dash_soc_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Voltage label (top)
    dash_voltage_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_label_set_text(dash_voltage_label, "---V");
    lv_obj_set_style_text_font(dash_voltage_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dash_voltage_label, lv_color_white(), 0);
    lv_obj_align(dash_voltage_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Power label (bottom)
    dash_power_label = lv_label_create(screens[SCREEN_DASHBOARD]);
    lv_label_set_text(dash_power_label, "0.0kW");
    lv_obj_set_style_text_font(dash_power_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dash_power_label, lv_color_white(), 0);
    lv_obj_align(dash_power_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ============================================================================
// STUB IMPLEMENTATIONS - To be completed following LVGL_MIGRATION.md
// ============================================================================

/**
 * Complete UIManager.cpp Implementation
 * Replace the stub implementations (lines 303-430) with this code
 * 
 * This provides all 11 screens fully implemented with LVGL widgets
 */

// ============================================================================
// POWER SCREEN - Power meter with multi-zone arcs
// ============================================================================

void UIManager::createPowerScreen() {
    screens[SCREEN_POWER] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_POWER], lv_color_black(), 0);
    
    // Central power meter (LARGER - 220x220 instead of 180x180)
    power_meter = lv_meter_create(screens[SCREEN_POWER]);
    lv_obj_set_size(power_meter, 220, 220);  // Increased size
    lv_obj_center(power_meter);
    lv_obj_set_style_bg_opa(power_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(power_meter, 0, 0);
    
    // Create power scale (-50kW to +150kW)
    lv_meter_scale_t* scale = lv_meter_add_scale(power_meter);
    lv_meter_set_scale_ticks(power_meter, scale, 41, 2, 10, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_meter_set_scale_major_ticks(power_meter, scale, 8, 3, 15, lv_color_white(), 10);
    lv_meter_set_scale_range(power_meter, scale, -50, 150, 270, 135);
    
    // Add colored arc zones (THICKER - 10 instead of 6)
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
    
    // Active power arc (THICKER - 12 instead of 8)
    power_arc = lv_meter_add_arc(power_meter, scale, 12, lv_color_white(), 0);
    lv_meter_set_indicator_start_value(power_meter, power_arc, 0);
    lv_meter_set_indicator_end_value(power_meter, power_arc, 0);
    
    // Needle
    power_needle = lv_meter_add_needle_line(power_meter, scale, 4, lv_color_white(), -10);
    
    // Power value label (center)
    power_label = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(power_label, "0.0");
    lv_obj_set_style_text_font(power_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(power_label, lv_color_white(), 0);
    lv_obj_align(power_label, LV_ALIGN_CENTER, 0, -10);
    
    // kW unit
    lv_obj_t* unit = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(unit, "kW");
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(unit, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(unit, LV_ALIGN_CENTER, 0, 20);
    
    // Create hidden labels for corner values (don't display them)
    power_voltage_label = lv_label_create(screens[SCREEN_POWER]);
    lv_obj_add_flag(power_voltage_label, LV_OBJ_FLAG_HIDDEN);
    
    power_current_label = lv_label_create(screens[SCREEN_POWER]);
    lv_obj_add_flag(power_current_label, LV_OBJ_FLAG_HIDDEN);
    
    // SOC (bottom)
    power_soc_label = lv_label_create(screens[SCREEN_POWER]);
    lv_label_set_text(power_soc_label, "SOC: ---%");
    lv_obj_set_style_text_font(power_soc_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(power_soc_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(power_soc_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ============================================================================
// TEMPERATURE SCREEN - Dual arc meters for motor and inverter
// ============================================================================

void UIManager::createTemperatureScreen() {
    screens[SCREEN_TEMPERATURE] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_TEMPERATURE], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(title, "TEMPERATURE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // Larger/bolder
    lv_obj_set_style_text_color(title, lv_color_white(), 0);  // White like battery
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // Lower to match battery
    
    // Motor temp arc (left side)
    temp_motor_arc = lv_arc_create(screens[SCREEN_TEMPERATURE]);
    lv_obj_set_size(temp_motor_arc, 100, 100);
    lv_obj_align(temp_motor_arc, LV_ALIGN_LEFT_MID, 15, 0);
    lv_arc_set_rotation(temp_motor_arc, 135);
    lv_arc_set_bg_angles(temp_motor_arc, 0, 270);
    lv_arc_set_value(temp_motor_arc, 0);
    lv_arc_set_range(temp_motor_arc, 0, 120);
    lv_obj_set_style_arc_width(temp_motor_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(temp_motor_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(temp_motor_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_remove_style(temp_motor_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(temp_motor_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Motor temp label
    temp_motor_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_motor_label, "Motor\n--°C");
    lv_obj_set_style_text_font(temp_motor_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(temp_motor_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(temp_motor_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(temp_motor_label, LV_ALIGN_LEFT_MID, 35, 0);
    
    // Inverter temp arc (right side)
    temp_inverter_arc = lv_arc_create(screens[SCREEN_TEMPERATURE]);
    lv_obj_set_size(temp_inverter_arc, 100, 100);
    lv_obj_align(temp_inverter_arc, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_arc_set_rotation(temp_inverter_arc, 135);
    lv_arc_set_bg_angles(temp_inverter_arc, 0, 270);
    lv_arc_set_value(temp_inverter_arc, 0);
    lv_arc_set_range(temp_inverter_arc, 0, 100);
    lv_obj_set_style_arc_width(temp_inverter_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(temp_inverter_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(temp_inverter_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_remove_style(temp_inverter_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(temp_inverter_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Inverter temp label
    temp_inverter_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_inverter_label, "Inverter\n--°C");
    lv_obj_set_style_text_font(temp_inverter_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(temp_inverter_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(temp_inverter_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(temp_inverter_label, LV_ALIGN_RIGHT_MID, -35, 0);
    
    // Battery temp (bottom)
    temp_battery_label = lv_label_create(screens[SCREEN_TEMPERATURE]);
    lv_label_set_text(temp_battery_label, "Battery: --°C");
    lv_obj_set_style_text_font(temp_battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp_battery_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(temp_battery_label, LV_ALIGN_BOTTOM_MID, 0, -15);
}

// ============================================================================
// BATTERY SCREEN - SOC gauge with battery metrics
// ============================================================================

void UIManager::createBatteryScreen() {
    screens[SCREEN_BATTERY] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_BATTERY], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(title, "SOC");  // Changed from BATTERY
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // Larger/bolder
    lv_obj_set_style_text_color(title, lv_color_white(), 0);  // White
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // Lower to be visible
    
    // Central SOC meter
    battery_soc_meter = lv_meter_create(screens[SCREEN_BATTERY]);
    lv_obj_set_size(battery_soc_meter, 160, 160);
    lv_obj_center(battery_soc_meter);
    lv_obj_set_style_bg_opa(battery_soc_meter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(battery_soc_meter, 0, 0);
    
    // Create SOC scale (0-100%)
    lv_meter_scale_t* scale = lv_meter_add_scale(battery_soc_meter);
    lv_meter_set_scale_ticks(battery_soc_meter, scale, 21, 2, 8, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_meter_set_scale_major_ticks(battery_soc_meter, scale, 5, 3, 12, lv_color_white(), 10);
    lv_meter_set_scale_range(battery_soc_meter, scale, 0, 100, 270, 135);
    
    // Arc indicator
    battery_soc_arc = lv_meter_add_arc(battery_soc_meter, scale, 8, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_meter_set_indicator_start_value(battery_soc_meter, battery_soc_arc, 0);
    lv_meter_set_indicator_end_value(battery_soc_meter, battery_soc_arc, 0);
    
    // Needle
    battery_soc_needle = lv_meter_add_needle_line(battery_soc_meter, scale, 4, lv_color_white(), -10);
    
    // SOC percentage (center)
    battery_soc_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_soc_label, "--");
    lv_obj_set_style_text_font(battery_soc_label, &lv_font_montserrat_40, 0);  // Larger for clarity
    lv_obj_set_style_text_color(battery_soc_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(battery_soc_label, LV_ALIGN_CENTER, 0, -10);
    
    // % symbol
    lv_obj_t* percent = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(percent, "%");
    lv_obj_set_style_text_font(percent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(percent, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(percent, LV_ALIGN_CENTER, 0, 20);
    
    // Voltage
    battery_voltage_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_voltage_label, "Voltage: ---V");
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_voltage_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(battery_voltage_label, LV_ALIGN_BOTTOM_MID, 0, -35);
    
    // Current
    battery_current_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_current_label, "Current: ---A");
    lv_obj_set_style_text_font(battery_current_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_current_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(battery_current_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Temperature
    battery_temp_label = lv_label_create(screens[SCREEN_BATTERY]);
    lv_label_set_text(battery_temp_label, "Temp: --°C");
    lv_obj_set_style_text_font(battery_temp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_temp_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(battery_temp_label, LV_ALIGN_BOTTOM_MID, 0, -5);
}

// Continue in next message...
/**
 * Complete UIManager.cpp Implementation - Part 2
 * BMS, Gear, Motor, Regen, WiFi, Settings screens
 */

// ============================================================================
// BMS SCREEN - Cell voltage and temperature info
// ============================================================================

void UIManager::createBMSScreen() {
    screens[SCREEN_BMS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_BMS], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(title, "BMS STATUS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);  // Larger
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);
    
    // Cell max voltage (LARGER FONT)
    bms_cell_max_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_max_label, "Max Cell: -.--V");
    lv_obj_set_style_text_font(bms_cell_max_label, &lv_font_montserrat_20, 0);  // Increased from 14
    lv_obj_set_style_text_color(bms_cell_max_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(bms_cell_max_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // Cell min voltage (LARGER FONT)
    bms_cell_min_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_min_label, "Min Cell: -.--V");
    lv_obj_set_style_text_font(bms_cell_min_label, &lv_font_montserrat_20, 0);  // Increased from 14
    lv_obj_set_style_text_color(bms_cell_min_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(bms_cell_min_label, LV_ALIGN_TOP_MID, 0, 95);
    
    // Cell delta (LARGER FONT)
    bms_cell_delta_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_cell_delta_label, "Delta: ---mV");
    lv_obj_set_style_text_font(bms_cell_delta_label, &lv_font_montserrat_16, 0);  // Increased from 12
    lv_obj_set_style_text_color(bms_cell_delta_label, lv_color_white(), 0);
    lv_obj_align(bms_cell_delta_label, LV_ALIGN_CENTER, 0, 0);
    
    // SOC bar - HIDDEN (create but don't display)
    bms_soc_bar = lv_bar_create(screens[SCREEN_BMS]);
    lv_obj_add_flag(bms_soc_bar, LV_OBJ_FLAG_HIDDEN);
    
    // Max temp (LARGER FONT)
    bms_temp_max_label = lv_label_create(screens[SCREEN_BMS]);
    lv_label_set_text(bms_temp_max_label, "Max Temp: --°C");
    lv_obj_set_style_text_font(bms_temp_max_label, &lv_font_montserrat_20, 0);  // Increased from 14
    lv_obj_set_style_text_color(bms_temp_max_label, lv_color_white(), 0);
    lv_obj_align(bms_temp_max_label, LV_ALIGN_BOTTOM_MID, 0, -30);
}

// ============================================================================
// GEAR SCREEN - Gear selection display
// ============================================================================

void UIManager::createGearScreen() {
    screens[SCREEN_GEAR] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_GEAR], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(title, "GEAR SELECTION");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);  // White for visibility
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // Lower to be visible
    
    // Current gear (large center)
    gear_current_label = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(gear_current_label, "AUTO");
    lv_obj_set_style_text_font(gear_current_label, &lv_font_montserrat_40, 0);  // Larger for clarity
    lv_obj_set_style_text_color(gear_current_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(gear_current_label, LV_ALIGN_CENTER, 0, -10);
    
    // Gear options around the edge
    const char* gearNames[] = {"LOW", "HIGH", "AUTO", "HI/LO"};
    lv_coord_t positions[][2] = {
        {30, 80},   // LOW - left
        {170, 80},  // HIGH - right  
        {120, 170}, // AUTO - bottom right
        {60, 170}   // HI/LO - bottom left
    };
    
    for (int i = 0; i < 4; i++) {
        gear_option_labels[i] = lv_label_create(screens[SCREEN_GEAR]);
        lv_label_set_text(gear_option_labels[i], gearNames[i]);
        lv_obj_set_style_text_font(gear_option_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(gear_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_pos(gear_option_labels[i], positions[i][0], positions[i][1]);
        
        // Indicator LED
        gear_indicators[i] = lv_obj_create(screens[SCREEN_GEAR]);
        lv_obj_set_size(gear_indicators[i], 8, 8);
        lv_obj_set_style_radius(gear_indicators[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_border_width(gear_indicators[i], 0, 0);
        lv_obj_set_pos(gear_indicators[i], positions[i][0] - 15, positions[i][1] + 5);
    }
    
    // Instruction
    lv_obj_t* inst = lv_label_create(screens[SCREEN_GEAR]);
    lv_label_set_text(inst, "Rotate to change");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(inst, LV_ALIGN_CENTER, 0, 30);
}

// ============================================================================
// MOTOR SCREEN - Motor mode selection
// ============================================================================

void UIManager::createMotorScreen() {
    screens[SCREEN_MOTOR] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_MOTOR], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(title, "MOTOR MODE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);  // White for visibility
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // Lower to be visible
    
    // Current mode (large center)
    motor_current_label = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(motor_current_label, "MG1+MG2");
    lv_obj_set_style_text_font(motor_current_label, &lv_font_montserrat_32, 0);  // Keep 32 for longer text
    lv_obj_set_style_text_color(motor_current_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align(motor_current_label, LV_ALIGN_CENTER, 0, -10);
    
    // Motor options
    const char* motorNames[] = {"MG1 only", "MG2 only", "MG1+MG2", "Blended"};
    lv_coord_t positions[][2] = {
        {20, 80},   // MG1 - left
        {150, 80},  // MG2 - right
        {110, 170}, // MG1+MG2 - bottom right
        {40, 170}   // Blended - bottom left
    };
    
    for (int i = 0; i < 4; i++) {
        motor_option_labels[i] = lv_label_create(screens[SCREEN_MOTOR]);
        lv_label_set_text(motor_option_labels[i], motorNames[i]);
        lv_obj_set_style_text_font(motor_option_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(motor_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
        lv_obj_set_pos(motor_option_labels[i], positions[i][0], positions[i][1]);
        
        // Indicator LED
        motor_indicators[i] = lv_obj_create(screens[SCREEN_MOTOR]);
        lv_obj_set_size(motor_indicators[i], 8, 8);
        lv_obj_set_style_radius(motor_indicators[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
        lv_obj_set_style_border_width(motor_indicators[i], 0, 0);
        lv_obj_set_pos(motor_indicators[i], positions[i][0] - 15, positions[i][1] + 5);
    }
    
    // Instruction
    lv_obj_t* inst = lv_label_create(screens[SCREEN_MOTOR]);
    lv_label_set_text(inst, "Rotate to change");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(inst, LV_ALIGN_CENTER, 0, 30);
}

// ============================================================================
// REGEN SCREEN - Regenerative braking adjustment
// ============================================================================

void UIManager::createRegenScreen() {
    screens[SCREEN_REGEN] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_REGEN], lv_color_black(), 0);
    
    // Title
    regen_title_label = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(regen_title_label, "REGEN MAX");
    lv_obj_set_style_text_font(regen_title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(regen_title_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(regen_title_label, LV_ALIGN_TOP_MID, 0, 5);
    
    // Regen arc (large, centered)
    regen_arc = lv_arc_create(screens[SCREEN_REGEN]);
    lv_obj_set_size(regen_arc, 180, 180);
    lv_obj_center(regen_arc);
    lv_arc_set_rotation(regen_arc, 135);
    lv_arc_set_bg_angles(regen_arc, 0, 270);
    lv_arc_set_value(regen_arc, 0);
    lv_arc_set_range(regen_arc, -35, 0);  // -35% to 0%
    lv_obj_set_style_arc_width(regen_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(regen_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(regen_arc, lv_palette_darken(LV_PALETTE_GREY, 3), LV_PART_MAIN);
    lv_obj_set_style_arc_color(regen_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_remove_style(regen_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(regen_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Regen value (center)
    regen_value_label = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(regen_value_label, "0%");
    lv_obj_set_style_text_font(regen_value_label, &lv_font_montserrat_40, 0);  // Larger for clarity
    lv_obj_set_style_text_color(regen_value_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(regen_value_label, LV_ALIGN_CENTER, 0, 0);
    
    // Instruction
    lv_obj_t* inst = lv_label_create(screens[SCREEN_REGEN]);
    lv_label_set_text(inst, "Rotate to adjust\n-35% to 0%");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -15);
}

// ============================================================================
// WIFI SCREEN - WiFi status and connection info
// ============================================================================

void UIManager::createWiFiScreen() {
    screens[SCREEN_WIFI] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_WIFI], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(title, "WiFi CONFIG");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Status
    wifi_status_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_status_label, "Status: Inactive");
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_status_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(wifi_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // SSID
    wifi_ssid_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_ssid_label, "SSID:\nZombieVerter-Display");
    lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_ssid_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(wifi_ssid_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_ssid_label, LV_ALIGN_CENTER, 0, -20);
    
    // Password
    wifi_password_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_password_label, "Password:\nzombieverter");
    lv_obj_set_style_text_font(wifi_password_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_password_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(wifi_password_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_password_label, LV_ALIGN_CENTER, 0, 20);
    
    // IP Address
    wifi_ip_label = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(wifi_ip_label, "IP: ---");
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_ip_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    // Instruction
    lv_obj_t* inst = lv_label_create(screens[SCREEN_WIFI]);
    lv_label_set_text(inst, "Hold button to activate");
    lv_obj_set_style_text_font(inst, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(inst, lv_palette_darken(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ============================================================================
// SETTINGS SCREEN - System information
// ============================================================================

void UIManager::createSettingsScreen() {
    screens[SCREEN_SETTINGS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_SETTINGS], lv_color_black(), 0);
    
    // Title
    lv_obj_t* title = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(title, "SYSTEM INFO");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    
    // CAN status
    settings_can_status_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_can_status_label, "CAN: Connected");
    lv_obj_set_style_text_font(settings_can_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(settings_can_status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_align(settings_can_status_label, LV_ALIGN_TOP_MID, 0, 50);
    
    // Parameter count
    settings_param_count_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_param_count_label, "Parameters: 0");
    lv_obj_set_style_text_font(settings_param_count_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settings_param_count_label, lv_color_white(), 0);
    lv_obj_align(settings_param_count_label, LV_ALIGN_CENTER, 0, -10);
    
    // Version
    settings_version_label = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(settings_version_label, "Version: 1.1.0\nLVGL UI");
    lv_obj_set_style_text_font(settings_version_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settings_version_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_set_style_text_align(settings_version_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(settings_version_label, LV_ALIGN_CENTER, 0, 30);
    
    // Hardware info
    lv_obj_t* hw_info = lv_label_create(screens[SCREEN_SETTINGS]);
    lv_label_set_text(hw_info, "M5Stack Dial\nESP32-S3");
    lv_obj_set_style_text_font(hw_info, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hw_info, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(hw_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hw_info, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Continue to Part 3 for update functions...
/**
 * Complete UIManager.cpp Implementation - Part 3
 * Update functions for all screens with real CAN data
 */

// ============================================================================
// UPDATE FUNCTIONS - Connect CAN data to LVGL widgets
// ============================================================================

void UIManager::updateDashboard() {
    if (!canManager) return;
    
    // Update RPM (divide by 100 for x100 scale)
    CANParameter* rpm = canManager->getParameter(1);  // Motor RPM
    if (rpm) {
        int32_t value = rpm->getValueAsInt() / 100;  // Convert to x100 scale
        lv_meter_set_indicator_value(dash_rpm_meter, dash_rpm_needle, value);
        lv_meter_set_indicator_end_value(dash_rpm_meter, dash_rpm_arc, value);
    }
    
    // Update voltage
    CANParameter* voltage = canManager->getParameter(3);  // DC Voltage
    if (voltage) {
        int32_t value = voltage->getValueAsInt();  // Already in volts
        lv_label_set_text_fmt(dash_voltage_label, "%dV", value);
    }
    
    // Update power
    CANParameter* power = canManager->getParameter(2);  // Power
    if (power) {
        int32_t value = power->getValueAsInt();  // in 0.1kW
        lv_label_set_text_fmt(dash_power_label, "%d.%dkW", value / 10, abs(value % 10));
    }
    
    // Update SOC ring
    CANParameter* soc = canManager->getParameter(7);  // SOC
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_arc_set_value(dash_soc_arc, value);
        
        // Color code based on SOC
        if (value > 80) {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        } else if (value > 20) {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_arc_color(dash_soc_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
        }
    }
}

void UIManager::updatePower() {
    if (!canManager) return;
    
    // Update power meter
    CANParameter* power = canManager->getParameter(2);
    if (power) {
        int32_t value = power->getValueAsInt();  // in 0.1kW
        
        lv_meter_set_indicator_value(power_meter, power_needle, value);
        
        // Update active arc
        if (value >= 0) {
            lv_meter_set_indicator_start_value(power_meter, power_arc, 0);
            lv_meter_set_indicator_end_value(power_meter, power_arc, value);
        } else {
            lv_meter_set_indicator_start_value(power_meter, power_arc, value);
            lv_meter_set_indicator_end_value(power_meter, power_arc, 0);
        }
        
        // Update label
        lv_label_set_text_fmt(power_label, "%d.%d", value / 10, abs(value % 10));
        
        // Color code
        if (value < 0) {
            lv_obj_set_style_text_color(power_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (value < 500) {
            lv_obj_set_style_text_color(power_label, lv_palette_main(LV_PALETTE_CYAN), 0);
        } else if (value < 1000) {
            lv_obj_set_style_text_color(power_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_obj_set_style_text_color(power_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
    
    // Update voltage
    CANParameter* voltage = canManager->getParameter(3);
    if (voltage) {
        int32_t value = voltage->getValueAsInt();
        lv_label_set_text_fmt(power_voltage_label, "%d.%dV", value / 10, abs(value % 10));
    }
    
    // Update current
    CANParameter* current = canManager->getParameter(4);
    if (current) {
        int32_t value = current->getValueAsInt();
        lv_label_set_text_fmt(power_current_label, "%d.%dA", value / 10, abs(value % 10));
    }
    
    // Update SOC
    CANParameter* soc = canManager->getParameter(7);
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
    
    // Update motor temp
    CANParameter* motorTemp = canManager->getParameter(5);
    if (motorTemp) {
        int32_t value = motorTemp->getValueAsInt();
        lv_arc_set_value(temp_motor_arc, value);
        lv_label_set_text_fmt(temp_motor_label, "Motor\n%d°C", value);
        
        // Color code
        if (value < 60) {
            lv_obj_set_style_arc_color(temp_motor_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_motor_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (value < 80) {
            lv_obj_set_style_arc_color(temp_motor_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_motor_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_obj_set_style_arc_color(temp_motor_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_motor_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
    
    // Update inverter temp
    CANParameter* invTemp = canManager->getParameter(6);
    if (invTemp) {
        int32_t value = invTemp->getValueAsInt();
        lv_arc_set_value(temp_inverter_arc, value);
        lv_label_set_text_fmt(temp_inverter_label, "Inverter\n%d°C", value);
        
        // Color code
        if (value < 60) {
            lv_obj_set_style_arc_color(temp_inverter_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_inverter_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (value < 80) {
            lv_obj_set_style_arc_color(temp_inverter_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_inverter_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
        } else {
            lv_obj_set_style_arc_color(temp_inverter_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
            lv_obj_set_style_text_color(temp_inverter_label, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
    
    // Update battery temp (if available)
    CANParameter* batTemp = canManager->getParameter(14);  // Shunt temperature
    if (batTemp) {
        int32_t value = batTemp->getValueAsInt();
        lv_label_set_text_fmt(temp_battery_label, "Battery: %d°C", value / 10);
    }
}

void UIManager::updateBattery() {
    if (!canManager) return;
    
    // Update SOC
    CANParameter* soc = canManager->getParameter(7);
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_meter_set_indicator_value(battery_soc_meter, battery_soc_needle, value);
        lv_meter_set_indicator_end_value(battery_soc_meter, battery_soc_arc, value);
        lv_label_set_text_fmt(battery_soc_label, "%d", value);
        
        // Color code
        if (value > 80) {
            lv_obj_set_style_text_color(battery_soc_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_arc_color(battery_soc_meter, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
        } else if (value > 20) {
            lv_obj_set_style_text_color(battery_soc_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            lv_obj_set_style_arc_color(battery_soc_meter, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_text_color(battery_soc_label, lv_palette_main(LV_PALETTE_RED), 0);
            lv_obj_set_style_arc_color(battery_soc_meter, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
        }
    }
    
    // Update voltage
    CANParameter* voltage = canManager->getParameter(3);
    if (voltage) {
        int32_t value = voltage->getValueAsInt();
        lv_label_set_text_fmt(battery_voltage_label, "Voltage: %dV", value);
    }
    
    // Update current
    CANParameter* current = canManager->getParameter(4);
    if (current) {
        int32_t value = current->getValueAsInt();
        lv_label_set_text_fmt(battery_current_label, "Current: %d.%dA", value / 10, abs(value % 10));
    }
    
    // Update temperature
    CANParameter* temp = canManager->getParameter(14);
    if (temp) {
        int32_t value = temp->getValueAsInt();
        lv_label_set_text_fmt(battery_temp_label, "Temp: %d°C", value / 10);
    }
}

void UIManager::updateBMS() {
    if (!canManager) return;
    
    // Update cell voltages (if BMS data available)
    // These would come from BMS CAN messages (0x373, etc.)
    // For now, show static text since BMS integration is optional
    
    // Update SOC bar
    CANParameter* soc = canManager->getParameter(7);
    if (soc) {
        int32_t value = soc->getValueAsInt();
        lv_bar_set_value(bms_soc_bar, value, LV_ANIM_ON);
    }
}

void UIManager::updateGear() {
    if (!canManager) return;
    
    CANParameter* gear = canManager->getParameter(27);  // Gear parameter
    if (gear) {
        int32_t value = gear->getValueAsInt();
        const char* gearNames[] = {"LOW", "HIGH", "AUTO", "HI/LO"};
        
        if (value >= 0 && value < 4) {
            lv_label_set_text(gear_current_label, gearNames[value]);
            
            // Update indicators
            for (int i = 0; i < 4; i++) {
                if (i == value) {
                    lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_main(LV_PALETTE_CYAN), 0);
                    lv_obj_set_style_text_color(gear_option_labels[i], lv_color_white(), 0);
                } else {
                    lv_obj_set_style_bg_color(gear_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
                    lv_obj_set_style_text_color(gear_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
                }
            }
        }
    }
}

void UIManager::updateMotor() {
    if (!canManager) return;
    
    CANParameter* motor = canManager->getParameter(129);  // Motor Active parameter
    if (motor) {
        int32_t value = motor->getValueAsInt();
        const char* motorNames[] = {"MG1 only", "MG2 only", "MG1+MG2", "Blended"};
        
        if (value >= 0 && value < 4) {
            lv_label_set_text(motor_current_label, motorNames[value]);
            
            // Update indicators
            for (int i = 0; i < 4; i++) {
                if (i == value) {
                    lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_main(LV_PALETTE_ORANGE), 0);
                    lv_obj_set_style_text_color(motor_option_labels[i], lv_color_white(), 0);
                } else {
                    lv_obj_set_style_bg_color(motor_indicators[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
                    lv_obj_set_style_text_color(motor_option_labels[i], lv_palette_darken(LV_PALETTE_GREY, 1), 0);
                }
            }
        }
    }
}

void UIManager::updateRegen() {
    if (!canManager) return;
    
    CANParameter* regen = canManager->getParameter(61);  // Regen Max parameter
    if (regen) {
        int32_t value = regen->getValueAsInt();  // -35 to 0
        lv_arc_set_value(regen_arc, value);
        lv_label_set_text_fmt(regen_value_label, "%d%%", value);
        
        // Color code based on how much regen
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

// WiFi and Settings screens don't need real-time updates
// They update on screen entry or when WiFi state changes

// ============================================================================
// EDIT MODE CONTROL - For programmable screens
// ============================================================================

void UIManager::toggleEditMode() {
    if (isEditableScreen()) {
        editMode = !editMode;
        
        // Visual feedback - change title color when in edit mode
        lv_obj_t* screen = screens[currentScreen];
        if (screen) {
            // Find the title label (first child)
            lv_obj_t* title = lv_obj_get_child(screen, 0);
            if (title) {
                if (editMode) {
                    // Edit mode - bright orange title
                    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_ORANGE), 0);
                } else {
                    // Normal mode - white title
                    lv_obj_set_style_text_color(title, lv_color_white(), 0);
                }
            }
        }
    }
}

bool UIManager::isEditableScreen() {
    return (currentScreen == SCREEN_GEAR || 
            currentScreen == SCREEN_MOTOR || 
            currentScreen == SCREEN_REGEN);
}

// ============================================================================
// LOCK SCREEN - Immobilizer UI
// ============================================================================

void UIManager::createLockScreen() {
    screens[SCREEN_LOCK] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_LOCK], lv_color_black(), 0);
    
    // Lock icon (large padlock symbol)
    lock_icon = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_icon, "\xEF\x80\xA3");  // Unicode padlock (Font Awesome)
    lv_obj_set_style_text_font(lock_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lock_icon, LV_ALIGN_CENTER, 0, -60);
    
    // Title
    lock_title_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
    lv_obj_set_style_text_font(lock_title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lock_title_label, LV_ALIGN_CENTER, 0, -10);
    
    // PIN display
    lock_pin_display = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_pin_display, "_ _ _ _");
    lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lock_pin_display, lv_color_white(), 0);
    lv_obj_align(lock_pin_display, LV_ALIGN_CENTER, 0, 30);
    
    // Current digit
    lock_digit_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_digit_label, "0");
    lv_obj_set_style_text_font(lock_digit_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(lock_digit_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(lock_digit_label, LV_ALIGN_CENTER, 0, 75);
    
    // Instruction
    lock_instruction_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_instruction_label, "Rotate: Select  |  Click: Enter");
    lv_obj_set_style_text_font(lock_instruction_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lock_instruction_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_align(lock_instruction_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void UIManager::updateLockScreen() {
    if (!immobilizer) return;
    
    // Update PIN display
    uint8_t pinPos = immobilizer->getPINPosition();
    char pinDisplay[16];
    
    for (int i = 0; i < 4; i++) {
        if (i < pinPos) {
            pinDisplay[i * 2] = '*';
        } else {
            pinDisplay[i * 2] = '_';
        }
        pinDisplay[i * 2 + 1] = ' ';
    }
    pinDisplay[7] = '\0';
    
    lv_label_set_text(lock_pin_display, pinDisplay);
    lv_label_set_text_fmt(lock_digit_label, "%d", immobilizer->getCurrentDigit());
    
    if (immobilizer->isUnlocked()) {
        lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(lock_title_label, "UNLOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
}
