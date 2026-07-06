#include "BleTelemetry.h"

#if BLE_TELEMETRY_ENABLED

// ============================================================================
// GATT server callbacks — connection state
// ============================================================================
class TelemetryServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        BleTelemetry::instance().onServerConnect();
    }
    void onDisconnect(BLEServer* server) override {
        BleTelemetry::instance().onServerDisconnect();
    }
};

// ============================================================================
// Generic parameter bus — directory table
// Virtual IDs are decoupled from ZombieVerter's internal SDO param numbers,
// so other hardware implementing this same bus doesn't need to replicate
// this firmware's exact parameter scheme. See PROTOCOL.md.
// ============================================================================
const ParamBusEntry BleTelemetry::paramDirectory[PARAM_BUS_DIR_COUNT] = {
    { PARAM_SOC,       PARAM_FLAG_READABLE,                        0, 0,     100,   {'S','O','C',0,0,0,0,0},                 {'%',0,0} },
    { PARAM_PACK_V,    PARAM_FLAG_READABLE,                       -1, 0,     6000,  {'P','a','c','k','V',0,0,0},             {'V',0,0} },
    { PARAM_MOTOR_TMP, PARAM_FLAG_READABLE,                       -1, -400,  2000,  {'M','o','t','o','r','T','m','p'},       {'C',0,0} },
    { PARAM_PACK_A,    PARAM_FLAG_READABLE,                       -1, -30000,30000, {'P','a','c','k','A',0,0,0},             {'A',0,0} },
    { PARAM_REGEN,     PARAM_FLAG_READABLE|PARAM_FLAG_WRITABLE,    0, 0,     35,    {'R','e','g','e','n',0,0,0},             {'%',0,0} },
    { PARAM_THROTTLE,  PARAM_FLAG_READABLE|PARAM_FLAG_WRITABLE,    0, 0,     100,   {'T','h','r','o','t','M','a','x'},       {'%',0,0} },
    { PARAM_GEAR,      PARAM_FLAG_READABLE|PARAM_FLAG_WRITABLE,    0, 0,     3,     {'G','e','a','r',0,0,0,0},               {'-',0,0} },
};

// ============================================================================
// Param bus write callbacks
// ============================================================================
class DirIndexWriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override {
        std::string val = chr->getValue();
        if (val.length() != 1) return;
        BleTelemetry::instance().onDirIndexWrite((uint8_t)val[0]);
    }
};

class ParamReadReqCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override {
        std::string val = chr->getValue();
        if (val.length() != 2) return;
        uint16_t id = (uint16_t)((uint8_t)val[0]) | ((uint16_t)((uint8_t)val[1]) << 8);
        BleTelemetry::instance().onParamReadReq(id);
    }
};

class ParamWriteReqCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override {
        std::string val = chr->getValue();
        if (val.length() != 4) return;
        uint16_t id = (uint16_t)((uint8_t)val[0]) | ((uint16_t)((uint8_t)val[1]) << 8);
        int16_t value = (int16_t)(((uint8_t)val[2]) | (((uint8_t)val[3]) << 8));
        BleTelemetry::instance().onParamWriteReq(id, value);
    }
};

// ============================================================================
// Auth write callback — proximity-unlock token, see Immobilizer.
// Any length is forwarded as-is; Immobilizer validates length/content.
// ============================================================================
class AuthWriteCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override {
        std::string val = chr->getValue();
        if (val.empty()) return;
        BleTelemetry::instance().onAuthWrite((const uint8_t*)val.data(), val.length());
    }
};

