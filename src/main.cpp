#include <Arduino.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include "Config.h"
#include "Hardware.h"
#include "CANData.h"
#include "CANMonitor.h"
#include "InputManager.h"
#include "UIManager.h"
#include "WiFiManager.h"
#include "TripLogger.h"
#include "GVRETServer.h"
#include "FaultLogger.h"
#include "HealthChecker.h"
#include "EfficiencyTracker.h"
#include "Immobilizer.h"
#include "SDOManager.h"

// ── Firmware version strings — update on each release ────────────────────────
#define DIAL_FW_VERSION   "v2.5.4"   // M5Dial firmware version
#define UI_VERSION        "v2.5.4"   // Web UI version (ui.js / index.html)

// Global objects
CANDataManager canManager;
InputManager inputManager;
UIManager uiManager;
WiFiManager wifiManager;
Immobilizer immobilizer;

// State tracking
bool systemReady = false;
bool wifiMode = false;
bool lvglSuspended = false;
uint32_t lastParamRequestTime = 0;
uint8_t currentParamIndex = 0;
uint8_t lastOpmode = 255;  // opmode change detection (255 = uninitialised)

// ── Cached parameter pointers ─────────────────────────────────────────────────
// Avoids 10 linear strcmp searches per loop() iteration.
// Set to nullptr on boot; lazily resolved on first use.
// Call invalidateParamCache() after any params reload (refetch).
static CANParameter* g_pSpeed   = nullptr;
static CANParameter* g_pUdc     = nullptr;
static CANParameter* g_pIdc     = nullptr;
static CANParameter* g_pSoc     = nullptr;
static CANParameter* g_pTmphs   = nullptr;
static CANParameter* g_pTmpm    = nullptr;
static CANParameter* g_pPwr     = nullptr;
static CANParameter* g_pPotnorm = nullptr;
static CANParameter* g_pOpmode  = nullptr;

void invalidateParamCache() {
    g_pSpeed = g_pUdc = g_pIdc = g_pSoc = nullptr;
    g_pTmphs = g_pTmpm = g_pPwr = g_pPotnorm = g_pOpmode = nullptr;
}

// Resolve all cached pointers in one pass — called once per loop after
// canManager.update() so we're always working with fresh table state.
void refreshParamCache() {
    if (!g_pSpeed)   g_pSpeed   = canManager.getParameterByName("speed");
    if (!g_pUdc)     g_pUdc     = canManager.getParameterByName("udc");
    if (!g_pIdc)     g_pIdc     = canManager.getParameterByName("idc");
    if (!g_pSoc)     g_pSoc     = canManager.getParameterByName("SOC");
    if (!g_pTmphs)   g_pTmphs   = canManager.getParameterByName("tmphs");
    if (!g_pTmpm)    g_pTmpm    = canManager.getParameterByName("tmpm");
    if (!g_pPwr)     g_pPwr     = canManager.getParameterByName("pwr");
    if (!g_pPotnorm) g_pPotnorm = canManager.getParameterByName("potnorm");
    if (!g_pOpmode)  g_pOpmode  = canManager.getParameterByName("opmode");
}

// Fallback parameters — used only if SPIFFS params.json is missing or corrupt.
const char* sampleParams = R"(
{
  "parameters": [
    {"id": 2,   "name": "speed",      "type": "int16", "unit": "rpm", "min": 0,    "max": 6000, "decimals": 0, "editable": false},
    {"id": 3,   "name": "udc",        "type": "int16", "unit": "V",   "min": 0,    "max": 400,  "decimals": 0, "editable": false},
    {"id": 7,   "name": "tmphs",      "type": "int16", "unit": "C",   "min": 0,    "max": 150,  "decimals": 0, "editable": false},
    {"id": 8,   "name": "tmpm",       "type": "int16", "unit": "C",   "min": 0,    "max": 150,  "decimals": 0, "editable": false},
    {"id": 27,  "name": "gear",       "type": "int16", "unit": "",    "min": 0,    "max": 3,    "decimals": 0, "editable": true},
    {"id": 61,  "name": "regenmax",   "type": "int16", "unit": "%",   "min": -35,  "max": 0,    "decimals": 0, "editable": true},
    {"id": 129, "name": "motactive",  "type": "int16", "unit": "",    "min": 0,    "max": 3,    "decimals": 0, "editable": true},
    {"id": 100, "name": "soc",        "type": "int16", "unit": "%",   "min": 0,    "max": 100,  "decimals": 0, "editable": false},
    {"id": 101, "name": "idc",        "type": "int16", "unit": "A",   "min": -500, "max": 500,  "decimals": 0, "editable": false},
    {"id": 102, "name": "battemp",    "type": "int16", "unit": "C",   "min": -40,  "max": 80,   "decimals": 0, "editable": false}
  ]
}
)";

