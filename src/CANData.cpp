#include "CANData.h"
#include "Config.h"
#include "driver/twai.h"
#include <SPIFFS.h>

// Static instance pointer for SDO callback
CANDataManager* CANDataManager::instance = nullptr;

void CANDataManager::onSDOResult(const SDOResult& result) {
    if (!instance) return;
    if (!result.success) {
        #if DEBUG_CAN
        Serial.printf("[SDO CB] Param %d failed (abort 0x%08X)\n", result.paramId, result.abortCode);
        #endif
        return;
    }
    if (result.isWrite) {
        #if DEBUG_CAN
        Serial.printf("[SDO CB] Write confirmed param %d\n", result.paramId);
        #endif
        return;
    }

    // ZombieVerter SDO values are always fixed-point ×32 regardless of param type
    instance->updateParameterBySDOId(result.paramId, result.value / 32);
    #if DEBUG_CAN
    Serial.printf("[SDO CB] Read param %d raw=%d scaled=%d\n",
                  result.paramId, result.value, result.value / 32);
    #endif
}

// ============================================================================
// CANParameter helpers — setValue and getValueAsInt are inline in the struct
// ============================================================================

// ============================================================================
// Constructor
// ============================================================================

CANDataManager::CANDataManager()
    : parameterCount(0), txHead(0), txTail(0), rxHead(0), rxTail(0),
      connected(false), lastMessageTime(0), bmsCellCount(0)
{
    instance = this;
    for (uint8_t i = 0; i < MAX_BMS_CELLS; i++) {
        bmsCellVoltages[i]    = 0;
        bmsCellUpdateTimes[i] = 0;
    }
}

// ============================================================================
// init
// ============================================================================

bool CANDataManager::init() {
    twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN,
        (gpio_num_t)CAN_RX_PIN,
        TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = 10;
    g_config.tx_queue_len = 10;

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        Serial.println("[CAN] TWAI driver install failed");
        return false;
    }
    if (twai_start() != ESP_OK) {
        Serial.println("[CAN] TWAI start failed");
        return false;
    }
    Serial.println("[CAN] TWAI initialized at 500kbps");
    // Note: SDOManager is NOT started here — call initSDO() after fetchParamsFromVCU()
    return true;
}

bool CANDataManager::initSDO() {
    if (!sdoManager.init(onSDOResult)) {
        Serial.println("[CAN] SDOManager init failed");
        return false;
    }
    Serial.println("[CAN] SDOManager initialized");
    return true;
}

// ============================================================================
// sdoSendRaw — send 8 bytes directly via TWAI (bypasses SDOManager)
// Used only during fetchParamsFromVCU before SDOManager starts
// ============================================================================

bool CANDataManager::sdoSendRaw(uint8_t* data8) {
    twai_message_t tx = {};
    tx.identifier       = SDO_TX_ID;
    tx.data_length_code = 8;
    tx.extd             = 0;
    memcpy(tx.data, data8, 8);
    return twai_transmit(&tx, pdMS_TO_TICKS(10)) == ESP_OK;
}

// ============================================================================
// sdoReceiveRaw — wait for an SDO response frame directly from TWAI
// Discards non-SDO frames (CAN broadcasts etc.)
// ============================================================================

bool CANDataManager::sdoReceiveRaw(twai_message_t* frame, uint32_t timeoutMs) {
    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (twai_receive(frame, pdMS_TO_TICKS(5)) == ESP_OK) {
            if (frame->identifier == SDO_RX_ID) return true;
        }
    }
    return false;
}

// ============================================================================
// fetchParamsFromVCU — download parameter schema from VCU via SDO segmented
// transfer on index 0x5001. Saves result to SPIFFS as /params.json and
// loads into parameters[].
//
// Must be called AFTER init() (TWAI running) but the SDOManager internal
// task is already started — however fetchParamsFromVCU drains the TWAI RX
// directly before SDOManager can consume any frames, so we pause SDOManager
// by calling this immediately after init() before any requestParameter calls.
//
// Blocks for up to ~10 seconds for a 30KB download.
// ============================================================================

