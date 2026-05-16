#include "Immobilizer.h"
// M5Unified is already included via Immobilizer.h — rfid accesses the RFID chip

#if BLE_ENABLED
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static Immobilizer* g_immob = nullptr;

// Thread-safe BLE result queue — written by core 0 BLE task,
// read by main loop (core 1) in Immobilizer::update().
// Uses a simple single-slot queue protected by an atomic flag.
struct BLEResult {
    char  uuid[37];
    int   rssi;
    bool  ready;  // true = new result waiting to be consumed
};
static volatile BLEResult g_bleResult = {"", 0, false};

class BLEScanCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        if (!g_immob) return;
        if (!device.haveServiceUUID()) return;
        if (g_bleResult.ready) return;  // previous result not yet consumed — skip

        std::string uuid = device.getServiceUUID(0).toString();
        if (uuid.empty() || uuid.length() >= 37) return;

        // Write result atomically — just a struct copy, no locks needed for
        // single-producer single-consumer with the ready flag
        strncpy((char*)g_bleResult.uuid, uuid.c_str(), 36);
        ((char*)g_bleResult.uuid)[36] = '\0';
        g_bleResult.rssi  = device.getRSSI();
        g_bleResult.ready = true;  // signal main loop
    }
};

static BLEScan*        bleScan = nullptr;
static BLEScanCallback bleCb;

static void bleScanTask(void*) {
    BLEDevice::init("ZVDial");
    bleScan = BLEDevice::getScan();
    bleScan->setAdvertisedDeviceCallbacks(&bleCb, /*wantDuplicates=*/true);
    bleScan->setActiveScan(true);
    bleScan->setInterval(100);
    bleScan->setWindow(99);
    for (;;) {
        bleScan->start(1, false);
        bleScan->clearResults();
        vTaskDelay(pdMS_TO_TICKS(BLE_SCAN_INTERVAL_MS));
    }
}
#endif // BLE_ENABLED

// ============================================================================
// Constructor
// ============================================================================

Immobilizer::Immobilizer()
    : sdoManager(nullptr),
      mode(ImmobMode::LOCKED),
      pinPosition(0),
      currentDigit(0),
      lastVCUHeartbeat(0),
      lastConfirmPollMs(0),
      writePending(false),
      writeInFlight(false),
      nextRetryMs(0),
      writeRetryCount(0),
      fobCount(0),
      bleUUIDCount(0),
      blePairStartMs(0),
      bleUnlockCooldown(0),
      bleEnabled(true),
      onBLEUnlockCb(nullptr)
{
    memset(enteredPIN, 0, sizeof(enteredPIN));
    memset(newPIN1,    0, sizeof(newPIN1));

    // Default PIN — overwritten by loadFromNVS() if saved value exists
    storedPIN[0] = 1; storedPIN[1] = 2;
    storedPIN[2] = 3; storedPIN[3] = 4;

    memset(authorizedFobs, 0, sizeof(authorizedFobs));
    memset(bleUUIDs,       0, sizeof(bleUUIDs));
}

// ============================================================================
// init
// ============================================================================

void Immobilizer::init(SDOManager* sdo) {
    sdoManager = sdo;

    // Load PIN and fob UIDs from NVS
    loadFromNVS();

    // Init RFID — arozcan I2C fork, address 0x28 via M5.In_I2C
    // begin() calls PCD_Init() which does reset, timer setup, PCD_AntennaOn()
#if RFID_ENABLED
    delay(100);
    rfid.begin();
    delay(10);
    rfid.PCD_Init();
    byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    if (ver == 0x00 || ver == 0xFF) {
        Serial.println("[IMMOBILIZER] WARNING: RFID chip not responding");
    } else {
        Serial.printf("[IMMOBILIZER] RFID ready (chip ver=0x%02X), %d fob(s) stored\n",
                      ver, fobCount);
    }
#endif

    Serial.println("[IMMOBILIZER] Init — reading VCU DriveInhibited state...");
    pollDriveInhibited();

    lastVCUHeartbeat  = millis();
    lastConfirmPollMs = millis();

#if BLE_ENABLED
    loadBLEFromNVS();
    g_immob = this;
    xTaskCreatePinnedToCore(bleScanTask, "BLEScan", 8192, nullptr, 1, nullptr, 0);
    Serial.printf("[IMMOBILIZER] BLE scanner started (%d UUID(s) stored)\n", bleUUIDCount);
#else
    // BLE disabled — clear any previously stored UUIDs from NVS so they
    // don't trigger auto-unlock when BLE is re-enabled
    bleUUIDCount = 0;
    saveBLEToNVS();
    Serial.println("[IMMOBILIZER] BLE disabled");
#endif

    Serial.println("[IMMOBILIZER] Init complete — LOCKED");
}