// ============================================================================
// SDO result callback — routes results to the immobilizer (param 156 / spot 2124)
// and to canManager for all other spot-value updates.
//
// IMPORTANT: Immobilizer.cpp uses a state-machine retry pattern — onSDOResult
// never calls sendDriveInhibit() directly on failure. It sets writePending and
// lets update() fire the retry after a backoff delay. This prevents the TWAI
// TX queue flood (ESP_ERR_TIMEOUT loop) seen with the direct-retry approach.
// ============================================================================

void onSDOResult(const SDOResult& result) {
    // Immobilizer owns param 156 (DriveInhibit write) and spot 2124 (DriveInhibited read)
    if (result.paramId == VCU_PARAM_DRIVE_INHIBIT ||
        result.paramId == VCU_SPOT_DRIVE_INHIBITED) {
        immobilizer.onSDOResult(result);
        return;   // don't pass these to canManager — they're not CAN data params
    }
    // Everything else: let canManager update its parameter table
    canManager.onSDOResult(result);
}

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
        wifiManager.stopAP(); immobilizer.setBLEEnabled(true);
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        return;
    }

    ScreenID currentScreen = uiManager.getCurrentScreen();

    // Lock screen
    if (currentScreen == SCREEN_LOCK) {
        ImmobMode m = immobilizer.getMode();
        if (uiManager.isLockPinPadVisible() ||
            m == ImmobMode::CHANGE_PIN_1 ||
            m == ImmobMode::CHANGE_PIN_2) {
            // Digit selection active
            if (delta > 0) immobilizer.incrementDigit();
            else           immobilizer.decrementDigit();
        } else if (m == ImmobMode::UNLOCKED) {
            // Padlock view, unlocked — rotate aborts back to dashboard
            uiManager.setScreen(SCREEN_DASHBOARD);
        }
        // Padlock view + locked: rotation does nothing
        return;
    }

    // Settings screen: rotate scrolls menu; scrolling past ends navigates away
    if (currentScreen == SCREEN_SETTINGS) {
        int current = uiManager.getSettingsSelectedItem();
        int next = current + (delta > 0 ? 1 : -1);
        if (next < 0) {
            // Scrolled past top — go to previous screen
            uiManager.setScreen(uiManager.getPreviousScreen());
        } else if (next >= uiManager.getSettingsMenuCount()) {
            // Scrolled past bottom — go to next screen
            uiManager.setScreen(uiManager.getNextScreen());
        } else {
            uiManager.scrollSettingsMenu(delta > 0 ? 1 : -1);
        }
        return;
    }

    if (currentScreen == SCREEN_GEAR) {
        if (!uiManager.isEditMode()) {
            ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
            uiManager.setScreen(dest);
            return;
        }
        // Safety: block gear change while moving — use cached pointer
        if (g_pSpeed && abs(g_pSpeed->getValueAsInt()) > 50) {
            uiManager.showWarning("Vehicle moving\nStop before\nchanging gear");
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
            ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
            uiManager.setScreen(dest);
            return;
        }
        // Safety: block motor mode change while moving — use cached pointer
        if (g_pSpeed && abs(g_pSpeed->getValueAsInt()) > 50) {
            uiManager.showWarning("Vehicle moving\nStop before\nchanging motor");
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
            ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
            uiManager.setScreen(dest);
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
    } else if (currentScreen == SCREEN_THROTMAX) {
        if (!uiManager.isEditMode()) {
            ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
            uiManager.setScreen(dest);
            return;
        }
        CANParameter* throtmax = canManager.getParameter(25);
        if (throtmax) {
            int32_t newThrotMax = throtmax->getValueAsInt();
            newThrotMax += delta > 0 ? 1 : -1;
            if (newThrotMax > 100) newThrotMax = 100;
            if (newThrotMax < 0)   newThrotMax = 0;
            canManager.setParameter(25, newThrotMax);
        }
    } else if (currentScreen == SCREEN_BRAKEREGEN) {
        if (!uiManager.isEditMode()) {
            ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
            uiManager.setScreen(dest);
            return;
        }
        CANParameter* brakeRegen = canManager.getParameter(122);
        if (brakeRegen) {
            int32_t newBrakeRegen = brakeRegen->getValueAsInt();
            newBrakeRegen += delta > 0 ? 1 : -1;
            if (newBrakeRegen > 0)   newBrakeRegen = 0;
            if (newBrakeRegen < -35) newBrakeRegen = -35;
            canManager.setParameter(122, newBrakeRegen);
        }
    } else {
        ScreenID dest = delta > 0 ? uiManager.getNextScreen() : uiManager.getPreviousScreen();
        if (dest == SCREEN_SETTINGS) {
        }
        uiManager.setScreen(dest);
    }
}

