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
uint32_t lastParamRequestTime = 0;
uint8_t currentParamIndex = 0;

// Fallback parameters — used only if SPIFFS params.json is missing or corrupt.
// IDs must match the openinverter/ZombieVerter param file exactly.
// Synthetic broadcast-only params (ID >= 100) are included so the UI has
// somewhere to store SOC/current/battemp from CAN broadcast frames.
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
// Input callbacks
// ============================================================================

void onEncoderRotate(int32_t delta) {
    #if DEBUG_SERIAL
    Serial.println("========================================");
    Serial.printf("ENCODER ROTATED: delta = %d\n", delta);
    Serial.printf("WiFi mode: %s\n", wifiMode ? "YES" : "NO");
    Serial.printf("Current screen: %d\n", uiManager.getCurrentScreen());
    Serial.println("========================================");
    #endif

    if (wifiMode) {
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.setScreen(SCREEN_DASHBOARD);
        return;
    }

    ScreenID currentScreen = uiManager.getCurrentScreen();

    if (currentScreen == SCREEN_GEAR) {
        CANParameter* gear = canManager.getParameter(27);
        #if DEBUG_SERIAL
        if (gear) Serial.printf("Gear param found, value=%d\n", gear->getValueAsInt());
        else      Serial.println("ERROR: Gear param 27 NOT FOUND");
        #endif
        if (gear) {
            int32_t newGear = gear->getValueAsInt();
            newGear = delta > 0 ? (newGear + 1) % 4 : (newGear - 1 + 4) % 4;
            #if DEBUG_SERIAL
            Serial.printf("Gear change -> %d\n", newGear);
            #endif
            canManager.setParameter(27, newGear);
        }

    } else if (currentScreen == SCREEN_MOTOR) {
        CANParameter* motor = canManager.getParameter(129);
        if (motor) {
            int32_t newMotor = motor->getValueAsInt();
            newMotor = delta > 0 ? (newMotor + 1) % 4 : (newMotor - 1 + 4) % 4;
            #if DEBUG_SERIAL
            Serial.printf("Motor change -> %d\n", newMotor);
            #endif
            canManager.setParameter(129, newMotor);
        }

    } else if (currentScreen == SCREEN_REGEN) {
        CANParameter* regen = canManager.getParameter(61);
        if (regen) {
            int32_t newRegen = regen->getValueAsInt();
            newRegen += delta > 0 ? 1 : -1;
            if (newRegen > 0)   newRegen = 0;
            if (newRegen < -35) newRegen = -35;
            #if DEBUG_SERIAL
            Serial.printf("Regen change -> %d\n", newRegen);
            #endif
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

    if (currentScreen == SCREEN_GEAR ||
        currentScreen == SCREEN_MOTOR ||
        currentScreen == SCREEN_REGEN) {
        uiManager.setScreen(uiManager.getNextScreen());
        #if DEBUG_SERIAL
        Serial.println("Button click - exiting control screen");
        #endif
        return;
    }

    if (!wifiMode) {
        wifiMode = true;
        wifiManager.startAP();
        uiManager.setScreen(SCREEN_WIFI);
        #if DEBUG_SERIAL
        Serial.println("WiFi mode enabled");
        Serial.printf("Connect to SSID: %s\n", WIFI_AP_SSID);
        #endif
    } else {
        wifiMode = false;
        wifiManager.stopAP();
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
        #if DEBUG_SERIAL
        Serial.println("WiFi mode disabled");
        #endif
    }
    uiManager.setScreen(SCREEN_DASHBOARD);
}

void onTouchTap(uint16_t x, uint16_t y) {
    ScreenID currentScreen = uiManager.getCurrentScreen();

    if (currentScreen == SCREEN_GEAR) {
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;
            if (option >= 0 && option <= 3) {
                #if DEBUG_SERIAL
                Serial.printf("Touch selected gear: %d\n", option);
                #endif
                canManager.setParameter(27, option);
            }
        }
    }

    if (currentScreen == SCREEN_MOTOR) {
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;
            if (option >= 0 && option <= 3) {
                #if DEBUG_SERIAL
                Serial.printf("Touch selected motor: %d\n", option);
                #endif
                canManager.setParameter(129, option);
            }
        }
    }

    if (currentScreen == SCREEN_REGEN) {
        CANParameter* regen = canManager.getParameter(61);
        if (regen) {
            int32_t newRegen = regen->getValueAsInt();
            newRegen += (x < SCREEN_CENTER_X) ? -5 : 5;
            if (newRegen > 0)   newRegen = 0;
            if (newRegen < -35) newRegen = -35;
            #if DEBUG_SERIAL
            Serial.printf("Touch adjusted regen -> %d\n", newRegen);
            #endif
            canManager.setParameter(61, newRegen);
        }
    }

    #if DEBUG_TOUCH
    Serial.printf("Tap at: %d, %d\n", x, y);
    #endif
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

    // Show splash for 2 seconds
    uiManager.init(&canManager);
    uint32_t splashStart = millis();
    while (millis() - splashStart < 2000) {
        lv_timer_handler();
        delay(5);
    }

    // Initialize CAN
    if (!canManager.init()) {
        #if DEBUG_SERIAL
        Serial.println("CAN init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("CAN initialized");
    #endif

    // Load sample parameters as baseline fallback
    if (canManager.loadParametersFromJSON(sampleParams)) {
        #if DEBUG_SERIAL
        Serial.printf("Loaded %d sample parameters\n", canManager.getParameterCount());
        #endif
    }

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

    // Mount SPIFFS and try to load params.json (overwrites sample params if successful)
    if (SPIFFS.begin(true)) {
        #if DEBUG_SERIAL
        Serial.println("SPIFFS mounted");
        Serial.printf("Total: %d bytes\n", SPIFFS.totalBytes());
        Serial.printf("Used: %d bytes\n",  SPIFFS.usedBytes());
        #endif

        if (SPIFFS.exists("/params.json")) {
            File paramFile = SPIFFS.open("/params.json", "r");
            if (paramFile) {
                size_t fileSize = paramFile.size();
                #if DEBUG_SERIAL
                Serial.printf("Found params.json (%d bytes)\n", fileSize);
                #endif
                if (fileSize > 0 && fileSize < MAX_JSON_SIZE) {
                    String jsonContent = paramFile.readString();
                    paramFile.close();
                    if (canManager.loadParametersFromJSON(jsonContent.c_str())) {
                        #if DEBUG_SERIAL
                        Serial.printf("Loaded %d parameters from SPIFFS\n", canManager.getParameterCount());
                        #endif
                    } else {
                        #if DEBUG_SERIAL
                        Serial.println("Failed to parse SPIFFS parameters, keeping sample params");
                        #endif
                    }
                } else {
                    paramFile.close();
                    #if DEBUG_SERIAL
                    Serial.println("SPIFFS parameters file invalid, keeping sample params");
                    #endif
                    SPIFFS.remove("/params.json");
                }
            }
        } else {
            #if DEBUG_SERIAL
            Serial.println("No params.json on SPIFFS, using sample params");
            #endif
        }
    } else {
        #if DEBUG_SERIAL
        Serial.println("SPIFFS mount failed, using sample params");
        #endif
    }

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
    }

    canManager.update();

    // Round-robin SDO parameter polling.
    // requestParameter() in CANData.cpp silently skips IDs >= 100 (broadcast-only).
    // No dirty check — params refresh continuously.
    if (millis() - lastParamRequestTime > PARAM_UPDATE_INTERVAL_MS) {
        lastParamRequestTime = millis();

        CANParameter* param = canManager.getParameterByIndex(currentParamIndex);
        if (param) {
            canManager.requestParameter(param->id);
            #if DEBUG_CAN
            Serial.printf("Requesting param %d (%s)\n", param->id, param->name);
            #endif
        }

        uint16_t paramCount = canManager.getParameterCount();
        if (paramCount > 0) {
            currentParamIndex = (currentParamIndex + 1) % paramCount;
        }
    }

    uiManager.update();

    delay(10);
}