#define SDO_IDX_STRINGS_LO  0x01
#define SDO_IDX_STRINGS_HI  0x50
#define SDO_CMD_UPLOAD_REQ  0x40
#define SDO_CMD_SEGMENT_REQ 0x60
#define SDO_TOGGLE_BIT_MASK 0x10
#define SDO_LAST_SEGMENT    0x01
#define SDO_ABORT_BYTE      0x80

FetchResult CANDataManager::fetchParamsFromVCU() {
    Serial.println("[Fetch] Starting VCU parameter download via SDO...");

    // Step 1: Send initiate upload request for index 0x5001 subindex 0
    uint8_t initReq[8] = {
        SDO_CMD_UPLOAD_REQ,
        SDO_IDX_STRINGS_LO,
        SDO_IDX_STRINGS_HI,
        0x00,
        0, 0, 0, 0
    };

    if (!sdoSendRaw(initReq)) {
        Serial.println("[Fetch] Failed to send initiate upload request");
        return FetchResult::CAN_ERROR;
    }

    // Step 2: Wait for initiate upload response
    twai_message_t frame;
    if (!sdoReceiveRaw(&frame, 1000)) {
        Serial.println("[Fetch] Timeout waiting for initiate upload response");
        return FetchResult::TIMEOUT;
    }

    if (frame.data[0] == SDO_ABORT_BYTE) {
        Serial.printf("[Fetch] SDO abort on init: 0x%08X\n",
            (unsigned)(frame.data[4] | (frame.data[5]<<8) |
                       (frame.data[6]<<16) | (frame.data[7]<<24)));
        return FetchResult::TIMEOUT;
    }

    uint32_t totalSize = 0;
    if (frame.data[0] & 0x01) {
        totalSize = frame.data[4] | (frame.data[5]<<8) |
                    (frame.data[6]<<16) | (frame.data[7]<<24);
        Serial.printf("[Fetch] Total JSON size: %u bytes\n", (unsigned)totalSize);
    }

    String jsonBuffer;
    if (totalSize > 0 && totalSize < 65536) {
        jsonBuffer.reserve(totalSize + 64);
    }

    // Step 3: Download segments
    bool toggleBit = false;
    uint32_t segmentCount = 0;
    uint32_t lastProgress = millis();

    while (true) {
        uint8_t segReq[8] = {
            (uint8_t)(SDO_CMD_SEGMENT_REQ | (toggleBit ? SDO_TOGGLE_BIT_MASK : 0)),
            0, 0, 0, 0, 0, 0, 0
        };

        if (!sdoSendRaw(segReq)) {
            Serial.println("[Fetch] Failed to send segment request");
            return FetchResult::CAN_ERROR;
        }

        if (!sdoReceiveRaw(&frame, 500)) {
            Serial.printf("[Fetch] Timeout on segment %u\n", (unsigned)segmentCount);
            return FetchResult::TIMEOUT;
        }

        if (frame.data[0] == SDO_ABORT_BYTE) {
            Serial.println("[Fetch] SDO abort during download");
            return FetchResult::TIMEOUT;
        }

        bool isLast = (frame.data[0] & SDO_LAST_SEGMENT) != 0;
        int unusedBytes = (frame.data[0] >> 1) & 0x07;
        int bytesInSegment = isLast ? (7 - unusedBytes) : 7;

        for (int i = 0; i < bytesInSegment; i++) {
            jsonBuffer += (char)frame.data[1 + i];
        }

        segmentCount++;
        toggleBit = !toggleBit;

        if (millis() - lastProgress > 2000) {
            lastProgress = millis();
            Serial.printf("[Fetch] %u bytes downloaded...\n", (unsigned)jsonBuffer.length());
        }

        if (isLast) break;
    }

    Serial.printf("[Fetch] Download complete: %u bytes in %u segments\n",
        (unsigned)jsonBuffer.length(), (unsigned)segmentCount);

    // Step 4: Validate JSON
    if (jsonBuffer.length() < 10 || jsonBuffer[0] != '{') {
        Serial.println("[Fetch] Invalid JSON received");
        return FetchResult::PARSE_ERROR;
    }

    // Quick parse to validate
    {
        JsonDocument testDoc;
        DeserializationError err = deserializeJson(testDoc, jsonBuffer.c_str(), jsonBuffer.length());
        if (err != DeserializationError::Ok) {
            Serial.printf("[Fetch] JSON parse error: %s\n", err.c_str());
            return FetchResult::PARSE_ERROR;
        }
        Serial.printf("[Fetch] JSON valid — %u top-level keys\n", (unsigned)testDoc.size());
    }

    // Step 5: Save to SPIFFS
    File f = SPIFFS.open("/params.json", "w");
    if (f) {
        f.print(jsonBuffer);
        f.close();
        Serial.printf("[Fetch] Saved /params.json (%u bytes)\n", (unsigned)jsonBuffer.length());
    } else {
        Serial.println("[Fetch] WARNING: Could not write /params.json");
    }

    // Step 6: Load into parameters[]
    if (!loadParametersFromJSON(jsonBuffer.c_str())) {
        Serial.println("[Fetch] Failed to load parameters");
        return FetchResult::PARSE_ERROR;
    }

    Serial.printf("[Fetch] Loaded %u parameters from VCU\n", (unsigned)parameterCount);
    return FetchResult::SUCCESS;
}

