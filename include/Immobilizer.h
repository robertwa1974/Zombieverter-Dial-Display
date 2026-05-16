#ifndef IMMOBILIZER_H
#define IMMOBILIZER_H

#include <Arduino.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <functional>
#include "MFRC522.h"
#include "Config.h"
#include "SDOManager.h"

// ============================================================================
// RFID — WS1850S built into M5Dial, I2C address 0x28
// Uses arozcan I2C MFRC522 fork in lib/MFRC522/
// Call rfid.begin() after M5.begin() — begin() calls PCD_Init() internally.
// ============================================================================
#define RFID_ENABLED true

// ============================================================================
// VCU DriveInhibit parameter IDs
// ============================================================================
#define VCU_PARAM_DRIVE_INHIBIT  156   // writeable param: 0=off, 2=always
#define VCU_SPOT_DRIVE_INHIBITED 2124  // read-only spot value: 0=allowed, 1=inhibited

// ============================================================================
// Timing
// ============================================================================
#define VCU_TIMEOUT_MS        5000   // 5 s without VCU heartbeat → auto-lock
#define VCU_HEARTBEAT_ID      0x500  // CAN ID to monitor for VCU presence

// CRITICAL: CONFIRM_POLL_INTERVAL must be long enough that a confirm-poll
// response can't arrive while a DriveInhibit write is still in-flight.
// 5 s avoids the response collision that caused the retry storm.
#define CONFIRM_POLL_INTERVAL 5000   // Re-read DriveInhibited every 5 s

// SDO retry state machine — onSDOResult must NEVER call sendDriveInhibit()
// directly on failure. That causes a retry storm: failure→retry→failure→...
// flooding the TWAI TX queue with ESP_ERR_TIMEOUT until total lock-up.
// Instead set writePending=true + nextRetryMs and let update() fire the retry.
#define INHIBIT_RETRY_BACKOFF_MS  3000  // wait 3 s before each retry
#define INHIBIT_MAX_RETRIES       5     // give up after 5 consecutive failures

// ── BLE proximity unlock (UUID-based, unlock-only) ───────────────────────────
// Uses advertised service UUIDs rather than MAC addresses — avoids Android MAC
// randomisation. Phone runs a BLE beacon app (nRF Connect or Beacon Simulator)
// advertising a fixed custom UUID. Hardware fobs use iBeacon profile UUID.
//
// BLE is UNLOCK ONLY — never triggers locking. Locking is always deliberate
// (long-press on lock screen). Prevents accidental immobilisation while driving.
#define BLE_ENABLED             false  // set true when BLE beacon app is configured
#define BLE_UNLOCK_RSSI         -75   // dBm  unlock threshold (phone in pocket ~1m)
#define BLE_PAIR_RSSI           -60   // dBm  must be this close to pair (avoids neighbours)
#define BLE_PAIR_TIMEOUT_MS     30000 // ms   pairing mode auto-cancels after 30 s
#define BLE_SCAN_INTERVAL_MS    1000  // ms   scan cycle
#define MAX_BLE_UUIDS           4     // max stored UUIDs
#define BLE_UUID_LEN            37    // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\0"
#define NVS_KEY_BLE_COUNT       "blecnt"
#define NVS_KEY_BLE_PREFIX      "ble"   // "ble0"..."ble3", BLE_UUID_LEN bytes each

// ============================================================================
// NVS storage keys
// ============================================================================
#define NVS_NAMESPACE        "immobilizer"
#define NVS_KEY_PIN          "pin"           // stored as 4-char string e.g. "1234"
#define NVS_KEY_FOB_COUNT    "fobCount"      // uint8
#define NVS_KEY_FOB_PREFIX   "fob"           // fob0, fob1, ... stored as 4-byte blob

// ============================================================================
// Limits
// ============================================================================
#define MAX_AUTHORIZED_FOBS  8
#define SECRET_PIN_LENGTH    4

// ============================================================================
// Immobilizer modes — used to drive lock screen UI state
// ============================================================================
enum class ImmobMode : uint8_t {
    LOCKED,          // normal locked state, PIN entry active
    UNLOCKED,        // normal operation
    PROGRAM_FOB,     // waiting to scan a new fob
    CHANGE_PIN_1,    // entering new PIN (first pass)
    CHANGE_PIN_2,    // confirming new PIN (second pass)
    PAIR_BLE,        // waiting to capture a BLE UUID to whitelist
};

// ============================================================================
// Immobilizer
// ============================================================================
class Immobilizer {
public:
    Immobilizer();

    // Call once after SDO manager is ready
    void init(SDOManager* sdo);

    // Call every loop iteration
    void update();

    // ── Lock state ────────────────────────────────────────────────────────
    bool        isUnlocked()    const { return !enabled || mode == ImmobMode::UNLOCKED; }
    ImmobMode   getMode()       const { return !enabled ? ImmobMode::UNLOCKED : mode; }

    // Enable/disable the immobilizer entirely (persisted to NVS via dialsettings)
    void setEnabled(bool en) { enabled = en; }
    bool isEnabled()   const { return enabled; }

    // Configurable VCU CAN parameter IDs — defaults match ZombieVerter firmware
    void setInhibitParams(uint16_t writeId, uint16_t readId) {
        inhibitWriteId = writeId;
        inhibitReadId  = readId;
        Serial.printf("[IMMOBILIZER] inhibitWriteId=%u inhibitReadId=%u\n", writeId, readId);
    }

