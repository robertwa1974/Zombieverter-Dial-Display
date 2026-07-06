#ifndef BLE_TELEMETRY_H
#define BLE_TELEMETRY_H

// =============================================================================
// BleTelemetry.h — ZombieVerter Dial Display
//
// BLE GATT peripheral (server) exposing a small set of parameters (SOC,
// pack voltage, motor temp, pack current, regen, throttle max, gear) via
// the Generic Parameter Bus (see PROTOCOL.md). Both the Wear OS watch
// companion and the Android phone app speak this same protocol — there is
// only one BLE parameter protocol in this firmware now, not one per client.
//
// (History: an earlier version had separate fixed characteristics just for
// the watch — SOC/voltage/temp/current/regen/gear — alongside this generic
// bus. Those were removed once both clients were migrated to the generic
// bus, to avoid maintaining two overlapping protocols in one firmware.)
//
// IMPORTANT — shares the BLE stack with Immobilizer's BLE proximity-unlock
// scanner (Immobilizer.cpp, guarded by BLE_ENABLED). Both use the classic
// ESP32 Arduino BLE library (BLEDevice.h / Bluedroid) rather than NimBLE —
// running two different BLE libraries in one firmware is not supported and
// will corrupt the controller state. If BLE_ENABLED (scanning) is ever
// turned on at the same time as BLE_TELEMETRY_ENABLED, only ONE of the two
// may call BLEDevice::init(); this class currently owns that call. Revisit
// this if/when both features are enabled together.
//
// RAM NOTE: this hardware has no PSRAM. The classic Bluedroid BLE stack
// typically costs 40-70KB of heap on top of WiFi + LVGL + TWAI, which are
// already tight. BleTelemetry::suspend() is called automatically whenever
// wifiMode is active (mirrors Immobilizer's bleEnabled-during-WiFi pattern)
// so the two radios/stacks are never both fully active at once. Watch
// ESP.getFreeHeap() after adding this — if it's marginal, consider stopping
// the BLE server entirely (not just pausing advertising) during WiFi mode.
// =============================================================================

#include <Arduino.h>
#include <functional>
#include "Config.h"
#include "CANData.h"

#if BLE_TELEMETRY_ENABLED

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =============================================================================
// Generic Parameter Bus — see PROTOCOL.md for the full spec.
//
// A small set of generic characteristics that can address ANY parameter by
// ID, rather than one characteristic per value. Any BLE central (phone,
// watch, PC) discovers what's available and its safe range from the
// directory, rather than the client needing to hardcode a parameter list.
// Deliberately kept fixed-size / small-payload so it works with the
// default ~20 byte ATT payload and needs no MTU negotiation, so it stays
// implementable by other hardware/firmware without extra BLE complexity.
// =============================================================================

#define PARAM_BUS_DIR_COUNT 7

struct ParamBusEntry {
    uint16_t id;
    uint8_t  flags;      // bit0 = readable, bit1 = writable
    int8_t   scale;      // real_value = raw * 10^scale
    int16_t  minVal;
    int16_t  maxVal;
    char     name[8];    // not null-terminated if exactly 8 chars — pad with \0 if shorter
    char     unit[3];
};

#define PARAM_FLAG_READABLE 0x01
#define PARAM_FLAG_WRITABLE 0x02

enum ParamBusId : uint16_t {
    PARAM_SOC = 0,
    PARAM_PACK_V = 1,
    PARAM_MOTOR_TMP = 2,
    PARAM_PACK_A = 3,
    PARAM_REGEN = 4,
    PARAM_THROTTLE = 5,
    PARAM_GEAR = 6
};

class BleTelemetry {
public:
    static BleTelemetry& instance() {
        static BleTelemetry inst;
        return inst;
    }

    // Call once from setup(), after canManager parameters are loaded.
    void begin(CANDataManager* canMgr);

    // Call every loop() iteration in normal (non-WiFi) mode. Internally
    // rate-limited to BLE_TELEMETRY_INTERVAL_MS — cheap to call every loop.
    // Auto-pushes current values of all readable params to any connected
    // client via notify, so watch complications stay snappy without having
    // to poll (the phone app also polls independently — harmless overlap).
    void update();