// ============================================================================
// NVS
// ============================================================================

void Immobilizer::loadFromNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);

    // Load PIN
    String pinStr = prefs.getString(NVS_KEY_PIN, "");
    if (pinStr.length() == SECRET_PIN_LENGTH) {
        for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
            storedPIN[i] = (uint8_t)(pinStr[i] - '0');
        }
        Serial.println("[IMMOBILIZER] PIN loaded from NVS");
    } else {
        Serial.println("[IMMOBILIZER] No saved PIN — using default 1234");
    }

    // Load fobs
    fobCount = prefs.getUChar(NVS_KEY_FOB_COUNT, 0);
    if (fobCount > MAX_AUTHORIZED_FOBS) fobCount = 0;

    for (uint8_t i = 0; i < fobCount; i++) {
        String key = String(NVS_KEY_FOB_PREFIX) + String(i);
        size_t len = prefs.getBytes(key.c_str(), authorizedFobs[i], 4);
        if (len != 4) {
            Serial.printf("[IMMOBILIZER] Fob %d load error — clearing fobs\n", i);
            fobCount = 0;
            break;
        }
        Serial.printf("[IMMOBILIZER] Fob %d: %02X %02X %02X %02X\n", i,
                      authorizedFobs[i][0], authorizedFobs[i][1],
                      authorizedFobs[i][2], authorizedFobs[i][3]);
    }

    prefs.end();
}

void Immobilizer::savePINToNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    String pinStr = "";
    for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
        pinStr += String(storedPIN[i]);
    }
    prefs.putString(NVS_KEY_PIN, pinStr);
    prefs.end();
    Serial.println("[IMMOBILIZER] PIN saved to NVS");
}

void Immobilizer::saveFobsToNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    prefs.putUChar(NVS_KEY_FOB_COUNT, fobCount);
    for (uint8_t i = 0; i < fobCount; i++) {
        String key = String(NVS_KEY_FOB_PREFIX) + String(i);
        prefs.putBytes(key.c_str(), authorizedFobs[i], 4);
    }
    prefs.end();
    Serial.printf("[IMMOBILIZER] %d fob(s) saved to NVS\n", fobCount);
}

// ============================================================================
// update — call every loop()
// ============================================================================

