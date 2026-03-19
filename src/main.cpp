#include <Arduino.h>
#include <SPIFFS.h>
#include "Config.h"
#include "Hardware.h"
#include "CANData.h"
#include "InputManager.h"
#include "UIManager.h"
#include "WiFiManager.h"

// Global objects
CANDataManager canManager;
InputManager inputManager;
UIManager uiManager;
WiFiManager wifiManager;

// State tracking
bool systemReady = false;
bool wifiMode = false;
bool lvglSuspended = false;
uint32_t lastParamRequestTime = 0;
uint8_t currentParamIndex = 0;

// Fallback parameters — used only if SPIFFS params.json is missing or corrupt.
const char* sampleParams = R"(
{
  "parameters": [
    {"id": 2,   "name": "speed",     "type": "int16", "unit": "rpm", "min": 0,    "max": 6000, "decimals": 0, "editable": false},
    {"id": 3,   "name": "udc",       "type": "int16", "unit": "V",   "min": 0,    "max": 400,  "decimals": 0, "editable": false},
    {"id": 7,   "name": "tmphs",     "type": "int16", "unit": "C",   "min": 0,    "max": 150,  "decimals": 0, "editable": false},
    {"id": 8,   "name": "tmpm",      "type": "int16", "unit": "C",   "min": 0,    "max": 150,  "decimals": 0, "editable": false},
    {"id": 27,  "name": "gear",      "type": "int16", "unit": "",    "min": 0,    "max": 3,    "decimals": 0, "editable": true},
    {"id": 61,  "name": "regenmax",  "type": "int16", "unit": "%",   "min": -35,  "max": 0,    "decimals": 0, "editable": true},
    {"id": 129, "name": "motactive", "type": "int16", "unit": "",    "min": 0,    "max": 3,    "decimals": 0, "editable": true},
    {"id": 100, "name": "soc",       "type": "int16", "unit": "%",   "min": 0,    "max": 100,  "decimals": 0, "editable": false},
    {"id": 101, "name": "idc",       "type": "int16", "unit": "A",   "min": -500, "max": 500,  "decimals": 0, "editable": false},
    {"id": 102, "name": "battemp",   "type": "int16", "unit": "C",   "min": -40,  "max": 80,   "decimals": 0, "editable": false}
  ]
}
)";

// ============================================================================
// WiFi mode helpers
// ============================================================================

void suspendLVGL() {
    lvglSuspended = true;
    #if DEBUG_SERIAL
    Serial.println("[UI] LVGL suspended for WiFi mode");
    #endif
}

void resumeLVGL() {
    lvglSuspended = false;
    // Force LVGL to redraw everything from scratch
    lv_obj_invalidate(lv_scr_act());
    #if DEBUG_SERIAL
    Serial.println("[UI] LVGL resumed");
    #endif
}

// ============================================================================
// Input callbacks
// ============================================================================

void onEncoderRotate(int32_t delta) {
    if (wifiMode) {
        // Any rotation exits WiFi mode
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        return;
    }

    ScreenID currentScreen = uiManager.getCurrentScreen();

    if (currentScreen == SCREEN_GEAR) {
        if (!uiManager.isEditMode()) {
            uiManager.setScreen(delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen());
            return;
        }
        CANParameter* gear = canManager.getParameter(27);
        if (gear) {
            int32_t newGear = gear->getValueAsInt();
            newGear = delta > 0 ? (newGear + 1) % 4 : (newGear - 1 + 4) % 4;
            canManager.setParameter(27, newGear);
        }
    } else if (currentScreen == SCREEN_MOTOR) {
        if (!uiManager.isEditMode()) {
            uiManager.setScreen(delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen());
            return;
        }
        CANParameter* motor = canManager.getParameter(129);
        if (motor) {
            int32_t newMotor = motor->getValueAsInt();
            newMotor = delta > 0 ? (newMotor + 1) % 4 : (newMotor - 1 + 4) % 4;
            canManager.setParameter(129, newMotor);
        }
    } else if (currentScreen == SCREEN_REGEN) {
        if (!uiManager.isEditMode()) {
            uiManager.setScreen(delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen());
            return;
        }
        CANParameter* regen = canManager.getParameter(61);
        if (regen) {
            int32_t newRegen = regen->getValueAsInt();
            newRegen += delta > 0 ? 1 : -1;
            if (newRegen > 0)   newRegen = 0;
            if (newRegen < -35) newRegen = -35;
            canManager.setParameter(61, newRegen);
        }
    } else {
        if (delta > 0) {
            uiManager.setScreen(uiManager.getNextScreen());
        } else {
            uiManager.setScreen(uiManager.getPreviousScreen());
        }
    }
}

