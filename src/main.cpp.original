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

// Sample parameters JSON (this would normally be loaded from SPIFFS)
const char* sampleParams = R"(
{
  "parameters": [
    {"id": 1, "name": "Speed", "type": "int16", "unit": "rpm", "min": 0, "max": 6000, "decimals": 0, "editable": false},
    {"id": 2, "name": "Power", "type": "int16", "unit": "kW", "min": 0, "max": 100, "decimals": 1, "editable": false},
    {"id": 3, "name": "Voltage", "type": "int16", "unit": "V", "min": 0, "max": 400, "decimals": 0, "editable": false},
    {"id": 4, "name": "Current", "type": "int16", "unit": "A", "min": -200, "max": 200, "decimals": 0, "editable": false},
    {"id": 5, "name": "Motor Temp", "type": "int16", "unit": "°C", "min": 0, "max": 150, "decimals": 0, "editable": false},
    {"id": 6, "name": "Inverter Temp", "type": "int16", "unit": "°C", "min": 0, "max": 150, "decimals": 0, "editable": false},
    {"id": 7, "name": "Battery SOC", "type": "uint8", "unit": "%", "min": 0, "max": 100, "decimals": 0, "editable": false},
    {"id": 8, "name": "Max Current", "type": "int16", "unit": "A", "min": 50, "max": 500, "decimals": 0, "editable": true},
    {"id": 9, "name": "Throttle", "type": "uint16", "unit": "%", "min": 0, "max": 100, "decimals": 0, "editable": false},
    {"id": 10, "name": "Brake Pressure", "type": "uint16", "unit": "%", "min": 0, "max": 100, "decimals": 0, "editable": false},
    {"id": 11, "name": "Torque Req", "type": "int16", "unit": "Nm", "min": -300, "max": 300, "decimals": 0, "editable": false},
    {"id": 12, "name": "Torque Act", "type": "int16", "unit": "Nm", "min": -300, "max": 300, "decimals": 0, "editable": false},
    {"id": 13, "name": "Vehicle Speed", "type": "uint16", "unit": "km/h", "min": 0, "max": 200, "decimals": 0, "editable": false},
    {"id": 14, "name": "Coolant Temp", "type": "int16", "unit": "°C", "min": 0, "max": 120, "decimals": 0, "editable": false},
    {"id": 15, "name": "Efficiency", "type": "uint8", "unit": "%", "min": 0, "max": 100, "decimals": 0, "editable": false},
    {"id": 20, "name": "Max Cell Voltage", "type": "uint16", "unit": "mV", "min": 2500, "max": 4200, "decimals": 0, "editable": false, "category": "BMS"},
    {"id": 21, "name": "Min Cell Voltage", "type": "uint16", "unit": "mV", "min": 2500, "max": 4200, "decimals": 0, "editable": false, "category": "BMS"},
    {"id": 22, "name": "Avg Cell Voltage", "type": "uint16", "unit": "mV", "min": 2500, "max": 4200, "decimals": 0, "editable": false, "category": "BMS"},
    {"id": 23, "name": "Delta Cell Voltage", "type": "uint16", "unit": "mV", "min": 0, "max": 500, "decimals": 0, "editable": false, "category": "BMS"},
    {"id": 24, "name": "Avg Cell Temp", "type": "int16", "unit": "°C", "min": -20, "max": 80, "decimals": 0, "editable": false, "category": "BMS"}
  ]
}
)";