void Immobilizer::update() {
    // When immobilizer is disabled, skip all lock/SDO logic entirely
    if (!enabled) return;

    uint32_t now = millis();

    // ── 1. SDO write state machine ────────────────────────────────────────
    // Only send when: pending, not in-flight, backoff elapsed, retries remaining
    if (writePending && !writeInFlight && now >= nextRetryMs) {
        if (writeRetryCount >= INHIBIT_MAX_RETRIES) {
            Serial.println("[IMMOBILIZER] Max retries reached — pausing. Will re-check at next confirm poll.");
            writePending    = false;
            writeRetryCount = 0;
            // Force confirm poll soon so we can re-sync state
            lastConfirmPollMs = now - CONFIRM_POLL_INTERVAL + 2000;
        } else {
            uint8_t val = (mode == ImmobMode::UNLOCKED) ? 0 : 2;
            Serial.printf("[IMMOBILIZER] Sending DriveInhibit=%d (attempt %d/%d)\n",
                          val, writeRetryCount + 1, INHIBIT_MAX_RETRIES);
            sendDriveInhibit(val);
            writeInFlight = true;
            writePending  = false;
        }
    }

    // ── 2. Periodic confirm poll ──────────────────────────────────────────
    // Skip while write is in-flight to avoid response collision
    if (!writeInFlight && (now - lastConfirmPollMs >= CONFIRM_POLL_INTERVAL)) {
        lastConfirmPollMs = now;
        pollDriveInhibited();
    }

    // ── 3. BLE pairing timeout ────────────────────────────────────────────
#if BLE_ENABLED
    // Consume BLE scan result from core 0 — safe to call LVGL/unlock from here
    if (bleEnabled && g_bleResult.ready) {
        char  uuid[37];
        int   rssi;
        strncpy(uuid, (const char*)g_bleResult.uuid, 37);
        rssi = g_bleResult.rssi;
        g_bleResult.ready = false;  // consume before processing (re-entrant safe)
        onBLEDevice(uuid, rssi);
    }

    if (mode == ImmobMode::PAIR_BLE && blePairStartMs != 0) {
        if (now - blePairStartMs >= BLE_PAIR_TIMEOUT_MS) {
            Serial.println("[BLE] Pairing timeout — returning to UNLOCKED");
            if (onWarningCb) onWarningCb("BLE pairing\ntimed out");
            mode = ImmobMode::UNLOCKED;
            blePairStartMs = 0;
        }
    }
#endif

    // ── 4. RFID check ─────────────────────────────────────────────────────
    // Poll at 500ms — slower than touch (every loop) to reduce I2C bus contention.
    // Both touch and RFID share M5.In_I2C (GPIO 11/12).
#if RFID_ENABLED
    static uint32_t lastRfidCheck = 0;
    static uint32_t lastCardTime  = 0;

    if (now - lastRfidCheck >= 500) {
        lastRfidCheck = now;

        if (!rfid.PICC_IsNewCardPresent()) goto rfid_done;
        if (!rfid.PICC_ReadCardSerial())   goto rfid_done;

        // Debounce before processing — card is still held open here
        if (now - lastCardTime < 2000) {
            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();
            goto rfid_done;
        }
        lastCardTime = now;

        {
            uint8_t uid[4] = {0, 0, 0, 0};
            uint8_t uidSize = min((uint8_t)rfid.uid.size, (uint8_t)4);
            for (uint8_t i = 0; i < uidSize; i++) uid[i] = rfid.uid.uidByte[i];

            rfid.PICC_HaltA();
            rfid.PCD_StopCrypto1();

            Serial.printf("[RFID] Card: %02X %02X %02X %02X  mode=%d\n",
                          uid[0], uid[1], uid[2], uid[3], (int)mode);

            if (mode == ImmobMode::PROGRAM_FOB) {
                if (saveFob(uid)) {
                    saveFobsToNVS();
                    Serial.printf("[RFID] Fob programmed! Total fobs: %d\n", fobCount);
                    if (onSuccessCb) onSuccessCb("Fob programmed!");
                    mode = ImmobMode::LOCKED;
                } else {
                    Serial.println("[RFID] Fob storage full — cannot save");
                    if (onWarningCb) onWarningCb("Fob storage full");
                    mode = ImmobMode::LOCKED;
                }
            } else if (mode == ImmobMode::LOCKED) {
                if (checkAuthorizedUID(uid)) {
                    Serial.println("[RFID] Authorized fob — unlocking");
                    unlock();   // onUnlockCb fires inside unlock()
                } else {
                    Serial.println("[RFID] Unauthorized fob");
                    if (onWarningCb) onWarningCb("Unknown fob");
                }
            } else if (mode == ImmobMode::UNLOCKED) {
                if (checkAuthorizedUID(uid)) {
                    Serial.println("[RFID] Authorized fob — locking");
                    lock();
                    if (onSuccessCb) onSuccessCb("Locked");
                }
            }
        }
    }
    rfid_done:;
#endif
}

// ============================================================================
// lock / unlock
// ============================================================================

void Immobilizer::lock() {
    mode = ImmobMode::LOCKED;
    pinPosition  = 0;
    currentDigit = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));

    // Schedule DriveInhibit=2 via state machine — do NOT call sendDriveInhibit()
    // directly here, as this may be called from onSDOResult or other callbacks.
    writePending    = true;
    writeInFlight   = false;
    nextRetryMs     = millis();
    writeRetryCount = 0;

    Serial.println("[IMMOBILIZER] LOCKING — DriveInhibit=2 scheduled");
}

void Immobilizer::unlock() {
    mode = ImmobMode::UNLOCKED;
    pinPosition  = 0;
    currentDigit = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));

    writePending    = true;
    writeInFlight   = false;
    nextRetryMs     = millis();
    writeRetryCount = 0;

    Serial.println("[IMMOBILIZER] UNLOCKING — DriveInhibit=0 scheduled");

    // Notify listeners (triggers health check, screen transition, etc.)
    if (onUnlockCb) onUnlockCb();
}