// ============================================================================
// update
// ============================================================================

void CANDataManager::update() {
    twai_message_t rx_message;
    while (twai_receive(&rx_message, 0) == ESP_OK) {
        #if DEBUG_CAN
        Serial.printf("CAN RX: ID=0x%03X Len=%d Data=[", rx_message.identifier, rx_message.data_length_code);
        for (int i = 0; i < rx_message.data_length_code; i++) {
            if (i) Serial.print(" ");
            Serial.printf("%02X", rx_message.data[i]);
        }
        Serial.println("]");
        #endif

        if (rx_message.identifier == SDO_RX_ID) {
            sdoManager.processIncomingFrame(rx_message);
            lastMessageTime = millis();
            connected = true;
            continue;
        }

        CANMessage msg;
        msg.id        = rx_message.identifier;
        msg.length    = rx_message.data_length_code;
        msg.timestamp = millis();
        for (int i = 0; i < rx_message.data_length_code && i < 8; i++) {
            msg.data[i] = rx_message.data[i];
        }
        enqueueRx(msg);
        lastMessageTime = millis();
        connected = true;
    }

    CANMessage rxMsg;
    while (dequeueRx(rxMsg)) {
        processReceivedMessage(rxMsg);
    }

    CANMessage txMsg;
    if (dequeueTx(txMsg)) {
        twai_message_t tx_message;
        tx_message.identifier       = txMsg.id;
        tx_message.data_length_code = txMsg.length;
        tx_message.flags            = TWAI_MSG_FLAG_NONE;
        for (int i = 0; i < txMsg.length; i++) {
            tx_message.data[i] = txMsg.data[i];
        }
        twai_transmit(&tx_message, pdMS_TO_TICKS(100));
    }

    if (connected && (millis() - lastMessageTime > 5000)) {
        connected = false;
    }
}

// ============================================================================
// requestParameter
// ============================================================================

