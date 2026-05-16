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
    SCREEN_CHARGING,       // Auto-shown when opmode == 3 (charge mode)
    SCREEN_HEALTH_CHECK,   // Pre-drive check on unlock
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
    void showLockPinPad();    // Reveal PIN pad (called on touch tap while locked)
    bool isLockPinPadVisible() const { return lockPinPadVisible; }
    
    // Edit mode control (for Gear, Motor, Regen screens)
    void toggleEditMode();
    bool isEditMode() { return editMode; }
    bool isEditableScreen();  // Check if current screen supports editing

    void updateWifiScreen(const String& ip);
    void showFetchStatus(const char* msg);
    void showWarning(const char* msg);
    void showSuccess(const char* msg);   // green timed overlay, 2 seconds
    void resetWifiScreen();
    void updateSettings();
    void showSystemInfo();                              // system info popup (4s)
    void setVersionInfo(const char* dialFW, const char* uiFW);  // called from setup()
    void reloadLogo();                                           // reload /logo.bin into live splash widget
    void scrollSettingsMenu(int delta);   // rotate encoder on settings screen
    int  getSettingsSelectedItem() const { return settings_selected_item; }
    int  getSettingsMenuCount()    const { return SETTINGS_MENU_COUNT; }
    uint32_t getSettingsArrivalTime() const { return settingsArrivalTime; }

    // Screen visibility mask — one bit per ScreenID; 1=enabled, 0=hidden from dial rotation
    // Dashboard (bit 2), WiFi (bit 10), Settings (bit 11) are always forced on.
    void     setScreenMask(uint16_t mask) { screenMask = mask | SCREEN_MASK_FORCED; }
    uint16_t getScreenMask()       const  { return screenMask; }
    static UIManager* getInstance()       { return instance; }
    
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
    void createChargingScreen();
    void createHealthCheckScreen();
    
    // Screen update functions
    void updateDashboard();
    void updatePower();
    void updateTemperature();
    void updateBattery();
    void updateBMS();
    void updateGear();
    void updateMotor();
    void updateRegen();
    void updateCharging();
    void updateHealthCheck();
    
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
    lv_obj_t* warningLabel = nullptr;
    uint32_t  warningExpiry = 0;
    lv_obj_t* wifi_status_label;
    
    // Settings screen widgets
    lv_obj_t* settings_can_status_label;
    lv_obj_t* settings_param_count_label;
    lv_obj_t* settings_version_label;
    // Settings menu items
    static const int SETTINGS_MENU_COUNT = 6;
    lv_obj_t* settings_menu_labels[6];
    lv_obj_t* settings_menu_indicators[6];
    int       settings_selected_item;      // currently highlighted item
    uint32_t  settingsArrivalTime;         // millis() when settings screen was entered

    // Charging screen widgets
    lv_obj_t* chg_title_label;
    lv_obj_t* chg_current_label;
    lv_obj_t* chg_voltage_label;
    lv_obj_t* chg_soc_arc;
    lv_meter_indicator_t* chg_soc_indicator;
    lv_obj_t* chg_soc_label;
    lv_obj_t* chg_eta_label;
    lv_obj_t* chg_energy_label;
    float     chg_voltageHistory[60];
    int       chg_historyHead;
    int       chg_historyCount;
    uint32_t  chg_sessionStart;
    float     chg_startSoc;

    // Health check screen widgets
    lv_obj_t* health_title_label;
    lv_obj_t* health_status_label;
    lv_obj_t* health_item_labels[8];
    int       health_item_count;

    // Dashboard efficiency widget
    lv_obj_t* dash_efficiency_label;

    // Splash screen members
    lv_obj_t*    splash_version_label = nullptr;
    lv_obj_t*    splash_fetch_label   = nullptr;   // updated by showFetchStatus()
    lv_obj_t*    splash_logo_img      = nullptr;   // lv_img widget (nullptr if no logo)
    lv_img_dsc_t splash_logo_dsc;                  // LVGL image descriptor
    uint8_t*     splash_logo_buf      = nullptr;   // heap buffer for raw RGB565 pixels
    
    // Data
    CANDataManager* canManager;
    Immobilizer* immobilizer;  // Security system
    ScreenID currentScreen;
    uint32_t lastUpdateTime;
    bool editMode;          // For programmable screens (Gear, Motor, Regen)
    bool lockPinPadVisible; // Lock screen: false=padlock view, true=PIN entry view
    
    // Version info — set from main.cpp via setVersionInfo()
    char dialFWVersion[16];
    char uiFWVersion[16];

    // Screen visibility bitmask — bit N = ScreenID N is navigable
    // Forced bits: DASHBOARD(2), WIFI(10), SETTINGS(11) always on
    static constexpr uint16_t SCREEN_MASK_FORCED = (1u << SCREEN_DASHBOARD) |
                                                    (1u << SCREEN_WIFI)      |
                                                    (1u << SCREEN_SETTINGS);
    static constexpr uint16_t SCREEN_MASK_DEFAULT = 0xFFFF;  // all on by default
    uint16_t screenMask = SCREEN_MASK_DEFAULT;

    // Static instance for callbacks
    static UIManager* instance;
};

#endif // UIMANAGER_LVGL_H