void Immobilizer::toggleLock() {
    if (mode == ImmobMode::UNLOCKED) lock();
    else                              unlock();
}

// ============================================================================
// Programming modes
// ============================================================================

void Immobilizer::startProgramFob() {
    if (mode != ImmobMode::UNLOCKED) return;  // must be unlocked first
    mode = ImmobMode::PROGRAM_FOB;
    Serial.println("[IMMOBILIZER] Fob programming mode — tap new fob now");
}

void Immobilizer::cancelProgramming() {
    if (mode == ImmobMode::UNLOCKED) return;
    // Return to whichever locked/unlocked state makes sense
    // PAIR_BLE is always entered from UNLOCKED, so return there
    mode = (mode == ImmobMode::PAIR_BLE) ? ImmobMode::UNLOCKED : ImmobMode::LOCKED;
    blePairStartMs = 0;
    pinPosition = 0;
    currentDigit = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));
    memset(newPIN1,    0, sizeof(newPIN1));
    Serial.println("[IMMOBILIZER] Programming cancelled");
}

void Immobilizer::clearAllFobs() {
    fobCount = 0;
    memset(authorizedFobs, 0, sizeof(authorizedFobs));
    saveFobsToNVS();
    Serial.println("[IMMOBILIZER] All fobs cleared");
}

void Immobilizer::startChangePin() {
    if (mode != ImmobMode::UNLOCKED) return;
    mode = ImmobMode::CHANGE_PIN_1;
    pinPosition  = 0;
    currentDigit = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));
    memset(newPIN1,    0, sizeof(newPIN1));
    Serial.println("[IMMOBILIZER] PIN change mode — enter new PIN");
}

// ============================================================================
// PIN entry
// ============================================================================

void Immobilizer::enterDigit(uint8_t digit) {
    if (pinPosition >= SECRET_PIN_LENGTH) return;

    enteredPIN[pinPosition++] = digit;
    Serial.printf("[IMMOBILIZER] Digit %d entered, pos %d/%d  mode=%d\n",
                  digit, pinPosition, SECRET_PIN_LENGTH, (int)mode);

    if (pinPosition < SECRET_PIN_LENGTH) return;  // not done yet

    // ── All 4 digits entered — what mode are we in? ───────────────────────

    if (mode == ImmobMode::LOCKED) {
        if (validatePIN()) {
            Serial.println("[IMMOBILIZER] PIN correct — unlocking");
            unlock();
        } else {
            Serial.println("[IMMOBILIZER] PIN incorrect — clearing");
            clearPIN();
        }
        return;
    }

    if (mode == ImmobMode::CHANGE_PIN_1) {
        // Save first pass, ask for confirmation
        memcpy(newPIN1, enteredPIN, SECRET_PIN_LENGTH);
        mode = ImmobMode::CHANGE_PIN_2;
        pinPosition  = 0;
        currentDigit = 0;
        memset(enteredPIN, 0, sizeof(enteredPIN));
        Serial.println("[IMMOBILIZER] PIN first pass stored — confirm new PIN");
        return;
    }

    if (mode == ImmobMode::CHANGE_PIN_2) {
        // Compare both passes
        bool match = true;
        for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
            if (enteredPIN[i] != newPIN1[i]) { match = false; break; }
        }
        if (match) {
            memcpy(storedPIN, newPIN1, SECRET_PIN_LENGTH);
            savePINToNVS();
            Serial.println("[IMMOBILIZER] New PIN saved");
        } else {
            Serial.println("[IMMOBILIZER] PIN confirmation mismatch — cancelled");
        }
        mode = ImmobMode::UNLOCKED;
        pinPosition  = 0;
        currentDigit = 0;
        memset(enteredPIN, 0, sizeof(enteredPIN));
        memset(newPIN1,    0, sizeof(newPIN1));
        return;
    }
}

void Immobilizer::clearPIN() {
    pinPosition  = 0;
    currentDigit = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));
}

bool Immobilizer::validatePIN() {
    for (int i = 0; i < SECRET_PIN_LENGTH; i++) {
        if (enteredPIN[i] != storedPIN[i]) return false;
    }
    return true;
}

// ============================================================================
// Fob helpers
// ============================================================================

