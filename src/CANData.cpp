#include "CANData.h"
#include "Config.h"
#include "driver/twai.h"

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

    // Real ZombieVerter SDO values are fixed-point ×32, so divide back to engineering units.
    // Update by SDO param ID (1-99 range, VCU's short IDs)
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

    if (!sdoManager.init(onSDOResult)) {
        Serial.println("[CAN] SDOManager init failed");
        return false;
    }
    return true;
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
    // Only SDO-poll params with short VCU IDs (< 1000)
    if (paramId >= 1000) return;
    sdoManager.requestRead((uint8_t)paramId);
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
            p.id       = param["id"].as<uint16_t>();
            strncpy(p.name, kv.key().c_str(), 31);
            p.name[31] = '\0';
            p.editable = param["isparam"] | false;
            // Seed value from JSON so UI shows something before CAN updates
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