// Input callbacks
void onEncoderRotate(int32_t delta) {
    #if DEBUG_SERIAL
    Serial.println("========================================");
    Serial.printf("ENCODER ROTATED: delta = %d\n", delta);
    Serial.printf("WiFi mode: %s\n", wifiMode ? "YES" : "NO");
    Serial.printf("Current screen: %d\n", uiManager.getCurrentScreen());
    Serial.println("========================================");
    #endif
    
    if (wifiMode) {
        // Exit WiFi mode
        wifiMode = false;
        wifiManager.stopAP();
        uiManager.setScreen(SCREEN_DASHBOARD);
        #if DEBUG_SERIAL
        Serial.println("WiFi mode disabled");
        #endif
    } else {
        // Check if we're on a control screen
        ScreenID currentScreen = uiManager.getCurrentScreen();
        
        #if DEBUG_SERIAL
        Serial.printf("Checking screen type: %d\n", currentScreen);
        Serial.printf("SCREEN_GEAR = %d\n", SCREEN_GEAR);
        Serial.printf("SCREEN_MOTOR = %d\n", SCREEN_MOTOR);
        Serial.printf("SCREEN_REGEN = %d\n", SCREEN_REGEN);
        if (currentScreen == SCREEN_GEAR) {
            Serial.println(">>> ON GEAR SCREEN - should adjust gear!");
        } else if (currentScreen == SCREEN_MOTOR) {
            Serial.println(">>> ON MOTOR SCREEN - should adjust motor!");
        } else if (currentScreen == SCREEN_REGEN) {
            Serial.println(">>> ON REGEN SCREEN - should adjust regen!");
        } else {
            Serial.println(">>> ON OTHER SCREEN - should switch screens");
        }
        #endif
        
        if (currentScreen == SCREEN_GEAR) {
            // Change gear
            CANParameter* gear = canManager.getParameter(27);
            
            #if DEBUG_SERIAL
            Serial.printf("Looking for parameter 27 (Gear)...\n");
            if (gear) {
                Serial.printf(">>> FOUND parameter 27!\n");
                Serial.printf("    Current value: %d\n", gear->getValueAsInt());
            } else {
                Serial.printf(">>> ERROR: Parameter 27 NOT FOUND!\n");
                Serial.printf("    Total parameters loaded: %d\n", canManager.getParameterCount());
            }
            #endif
            
            if (gear) {
                int32_t currentGear = gear->getValueAsInt();
                int32_t newGear = currentGear;
                
                if (delta > 0) {
                    newGear = (currentGear + 1) % 4;  // Cycle 0-3
                } else {
                    newGear = (currentGear - 1 + 4) % 4;  // Cycle backwards
                }
                
                #if DEBUG_SERIAL
                Serial.println("========================================");
                Serial.printf("GEAR CHANGE REQUESTED\n");
                Serial.printf("Current gear: %d -> New gear: %d\n", currentGear, newGear);
                Serial.printf("Sending SDO Write to param 27...\n");
                Serial.println("========================================");
                #endif
                
                canManager.setParameter(27, newGear);
            }
        } else if (currentScreen == SCREEN_MOTOR) {
            // Change motor mode
            CANParameter* motor = canManager.getParameter(129);
            if (motor) {
                int32_t currentMotor = motor->getValueAsInt();
                int32_t newMotor = currentMotor;
                
                if (delta > 0) {
                    newMotor = (currentMotor + 1) % 4;  // Cycle 0-3
                } else {
                    newMotor = (currentMotor - 1 + 4) % 4;  // Cycle backwards
                }
                
                #if DEBUG_SERIAL
                Serial.printf("Motor change: %d -> %d\n", currentMotor, newMotor);
                #endif
                
                canManager.setParameter(129, newMotor);
            }
        } else if (currentScreen == SCREEN_REGEN) {
            // Adjust regen - rotate changes by 1% increments
            CANParameter* regen = canManager.getParameter(61);
            if (regen) {
                int32_t currentRegen = regen->getValueAsInt();
                int32_t newRegen = currentRegen;
                
                if (delta > 0) {
                    newRegen = currentRegen + 1;  // Increase (less negative)
                    if (newRegen > 0) newRegen = 0;  // Max is 0
                } else {
                    newRegen = currentRegen - 1;  // Decrease (more negative)
                    if (newRegen < -35) newRegen = -35;  // Min is -35
                }
                
                #if DEBUG_SERIAL
                Serial.printf("Regen change: %d -> %d\n", currentRegen, newRegen);
                #endif
                
                canManager.setParameter(61, newRegen);
            }
        } else {
            // Normal screen navigation
            if (delta > 0) {
                uiManager.setScreen(uiManager.getNextScreen());
            } else {
                uiManager.setScreen(uiManager.getPreviousScreen());
            }
        }
    }
}