void CANDataManager::requestParameter(uint16_t paramId) {
    // Skip SDO polling for params that come from CAN broadcasts.
    // These have canid in params.json and are updated by handleGenericMessage.
    // Polling them via SDO returns ×32 encoded values which would corrupt
    // the correctly-scaled broadcast values used by the dial display.
    static const uint16_t broadcastIds[] = {
        2006,  // udc       (0x522)
        2012,  // idc       (0x411)
        2015,  // SOC       (0x355)
        2016,  // speed     (0x257)
        2028,  // tmphs     (0x126)
        2029,  // tmpm      (0x356)
        2084,  // BMS_Vmin  (0x373)
        2085,  // BMS_Vmax  (0x373)
        2070,  // U12V      (0x528)
        0
    };
    for (int i = 0; broadcastIds[i] != 0; i++) {
        if (paramId == broadcastIds[i]) return;
    }
    sdoManager.requestRead(paramId);
}

// ============================================================================
// setParameter — called from web UI with VCU id
// ============================================================================

void CANDataManager::setParameter(uint16_t paramId, int32_t value) {
    CANMessage msg;
    msg.timestamp = millis();

    // Map VCU param IDs to CAN frames for real-time control params
    // Gear: VCU id=27
    if (paramId == 27) {
        msg.id = 0x300; msg.length = 8;
        msg.data[0] = (uint8_t)value;
        memset(msg.data + 1, 0, 7);
        updateParameterBySDOId(27, value);
        enqueueTx(msg);
        return;
    }
    // MotActive: VCU id=129
    if (paramId == 129) {
        msg.id = 0x301; msg.length = 8;
        msg.data[0] = (uint8_t)value;
        memset(msg.data + 1, 0, 7);
        updateParameterBySDOId(129, value);
        enqueueTx(msg);
        return;
    }
    // regenmax: VCU id=61
    if (paramId == 61) {
        msg.id = 0x302; msg.length = 8;
        msg.data[0] =  value       & 0xFF;
        msg.data[1] = (value >> 8) & 0xFF;
        memset(msg.data + 2, 0, 6);
        updateParameterBySDOId(61, value);
        enqueueTx(msg);
        return;
    }

    // All other params: SDO write with ×32 fixed-point scaling
    if (paramId < 1000) {
        sdoManager.requestWrite((uint8_t)paramId, value * 32, true);
    }
}

// ============================================================================
// loadParametersFromJSON
// Handles both formats:
//   Array:  {"parameters":[{"id":N,"name":"x",...},...]}
//   Flat:   {"name":{"id":N,"isparam":true,"minimum":x,"maximum":y,...},...}
//           (openinverter/ZombieVerter VCU direct export)
// ============================================================================

bool CANDataManager::loadParametersFromJSON(const char* jsonString) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.printf("[CAN] JSON parse failed: %s\n", error.c_str());
        return false;
    }

    parameterCount = 0;

    if (doc["parameters"].is<JsonArray>()) {
        // ---- Array format ----
        for (JsonObject param : doc["parameters"].as<JsonArray>()) {
            if (parameterCount >= MAX_PARAMETERS) break;

            CANParameter& p = parameters[parameterCount];
            p.id       = param["id"];
            strncpy(p.name, param["name"] | "Unknown", 31);
            p.name[31] = '\0';
            p.editable = param["editable"] | false;
            p.setValue((int32_t)(param["value"] | 0.0f));
            parameterCount++;
        }
    } else {
        // ---- Flat openinverter/ZombieVerter VCU format ----
        for (JsonPair kv : doc.as<JsonObject>()) {
            if (parameterCount >= MAX_PARAMETERS) break;

            JsonObject param = kv.value().as<JsonObject>();

            // Skip entries without an id field (e.g. "serial")
            if (!param["id"].is<int>()) continue;

            CANParameter& p = parameters[parameterCount];
            p.id            = param["id"].as<uint16_t>();
            strncpy(p.name, kv.key().c_str(), 31);
            p.name[31]      = '\0';
            p.editable      = param["isparam"] | false;
            p.fromBroadcast = param["canid"].is<int>();  // has canid = comes from CAN broadcast
            p.setValue((int32_t)(param["value"] | 0.0f));
            parameterCount++;
        }
    }

    Serial.printf("[CAN] Loaded %d parameters\n", parameterCount);
    return parameterCount > 0;
}