bool Immobilizer::saveFob(const uint8_t* uid) {
    // Check for duplicate
    if (checkAuthorizedUID(uid)) {
        Serial.println("[RFID] Fob already stored — skipping duplicate");
        mode = ImmobMode::LOCKED;
        return true;  // treat as success
    }
    if (fobCount >= MAX_AUTHORIZED_FOBS) return false;
    memcpy(authorizedFobs[fobCount], uid, 4);
    fobCount++;
    return true;
}

bool Immobilizer::checkAuthorizedUID(const uint8_t* uid) {
    for (uint8_t i = 0; i < fobCount; i++) {
        if (memcmp(authorizedFobs[i], uid, 4) == 0) return true;
    }
    return false;
}

// ============================================================================
// SDO helpers
// ============================================================================

void Immobilizer::sendDriveInhibit(uint8_t value) {
    if (!sdoManager) {
        Serial.println("[IMMOBILIZER] ERROR: no SDO manager");
        return;
    }
    sdoManager->requestWrite(inhibitWriteId, (int32_t)value,
                             /*highPriority=*/true);
}

void Immobilizer::pollDriveInhibited() {
    if (!sdoManager) return;
    sdoManager->requestRead(inhibitReadId, /*highPriority=*/true);
}

// ============================================================================
// onSDOResult
// ============================================================================

void Immobilizer::onSDOResult(const SDOResult& result) {

    if (result.isWrite && result.paramId == inhibitWriteId) {
        writeInFlight = false;

        if (result.success) {
            writeRetryCount = 0;
            writePending    = false;
            Serial.printf("[IMMOBILIZER] VCU accepted DriveInhibit=%s\n",
                          (mode == ImmobMode::UNLOCKED) ? "0 (off)" : "2 (always)");
            // Confirm by reading back spot value
            pollDriveInhibited();
        } else {
            writeRetryCount++;
            Serial.printf("[IMMOBILIZER] DriveInhibit write FAILED (0x%08X) — retry %d/%d in %ds\n",
                          result.abortCode, writeRetryCount, INHIBIT_MAX_RETRIES,
                          INHIBIT_RETRY_BACKOFF_MS / 1000);
            // Schedule retry via update() — NEVER call sendDriveInhibit() directly
            // here, as that causes a retry storm flooding the TWAI TX queue.
            writePending = true;
            nextRetryMs  = millis() + INHIBIT_RETRY_BACKOFF_MS;
        }
        return;
    }

    if (!result.isWrite && result.paramId == inhibitReadId) {
        if (result.success) {
            bool vcuInhibited = (result.value != 0);
            Serial.printf("[IMMOBILIZER] DriveInhibited readback=%d (%s)\n",
                          result.value, vcuInhibited ? "INHIBITED" : "ALLOWED");

            bool weShouldInhibit = (mode != ImmobMode::UNLOCKED);

            if (weShouldInhibit && !vcuInhibited) {
                Serial.println("[IMMOBILIZER] VCU not inhibited but locked — re-queuing write");
                if (!writePending && !writeInFlight) {
                    writePending    = true;
                    nextRetryMs     = millis();
                    writeRetryCount = 0;
                }
            } else if (!weShouldInhibit && vcuInhibited) {
                Serial.println("[IMMOBILIZER] VCU inhibited but unlocked — re-queuing write");
                if (!writePending && !writeInFlight) {
                    writePending    = true;
                    nextRetryMs     = millis();
                    writeRetryCount = 0;
                }
            }
        } else {
            Serial.printf("[IMMOBILIZER] DriveInhibited read failed (0x%08X)\n",
                          result.abortCode);
        }
        return;
    }
}

// ============================================================================
// CAN monitoring
// ============================================================================

void Immobilizer::processCANMessage(uint32_t id, uint8_t* data, uint8_t len) {
    if (id == VCU_HEARTBEAT_ID) {
        lastVCUHeartbeat = millis();
    }
}

// ============================================================================
// BLE UUID management
// ============================================================================

void Immobilizer::startPairBLE() {
    if (mode != ImmobMode::UNLOCKED) return;
    mode = ImmobMode::PAIR_BLE;
    blePairStartMs = millis();
    Serial.println("[BLE] Pairing mode — bring phone/fob close to dial");
}

void Immobilizer::clearBLEUUIDs() {
    bleUUIDCount = 0;
    memset(bleUUIDs, 0, sizeof(bleUUIDs));
    saveBLEToNVS();
    Serial.println("[BLE] All UUIDs cleared");
}