void onButtonClick() {
    ScreenID currentScreen = uiManager.getCurrentScreen();

    // Lock screen
    if (currentScreen == SCREEN_LOCK) {
        ImmobMode m = immobilizer.getMode();
        if (uiManager.isLockPinPadVisible() ||
            m == ImmobMode::CHANGE_PIN_1 ||
            m == ImmobMode::CHANGE_PIN_2) {
            // Digit entry active — confirm current digit
            immobilizer.enterDigit(immobilizer.getCurrentDigit());
        } else if (m == ImmobMode::UNLOCKED) {
            // Padlock view, unlocked — button aborts back to dashboard
            uiManager.setScreen(SCREEN_DASHBOARD);
        }
        return;
    }

    // Health check screen — button press acknowledges warn/fail result
    if (currentScreen == SCREEN_HEALTH_CHECK) {
        if (HealthChecker::getInstance().needsAck()) {
            bool relock = HealthChecker::getInstance().relockNeeded();
            HealthChecker::getInstance().acknowledge();
            if (relock) {
                immobilizer.lock();
                uiManager.setScreen(SCREEN_LOCK);
            }
        }
        return;
    }

    // Settings screen: activated by touch tap (see onTouchTap)
    // Button click does nothing on settings screen — prevents accidental activation
    if (currentScreen == SCREEN_SETTINGS) return;

    // On editable screens: first click enters edit mode, second click exits
    if (currentScreen == SCREEN_GEAR ||
        currentScreen == SCREEN_MOTOR ||
        currentScreen == SCREEN_REGEN ||
        currentScreen == SCREEN_THROTMAX ||
        currentScreen == SCREEN_BRAKEREGEN) {
        if (uiManager.isEditMode()) {
            uiManager.toggleEditMode();  // exit edit mode
        } else {
            // Safety: block entering edit mode on Gear/Motor while moving
            if (currentScreen == SCREEN_GEAR || currentScreen == SCREEN_MOTOR) {
                if (g_pSpeed && abs(g_pSpeed->getValueAsInt()) > 50) {
                    uiManager.showWarning("Vehicle moving\nStop before\nediting");
                    return;
                }
            }
            uiManager.toggleEditMode();  // enter edit mode
        }
        return;
    }

    if (!wifiMode) {
        wifiMode = true;
        immobilizer.setBLEEnabled(false);  // pause BLE to avoid radio contention
        wifiManager.startAP();
        uiManager.setScreen(SCREEN_WIFI);
        uiManager.updateWifiScreen(wifiManager.getIPAddress());
        for (int i = 0; i < 10; i++) { lv_timer_handler(); delay(10); }
        lvglSuspended = true;
        #if DEBUG_SERIAL
        Serial.println("WiFi mode enabled");
        #endif
    } else {
        wifiMode = false;
        wifiManager.stopAP(); immobilizer.setBLEEnabled(true);
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        #if DEBUG_SERIAL
        Serial.println("WiFi mode disabled");
        #endif
    }
}