// ============================================================================
// begin
// ============================================================================
void BleTelemetry::begin(CANDataManager* canMgr) {
    canManager = canMgr;

    BLEDevice::init(BLE_DEVICE_NAME);

    pServer = BLEDevice::createServer();
    if (!pServer) { Serial.println("[BLE] FAILED — createServer() returned null"); return; }
    pServer->setCallbacks(new TelemetryServerCallbacks());

    // Explicit handle count: default (15) is far too small. One shared ack
    // char + 6 generic bus chars (2 notify + 4 write) need well under 40,
    // but keep headroom for future additions without re-tuning this again.
    BLEService* pService = pServer->createService(BLEUUID(BLE_SERVICE_UUID), 40);
    if (!pService) { Serial.println("[BLE] FAILED — createService() returned null"); return; }

    pAckChar = pService->createCharacteristic(
        BLE_CHAR_ACK_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pAckChar->addDescriptor(new BLE2902());

    // ---- Generic parameter bus ----
    pDirCountChar = pService->createCharacteristic(
        BLE_CHAR_DIR_COUNT_UUID,
        BLECharacteristic::PROPERTY_READ);
    uint8_t dirCount = PARAM_BUS_DIR_COUNT;
    pDirCountChar->setValue(&dirCount, 1);

    pDirIndexChar = pService->createCharacteristic(
        BLE_CHAR_DIR_INDEX_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pDirIndexChar->setCallbacks(new DirIndexWriteCallbacks());

    pDirEntryChar = pService->createCharacteristic(
        BLE_CHAR_DIR_ENTRY_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pDirEntryChar->addDescriptor(new BLE2902());

    pParamReadReqChar = pService->createCharacteristic(
        BLE_CHAR_PARAM_READ_REQ_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pParamReadReqChar->setCallbacks(new ParamReadReqCallbacks());

    pParamValueChar = pService->createCharacteristic(
        BLE_CHAR_PARAM_VALUE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pParamValueChar->addDescriptor(new BLE2902());

    pParamWriteReqChar = pService->createCharacteristic(
        BLE_CHAR_PARAM_WRITE_REQ_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pParamWriteReqChar->setCallbacks(new ParamWriteReqCallbacks());

    // ---- Proximity-unlock auth token (see Immobilizer) ----
    pAuthChar = pService->createCharacteristic(
        BLE_CHAR_AUTH_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pAuthChar->setCallbacks(new AuthWriteCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    started   = true;
    suspended = false;
    Serial.println("[BLE] Telemetry GATT server started, advertising as " BLE_DEVICE_NAME);
}

// ============================================================================
// update — call every loop() iteration; internally rate-limited.
// Auto-pushes all readable param values so any connected client (watch
// complications especially) gets fresh data without needing to poll.
// ============================================================================
void BleTelemetry::update() {
    if (!started || suspended) return;

    // Consume any buffered auth token first — safe to touch LVGL from here,
    // unlike from onAuthWrite() itself (see header comment).
    if (authPending) {
        authPending = false;
        if (onAuthReceivedCb) onAuthReceivedCb(authBuffer, authBufferLen);
    }

    if (millis() - lastNotifyMs < BLE_TELEMETRY_INTERVAL_MS) return;
    lastNotifyMs = millis();

    if (connected) {
        pushAllReadableParams();
    }
}

void BleTelemetry::pushAllReadableParams() {
    for (int i = 0; i < PARAM_BUS_DIR_COUNT; i++) {
        const ParamBusEntry &e = paramDirectory[i];
        if (!(e.flags & PARAM_FLAG_READABLE)) continue;
        int16_t raw;
        if (readParamRaw(e.id, raw)) {
            sendParamValue(e.id, raw);
        }
    }
}

// ============================================================================
// sendAck
// ============================================================================
void BleTelemetry::sendAck(uint8_t code) {
    if (!pAckChar) return;
    pAckChar->setValue(&code, 1);
    pAckChar->notify();
}

// ============================================================================
// Connection state
// ============================================================================
void BleTelemetry::onServerConnect() {
    connected = true;
    Serial.println("[BLE] Client connected");
}

void BleTelemetry::onServerDisconnect() {
    connected = false;
    lastAuthValid = false;
    Serial.println("[BLE] Client disconnected — restarting advertising");
    if (!suspended) BLEDevice::startAdvertising();
}

// ============================================================================
// suspend / resume — paired with wifiMode in main.cpp, same pattern as
// Immobilizer's bleEnabled-during-WiFi handling.
// ============================================================================
void BleTelemetry::suspend() {
    if (!started || suspended) return;
    suspended = true;
    BLEDevice::getAdvertising()->stop();
    Serial.println("[BLE] Telemetry server suspended (WiFi AP active)");
}

void BleTelemetry::resume() {
    if (!started || !suspended) return;
    suspended = false;
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Telemetry server resumed");
}

// ============================================================================
// Generic parameter bus — directory + read/write handlers
// ============================================================================
void BleTelemetry::sendDirEntry(uint8_t index) {
    if (!pDirEntryChar) return;
    if (index >= PARAM_BUS_DIR_COUNT) return;

    const ParamBusEntry &e = paramDirectory[index];
    uint8_t buf[19];
    buf[0] = (uint8_t)(e.id & 0xFF);
    buf[1] = (uint8_t)((e.id >> 8) & 0xFF);
    buf[2] = e.flags;
    buf[3] = (uint8_t)e.scale;
    buf[4] = (uint8_t)(e.minVal & 0xFF);
    buf[5] = (uint8_t)((e.minVal >> 8) & 0xFF);
    buf[6] = (uint8_t)(e.maxVal & 0xFF);
    buf[7] = (uint8_t)((e.maxVal >> 8) & 0xFF);
    memcpy(&buf[8], e.name, 8);
    memcpy(&buf[16], e.unit, 3);

    pDirEntryChar->setValue(buf, sizeof(buf));
    pDirEntryChar->notify();
}

void BleTelemetry::onDirIndexWrite(uint8_t index) {
    sendDirEntry(index);
}

bool BleTelemetry::readParamRaw(uint16_t paramId, int16_t &outRaw) {
    if (!canManager) return false;

    switch (paramId) {
        case PARAM_SOC: {
            CANParameter* p = canManager->getParameterByName("SOC");
            if (!p) return false;
            outRaw = (int16_t)constrain(p->getValueAsInt(), 0, 100);
            return true;
        }
        case PARAM_PACK_V: {
            CANParameter* p = canManager->getParameterByName("udc");
            if (!p) return false;
            outRaw = (int16_t)(p->getValueAsInt() * 10);
            return true;
        }
        case PARAM_MOTOR_TMP: {
            CANParameter* p = canManager->getParameterByName("tmpm");
            if (!p) p = canManager->getParameterByName("tmphs");
            if (!p) return false;
            outRaw = (int16_t)(p->getValueAsInt() * 10);
            return true;
        }
        case PARAM_PACK_A: {
            CANParameter* p = canManager->getParameterByName("idc");
            if (!p) return false;
            outRaw = (int16_t)(p->getValueAsInt() * 10);
            return true;
        }
        case PARAM_REGEN: {
            CANParameter* p = canManager->getParameter(61);
            if (!p) return false;
            outRaw = (int16_t)abs(p->getValueAsInt()); // stored negative internally
            return true;
        }
        case PARAM_THROTTLE: {
            CANParameter* p = canManager->getParameter(25);
            if (!p) return false;
            outRaw = (int16_t)p->getValueAsInt();
            return true;
        }
        case PARAM_GEAR: {
            CANParameter* p = canManager->getParameter(27);
            if (!p) return false;
            outRaw = (int16_t)p->getValueAsInt();
            return true;
        }
        default:
            return false;
    }
}

void BleTelemetry::sendParamValue(uint16_t paramId, int16_t rawValue) {
    if (!pParamValueChar) return;
    uint8_t buf[4];
    buf[0] = (uint8_t)(paramId & 0xFF);
    buf[1] = (uint8_t)((paramId >> 8) & 0xFF);
    buf[2] = (uint8_t)(rawValue & 0xFF);
    buf[3] = (uint8_t)((rawValue >> 8) & 0xFF);
    pParamValueChar->setValue(buf, sizeof(buf));
    pParamValueChar->notify();
}

void BleTelemetry::onParamReadReq(uint16_t paramId) {
    int16_t raw;
    if (readParamRaw(paramId, raw)) {
        sendParamValue(paramId, raw);
    }
}

void BleTelemetry::onParamWriteReq(uint16_t paramId, int16_t value) {
    if (!canManager) { sendAck(1); return; }

    const ParamBusEntry* entry = nullptr;
    for (int i = 0; i < PARAM_BUS_DIR_COUNT; i++) {
        if (paramDirectory[i].id == paramId) { entry = &paramDirectory[i]; break; }
    }
    if (!entry) { sendAck(1); return; }
    if (!(entry->flags & PARAM_FLAG_WRITABLE)) { sendAck(1); return; }
    if (value < entry->minVal || value > entry->maxVal) { sendAck(1); return; }

    switch (paramId) {
        case PARAM_REGEN:
            canManager->setParameter(61, -(int32_t)value); // VCU wants negative
            break;
        case PARAM_THROTTLE:
            canManager->setParameter(25, (int32_t)value);
            break;
        case PARAM_GEAR: {
            // Soft speed interlock — mirror any hard interlock the VCU
            // itself enforces; don't rely on this BLE-layer check alone.
            CANParameter* pSpeed = canManager->getParameterByName("speed");
            if (pSpeed && pSpeed->getValueAsInt() > BLE_GEAR_INTERLOCK_RPM) {
                Serial.println("[BLE] Gear write rejected — vehicle moving");
                sendAck(1);
                return;
            }
            canManager->setParameter(27, (int32_t)value);
            break;
        }
        default:
            sendAck(1);
            return;
    }
    sendAck(0);
}

void BleTelemetry::onAuthWrite(const uint8_t* data, size_t len) {
    // Do NOT call onAuthReceivedCb here — this runs on the Bluedroid stack's
    // task, not the main loop task LVGL requires. Buffer and defer to
    // update() instead (see header comment).
    if (len > AUTH_BUFFER_LEN) return;
    memcpy(authBuffer, data, len);
    authBufferLen = len;
    authPending = true;

    // Plain byte copy — safe from any thread, unlike the deferred callback
    // dispatch above. Cache so Immobilizer can re-check "is this still the
    // same paired device" later without needing a fresh BLE write.
    memcpy(lastAuthToken, data, len);
    lastAuthLen = len;
    lastAuthValid = true;
}

#endif // BLE_TELEMETRY_ENABLED