// ============================================================================
// getParameter — lookup by VCU id
// ============================================================================

CANParameter* CANDataManager::getParameter(uint16_t id) {
    for (uint16_t i = 0; i < parameterCount; i++) {
        if (parameters[i].id == id) return &parameters[i];
    }
    return nullptr;
}

// ============================================================================
// getParameterByName — lookup by name string
// ============================================================================

CANParameter* CANDataManager::getParameterByName(const char* name) {
    for (uint16_t i = 0; i < parameterCount; i++) {
        if (strcmp(parameters[i].name, name) == 0) return &parameters[i];
    }
    return nullptr;
}

CANParameter* CANDataManager::getParameterByIndex(uint8_t index) {
    if (index < parameterCount) return &parameters[index];
    return nullptr;
}

// ============================================================================
// updateParameterBySDOId — updates by VCU short id (1-999)
// Used by SDO callback and CAN broadcast handlers
// ============================================================================

void CANDataManager::updateParameterBySDOId(uint16_t sdoId, int32_t value) {
    CANParameter* param = getParameter(sdoId);
    if (param) {
        param->setValue(value);
        return;
    }
    // VCU spot values use id >= 2000; try name-based mapping for key spots
    // These are the broadcast params we care about most
    static const struct { uint16_t sdoId; const char* name; } sdoMap[] = {
        {2,   "speed"},
        {3,   "udc"},
        {7,   "tmphs"},
        {8,   "tmpm"},
        {14,  "din_forward"},
        {15,  "din_reverse"},
        {0,   nullptr}
    };
    for (int i = 0; sdoMap[i].name; i++) {
        if (sdoMap[i].sdoId == sdoId) {
            CANParameter* p = getParameterByName(sdoMap[i].name);
            if (p) p->setValue(value);
            return;
        }
    }
}

// ============================================================================
// updateParameterIfExists — legacy alias, looks up by VCU id then by name map
// ============================================================================

void CANDataManager::updateParameterIfExists(uint16_t paramId, int32_t value) {
    updateParameterBySDOId(paramId, value);
}

// ============================================================================
// processReceivedMessage
// ============================================================================

void CANDataManager::processReceivedMessage(CANMessage& msg) {
    #if DEBUG_CAN
    Serial.printf("Processing CAN ID: 0x%03X\n", msg.id);
    #endif

    if ((msg.id >= 0x400 && msg.id < 0x500) || (msg.id >= 0x600 && msg.id < 0x700)) {
        handleBMSCellVoltage(msg);
        return;
    }

    if (msg.id == (uint32_t)(0x580 + CAN_NODE_ID)) {
        handleSDOResponse(msg);
    } else if (msg.id == (uint32_t)(0x180 + CAN_NODE_ID) ||
               msg.id == (uint32_t)(0x280 + CAN_NODE_ID) ||
               msg.id == (uint32_t)(0x380 + CAN_NODE_ID) ||
               msg.id == (uint32_t)(0x480 + CAN_NODE_ID)) {
        handlePDOMessage(msg);
    } else {
        handleGenericMessage(msg);
    }
}

// ============================================================================
// handleSDOResponse — legacy fallback, also applies ÷32 scaling
// ============================================================================

void CANDataManager::handleSDOResponse(CANMessage& msg) {
    if (msg.length < 8) return;
    uint8_t cmd      = msg.data[0];
    uint8_t indexLow = msg.data[1];
    uint8_t indexHigh= msg.data[2];
    uint8_t paramId  = msg.data[3];
    uint16_t index   = indexLow | (indexHigh << 8);
    if (index != 0x2100) return;
    if (cmd == 0x43 || cmd == 0x4B) {
        int32_t raw = msg.data[4] | (msg.data[5] << 8) |
                      (msg.data[6] << 16) | (msg.data[7] << 24);
        updateParameterBySDOId(paramId, raw / 32);
    }
}

// ============================================================================
// handlePDOMessage
// ============================================================================