void onButtonClick() {
    ScreenID currentScreen = uiManager.getCurrentScreen();
    
    // If on a control screen (GEAR, MOTOR, REGEN), click exits to next screen
    if (currentScreen == SCREEN_GEAR || currentScreen == SCREEN_MOTOR || currentScreen == SCREEN_REGEN) {
        uiManager.setScreen(uiManager.getNextScreen());
        #if DEBUG_SERIAL
        Serial.println("Button click - exiting control screen");
        #endif
        return;
    }
    
    // Otherwise, toggle WiFi mode
    if (!wifiMode) {
        wifiMode = true;
        wifiManager.startAP();
        uiManager.setScreen(SCREEN_WIFI);
        #if DEBUG_SERIAL
        Serial.println("WiFi mode enabled");
        Serial.print("Connect to SSID: ");
        Serial.println(WIFI_AP_SSID);
        Serial.print("IP: ");
        Serial.println(wifiManager.getIPAddress());
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
    // Reserved for future use
}

void onButtonLongPress() {
    // Back to dashboard (works in any mode)
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
    
    // Gear screen - tap to select gear directly
    if (currentScreen == SCREEN_GEAR) {
        // Touch zones for 4 gear options
        // Screen is 240x240, options displayed at y ~170-236 (bottom area)
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;  // 22 pixels per option
            if (option >= 0 && option <= 3) {
                #if DEBUG_SERIAL
                Serial.printf("Touch selected gear: %d\n", option);
                #endif
                canManager.setParameter(27, option);
            }
        }
    }
    
    // Motor screen - tap to select motor mode directly
    if (currentScreen == SCREEN_MOTOR) {
        // Touch zones for 4 motor options (same layout as gear)
        if (y >= 170 && y <= 236) {
            int option = (y - 170) / 22;  // 22 pixels per option
            if (option >= 0 && option <= 3) {
                #if DEBUG_SERIAL
                Serial.printf("Touch selected motor: %d\n", option);
                #endif
                canManager.setParameter(129, option);
            }
        }
    }
    
    // Regen screen - tap left/right for adjustment
    if (currentScreen == SCREEN_REGEN) {
        CANParameter* regen = canManager.getParameter(61);
        if (regen) {
            int32_t currentRegen = regen->getValueAsInt();
            int32_t newRegen = currentRegen;
            
            // Left half = decrease (more regen), right half = increase (less regen)
            if (x < SCREEN_CENTER_X) {
                // Left tap - decrease by 5 (more negative = stronger regen)
                newRegen = currentRegen - 5;
                if (newRegen < -35) newRegen = -35;
            } else {
                // Right tap - increase by 5 (less negative = lighter regen)
                newRegen = currentRegen + 5;
                if (newRegen > 0) newRegen = 0;
            }
            
            #if DEBUG_SERIAL
            Serial.printf("Touch adjusted regen: %d -> %d\n", currentRegen, newRegen);
            #endif
            
            canManager.setParameter(61, newRegen);
        }
    }
    
    #if DEBUG_TOUCH
    Serial.printf("Tap at: %d, %d\n", x, y);
    #endif
}