    // Stop advertising/notifying during WiFi AP mode (radio + heap
    // contention) and resume when WiFi mode ends. Safe to call repeatedly.
    void suspend();
    void resume();

    bool isClientConnected() const { return connected; }

    // Re-checkable "is a device with a known token still connected" — see
    // header note above on lastAuthToken. Returns false if no token has
    // been seen this connection (e.g. device connected but hasn't paired,
    // or just disconnected).
    bool getLastAuthToken(uint8_t* outBuf, size_t& outLen) const {
        if (!lastAuthValid) return false;
        memcpy(outBuf, lastAuthToken, lastAuthLen);
        outLen = lastAuthLen;
        return true;
    }

    // Called with raw token bytes whenever a client writes to the Auth
    // characteristic. BleTelemetry does no interpretation of the token
    // itself — that's Immobilizer's job (pairing vs. unlock-check, storage,
    // etc.), same separation of concerns as SDO result routing in main.cpp.
    void setOnAuthReceived(std::function<void(const uint8_t*, size_t)> cb) {
        onAuthReceivedCb = cb;
    }

    // ---- Internal — called from BLE callback classes in the .cpp only ----
    void onServerConnect();
    void onServerDisconnect();
    void onDirIndexWrite(uint8_t index);
    void onParamReadReq(uint16_t paramId);
    void onParamWriteReq(uint16_t paramId, int16_t value);
    void onAuthWrite(const uint8_t* data, size_t len);

private:
    BleTelemetry() = default;
    BleTelemetry(const BleTelemetry&) = delete;
    void operator=(const BleTelemetry&) = delete;

    CANDataManager* canManager = nullptr;

    BLEServer*         pServer   = nullptr;
    BLECharacteristic* pAckChar  = nullptr;
    BLECharacteristic* pAuthChar = nullptr;

    std::function<void(const uint8_t*, size_t)> onAuthReceivedCb;

    // Deferred auth handling — see .cpp update() for why. The BLE write
    // callback runs on the Bluedroid stack's own task, NOT the main loop
    // task that LVGL requires. Calling onAuthReceivedCb directly from the
    // callback (which ends up touching LVGL via Immobilizer's success/
    // unlock callbacks) caused a StoreProhibited crash. Buffer here instead
    // and only fire the callback from update(), same pattern the existing
    // BLE scanner uses (g_bleResult.ready) for exactly this reason.
    static const size_t AUTH_BUFFER_LEN = 32;
    volatile bool authPending = false;
    uint8_t authBuffer[AUTH_BUFFER_LEN];
    size_t authBufferLen = 0;

    // Last valid auth token seen THIS connection — lets Immobilizer
    // re-check "is the paired device still here" continuously (e.g. right
    // after a deliberate lock) rather than only at the moment of connect.
    // Cleared on disconnect. Plain byte copy only — safe from any thread.
    bool    lastAuthValid = false;
    uint8_t lastAuthToken[AUTH_BUFFER_LEN];
    size_t  lastAuthLen = 0;

    // Generic parameter bus
    BLECharacteristic* pDirCountChar      = nullptr;
    BLECharacteristic* pDirIndexChar      = nullptr;
    BLECharacteristic* pDirEntryChar      = nullptr;
    BLECharacteristic* pParamReadReqChar  = nullptr;
    BLECharacteristic* pParamValueChar    = nullptr;
    BLECharacteristic* pParamWriteReqChar = nullptr;

    static const ParamBusEntry paramDirectory[PARAM_BUS_DIR_COUNT];

    bool     started   = false;
    bool     connected = false;
    bool     suspended = false;
    uint32_t lastNotifyMs = 0;

    void pushAllReadableParams();
    void sendAck(uint8_t code);
    void sendDirEntry(uint8_t index);
    void sendParamValue(uint16_t paramId, int16_t rawValue);
    bool readParamRaw(uint16_t paramId, int16_t &outRaw);
};

#endif // BLE_TELEMETRY_ENABLED
#endif // BLE_TELEMETRY_H