void CANDataManager::handlePDOMessage(CANMessage& msg) {
    uint8_t pdoNum = 0;
    if      (msg.id == (uint32_t)(0x180 + CAN_NODE_ID)) pdoNum = 1;
    else if (msg.id == (uint32_t)(0x280 + CAN_NODE_ID)) pdoNum = 2;
    else if (msg.id == (uint32_t)(0x380 + CAN_NODE_ID)) pdoNum = 3;
    else if (msg.id == (uint32_t)(0x480 + CAN_NODE_ID)) pdoNum = 4;

    if (pdoNum == 1 && msg.length >= 8) {
        // Use name-based lookup since VCU ids are now 2000+
        CANParameter* p;
        p = getParameterByName("speed");
        if (p) p->setValue((int16_t)(msg.data[0] | (msg.data[1] << 8)));
        p = getParameterByName("udc");
        if (p) p->setValue((int16_t)(msg.data[2] | (msg.data[3] << 8)));
        p = getParameterByName("tmphs");
        if (p) p->setValue((int16_t)(msg.data[4] | (msg.data[5] << 8)));
        p = getParameterByName("tmpm");
        if (p) p->setValue((int16_t)(msg.data[6] | (msg.data[7] << 8)));
    } else if (pdoNum == 2 && msg.length >= 8) {
        CANParameter* p;
        p = getParameterByName("tmphs");
        if (p) p->setValue((int16_t)(msg.data[0] | (msg.data[1] << 8)));
        p = getParameterByName("tmpm");
        if (p) p->setValue((int16_t)(msg.data[2] | (msg.data[3] << 8)));
    }
}

// ============================================================================
// handleGenericMessage
// ============================================================================

void CANDataManager::handleGenericMessage(CANMessage& msg) {
    // Direct CAN control echoes
    if (msg.id == 0x300 && msg.length >= 1) {
        CANParameter* p = getParameterByName("Gear");
        if (p) p->setValue(msg.data[0]);
        return;
    }
    if (msg.id == 0x301 && msg.length >= 1) {
        CANParameter* p = getParameterByName("MotActive");
        if (p) p->setValue(msg.data[0]);
        return;
    }
    if (msg.id == 0x302 && msg.length >= 2) {
        CANParameter* p = getParameterByName("regenmax");
        if (p) p->setValue((int16_t)(msg.data[0] | (msg.data[1] << 8)));
        return;
    }

    // IVT-S shunt — 24-bit signed value in bytes 2-4, little-endian
    auto ivt24 = [](const CANMessage& m) -> int32_t {
        int32_t v = m.data[2] | (m.data[3] << 8) | (m.data[4] << 16);
        if (v & 0x800000) v |= 0xFF000000;
        return v;
    };

    if (msg.id == 0x522 && msg.length == 6) {
        CANParameter* p = getParameterByName("udc");
        if (p) p->setValue(ivt24(msg) / 1000);
        return;
    }
    if (msg.id == 0x411 && msg.length == 6) {
        CANParameter* p = getParameterByName("idc");
        if (p) p->setValue(ivt24(msg) / 1000);
        return;
    }
    if (msg.id == 0x526 && msg.length == 6) {
        // shunt temp — no standard VCU param name, skip
        return;
    }
    // Unused IVT-S channels
    if ((msg.id == 0x521 || msg.id == 0x523 || msg.id == 0x524 ||
         msg.id == 0x525 || msg.id == 0x527 || msg.id == 0x528) &&
        msg.length == 6) { return; }

    if (msg.id == 0x355 && msg.length >= 2) {
        CANParameter* p = getParameterByName("SOC");
        if (p) p->setValue((int16_t)(msg.data[0] | (msg.data[1] << 8)));
        return;
    }

    if (msg.id == 0x356 && msg.length >= 6) {
        int16_t tmpm = msg.data[4] | (msg.data[5] << 8);
        if (tmpm >= 0 && tmpm <= 150) {
            CANParameter* p = getParameterByName("tmpm");
            if (p) p->setValue(tmpm);
        }
        return;
    }

    if (msg.id == 0x373 && msg.length >= 6) {
        CANParameter* p;
        p = getParameterByName("BMS_Vmax");
        if (p) p->setValue((uint16_t)(msg.data[2] | (msg.data[3] << 8)));
        p = getParameterByName("BMS_Vmin");
        if (p) p->setValue((uint16_t)(msg.data[0] | (msg.data[1] << 8)));
        return;
    }

    if (msg.id == 0x126 && msg.length >= 6) {
        CANParameter* p = getParameterByName("tmphs");
        if (p) p->setValue((int16_t)(msg.data[4] | (msg.data[5] << 8)));
        return;
    }

    if (msg.id == 0x257 && msg.length >= 2) {
        int16_t raw = msg.data[0] | (msg.data[1] << 8);
        CANParameter* p = getParameterByName("speed");
        if (p) p->setValue((int32_t)(raw * 0.09f));
        return;
    }

    if (msg.id == 0x210) { return; }
}