    void lock();
    void unlock();
    void toggleLock();

    // ── PIN entry (used on lock screen and change-PIN flow) ───────────────
    void    enterDigit(uint8_t digit);
    void    clearPIN();
    uint8_t getCurrentDigit()  const { return currentDigit; }
    uint8_t getPINPosition()   const { return pinPosition; }
    void    setCurrentDigit(uint8_t d)  { if (d <= 9) currentDigit = d; }
    void    incrementDigit()            { currentDigit = (currentDigit + 1) % 10; }
    void    decrementDigit()            { currentDigit = (currentDigit == 0) ? 9 : currentDigit - 1; }

    // ── Programming modes (called from UI long-press while unlocked) ───────
    // Enter fob programming mode — next card scanned is saved
    void startProgramFob();
    // Cancel fob programming / PIN change without saving
    void cancelProgramming();
    // Remove all saved fobs
    void clearAllFobs();
    // How many fobs are stored
    uint8_t getFobCount() const { return fobCount; }

    // Enter PIN change mode
    void startChangePin();

    // ── Unlock callback — called whenever vehicle is unlocked (PIN or RFID) ──
    // Use this to trigger health check, screen transition, etc.
    void setOnUnlock(std::function<void()> cb) { onUnlockCb = cb; }
    // BLE proximity unlock goes straight to dashboard — no health check
    void setOnBLEUnlock(std::function<void()> cb) { onBLEUnlockCb = cb; }

    // ── Status callbacks — for UI feedback on RFID/BLE/programming events ──
    void setOnSuccess(std::function<void(const char*)> cb) { onSuccessCb = cb; }
    void setOnWarning(std::function<void(const char*)> cb) { onWarningCb = cb; }

    // ── SDO result handler ────────────────────────────────────────────────
    void onSDOResult(const SDOResult& result);

    // ── CAN monitoring ────────────────────────────────────────────────────
    void processCANMessage(uint32_t id, uint8_t* data, uint8_t len);

    // ── RFID ─────────────────────────────────────────────────────────────
    // Called from update(); also callable directly for test purposes
    bool checkRFID();

    // ── BLE proximity unlock (UUID-based, unlock-only) ────────────────────
    void    startPairBLE();               // enter PAIR_BLE mode (must be unlocked)
    void    clearBLEUUIDs();
    uint8_t getBLEUUIDCount() const { return bleUUIDCount; }
    void    setBLEEnabled(bool enabled) { bleEnabled = enabled; }
    // Called by BLE scanner task on core 0
    void    onBLEDevice(const char* uuid, int rssi);

private:
    SDOManager* sdoManager;
    ImmobMode   mode;
    bool        enabled        = true;   // false = bypass all locking
    uint16_t    inhibitWriteId = VCU_PARAM_DRIVE_INHIBIT;   // configurable via web UI
    uint16_t    inhibitReadId  = VCU_SPOT_DRIVE_INHIBITED;  // configurable via web UI

    // PIN entry
    uint8_t enteredPIN[SECRET_PIN_LENGTH];
    uint8_t newPIN1[SECRET_PIN_LENGTH];    // first pass of change-PIN flow
    uint8_t pinPosition;
    uint8_t currentDigit;

    // Timing
    uint32_t lastVCUHeartbeat;
    uint32_t lastConfirmPollMs;

    // SDO retry state machine
    // writePending:    a write is needed but not yet sent (or needs retry after backoff)
    // writeInFlight:   write sent, awaiting ACK — do not poll or re-send until resolved
    // nextRetryMs:     earliest millis() at which next attempt is allowed
    // writeRetryCount: consecutive failures; reset to 0 on success
    bool     writePending;
    bool     writeInFlight;
    uint32_t nextRetryMs;
    uint8_t  writeRetryCount;

    // NVS-backed PIN (loaded on init)
    uint8_t storedPIN[SECRET_PIN_LENGTH];

    // NVS-backed fob UIDs
    uint8_t authorizedFobs[MAX_AUTHORIZED_FOBS][4];
    uint8_t fobCount;

    // BLE UUID whitelist (unlock-only, UUID-based to survive Android MAC randomisation)
    char     bleUUIDs[MAX_BLE_UUIDS][BLE_UUID_LEN];
    uint8_t  bleUUIDCount;
    uint32_t blePairStartMs;      // millis() when PAIR_BLE mode started; 0 = not pairing
    uint32_t bleUnlockCooldown;   // millis() after which BLE unlock is allowed; 0 = always allowed
    bool     bleEnabled;          // false while WiFi AP is active (radio contention)

    // NVS helpers
    void loadFromNVS();
    void savePINToNVS();
    void saveFobsToNVS();
    void loadBLEFromNVS();
    void saveBLEToNVS();

    // Auth helpers
    bool validatePIN();
    bool checkAuthorizedUID(const uint8_t* uid);
    bool saveFob(const uint8_t* uid);

    // SDO helpers
    void sendDriveInhibit(uint8_t value);
    void pollDriveInhibited();

    // Unlock callback
    std::function<void()> onUnlockCb;
    std::function<void()> onBLEUnlockCb;
    std::function<void(const char*)> onSuccessCb;
    std::function<void(const char*)> onWarningCb;

    // RFID object — I2C address 0x28, uses M5.In_I2C internally
    MFRC522 rfid = MFRC522(RC522_I2C_ADDRESS);
};

#endif // IMMOBILIZER_H
