#ifndef UIMANAGER_LVGL_H
#define UIMANAGER_LVGL_H

#include <Arduino.h>
#include <M5Unified.h>
#include <lvgl.h>
#include "CANData.h"
#include "Config.h"

// Forward declarations
class Immobilizer;

// Screen IDs
enum ScreenID {
    SCREEN_SPLASH = 0,
    SCREEN_LOCK,        // Immobilizer lock screen (shows when locked)
    SCREEN_DASHBOARD,
    SCREEN_POWER,
    SCREEN_TEMPERATURE,
    SCREEN_BATTERY,
    SCREEN_BMS,
    SCREEN_GEAR,
    SCREEN_MOTOR,
    SCREEN_REGEN,
    SCREEN_WIFI,
    SCREEN_SETTINGS,
    SCREEN_COUNT
};

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    bool init(CANDataManager* canMgr, Immobilizer* immob = nullptr);
    void update();
    void setScreen(ScreenID screen);
    ScreenID getCurrentScreen() { return currentScreen; }
    ScreenID getNextScreen();
    ScreenID getPreviousScreen();
    
    // Immobilizer integration
    void setImmobilizer(Immobilizer* immob) { immobilizer = immob; }
    void updateLockScreen();  // Update lock screen PIN display
    
    // Edit mode control (for Gear, Motor, Regen screens)
    void toggleEditMode();
    bool isEditMode() { return editMode; }
    bool isEditableScreen();  // Check if current screen supports editing
    
private:
    // LVGL Setup
    static void lvgl_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
    static void lvgl_encoder_read_cb(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
    void setupLVGL();
    
    // Screen creation functions
    void createSplashScreen();
    void createLockScreen();  // Immobilizer lock screen
    void createDashboardScreen();
    void createPowerScreen();
    void createTemperatureScreen();
    void createBatteryScreen();
    void createBMSScreen();
    void createGearScreen();
    void createMotorScreen();
    void createRegenScreen();
    void createWiFiScreen();
    void createSettingsScreen();
    
    // Screen update functions
    void updateDashboard();
    void updatePower();
    void updateTemperature();
    void updateBattery();
    void updateBMS();
    void updateGear();
    void updateMotor();
    void updateRegen();
    
    // Helper functions
    void clearAllScreens();
    lv_color_t getColorForValue(int32_t value, int32_t min_val, int32_t max_val);
    void setMeterValue(lv_obj_t* meter, lv_meter_indicator_t* indic, int32_t value, int32_t min_val, int32_t max_val);
    
    // LVGL Objects
    lv_disp_draw_buf_t draw_buf;
    lv_color_t *buf1;
    lv_color_t *buf2;
    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;
    
    // Screens
    lv_obj_t* screens[SCREEN_COUNT];
    
    // Dashboard widgets
    lv_obj_t* dash_rpm_meter;
    lv_meter_indicator_t* dash_rpm_needle;
    lv_meter_indicator_t* dash_rpm_arc;
    lv_obj_t* dash_rpm_label;
    lv_obj_t* dash_voltage_label;
    lv_obj_t* dash_power_label;
    lv_obj_t* dash_soc_arc;
    lv_meter_indicator_t* dash_soc_indicator;
    
    // Power screen widgets
    lv_obj_t* power_meter;
    lv_meter_indicator_t* power_needle;
    lv_meter_indicator_t* power_arc;
    lv_obj_t* power_label;
    lv_obj_t* power_voltage_label;
    lv_obj_t* power_current_label;
    lv_obj_t* power_soc_label;
    
    // Temperature screen widgets
    lv_obj_t* temp_motor_arc;
    lv_meter_indicator_t* temp_motor_indicator;
    lv_obj_t* temp_motor_label;
    lv_obj_t* temp_inverter_arc;
    lv_meter_indicator_t* temp_inverter_indicator;
    lv_obj_t* temp_inverter_label;
    lv_obj_t* temp_battery_label;
    
    // Battery screen widgets
    lv_obj_t* battery_soc_meter;
    lv_meter_indicator_t* battery_soc_needle;
    lv_meter_indicator_t* battery_soc_arc;
    lv_obj_t* battery_soc_label;
    lv_obj_t* battery_voltage_label;
    lv_obj_t* battery_current_label;
    lv_obj_t* battery_temp_label;
    
    // BMS screen widgets
    lv_obj_t* bms_cell_max_label;
    lv_obj_t* bms_cell_min_label;
    lv_obj_t* bms_cell_delta_label;
    lv_obj_t* bms_temp_max_label;
    lv_obj_t* bms_soc_bar;
    
    // Gear screen widgets
    lv_obj_t* gear_current_label;
    lv_obj_t* gear_option_labels[4];
    lv_obj_t* gear_indicators[4];
    
    // Motor screen widgets
    lv_obj_t* motor_current_label;
    lv_obj_t* motor_option_labels[4];
    lv_obj_t* motor_indicators[4];
    
    // Lock screen widgets
    lv_obj_t* lock_title_label;
    lv_obj_t* lock_status_label;
    lv_obj_t* lock_pin_display;
    lv_obj_t* lock_digit_label;
    lv_obj_t* lock_instruction_label;
    lv_obj_t* lock_icon;
    
    // Regen screen widgets
    lv_obj_t* regen_arc;
    lv_meter_indicator_t* regen_indicator;
    lv_obj_t* regen_value_label;
    lv_obj_t* regen_title_label;
    
    // WiFi screen widgets
    lv_obj_t* wifi_ssid_label;
    lv_obj_t* wifi_password_label;
    lv_obj_t* wifi_ip_label;
    lv_obj_t* wifi_status_label;
    
    // Settings screen widgets
    lv_obj_t* settings_can_status_label;
    lv_obj_t* settings_param_count_label;
    lv_obj_t* settings_version_label;
    
    // Data
    CANDataManager* canManager;
    Immobilizer* immobilizer;  // Security system
    ScreenID currentScreen;
    uint32_t lastUpdateTime;
    bool editMode;  // For programmable screens (Gear, Motor, Regen)
    
    // Static instance for callbacks
    static UIManager* instance;
};

#endif // UIMANAGER_LVGL_H