// ============================================================================
// handleBMSCellVoltage
// ============================================================================

void CANDataManager::handleBMSCellVoltage(CANMessage& msg) {
    uint8_t baseCell = 0;
    if      (msg.id >= 0x400 && msg.id < 0x500) baseCell = (msg.id - 0x400) * 4;
    else if (msg.id >= 0x600 && msg.id < 0x700) baseCell = (msg.id - 0x600) * 4;
    else return;

    uint8_t cellsInMsg = msg.length / 2;
    if (cellsInMsg > 4) cellsInMsg = 4;

    for (uint8_t i = 0; i < cellsInMsg; i++) {
        uint8_t idx = baseCell + i;
        if (idx < MAX_BMS_CELLS) {
            bmsCellVoltages[idx]    = msg.data[i*2] | (msg.data[i*2+1] << 8);
            bmsCellUpdateTimes[idx] = millis();
            if (idx >= bmsCellCount) bmsCellCount = idx + 1;
        }
    }
}

// ============================================================================
// Cell voltage accessors
// ============================================================================

uint16_t CANDataManager::getCellVoltage(uint8_t cellIndex) {
    return (cellIndex < MAX_BMS_CELLS) ? bmsCellVoltages[cellIndex] : 0;
}

uint32_t CANDataManager::getCellLastUpdate(uint8_t cellIndex) {
    return (cellIndex < MAX_BMS_CELLS) ? bmsCellUpdateTimes[cellIndex] : 0;
}

// ============================================================================
// Queue helpers
// ============================================================================

bool CANDataManager::enqueueTx(CANMessage& msg) {
    uint8_t next = (txHead + 1) % TX_QUEUE_SIZE;
    if (next == txTail) return false;
    txQueue[txHead] = msg;
    txHead = next;
    return true;
}

bool CANDataManager::dequeueTx(CANMessage& msg) {
    if (txHead == txTail) return false;
    msg = txQueue[txTail];
    txTail = (txTail + 1) % TX_QUEUE_SIZE;
    return true;
}

bool CANDataManager::enqueueRx(CANMessage& msg) {
    uint8_t next = (rxHead + 1) % RX_QUEUE_SIZE;
    if (next == rxTail) return false;
    rxQueue[rxHead] = msg;
    rxHead = next;
    return true;
}

bool CANDataManager::dequeueRx(CANMessage& msg) {
    if (rxHead == rxTail) return false;
    msg = rxQueue[rxTail];
    rxTail = (rxTail + 1) % RX_QUEUE_SIZE;
    return true;
}

bool CANDataManager::sendMessage(uint32_t id, uint8_t* data, uint8_t length) {
    CANMessage msg;
    msg.id        = id;
    msg.length    = length;
    msg.timestamp = millis();
    memcpy(msg.data, data, length);
    return enqueueTx(msg);
}

// ============================================================================
// end of CANData.cpp