void onButtonClick() {
    ScreenID currentScreen = uiManager.getCurrentScreen();

    // On editable screens: first click enters edit mode, second click exits
    if (currentScreen == SCREEN_GEAR ||
        currentScreen == SCREEN_MOTOR ||
        currentScreen == SCREEN_REGEN) {
        if (uiManager.isEditMode()) {
            uiManager.toggleEditMode();  // exit edit mode
        } else {
            uiManager.toggleEditMode();  // enter edit mode
        }
        return;
    }

    if (!wifiMode) {
        wifiMode = true;
        wifiManager.startAP();
        // Update WiFi screen with IP while LVGL still running
        uiManager.setScreen(SCREEN_WIFI);
        uiManager.updateWifiScreen(wifiManager.getIPAddress());
        for (int i = 0; i < 10; i++) { lv_timer_handler(); delay(10); }
        lvglSuspended = true;
        #if DEBUG_SERIAL
        Serial.println("WiFi mode enabled");
        #endif
    } else {
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        #if DEBUG_SERIAL
        Serial.println("WiFi mode disabled");
        #endif
    }
}

void onButtonDoubleClick() {
    // Reserved
}

void onButtonLongPress() {
    if (wifiMode) {
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.resetWifiScreen();
        resumeLVGL();
    }
    uiManager.setScreen(SCREEN_DASHBOARD);
}

void onTouchTap(uint16_t x, uint16_t y) {
    if (wifiMode) {
        // Touch exits WiFi mode
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        return;
    }

    ScreenID currentScreen = uiManager.getCurrentScreen();

    if (currentScreen == SCREEN_GEAR) {
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;
            if (option >= 0 && option <= 3)
                canManager.setParameter(27, option);
        }
    }

    if (currentScreen == SCREEN_MOTOR) {
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;
            if (option >= 0 && option <= 3)
                canManager.setParameter(129, option);
        }
    }

    if (currentScreen == SCREEN_REGEN) {
        CANParameter* regen = canManager.getParameter(61);
        if (regen) {
            int32_t newRegen = regen->getValueAsInt();
            newRegen += (x < SCREEN_CENTER_X) ? -5 : 5;
            if (newRegen > 0)   newRegen = 0;
            if (newRegen < -35) newRegen = -35;
            canManager.setParameter(61, newRegen);
        }
    }
}

// ============================================================================
// setup
// ============================================================================