void onButtonDoubleClick() {
    // Double-click cancels any active programming/pairing mode
    ImmobMode m = immobilizer.getMode();
    if (m == ImmobMode::PROGRAM_FOB ||
        m == ImmobMode::CHANGE_PIN_1 ||
        m == ImmobMode::CHANGE_PIN_2 ||
        m == ImmobMode::PAIR_BLE) {
        immobilizer.cancelProgramming();
        uiManager.setScreen(SCREEN_LOCK);
    }
}

void onButtonTripleClick() {
    // Reserved for future use
}

void onButtonLongPress() {
    ScreenID currentScreen = uiManager.getCurrentScreen();

    if (wifiMode) {
        wifiMode = false;
        wifiManager.stopAP(); immobilizer.setBLEEnabled(true);
        uiManager.resetWifiScreen();
        resumeLVGL();
        return;
    }

    // Long-press from any screen while unlocked → navigate to lock screen.
    // Actual locking requires a deliberate tap on that screen (two-step safety).
    if (immobilizer.getMode() == ImmobMode::UNLOCKED) {
        uiManager.setScreen(SCREEN_LOCK);
        return;
    }

    // Long-press on lock screen while already locked → lock (redundant safety)
    if (currentScreen == SCREEN_LOCK) {
        if (immobilizer.isUnlocked()) {
            immobilizer.lock();
            uiManager.setScreen(SCREEN_LOCK);
        }
        return;
    }

    uiManager.setScreen(SCREEN_DASHBOARD);
}

// Fires immediately on finger-down — used for lock screen reveal only
void onTouchPress(uint16_t x, uint16_t y) {
    if (uiManager.getCurrentScreen() == SCREEN_LOCK) {
        if (immobilizer.getMode() == ImmobMode::LOCKED &&
            !uiManager.isLockPinPadVisible()) {
            // Locked + tapped → show PIN pad to unlock
            uiManager.showLockPinPad();
        } else if (immobilizer.getMode() == ImmobMode::UNLOCKED) {
            // Unlocked + tapped → lock the vehicle (step 2 of deliberate lock process)
            immobilizer.lock();
            uiManager.setScreen(SCREEN_LOCK);
        }
    }
}