// Called from BLE scanner task (core 0).
// Two behaviours:
//   PAIR_BLE mode:  if RSSI >= BLE_PAIR_RSSI, save UUID and return to UNLOCKED
//   Normal mode:    if UUID whitelisted and RSSI >= BLE_UNLOCK_RSSI, unlock
void Immobilizer::onBLEDevice(const char* uuid, int rssi) {
    if (!uuid || uuid[0] == '\0') return;

    if (mode == ImmobMode::PAIR_BLE) {
        if (rssi < BLE_PAIR_RSSI) return;  // too far away — ignore

        // Check for duplicate
        for (uint8_t i = 0; i < bleUUIDCount; i++) {
            if (strncmp(bleUUIDs[i], uuid, BLE_UUID_LEN - 1) == 0) {
                Serial.printf("[BLE] UUID already stored — %s\n", uuid);
                mode = ImmobMode::UNLOCKED;
                blePairStartMs = 0;
                return;
            }
        }
        if (bleUUIDCount >= MAX_BLE_UUIDS) {
            Serial.println("[BLE] UUID storage full");
            mode = ImmobMode::UNLOCKED;
            blePairStartMs = 0;
            return;
        }
        strncpy(bleUUIDs[bleUUIDCount++], uuid, BLE_UUID_LEN - 1);
        bleUUIDs[bleUUIDCount - 1][BLE_UUID_LEN - 1] = '\0';
        saveBLEToNVS();
        Serial.printf("[BLE] UUID paired: %s  total=%d\n", uuid, bleUUIDCount);
        if (onSuccessCb) onSuccessCb("BLE beacon paired!");
        mode = ImmobMode::UNLOCKED;
        blePairStartMs = 0;
        // Apply a 10-second cooldown before BLE proximity unlock is active.
        // Prevents the just-paired phone from immediately triggering unlock
        // (and re-firing onUnlockCb in a loop) while still in range.
        bleUnlockCooldown = millis() + 10000;
        return;
    }

    // Normal operation — unlock only, never lock
    if (mode != ImmobMode::LOCKED) return;
    if (rssi < BLE_UNLOCK_RSSI) return;
    // Respect cooldown after pairing to prevent immediate re-unlock loop
    if (bleUnlockCooldown != 0 && millis() < bleUnlockCooldown) return;

    for (uint8_t i = 0; i < bleUUIDCount; i++) {
        if (strncmp(bleUUIDs[i], uuid, BLE_UUID_LEN - 1) == 0) {
            Serial.printf("[BLE] Known UUID in range (RSSI=%d) — unlocking\n", rssi);
            // BLE unlock bypasses health check — use dedicated callback
            // that goes straight to dashboard rather than health check screen
            mode = ImmobMode::UNLOCKED;
            clearPIN();
            writePending    = true;
            writeInFlight   = false;
            nextRetryMs     = millis();
            writeRetryCount = 0;
            if (onBLEUnlockCb) onBLEUnlockCb();
            else if (onUnlockCb) onUnlockCb();  // fallback if BLE callback not set
            return;
        }
    }
}

// ============================================================================
// BLE NVS helpers
// ============================================================================

void Immobilizer::loadBLEFromNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    bleUUIDCount = prefs.getUChar(NVS_KEY_BLE_COUNT, 0);
    if (bleUUIDCount > MAX_BLE_UUIDS) bleUUIDCount = 0;
    for (uint8_t i = 0; i < bleUUIDCount; i++) {
        String key = String(NVS_KEY_BLE_PREFIX) + String(i);
        size_t len = prefs.getBytes(key.c_str(), bleUUIDs[i], BLE_UUID_LEN - 1);
        bleUUIDs[i][len] = '\0';
    }
    prefs.end();
    Serial.printf("[BLE] %d UUID(s) loaded from NVS\n", bleUUIDCount);
}

void Immobilizer::saveBLEToNVS() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_KEY_BLE_COUNT, bleUUIDCount);
    for (uint8_t i = 0; i < bleUUIDCount; i++) {
        String key = String(NVS_KEY_BLE_PREFIX) + String(i);
        prefs.putBytes(key.c_str(), bleUUIDs[i], strlen(bleUUIDs[i]) + 1);
    }
    prefs.end();
}