void setup() {
    #if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ZombieVerter Display - M5Stack Dial");
    Serial.println("====================================");
    #endif

    if (!Hardware::init()) {
        #if DEBUG_SERIAL
        Serial.println("Hardware init failed!");
        #endif
        while (1) delay(100);
    }
    #if DEBUG_SERIAL
    Serial.println("Hardware initialized");
    #endif

    uiManager.init(&canManager);
    uint32_t splashStart = millis();
    while (millis() - splashStart < 2000) {
        lv_timer_handler();
        delay(5);
    }

    if (!canManager.init()) {
        #if DEBUG_SERIAL
        Serial.println("CAN init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("CAN initialized");
    #endif

    // Load sample params as placeholder while we attempt VCU fetch
    canManager.loadParametersFromJSON(sampleParams);

    if (!inputManager.init()) {
        #if DEBUG_SERIAL
        Serial.println("Input init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("Input initialized");
    #endif

    if (!wifiManager.init(&canManager)) {
        #if DEBUG_SERIAL
        Serial.println("WiFi manager init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("WiFi manager initialized");
    #endif

    wifiMode = false;
    lvglSuspended = false;

    SPIFFS.begin(true);

    // -----------------------------------------------------------------------
    // Parameter loading — try in order:
    //   1. Fetch live from VCU via SDO (always up to date)
    //   2. Load from SPIFFS params.json (cached from previous fetch)
    //   3. Keep sample params (basic functionality only)
    // -----------------------------------------------------------------------
    bool paramsLoaded = false;

    Serial.println("Attempting to fetch parameters from VCU...");
    // Show a "Fetching..." message on splash screen
    uiManager.showFetchStatus("Fetching params\nfrom VCU...");
    for (int i = 0; i < 3; i++) { lv_timer_handler(); delay(10); }

    FetchResult fetchResult = canManager.fetchParamsFromVCU();

    if (fetchResult == FetchResult::SUCCESS) {
        Serial.printf("Fetched %d parameters from VCU\n", canManager.getParameterCount());
        paramsLoaded = true;
        uiManager.showFetchStatus("VCU params loaded!");
    } else {
        Serial.printf("VCU fetch failed (%d), trying SPIFFS...\n", (int)fetchResult);
        uiManager.showFetchStatus("VCU unavailable\nLoading cached...");

        if (SPIFFS.exists("/params.json")) {
            File paramFile = SPIFFS.open("/params.json", "r");
            if (paramFile) {
                size_t fileSize = paramFile.size();
                if (fileSize > 0 && fileSize < MAX_JSON_SIZE) {
                    String jsonContent = paramFile.readString();
                    paramFile.close();
                    if (canManager.loadParametersFromJSON(jsonContent.c_str())) {
                        Serial.printf("Loaded %d parameters from SPIFFS\n", canManager.getParameterCount());
                        paramsLoaded = true;
                        uiManager.showFetchStatus("Cached params\nloaded OK");
                    }
                } else {
                    paramFile.close();
                    SPIFFS.remove("/params.json");
                }
            }
        }

        if (!paramsLoaded) {
            Serial.println("Using sample parameters only");
            uiManager.showFetchStatus("Using defaults\nConnect VCU!");
        }
    }

    // Start SDO manager AFTER fetch so it doesn't consume our response frames
    canManager.initSDO();

    // Brief pause so user can see the status message
    for (int i = 0; i < 100; i++) { lv_timer_handler(); delay(10); }

    inputManager.setOnEncoderRotate(onEncoderRotate);
    inputManager.setOnButtonClick(onButtonClick);
    inputManager.setOnButtonDoubleClick(onButtonDoubleClick);
    inputManager.setOnButtonLongPress(onButtonLongPress);
    inputManager.setOnTouchTap(onTouchTap);

    uiManager.setScreen(SCREEN_DASHBOARD);
    systemReady = true;

    #if DEBUG_SERIAL
    Serial.println("System ready!");
    Serial.println("====================================");
    Serial.println("Controls:");
    Serial.println("  Rotate: Switch screens");
    Serial.println("  Click: Toggle WiFi mode");
    Serial.println("  Double-click: (Reserved)");
    Serial.println("  Long-press: Back to Dashboard");
    Serial.println("====================================");
    Serial.println("WiFi Mode:");
    Serial.println("  Click button to enable WiFi AP");
    Serial.println("  Connect to: " WIFI_AP_SSID);
    Serial.println("  Password: " WIFI_AP_PASSWORD);
    Serial.println("  Browse to: 192.168.4.1");
    Serial.println("====================================");
    #endif
}

// ============================================================================
// loop
// ============================================================================

void loop() {
    if (!systemReady) return;

    Hardware::update();
    inputManager.update();

    if (wifiMode) {
        wifiManager.update();
        canManager.update();

        // Keep SDO polling running in WiFi mode so spot values stay live
        if (millis() - lastParamRequestTime > PARAM_UPDATE_INTERVAL_MS) {
            lastParamRequestTime = millis();
            CANParameter* param = canManager.getParameterByIndex(currentParamIndex);
            if (param) {
                canManager.requestParameter(param->id);
            }
            uint16_t paramCount = canManager.getParameterCount();
            if (paramCount > 0) {
                currentParamIndex = (currentParamIndex + 1) % paramCount;
            }
        }

        delay(10);
        return;
    }

    // Normal mode: full CAN + LVGL update
    canManager.update();

    // Round-robin SDO parameter polling
    if (millis() - lastParamRequestTime > PARAM_UPDATE_INTERVAL_MS) {
        lastParamRequestTime = millis();
        CANParameter* param = canManager.getParameterByIndex(currentParamIndex);
        if (param) {
            canManager.requestParameter(param->id);
        }
        uint16_t paramCount = canManager.getParameterCount();
        if (paramCount > 0) {
            currentParamIndex = (currentParamIndex + 1) % paramCount;
        }
    }

    if (!lvglSuspended) {
        uiManager.update();
    }

    delay(10);
}