void onTouchTap(uint16_t x, uint16_t y) {
    if (wifiMode) {
        // Touch exits WiFi mode
        wifiMode = false;
        wifiManager.stopAP(); immobilizer.setBLEEnabled(true);
        uiManager.resetWifiScreen();
        resumeLVGL();
        uiManager.setScreen(SCREEN_DASHBOARD);
        return;
    }

    ScreenID currentScreen = uiManager.getCurrentScreen();

    // Lock screen handled by onTouchPress — nothing more to do here
    if (currentScreen == SCREEN_LOCK) return;

    // Settings screen: tap activates the highlighted menu item.
    // Debounce: ignore taps for 600ms after arrival — rotating the M5Dial
    // capacitive bezel generates spurious touch events while turning.
    if (currentScreen == SCREEN_SETTINGS && immobilizer.isUnlocked()) {
        if (millis() - uiManager.getSettingsArrivalTime() < 600) return;
        switch (uiManager.getSettingsSelectedItem()) {
            case 0:  // Pair BLE Beacon
                immobilizer.startPairBLE();
                uiManager.setScreen(SCREEN_LOCK);
                break;
            case 1:  // Program RFID Fob
                immobilizer.startProgramFob();
                uiManager.setScreen(SCREEN_LOCK);
                break;
            case 2:  // Clear BLE Beacons
                immobilizer.clearBLEUUIDs();
                uiManager.showWarning("BLE beacons\ncleared");
                break;
            case 3:  // Clear RFID Fobs
                immobilizer.clearAllFobs();
                uiManager.showWarning("RFID fobs\ncleared");
                break;
            case 4:  // System Info
                uiManager.showSystemInfo();
                break;
            case 5:  // Change PIN
                immobilizer.startChangePin();
                uiManager.setScreen(SCREEN_LOCK);
                break;
        }
        return;
    }

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
// Shared SDO poll helper — identical logic used in both WiFi and normal mode
// ============================================================================
void pollNextSDOParam() {
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
}

// ============================================================================
// setup
// ============================================================================

void setup() {
    #if DEBUG_SERIAL
    Serial.begin(115200);
    delay(1000);
    Serial.println("ZombieVerter Display - M5Stack Dial");
    Serial.println("=============================================");
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

    SPIFFS.begin(true);  // Must be before uiManager.init so /logo.bin is visible at splash creation

    uiManager.init(&canManager, &immobilizer);
    uiManager.setVersionInfo(DIAL_FW_VERSION, UI_VERSION);
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
    wifiManager.setUIManager(&uiManager);
    wifiManager.setImmobilizer(&immobilizer);
    #if DEBUG_SERIAL
    Serial.println("WiFi manager initialized");
    #endif

    CANMonitor::instance().init(&canManager);

    wifiMode = false;
    lvglSuspended = false;

    // ---------------------------------------------------------------------------
    // Parameter loading (controlled by fetchOnBoot NVS toggle):
    //   - If fetchOnBoot enabled: attempt VCU fetch → reboot on success
    //   - If fetchOnBoot disabled or fetch fails: SPIFFS → sample params
    // ---------------------------------------------------------------------------
    bool paramsLoaded = false;
    bool fetchOnBoot = false;
    
    // Read fetch-on-boot preference from NVS
    {
        Preferences prefs;
        prefs.begin("dialsettings", true);
        fetchOnBoot = prefs.getBool("fetchOnBoot", false);  // default OFF
        prefs.end();
    }

    if (fetchOnBoot) {
        // Check heap before attempting fetch — need ~100KB + margin
        size_t freeHeap = ESP.getFreeHeap();
        Serial.printf("[Fetch] Free heap before fetch: %u bytes\n", freeHeap);

        if (freeHeap < 150000) {
            Serial.println("[Fetch] Heap too low — skipping auto-fetch");
            Serial.println("[Fetch] Use web UI Refetch button if params changed");
            uiManager.showFetchStatus("Heap low\nSkipping fetch");
            delay(1500);
        } else {
            Serial.println("[Fetch] Auto-fetch enabled — attempting VCU download...");
            uiManager.showFetchStatus("Fetching params\nfrom VCU...");
            for (int i = 0; i < 3; i++) { lv_timer_handler(); delay(10); }

            FetchResult fetchResult = canManager.fetchParamsFromVCU();

            if (fetchResult == FetchResult::SUCCESS) {
                Serial.printf("[Fetch] Success: %d parameters\n", canManager.getParameterCount());
                
                // Clear the fetchOnBoot flag so next boot loads from SPIFFS (no infinite loop)
                {
                    Preferences prefs;
                    prefs.begin("dialsettings", false);
                    prefs.putBool("fetchOnBoot", false);
                    prefs.end();
                    Serial.println("[Fetch] Cleared fetchOnBoot flag for next boot");
                }
                
                // Reboot for clean heap state — ensures WiFi/GVRET work reliably
                uiManager.showFetchStatus("VCU params loaded!\nRebooting...");
                delay(2000);
                ESP.restart();
                // Execution stops here — next boot has clean heap
            } else {
                Serial.printf("[Fetch] Failed (%d) — trying SPIFFS fallback\n", (int)fetchResult);
                uiManager.showFetchStatus("VCU unavailable\nLoading cached...");
                delay(1000);
            }
        }
    } else {
        Serial.println("[Fetch] Auto-fetch disabled — loading from SPIFFS or defaults");
    }

    // SPIFFS fallback (used when fetch disabled, failed, or skipped due to low heap)
    if (!paramsLoaded && SPIFFS.exists("/params.json")) {
        File paramFile = SPIFFS.open("/params.json", "r");
        if (paramFile) {
            size_t fileSize = paramFile.size();
            if (fileSize > 0 && fileSize < MAX_JSON_SIZE) {
                if (canManager.loadParametersFromJSON(paramFile)) {
                    Serial.printf("[Params] Loaded %d from SPIFFS\n", canManager.getParameterCount());
                    paramsLoaded = true;
                    uiManager.showFetchStatus("Cached params\nloaded OK");
                    delay(1000);
                }
                paramFile.close();
            } else {
                paramFile.close();
                SPIFFS.remove("/params.json");
            }
        }
    }

    // Final fallback: sample params
    if (!paramsLoaded) {
        Serial.println("[Params] Using sample parameters only");
        uiManager.showFetchStatus("Using defaults\nConnect VCU!");
        delay(1500);
    }

    // Invalidate cache after params load so pointers are resolved fresh on first loop
    invalidateParamCache();

    // Start SDO manager AFTER fetch so it doesn't consume our response frames
    canManager.initSDO();

    // Route SDO results: immobilizer handles param 156 and spot 2124;
    // everything else goes to canManager for spot-value updates.
    canManager.getSDOManager()->setResultCallback(onSDOResult);

    // Initialize immobilizer — boots locked, reads VCU state via SDO.
    // NOTE: Requires CANDataManager::getSDOManager() to return SDOManager*.
    //       If that method doesn't exist yet, add it to CANData.h/.cpp.
    immobilizer.init(canManager.getSDOManager());

    // Apply persisted display settings: screen visibility mask and immobilizer enable
    {
        Preferences prefs;
        prefs.begin("dialsettings", true);
        uint16_t screenMask   = prefs.getUShort("screenMask",   0xFFFF);
        bool     immobEnabled = prefs.getBool("immobEnabled",   true);
        uint16_t immobWriteId = prefs.getUShort("immobWriteId", VCU_PARAM_DRIVE_INHIBIT);
        uint16_t immobReadId  = prefs.getUShort("immobReadId",  VCU_SPOT_DRIVE_INHIBITED);
        prefs.end();
        uiManager.setScreenMask(screenMask);
        immobilizer.setEnabled(immobEnabled);
        immobilizer.setInhibitParams(immobWriteId, immobReadId);
        Serial.printf("[Main] screenMask=0x%04X immobEnabled=%d writeId=%u readId=%u\n",
                      screenMask, immobEnabled, immobWriteId, immobReadId);
    }

    // On every unlock (PIN or RFID): run pre-drive health check then go to dashboard
    immobilizer.setOnUnlock([]() {
        HealthChecker::getInstance().reset();
        HealthChecker::getInstance().startCheck();
        uiManager.setScreen(SCREEN_HEALTH_CHECK);
    });

    // BLE proximity unlock skips the health check — goes straight to dashboard
    // Health check is for PIN/RFID (about to drive); BLE may just be parking nearby
    immobilizer.setOnBLEUnlock([]() {
        uiManager.setScreen(SCREEN_DASHBOARD);
    });

    immobilizer.setOnSuccess([](const char* msg) {
        uiManager.showSuccess(msg);
    });

    immobilizer.setOnWarning([](const char* msg) {
        uiManager.showWarning(msg);
    });

    // Initialize trip logger (reads existing NVS ring buffer state)
    TripLogger::getInstance().begin();

    // Fault logger — reads existing NVS fault history
    FaultLogger::getInstance().begin();

    // Health checker — wired to canManager for SDO polling
    HealthChecker::getInstance().begin(&canManager);

    // Efficiency tracker — load saved drive params from NVS
    {
        Preferences prefs;
        prefs.begin("dialsettings", true);
        float finalDrive = prefs.getFloat("finalDrive", 4.10f);
        float wheelCirc  = prefs.getFloat("wheelCirc",  1.88f);
        prefs.end();
        EfficiencyTracker::getInstance().begin();
        EfficiencyTracker::getInstance().setDriveParams(finalDrive, wheelCirc);
        Serial.printf("[Main] EfficiencyTracker: finalDrive=%.2f wheelCirc=%.2f\n",
                      finalDrive, wheelCirc);
    }

    // Register GVRET bridge as CAN frame observer — pushes every frame to
    // connected SavvyCAN clients in real time when WiFi AP is active
    canManager.setFrameObserver([](uint32_t id, bool extd, const uint8_t* data, uint8_t dlc) {
        GVRETServer::getInstance().pushFrame(id, extd, data, dlc);
    });

    // Brief pause so user can see the status message
    for (int i = 0; i < 100; i++) { lv_timer_handler(); delay(10); }

    inputManager.setOnEncoderRotate(onEncoderRotate);
    inputManager.setOnButtonClick(onButtonClick);
    inputManager.setOnButtonDoubleClick(onButtonDoubleClick);
    inputManager.setOnButtonTripleClick(onButtonTripleClick);
    inputManager.setOnButtonLongPress(onButtonLongPress);
    inputManager.setOnTouchPress(onTouchPress);
    inputManager.setOnTouchTap(onTouchTap);

    // Start locked if immobilizer is enabled, otherwise go straight to dashboard
    if (immobilizer.isEnabled()) {
        uiManager.setScreen(SCREEN_LOCK);
    } else {
        uiManager.setScreen(SCREEN_DASHBOARD);
        Serial.println("[Main] Immobilizer disabled — starting at Dashboard");
    }
    systemReady = true;

    #if DEBUG_SERIAL
    Serial.println("System ready!");
    Serial.println("=============================================");
    Serial.println("Controls:");
    Serial.println("  Rotate: Switch screens");
    Serial.println("  Click: Toggle WiFi mode");
    Serial.println("  Double-click: (Reserved)");
    Serial.println("  Long-press: Back to Dashboard");
    Serial.println("=============================================");
    Serial.println("WiFi Mode:");
    Serial.println("  Click button to enable WiFi AP");
    Serial.println("  Connect to: " WIFI_AP_SSID);
    Serial.println("  Password: " WIFI_AP_PASSWORD);
    Serial.println("  Browse to: 192.168.4.1");
    Serial.println("=============================================");
    #endif
}

// ============================================================================
// loop
// ============================================================================

void loop() {
    if (!systemReady) return;

    // ── DIAGNOSTIC: loop gap timing ──────────────────────────────────────
    // Prints whenever a single loop iteration takes longer than the previous
    // max — finds blocking calls that stall the async TCP server.
    {
        static uint32_t lastLoopMs   = 0;
        static uint32_t maxLoopGapMs = 0;
        uint32_t        nowMs        = millis();
        if (lastLoopMs != 0) {
            uint32_t gap = nowMs - lastLoopMs;
            if (gap > maxLoopGapMs && gap > 20) {
                maxLoopGapMs = gap;
                Serial.printf("[LOOP] new max gap = %u ms\n", gap);
            }
        }
        lastLoopMs = nowMs;
    }

    Hardware::update();
    inputManager.update();
    immobilizer.update();

    if (wifiMode) {
        wifiManager.update();
        canManager.update();

        // Handle logo upload — reload the splash screen widget without reboot
        if (wifiManager.isLogoReloadRequested()) {
            wifiManager.clearLogoReloadRequest();
            Serial.println("[Main] Logo reload triggered from web UI");
            uiManager.reloadLogo();
            // Briefly show splash with confirmation message, then return to WiFi screen
            lvglSuspended = false;
            uiManager.setScreen(SCREEN_SPLASH);
            uiManager.showFetchStatus("Logo updated!");
            for (int i = 0; i < 150; i++) { lv_timer_handler(); delay(10); }
            lvglSuspended = true;
            uiManager.setScreen(SCREEN_WIFI);
        }

        // Handle refetch request from web UI
        if (wifiManager.isRefetchRequested()) {
            wifiManager.clearRefetchRequest();
            Serial.println("[Main] Refetch triggered from web UI");
            // Show status on dial — briefly resume LVGL
            lvglSuspended = false;
            uiManager.setScreen(SCREEN_SPLASH);
            uiManager.showFetchStatus("Refetching from\nVCU...");
            for (int i = 0; i < 10; i++) { lv_timer_handler(); delay(10); }
            lvglSuspended = true;

            // Stop SDO manager, run fetch, restart SDO manager
            FetchResult result = canManager.fetchParamsFromVCU();

            // Invalidate param cache — table has been reloaded
            invalidateParamCache();

            lvglSuspended = false;
            if (result == FetchResult::SUCCESS) {
                uiManager.showFetchStatus("VCU params\nreloaded!");
                Serial.printf("[Main] Refetch success — %d params\n",
                    canManager.getParameterCount());
            } else {
                uiManager.showFetchStatus("Refetch failed\nUsing cached");
                Serial.println("[Main] Refetch failed");
            }
            for (int i = 0; i < 200; i++) { lv_timer_handler(); delay(10); }
            lvglSuspended = true;
            uiManager.setScreen(SCREEN_WIFI);
        }

        // Keep SDO polling running in WiFi mode so spot values stay live
        pollNextSDOParam();

        vTaskDelay(pdMS_TO_TICKS(5));
        return;
    }

    // Normal mode: full CAN + LVGL update
    canManager.update();

    // Resolve cached parameter pointers (no-op after first successful resolution)
    refreshParamCache();

    // Trip logger — records one entry every 5s while speed > 10 RPM
    // Uses cached pointers — no linear searches here
    TripLogger::getInstance().update(
        g_pSpeed   ? g_pSpeed->getValueAsInt()              : 0,   // speed_rpm
        g_pUdc     ? (int)(g_pUdc->getValueAsInt() * 10)   : 0,   // udc_dv  (V × 10)
        g_pIdc     ? (int)(g_pIdc->getValueAsInt() * 10)   : 0,   // idc_da  (A × 10)
        g_pPwr     ? (int)(g_pPwr->getValueAsInt() * 100)  : 0,   // pwr_dkw (kW × 100)
        g_pSoc     ? g_pSoc->getValueAsInt()                : 0,   // soc_pct
        g_pTmphs   ? g_pTmphs->getValueAsInt()              : 0,   // tmphs_c
        g_pTmpm    ? g_pTmpm->getValueAsInt()               : 0,   // tmpm_c
        g_pPotnorm ? g_pPotnorm->getValueAsInt()            : 0    // potnorm (0-1000)
    );

    // Opmode change detection — uses cached pointer, no linear search
    if (g_pOpmode) {
        uint8_t opmode = (uint8_t)g_pOpmode->getValueAsInt();
        if (opmode != lastOpmode) {
            FaultLogger::getInstance().logOpmodeChange(opmode);

            if (opmode == 3) {
                // Entered charge mode
                EfficiencyTracker::getInstance().startChargeSession();
                uiManager.setScreen(SCREEN_CHARGING);
                Serial.println("[Main] Charge mode detected — switching to charging screen");
            } else if (lastOpmode == 3) {
                // Left charge mode
                EfficiencyTracker::getInstance().endChargeSession();
                uiManager.setScreen(SCREEN_DASHBOARD);
                Serial.println("[Main] Charge mode ended — returning to dashboard");
            }
            lastOpmode = opmode;
        }
    }

    // Health checker update — drives async SDO poll sequence
    HealthChecker::getInstance().update();

    // Efficiency tracker — uses cached pointers, updates once per second
    {
        static uint32_t lastEffUpdate = 0;
        if (millis() - lastEffUpdate >= 1000) {
            lastEffUpdate = millis();
            float powerW   = g_pPwr   ? (float)g_pPwr->getValueAsInt() * 1000.0f : 0.0f;
            int   speedRPM = g_pSpeed ? g_pSpeed->getValueAsInt() : 0;
            EfficiencyTracker::getInstance().update(powerW, speedRPM);
        }
    }

    // Round-robin SDO parameter polling
    pollNextSDOParam();

    if (!lvglSuspended) {
        uiManager.update();
    }

    delay(10);
}