void setup() {
    #if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ZombieVerter Display - M5Stack Dial");
    Serial.println("====================================");
    #endif
    
    // Initialize hardware
    if (!Hardware::init()) {
        #if DEBUG_SERIAL
        Serial.println("Hardware init failed!");
        #endif
        while (1) delay(100);
    }
    #if DEBUG_SERIAL
    Serial.println("Hardware initialized");
    #endif
    
    // Show splash screen for 2 seconds
    uiManager.init(&canManager);
    
    uint32_t splashStart = millis();
    while (millis() - splashStart < 2000) {  // 2 seconds
        lv_timer_handler();  // Update LVGL display
        delay(5);
    }
    
    // Initialize CAN
    if (!canManager.init()) {
        #if DEBUG_SERIAL
        Serial.println("CAN init failed!");
        #endif
        // Continue anyway - will show disconnected
    }
    #if DEBUG_SERIAL
    Serial.println("CAN initialized");
    #endif
    
    // Load parameters
    if (canManager.loadParametersFromJSON(sampleParams)) {
        #if DEBUG_SERIAL
        Serial.printf("Loaded %d parameters\n", canManager.getParameterCount());
        #endif
    }
    
    // Initialize input
    if (!inputManager.init()) {
        #if DEBUG_SERIAL
        Serial.println("Input init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("Input initialized");
    #endif
    
    // Initialize WiFi manager
    if (!wifiManager.init(&canManager)) {
        #if DEBUG_SERIAL
        Serial.println("WiFi manager init failed!");
        #endif
    }
    #if DEBUG_SERIAL
    Serial.println("WiFi manager initialized");
    #endif
    
    // WiFi is DISABLED at startup - use button to enable if needed
    #if DEBUG_SERIAL
    Serial.println("========================================");
    Serial.println("WiFi DISABLED at startup");
    Serial.println("Press button to enable WiFi mode");
    Serial.println("========================================");
    #endif
    
    wifiMode = false;  // Start with WiFi OFF
    // Do NOT start WiFi automatically
    // uiManager.setScreen will be set below after loading parameters
    
    // Try to load parameters from SPIFFS
    if (SPIFFS.exists("/params.json")) {
        File paramFile = SPIFFS.open("/params.json", "r");
        if (paramFile) {
            size_t fileSize = paramFile.size();
            
            #if DEBUG_SERIAL
            Serial.printf("Found params.json (%d bytes)\n", fileSize);
            #endif
            
            // Check file size is reasonable
            if (fileSize > 0 && fileSize < MAX_JSON_SIZE) {
                String jsonContent = paramFile.readString();
                paramFile.close();
                
                if (canManager.loadParametersFromJSON(jsonContent.c_str())) {
                    #if DEBUG_SERIAL
                    Serial.printf("Loaded %d parameters from SPIFFS\n", canManager.getParameterCount());
                    #endif
                } else {
                    #if DEBUG_SERIAL
                    Serial.println("Failed to parse saved parameters, using defaults");
                    #endif
                    // Load defaults on parse failure
                    canManager.loadParametersFromJSON(sampleParams);
                }
            } else {
                paramFile.close();
                #if DEBUG_SERIAL
                Serial.println("Saved parameters file invalid, using defaults");
                #endif
                // Delete corrupted file
                SPIFFS.remove("/params.json");
                canManager.loadParametersFromJSON(sampleParams);
            }
        } else {
            #if DEBUG_SERIAL
            Serial.println("Failed to open saved parameters, using defaults");
            #endif
            canManager.loadParametersFromJSON(sampleParams);
        }
    } else {
        #if DEBUG_SERIAL
        Serial.println("No saved parameters, using defaults");
        #endif
        canManager.loadParametersFromJSON(sampleParams);
    }
    
    // Set input callbacks
    inputManager.setOnEncoderRotate(onEncoderRotate);
    inputManager.setOnButtonClick(onButtonClick);
    inputManager.setOnButtonDoubleClick(onButtonDoubleClick);
    inputManager.setOnButtonLongPress(onButtonLongPress);
    inputManager.setOnTouchTap(onTouchTap);
    
    // Switch to dashboard
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
    Serial.println("Screens:");
    Serial.println("  1. Dashboard - Main overview");
    Serial.println("  2. Power - Power metrics");
    Serial.println("  3. Temperature - Motor/inverter temps");
    Serial.println("  4. Battery - SOC and voltage");
    Serial.println("  5. BMS - Cell voltages & temps");
    Serial.println("  6. WiFi - Upload parameters");
    Serial.println("  7. Settings - Configuration");
    Serial.println("====================================");
    Serial.println("WiFi Mode:");
    Serial.println("  Click button to enable WiFi AP");
    Serial.println("  Connect to: " WIFI_AP_SSID);
    Serial.println("  Password: " WIFI_AP_PASSWORD);
    Serial.println("  Browse to: 192.168.4.1");
    Serial.println("  Upload your params.json file");
    Serial.println("  Rotate dial to exit WiFi mode");
    Serial.println("====================================");
    #endif
}

void loop() {
    if (!systemReady) return;
    
    // Update hardware
    Hardware::update();
    
    // Update input
    inputManager.update();
    
    // Update WiFi if in WiFi mode
    if (wifiMode) {
        wifiManager.update();
    }
    
    // Update CAN and track last ID received
    static uint32_t lastCanID = 0;
    static bool hadConnection = false;
    canManager.update();
    
    // Check if CAN connection status changed and update UI with last CAN ID
    bool nowConnected = canManager.isConnected();
    if (nowConnected != hadConnection || nowConnected) {
        // For debugging: assume we're getting SOME CAN traffic
        // We'll update with a placeholder - real implementation would track in CAN manager
        if (nowConnected) {
            // Just show that we're connected - actual CAN ID tracking needs more work
            // For now, this will at least show connection status
        }
        hadConnection = nowConnected;
    }
    
    // Request parameters periodically
    if (millis() - lastParamRequestTime > PARAM_UPDATE_INTERVAL_MS) {
        lastParamRequestTime = millis();
        
        // Request next parameter
        CANParameter* param = canManager.getParameterByIndex(currentParamIndex);
        if (param && param->dirty) {
            canManager.requestParameter(param->id);
            #if DEBUG_CAN
            Serial.printf("Requesting param %d (%s)\n", param->id, param->name);
            #endif
        }
        
        currentParamIndex = (currentParamIndex + 1) % canManager.getParameterCount();
    }
    
    // Update UI
    uiManager.update();
    
    // Small delay to prevent watchdog
    delay(10);
}
